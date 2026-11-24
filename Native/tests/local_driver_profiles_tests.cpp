#include "local_driver_profiles.h"
#include <array>
#include <chrono>
#include <initializer_list>
#include <iterator>
#include <vector>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace idas3;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void append(const std::filesystem::path& file){std::ofstream out(file,std::ios::binary|std::ios::app);out.put('!');}
std::vector<char> bytes(const std::filesystem::path& file){std::ifstream in(file,std::ios::binary);require(bool(in),"fixture input unreadable");return {std::istreambuf_iterator<char>(in),{}};}
std::array<unsigned,35> sourceMakers(){
    // Original manufacturer roster pointers at0C31D88C, not the production
    // reverse lookup. Includes appended ER34/CP9A/CP9AT/SE3P entries.
    const std::array<std::initializer_list<unsigned>,7> rosters{{
        {0,1,2,3,4,5,6},{7,8,31,9,10,11,12,13,14},{16,15,17,18},
        {19,20,32,33,21},{22,23,24,34,25,26},{27,28,29},{30}
    }};
    std::array<unsigned,35> makers{};std::array<bool,35> seen{};
    for(unsigned maker=0;maker<rosters.size();++maker)for(const auto car:rosters[maker]){
        require(car<seen.size()&&!seen[car],"source roster duplicate or invalid car");seen[car]=true;makers[car]=maker;
    }
    for(const auto present:seen)require(present,"source roster omitted car");return makers;
}
original::OriginalBattleProfile freshFor(unsigned car,unsigned maker){auto p=original::makeOriginalFreshBattleProfile();p.setu(16,car);p.setu(40,maker);return p;}

}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("scratch parent directory required");
    const auto directory=std::filesystem::path(argv[1])/("profiles-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    LocalDriverProfiles store(directory);const auto makers=sourceMakers();
    for(unsigned car=0;car<35;++car){
        auto loaded=store.load(car);require(loaded.origin==LocalDriverProfiles::Origin::Fresh,"missing profile not fresh");
        auto p=loaded.profile;require(p.u(16)==car,"fresh profile has wrong car");
        require(p.u(40)==makers[car],"fresh profile has wrong original manufacturer");
        require(p.words==freshFor(car,makers[car]).words,"fresh manufacturer initialization changed another profile field");
        require(!std::filesystem::exists(store.path(car)),"fresh read created a save file");
        // A committed card may carry historical maker workspace. Loading,
        // replacing or recovering it must not normalize that field or tuning.
        p.setu(40,(makers[car]+1)%7);p.setu(68,1);p.setByte(152,2);p.setByte(164,17);
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
    append(store.path(7));const auto damagedBackup=std::filesystem::path(store.path(7).wstring()+L".previous");append(damagedBackup);
    const auto primaryBytes=bytes(store.path(7)),backupBytes=bytes(damagedBackup);
    const auto damagedNissan=store.load(7);
    require(damagedNissan.origin==LocalDriverProfiles::Origin::Unreadable&&damagedNissan.profile.words==freshFor(7,1).words,"unreadable non-Toyota fallback has wrong manufacturer or leaked saved fields");
    require(bytes(store.path(7))==primaryBytes&&bytes(damagedBackup)==backupBytes,"unreadable load repaired or rewrote saved data");
    std::ofstream(directory/"car_01.profile.new-abandoned")<<"partial";
    require(store.load(1).origin==LocalDriverProfiles::Origin::Saved,"unfinished save displaced committed profile");
    bool rejected=false;try{store.load(35);}catch(const std::out_of_range&){rejected=true;}require(rejected,"invalid car path accepted");
    unsigned preserved=0;for(const auto& entry:std::filesystem::directory_iterator(directory))if(entry.path().filename().string().find(".unreadable.new-")!=std::string::npos)++preserved;
    require(preserved==35,"unreadable originals were not preserved");
    std::filesystem::remove_all(directory);
    std::cout<<"PASS35 source-roster manufacturers, complete fresh-profile invariance, preserved historical maker/tuning, exact1228-byte restart/replace, backup recovery, unreadable-data preservation and incomplete-save isolation.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
