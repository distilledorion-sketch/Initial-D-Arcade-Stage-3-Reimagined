#include "local_driver_setup.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
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
using Bytes=std::array<std::uint8_t,32>;
constexpr std::array<std::uint8_t,8> magic{'I','D','3','S','E','T','1',0};
std::uint32_t word(const Bytes& b,unsigned offset){std::uint32_t value=0;for(unsigned i=0;i<4;++i)value|=std::uint32_t(b[offset+i])<<(i*8);return value;}
void put(Bytes& b,unsigned offset,std::uint32_t value){for(unsigned i=0;i<4;++i)b[offset+i]=std::uint8_t(value>>(i*8));}
std::uint32_t checksum(const Bytes& b){std::uint32_t value=2166136261u;for(unsigned i=0;i<28;++i){value^=b[i];value*=16777619u;}return value;}
bool read(const std::filesystem::path& file,unsigned car){
    std::ifstream in(file,std::ios::binary);if(!in)return false;Bytes b{};in.read(reinterpret_cast<char*>(b.data()),b.size());
    return in&&in.peek()==std::char_traits<char>::eof()&&std::equal(magic.begin(),magic.end(),b.begin())&&word(b,8)==1&&word(b,12)==car&&word(b,16)==1&&word(b,20)==0&&word(b,24)==0&&word(b,28)==checksum(b);
}
bool replace(const std::filesystem::path& from,const std::filesystem::path& to){
#ifdef _WIN32
    return MoveFileExW(from.c_str(),to.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    std::error_code error;std::filesystem::rename(from,to,error);return !error;
#endif
}
}
std::filesystem::path LocalDriverSetup::path(unsigned car)const{
    if(car>=35)throw std::out_of_range("Local driver setup car must be0..34");
    return directory_/(std::string("car_")+(car<10?"0":"")+std::to_string(car)+".setup");
}
LocalDriverSetup::Loaded LocalDriverSetup::load(unsigned car)const{
    const auto file=path(car),backup=std::filesystem::path(file.wstring()+L".previous");
    if(read(file,car))return {Status::Complete,false};
    if(read(backup,car))return {Status::Complete,true};
    std::error_code error;const bool primaryExists=std::filesystem::exists(file,error);if(error||primaryExists)return {Status::Unreadable,false};
    const bool backupExists=std::filesystem::exists(backup,error);if(error||backupExists)return {Status::Unreadable,false};
    return {};
}
bool LocalDriverSetup::markComplete(unsigned car)const{
    const auto file=path(car),backup=std::filesystem::path(file.wstring()+L".previous");
    std::error_code error;std::filesystem::create_directories(directory_,error);if(error)return false;
    static std::atomic<std::uint64_t> sequence{0};
    const auto suffix=L".new-"+std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count())+L"-"+std::to_wstring(sequence.fetch_add(1,std::memory_order_relaxed));
    const auto temporary=std::filesystem::path(file.wstring()+suffix),backupTemporary=std::filesystem::path(backup.wstring()+suffix);
    const auto cleanup=[&](){std::error_code ignored;std::filesystem::remove(temporary,ignored);std::filesystem::remove(backupTemporary,ignored);};
    Bytes bytes{};std::copy(magic.begin(),magic.end(),bytes.begin());put(bytes,8,1);put(bytes,12,car);put(bytes,16,1);put(bytes,28,checksum(bytes));
    std::ofstream out(temporary,std::ios::binary|std::ios::trunc);if(!out)return false;out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());out.close();if(!out){cleanup();return false;}
    if(read(file,car)){
        std::filesystem::copy_file(file,backupTemporary,std::filesystem::copy_options::none,error);
        if(error||!replace(backupTemporary,backup)){cleanup();return false;}
    }else{
        const bool exists=std::filesystem::exists(file,error);if(error){cleanup();return false;}
        if(exists){
            // Retain damaged primary bytes for inspection without displacing a
            // valid previous completion marker. They do not confer readiness.
            const auto rejected=std::filesystem::path(file.wstring()+L".unreadable"+suffix);
            std::filesystem::copy_file(file,rejected,std::filesystem::copy_options::none,error);if(error){cleanup();return false;}
        }
    }
    if(!replace(temporary,file)){cleanup();return false;}return true;
}
}
