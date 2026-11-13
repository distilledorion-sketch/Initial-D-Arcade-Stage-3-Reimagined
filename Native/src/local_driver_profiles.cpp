#include "local_driver_profiles.h"
#include "time_attack_season.h"
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
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace idas3 {
namespace {
constexpr std::array<std::uint8_t,8> magic{'I','D','3','P','R','F','1',0};
constexpr std::size_t legacyHeaderSize=24,headerSize=28,payloadWords=307;
using Bytes=std::array<std::uint8_t,headerSize+payloadWords*4>;
std::uint32_t word(const Bytes& bytes,std::size_t offset){
    std::uint32_t value=0;for(unsigned i=0;i<4;++i)value|=std::uint32_t(bytes[offset+i])<<(i*8);return value;
}
void put(Bytes& bytes,std::size_t offset,std::uint32_t value){for(unsigned i=0;i<4;++i)bytes[offset+i]=std::uint8_t(value>>(i*8));}
std::uint32_t checksum(const Bytes& bytes,std::size_t size=Bytes{}.size()){
    std::uint32_t hash=2166136261u;
    for(std::size_t i=0;i<size;++i)if(i<20||i>=24){hash^=bytes[i];hash*=16777619u;}
    return hash;
}
bool valid(unsigned car,const original::OriginalBattleProfile& p){
    if(car>=35||p.u(16)!=car||p.u(0)>3||p.u(4)>9||p.u(8)>1||p.u(12)>1||p.u(20)>=35||p.u(24)>=31||p.u(28)>=20||p.u(32)>1)return false;
    for(unsigned i=0;i<9;++i)if(p.u(80+i*4)>6)return false;
    for(unsigned i=0;i<8;++i)if(p.u(1080+i*4)>16)return false;
    return true;
}
bool read(const std::filesystem::path& file,unsigned car,original::OriginalBattleProfile& result,
          unsigned& season,bool& future){
    future=false;
    std::ifstream input(file,std::ios::binary|std::ios::ate);if(!input)return false;Bytes bytes{};
    const auto size=input.tellg();
    if(size<std::streamoff(legacyHeaderSize))return false;
    input.seekg(0);input.read(reinterpret_cast<char*>(bytes.data()),legacyHeaderSize);
    if(!input||!std::equal(magic.begin(),magic.end(),bytes.begin())||word(bytes,12)!=car||word(bytes,16)!=payloadWords)return false;
    const auto version=word(bytes,8);
    if(version>2){future=true;return false;}
    if(size>std::streamoff(bytes.size()))return false;
    input.read(reinterpret_cast<char*>(bytes.data()+legacyHeaderSize),std::streamsize(size)-legacyHeaderSize);
    if(!input)return false;
    const auto payloadOffset=version==1?legacyHeaderSize:headerSize;
    if((version!=1&&version!=2)||size!=std::streamoff(payloadOffset+payloadWords*4)||word(bytes,20)!=checksum(bytes,std::size_t(size)))return false;
    season=version==1?1:word(bytes,24);
    if(season>timeAttackSeason){future=true;return false;}if(!season)return false;
    original::OriginalBattleProfile candidate;
    for(unsigned i=0;i<payloadWords;++i)candidate.words[i]=word(bytes,payloadOffset+i*4);
    if(!valid(car,candidate))return false;result=candidate;return true;
}
bool archiveSeason(const std::filesystem::path& file){
    const auto archive=std::filesystem::path(file.wstring()+L".before-time-attack-season-"+std::to_wstring(timeAttackSeason)+L".bak");
    std::error_code error;const auto state=std::filesystem::symlink_status(archive,error);
    if(error&&error!=std::errc::no_such_file_or_directory)return false;
    if(state.type()!=std::filesystem::file_type::not_found)
        return !error&&std::filesystem::is_regular_file(state);
    auto pending=archive;pending+=L".tmp";
    error.clear();std::filesystem::copy_file(file,pending,std::filesystem::copy_options::overwrite_existing,error);
    if(error)return false;
    std::filesystem::rename(pending,archive,error);return !error;
}
bool replace(const std::filesystem::path& from,const std::filesystem::path& to){
#ifdef _WIN32
    return MoveFileExW(from.c_str(),to.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    std::error_code error;std::filesystem::rename(from,to,error);return !error;
#endif
}
class ProfileWriteLock {
#ifdef _WIN32
    HANDLE handle_=INVALID_HANDLE_VALUE;
#else
    int handle_=-1;
#endif
public:
    explicit ProfileWriteLock(const std::filesystem::path& file){
        const auto lockFile=std::filesystem::path(file.wstring()+L".lock");
#ifdef _WIN32
        handle_=CreateFileW(lockFile.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
#else
        handle_=open(lockFile.c_str(),O_RDWR|O_CREAT,0600);
        if(handle_>=0&&flock(handle_,LOCK_EX|LOCK_NB)!=0){close(handle_);handle_=-1;}
#endif
    }
    ProfileWriteLock(const ProfileWriteLock&)=delete;
    ProfileWriteLock& operator=(const ProfileWriteLock&)=delete;
    explicit operator bool()const{
#ifdef _WIN32
        return handle_!=INVALID_HANDLE_VALUE;
#else
        return handle_>=0;
#endif
    }
    ~ProfileWriteLock(){
#ifdef _WIN32
        if(handle_!=INVALID_HANDLE_VALUE)CloseHandle(handle_);
#else
        if(handle_>=0){flock(handle_,LOCK_UN);close(handle_);}
#endif
    }
};
}
std::filesystem::path LocalDriverProfiles::path(unsigned car)const{
    if(car>=35)throw std::out_of_range("Local driver car must be0..34");
    return directory_/(std::string("car_")+(car<10?"0":"")+std::to_string(car)+".profile");
}
LocalDriverProfiles::Loaded LocalDriverProfiles::load(unsigned car)const{
    const auto file=path(car),backup=std::filesystem::path(file.wstring()+L".previous");
    Loaded result{original::makeOriginalFreshBattleProfile(),Origin::Fresh};result.profile.setu(16,car);
    unsigned season=0;bool future=false;
    const auto accepted=[&](Origin origin){result.origin=origin;result.needsTimeAttackSeasonMigration=season<timeAttackSeason;
        if(result.needsTimeAttackSeasonMigration)clearPreviousSeasonTimeAttack(result.profile);return result;};
    if(read(file,car,result.profile,season,future))return accepted(Origin::Saved);
    if(future){result.origin=Origin::Unreadable;return result;}
    if(read(backup,car,result.profile,season,future))return accepted(Origin::Backup);
    if(std::filesystem::exists(file)||std::filesystem::exists(backup))result.origin=Origin::Unreadable;
    return result;
}
bool LocalDriverProfiles::save(unsigned car,const original::OriginalBattleProfile& profile)const{
    const auto file=path(car);if(!valid(car,profile))return false;
    std::error_code error;std::filesystem::create_directories(directory_,error);if(error)return false;
    ProfileWriteLock lock(file);if(!lock)return false;
    return saveLocked(car,profile);
}
LocalDriverProfiles::ConditionalSave LocalDriverProfiles::saveExpected(unsigned car,
        const original::OriginalBattleProfile& before,const original::OriginalBattleProfile& after)const{
    const auto file=path(car);
    if(!valid(car,before)||!valid(car,after))return ConditionalSave::Invalid;
    std::error_code error;std::filesystem::create_directories(directory_,error);if(error)return ConditionalSave::IoError;
    ProfileWriteLock lock(file);if(!lock)return ConditionalSave::IoError;
    const auto current=load(car);
    if(current.origin==Origin::Unreadable)return ConditionalSave::Unreadable;
    if(current.profile.words==after.words){
        // A historical backup must be repaired before recovery is acknowledged.
        if(current.origin==Origin::Backup&&!saveLocked(car,after))return ConditionalSave::IoError;
        return ConditionalSave::AlreadySaved;
    }
    if(current.profile.words!=before.words)return ConditionalSave::Conflict;
    return saveLocked(car,after)?ConditionalSave::Saved:ConditionalSave::IoError;
}
bool LocalDriverProfiles::saveLocked(unsigned car,const original::OriginalBattleProfile& profile)const{
    const auto file=path(car);if(!valid(car,profile))return false;
    std::error_code error;std::filesystem::create_directories(directory_,error);if(error)return false;
    const auto suffix=L".new-"+std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto temporary=std::filesystem::path(file.wstring()+suffix),backup=std::filesystem::path(file.wstring()+L".previous");
    const auto backupTemporary=std::filesystem::path(backup.wstring()+suffix);
    const auto cleanup=[&](){std::error_code ignored;std::filesystem::remove(temporary,ignored);std::filesystem::remove(backupTemporary,ignored);};
    // Preserve the complete old card before rewriting only its Time Attack
    // fields. Restoring an archived v1 card will be filtered again on load.
    for(const auto& prior:{file,backup}){
        original::OriginalBattleProfile ignored;unsigned season=0;bool future=false;
        const bool readable=read(prior,car,ignored,season,future);
        if(future)return false;
        if(readable&&season<timeAttackSeason&&!archiveSeason(prior))return false;
    }
    Bytes bytes{};std::copy(magic.begin(),magic.end(),bytes.begin());put(bytes,8,2);put(bytes,12,car);put(bytes,16,payloadWords);put(bytes,24,timeAttackSeason);
    for(unsigned i=0;i<payloadWords;++i)put(bytes,headerSize+i*4,profile.words[i]);put(bytes,20,checksum(bytes));
    std::ofstream output(temporary,std::ios::binary|std::ios::trunc);if(!output)return false;
    output.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());output.close();if(!output){cleanup();return false;}
    original::OriginalBattleProfile previous;unsigned previousSeason=0;bool future=false;
    if(read(file,car,previous,previousSeason,future)){
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
