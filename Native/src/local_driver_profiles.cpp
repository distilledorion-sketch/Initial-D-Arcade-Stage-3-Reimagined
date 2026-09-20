#include "local_driver_profiles.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace idas3 {
namespace {
constexpr std::array<std::uint8_t,8> magic{'I','D','3','P','R','F','1',0};
constexpr std::size_t headerSize=24,payloadWords=307;
using Bytes=std::array<std::uint8_t,headerSize+payloadWords*4>;
std::uint32_t word(const Bytes& bytes,std::size_t offset){
    std::uint32_t value=0;for(unsigned i=0;i<4;++i)value|=std::uint32_t(bytes[offset+i])<<(i*8);return value;
}
void put(Bytes& bytes,std::size_t offset,std::uint32_t value){for(unsigned i=0;i<4;++i)bytes[offset+i]=std::uint8_t(value>>(i*8));}
std::uint32_t checksum(const Bytes& bytes){
    std::uint32_t hash=2166136261u;
    for(std::size_t i=0;i<bytes.size();++i)if(i<20||i>=headerSize){hash^=bytes[i];hash*=16777619u;}
    return hash;
}
bool valid(unsigned car,const original::OriginalBattleProfile& p){
    if(car>=35||p.u(16)!=car||p.u(0)>3||p.u(4)>9||p.u(8)>1||p.u(12)>1||p.u(20)>=35||p.u(24)>=31||p.u(28)>=20||p.u(32)>1)return false;
    for(unsigned i=0;i<9;++i)if(p.u(80+i*4)>6)return false;
    for(unsigned i=0;i<8;++i)if(p.u(1080+i*4)>16)return false;
    return true;
}
bool read(const std::filesystem::path& file,unsigned car,original::OriginalBattleProfile& result){
    std::ifstream input(file,std::ios::binary);if(!input)return false;Bytes bytes{};
    input.read(reinterpret_cast<char*>(bytes.data()),bytes.size());
    if(!input||input.peek()!=std::char_traits<char>::eof()||!std::equal(magic.begin(),magic.end(),bytes.begin())||word(bytes,8)!=1||word(bytes,12)!=car||word(bytes,16)!=payloadWords||word(bytes,20)!=checksum(bytes))return false;
    original::OriginalBattleProfile candidate;
    for(unsigned i=0;i<payloadWords;++i)candidate.words[i]=word(bytes,headerSize+i*4);
    if(!valid(car,candidate))return false;result=candidate;return true;
}
bool replace(const std::filesystem::path& from,const std::filesystem::path& to){
#ifdef _WIN32
    return MoveFileExW(from.c_str(),to.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    std::error_code error;std::filesystem::rename(from,to,error);return !error;
#endif
}
}
std::filesystem::path LocalDriverProfiles::path(unsigned car)const{
    if(car>=35)throw std::out_of_range("Local driver car must be0..34");
    return directory_/(std::string("car_")+(car<10?"0":"")+std::to_string(car)+".profile");
}
LocalDriverProfiles::Loaded LocalDriverProfiles::load(unsigned car)const{
    const auto file=path(car),backup=std::filesystem::path(file.wstring()+L".previous");
    Loaded result{original::makeOriginalFreshBattleProfile(),Origin::Fresh};result.profile.setu(16,car);
    if(read(file,car,result.profile)){result.origin=Origin::Saved;return result;}
    if(read(backup,car,result.profile)){result.origin=Origin::Backup;return result;}
    if(std::filesystem::exists(file)||std::filesystem::exists(backup))result.origin=Origin::Unreadable;
    return result;
}
bool LocalDriverProfiles::save(unsigned car,const original::OriginalBattleProfile& profile)const{
    const auto file=path(car);if(!valid(car,profile))return false;
    std::error_code error;std::filesystem::create_directories(directory_,error);if(error)return false;
    const auto suffix=L".new-"+std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto temporary=std::filesystem::path(file.wstring()+suffix),backup=std::filesystem::path(file.wstring()+L".previous");
    const auto backupTemporary=std::filesystem::path(backup.wstring()+suffix);
    const auto cleanup=[&](){std::error_code ignored;std::filesystem::remove(temporary,ignored);std::filesystem::remove(backupTemporary,ignored);};
    Bytes bytes{};std::copy(magic.begin(),magic.end(),bytes.begin());put(bytes,8,1);put(bytes,12,car);put(bytes,16,payloadWords);
    for(unsigned i=0;i<payloadWords;++i)put(bytes,headerSize+i*4,profile.words[i]);put(bytes,20,checksum(bytes));
    std::ofstream output(temporary,std::ios::binary|std::ios::trunc);if(!output)return false;
    output.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());output.close();if(!output){cleanup();return false;}
    original::OriginalBattleProfile previous;
    if(read(file,car,previous)){
        std::filesystem::copy_file(file,backupTemporary,std::filesystem::copy_options::overwrite_existing,error);
        if(error||!replace(backupTemporary,backup)){cleanup();return false;}
    }else if(std::filesystem::exists(file)){
        // Preserve unreadable primary data while retaining any valid backup.
        const auto rejected=std::filesystem::path(file.wstring()+L".unreadable"+suffix);
        std::filesystem::copy_file(file,rejected,std::filesystem::copy_options::none,error);
        if(error){cleanup();return false;}
    }
    if(!replace(temporary,file)){cleanup();return false;}return true;
}
}
