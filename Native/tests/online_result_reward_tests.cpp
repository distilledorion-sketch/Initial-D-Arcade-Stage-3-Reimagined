#include "online_result_reward.h"
#include "local_save_slots.h"
#include "online_car_selection.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace idas3;
namespace fs=std::filesystem;
namespace {
unsigned checks=0;
void require(bool value,const char* text){if(!value)throw std::runtime_error(text);++checks;}
template<class F>void rejects(F action,const char* text){bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}require(rejected,text);}
auto profile(unsigned car,unsigned balance){auto p=original::makeOriginalFreshBattleProfile();p.setu(16,car);p.setu(72,balance);return p;}
auto bytes(const fs::path& path){std::ifstream input(path,std::ios::binary);return std::string(std::istreambuf_iterator<char>(input),{});}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("private scratch parent required");
    const auto root=fs::absolute(argv[1])/("online-rewards-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    require(fs::create_directories(root),"scratch root must be new");
    OnlineResultReward rewards(root);LocalDriverProfiles legacy(root/"driver_profiles_v1");
    const auto original=profile(0,321);require(legacy.save(0,original),"baseline save failed");
    auto selected=original;selected.words[150]=0x87654321;selected.setu(72,400);
    const auto primary=bytes(legacy.path(0));const auto stamp=fs::last_write_time(legacy.path(0));
    const auto win=rewards.prepare(5,selected,true);
    require(win.before.words==original.words&&win.after.words[150]==selected.words[150],"disk and in-memory profile snapshots were conflated");
    require(win.points==2000&&win.after.u(72)==2400,"winner award incorrect");
    require(primary==bytes(legacy.path(0))&&stamp==fs::last_write_time(legacy.path(0)),"preparation changed saved progress");
    rewards.apply(win);require(legacy.load(0).profile.words==win.after.words,"selected garage profile not preserved exactly");
    const auto awarded=bytes(legacy.path(0));const auto awardedStamp=fs::last_write_time(legacy.path(0));
    OnlineResultReward(root).apply(win);
    require(awarded==bytes(legacy.path(0))&&awardedStamp==fs::last_write_time(legacy.path(0)),"restart repeated award or rewrote profile");
    const auto loss=rewards.prepare(5,win.after,false);rewards.apply(loss);
    require(loss.points==1000&&legacy.load(0).profile.u(72)==3400,"participation award incorrect");
    rejects([&]{rewards.apply(win);},"old result overwrote newer progress");
    require(legacy.load(0).profile.u(72)==3400,"conflict damaged newer progress");

    LocalSaveSlots slots(root/"saves");
    require(slots.adopt(2,profile(3,777)),"slot fixture failed");
    LocalDriverProfiles garage(slots.profileDirectory(2));
    require(garage.save(3,profile(3,777))&&garage.save(0,original),"garage fixture failed");
    const auto primaryCar=bytes(garage.path(3)),legacyCar=bytes(legacy.path(0));
    const auto secondary=rewards.prepare(onlineSlotCarSelection(2,0),original,true);rewards.apply(secondary);
    require(garage.load(0).profile.u(72)==2321,"secondary garage car not credited");
    require(bytes(garage.path(3))==primaryCar&&bytes(legacy.path(0))==legacyCar,"reward touched another car or driver");
    rejects([&]{rewards.prepare(2,original,true);},"mutable primary alias accepted");
    rejects([&]{rewards.prepare(onlineSlotCarSelection(4,0),original,true);},"missing driver slot accepted");
    rejects([&]{rewards.prepare(6,original,true);},"wrong profile owner accepted");
    auto invalid=secondary;invalid.after.setu(16,3);
    rejects([&]{rewards.apply(invalid);},"wrong recovery owner accepted");
    invalid=secondary;invalid.points=2001;rejects([&]{rewards.apply(invalid);},"invalid award accepted");

    auto nearlyFull=profile(8,999999500);require(legacy.save(8,nearlyFull),"cap fixture failed");
    auto capped=rewards.prepare(13,nearlyFull,true);rewards.apply(capped);
    require(capped.points==499&&legacy.load(8).profile.u(72)==999999999,"balance cap or reported earned points incorrect");
    auto noChange=rewards.prepare(13,capped.after,false);const auto fullBytes=bytes(legacy.path(8));
    rewards.apply(noChange);require(noChange.points==0&&bytes(legacy.path(8))==fullBytes,"capped repeat changed points");
    auto overflow=profile(9,0xffffffff);rejects([&]{rewards.prepare(14,overflow,true);},"invalid balance overflow accepted");

    const auto fresh=profile(10,0);const auto first=rewards.prepare(15,fresh,false);rewards.apply(first);rewards.apply(first);
    require(legacy.load(10).profile.u(72)==1000,"fresh legacy profile credited more than once");
    // Corrupted primary may recover from the prepared before-image backup.
    const auto repairRoot=root/"damaged";LocalDriverProfiles repair(repairRoot/"driver_profiles_v1");
    require(repair.save(0,original)&&repair.save(0,original),"repair fixture failed");
    auto repairPlan=OnlineResultReward(repairRoot).prepare(5,original,true);
    std::ofstream(repair.path(0),std::ios::app).put('!');OnlineResultReward(repairRoot).apply(repairPlan);
    require(repair.load(0).origin==LocalDriverProfiles::Origin::Saved&&repair.load(0).profile.u(72)==2321,"recoverable primary not repaired");
    std::ofstream(repair.path(0),std::ios::app).put('!');
    std::ofstream(fs::path(repair.path(0).wstring()+L".previous"),std::ios::app).put('!');
    rejects([&]{OnlineResultReward(repairRoot).prepare(5,original,true);},"unreadable history accepted");
    rejects([&]{OnlineResultReward(repairRoot).apply(repairPlan);},"unreadable history replaced");
    std::cout<<"PASS "<<checks<<" online reward planning/recovery checks; fixtures retained at "<<root<<"\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}
