#include "local_driver_profiles.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace idas3;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void append(const std::filesystem::path& file){std::ofstream out(file,std::ios::binary|std::ios::app);out.put('!');}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("scratch parent directory required");
    const auto directory=std::filesystem::path(argv[1])/("profiles-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    LocalDriverProfiles store(directory);
    for(unsigned car=0;car<35;++car){
        auto loaded=store.load(car);require(loaded.origin==LocalDriverProfiles::Origin::Fresh,"missing profile not fresh");
        auto p=loaded.profile;require(p.u(16)==car,"fresh profile has wrong car");
        p.setByte(116+car%31,std::uint8_t(0x10+car));p.words[150]=0x87123456u+car;
        require(store.save(car,p),"initial save failed");
        LocalDriverProfiles restarted(directory);auto fromDisk=restarted.load(car);
        require(fromDisk.origin==LocalDriverProfiles::Origin::Saved&&fromDisk.profile.words==p.words,"full profile did not survive restart");
        const auto previous=p;p.setByte(116+car%31,std::uint8_t(0x20+car));require(store.save(car,p),"replacement save failed");
        require(store.load(car).profile.words==p.words,"replacement contents wrong");
        append(store.path(car));auto recovered=store.load(car);
        require(recovered.origin==LocalDriverProfiles::Origin::Backup&&recovered.profile.words==previous.words,"corruption did not recover backup");
        require(store.save(car,recovered.profile),"backup recovery save failed");
        require(store.load(car).profile.words==previous.words,"recovered primary wrong");
        auto invalid=previous;invalid.setu(16,(car+1)%35);require(!store.save(car,invalid),"wrong-owner profile accepted");
        require(store.load(car).profile.words==previous.words,"rejected save changed original");
    }
    append(store.path(0));append(std::filesystem::path(store.path(0).wstring()+L".previous"));
    const auto damaged=store.load(0);require(damaged.origin==LocalDriverProfiles::Origin::Unreadable,"two invalid copies were accepted");
    require(damaged.profile.words==original::makeOriginalFreshBattleProfile().words,"invalid save leaked partial fields into fresh profile");
    std::ofstream(directory/"car_01.profile.new-abandoned")<<"partial";
    require(store.load(1).origin==LocalDriverProfiles::Origin::Saved,"unfinished save displaced committed profile");
    bool rejected=false;try{store.load(35);}catch(const std::out_of_range&){rejected=true;}require(rejected,"invalid car path accepted");
    unsigned preserved=0;for(const auto& entry:std::filesystem::directory_iterator(directory))if(entry.path().filename().string().find(".unreadable.new-")!=std::string::npos)++preserved;
    require(preserved==35,"unreadable originals were not preserved");
    std::filesystem::remove_all(directory);
    std::cout<<"PASS35 independent car profiles, exact1228-byte restart/replace, backup recovery, rejected-owner preservation and incomplete-save isolation.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
