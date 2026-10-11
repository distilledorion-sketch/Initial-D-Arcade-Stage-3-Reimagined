#include "time_attack_ghost.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace idas3;
int main(){
    unsigned checks=0;
    const auto check=[&](bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);};
    const auto root=std::filesystem::temp_directory_path()/("idas3-ghost-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    const auto file=TimeAttackGhost::path(root/"slot_1",15,false,false);
    Replay run;run.beginCapture(true);run.finishTicks6000=200;
    ReplayDetail a{},b{};a.pitch=.1f;b.pitch=.3f;a.roll=-.2f;b.roll=.2f;
    run.record(1,{1,2,3},3.1f,10,2,&a);run.record(2,{3,4,5},-3.1f,20,3,&b);
    check(TimeAttackGhost::saveBest(file,run,0)==TimeAttackGhost::Saved::Replaced,"First run not saved");
    TimeAttackGhost ghost;check(ghost.load(file),"Ghost reload failed");
    check(ghost.car==0&&ghost.replay.finishTicks6000==200,"Best metadata changed");
    const auto pose=ghost.sample(1.5);
    check(length(pose.position-Vec3{2,3,4})<.00001f,"Position interpolation changed");
    check(std::abs(pose.yaw-pi)<.00001f,"Yaw did not cross wrap smoothly");
    check(std::abs(pose.detail.pitch-.2f)<.00001f&&std::abs(pose.detail.roll)<.00001f,"Slope interpolation changed");
    check(ghost.sample(0).position.x==1&&ghost.sample(200).position.x==3,"Sampling endpoints escaped recording");
    check(TimeAttackGhost::saveBest(file,run,1)==TimeAttackGhost::Saved::Unchanged,"Tie replaced best");
    run.finishTicks6000=150;check(TimeAttackGhost::saveBest(file,run,1)==TimeAttackGhost::Saved::Replaced,"Faster other car not selected");
    run.finishTicks6000=180;check(TimeAttackGhost::saveBest(file,run,2)==TimeAttackGhost::Saved::Unchanged,"Slower other car replaced best");
    check(ghost.load(file)&&ghost.car==1&&ghost.replay.finishTicks6000==150,"Wrong car/time after replacement");
    for(const auto& other:{TimeAttackGhost::path(root/"slot_2",15,false,false),TimeAttackGhost::path(root/"slot_1",15,true,false),
        TimeAttackGhost::path(root/"slot_1",15,false,true),TimeAttackGhost::path(root/"slot_1",9,false,false)})
        check(!ghost.load(other)&&ghost.replay.frames.empty(),"Cross-save/course/conditions ghost leaked");
    run.finishTicks6000=0;check(TimeAttackGhost::saveBest(file,run,0)==TimeAttackGhost::Saved::Failed,"DNF accepted");
    run.finishTicks6000=100;run.truncated=true;check(TimeAttackGhost::saveBest(file,run,0)==TimeAttackGhost::Saved::Failed,"Truncation accepted");
    run.truncated=false;run.frames[0].tick=0;check(TimeAttackGhost::saveBest(file,run,0)==TimeAttackGhost::Saved::Failed,"Missing start accepted");
    run.frames[0].tick=1;run.frames[0].position.x=std::numeric_limits<float>::quiet_NaN();
    check(TimeAttackGhost::saveBest(file,run,0)==TimeAttackGhost::Saved::Failed,"NaN accepted");
    check(ghost.load(file)&&ghost.replay.finishTicks6000==150,"Invalid run damaged best");
    auto corrupt=root/"corrupt.idghost";std::filesystem::copy_file(file,corrupt);
    {std::ofstream out(corrupt,std::ios::binary|std::ios::app);out.put(0);}
    check(!ghost.load(corrupt)&&ghost.replay.frames.empty(),"Trailing corrupt data accepted");
    {std::ofstream out(corrupt,std::ios::binary|std::ios::trunc);out<<"IDG1";for(int i=0;i<12;++i)out.put(char(255));}
    check(!ghost.load(corrupt),"Unbounded allocation accepted");
    run.frames[0].position.x=1;run.finishTicks6000=100;
    for(unsigned course:{16u,17u})for(bool reverse:{false,true})for(bool wet:{false,true}){
        auto legacy=root/"slot_1"/"ghosts_v1"/("course_"+std::to_string(course)+(reverse?"_reverse":"_forward")+(wet?"_wet":"_dry")+".idghost");
        const auto current=TimeAttackGhost::path(root/"slot_1",course,reverse,wet);
        check(TimeAttackGhost::saveBest(legacy,run,0)==TimeAttackGhost::Saved::Replaced,"Legacy Gunsai/Odawara ghost fixture");
        check(current!=legacy&&!ghost.load(current),"Old handling ghost returned after record reset");
        run.finishTicks6000=200;
        check(TimeAttackGhost::saveBest(current,run,1)==TimeAttackGhost::Saved::Replaced,"Old fast ghost blocked new slower record");
        check(ghost.load(current)&&ghost.car==1&&ghost.replay.finishTicks6000==200,"Versioned Gunsai/Odawara ghost restart");
        check(ghost.load(legacy)&&ghost.replay.finishTicks6000==100,"Archived ghost was modified");
        run.finishTicks6000=100;
    }
    check(TimeAttackGhost::path(root,3,false,false)==root/"ghosts_v1"/"course_3_forward_dry.idghost","Unrelated ghost identity changed");
    const auto resetRoot=root/"odawara-reset";
    std::filesystem::create_directories(resetRoot/"replays");
    const auto archivedReplay=resetRoot/"replays"/"driver_odawara_cw_day_dry_tat.idreplay";
    {std::ofstream out(archivedReplay);out<<"Optional replay archive must remain";}
    std::vector<std::filesystem::path> retired,current,unrelated;
    for(int slot=-1;slot<5;++slot){
        const auto profile=slot<0?resetRoot/"driver_profiles_v1":resetRoot/"saves"/("slot_"+std::to_string(slot+1))/"driver_profiles_v1";
        for(bool reverse:{false,true})for(bool wet:{false,true}){
            for(unsigned revision:{0u,1u}){
                const auto old=profile/"ghosts_v1"/(std::string("course_17")+(reverse?"_reverse":"_forward")+(wet?"_wet":"_dry")+(revision?"_r1":"")+".idghost");
                run.finishTicks6000=100;
                check(TimeAttackGhost::saveBest(old,run,34)==TimeAttackGhost::Saved::Replaced,"Retired Odawara personal ghost fixture");
                for(const char* suffix:{"",".tmp",".previous"}){auto copy=old;copy+=suffix;if(*suffix)std::filesystem::copy_file(old,copy);retired.push_back(copy);}
            }
            const auto fresh=TimeAttackGhost::path(profile,17,reverse,wet);run.finishTicks6000=200;
            check(fresh.filename().string().find("_r2.idghost")!=std::string::npos&&TimeAttackGhost::saveBest(fresh,run,1)==TimeAttackGhost::Saved::Replaced,"New slower Odawara ghost uses revision 2");
            current.push_back(fresh);
            for(unsigned course:{3u,16u}){
                const auto other=TimeAttackGhost::path(profile,course,reverse,wet);
                check(TimeAttackGhost::saveBest(other,run,2)==TimeAttackGhost::Saved::Replaced,"Unrelated course ghost fixture");unrelated.push_back(other);
            }
        }
    }
    for(const char* owner:{"original_race_v3_","development_v01_"})for(bool reverse:{false,true})for(bool wet:{false,true})
    for(unsigned car=0;car<35;++car)for(bool automatic:{false,true}){
        const auto key=std::string(owner)+"odawara"+(reverse?"_rev":"_fwd")+(wet?"_wet":"_dry")+"_car"+std::to_string(car)+(automatic?"_at":"_mt");
        const auto old=resetRoot/(key+".csv");
        for(const char* suffix:{"",".tmp",".previous"}){auto copy=old;copy+=suffix;{std::ofstream out(copy);out<<"Retired best telemetry";}retired.push_back(copy);}
        const auto fresh=TimeAttackGhost::telemetryPath(resetRoot,key,17);{std::ofstream out(fresh);out<<"Current best telemetry";}current.push_back(fresh);
    }
    const auto gunsaiKey=std::string("original_race_v3_gunsai_fwd_dry_car0_mt");
    check(TimeAttackGhost::telemetryPath(resetRoot,gunsaiKey,16)==resetRoot/(gunsaiKey+".csv"),"Gunsai best telemetry identity must not change");
    const auto gunsaiTelemetry=resetRoot/(gunsaiKey+".csv");{std::ofstream out(gunsaiTelemetry);out<<"Gunsai sentinel";}unrelated.push_back(gunsaiTelemetry);
    check(TimeAttackGhost::removeRetiredOdawaraGhosts(resetRoot),"Targeted retired Odawara ghost cleanup failed");
    check(std::all_of(retired.begin(),retired.end(),[](const auto& p){return !std::filesystem::exists(p);}),"Old Odawara ghosts or their temp/backup copies remain in a save or per-car variant");
    check(std::all_of(current.begin(),current.end(),[](const auto& p){return std::filesystem::is_regular_file(p);}),"Cleanup removed a new revision-2 Odawara ghost");
    check(std::all_of(unrelated.begin(),unrelated.end(),[](const auto& p){return std::filesystem::is_regular_file(p);})&&std::filesystem::is_regular_file(archivedReplay),"Cleanup removed Gunsai/other-course ghosts or optional replay archives");
    check(TimeAttackGhost::removeRetiredOdawaraGhosts(resetRoot)&&ghost.load(current.front())&&ghost.replay.finishTicks6000==200,"Restart cleanup reset a new slower personal ghost");
    {std::ofstream out(retired.front());out<<"Restored old ghost";}
    check(TimeAttackGhost::removeRetiredOdawaraGhosts(resetRoot)&&!std::filesystem::exists(retired.front()),"Restoring an old ghost bypassed repeat cleanup");
    // Refuse unexpected directory/link entries instead of traversing a tree.
    std::filesystem::create_directory(retired.front());
    const auto sentinel=retired.front()/"preserved.txt";{std::ofstream out(sentinel);out<<"Do not recursively delete";}
    check(!TimeAttackGhost::removeRetiredOdawaraGhosts(resetRoot)&&std::filesystem::is_regular_file(sentinel),"An unexpected directory under an old ghost name was traversed or removed");
    check(!TimeAttackGhost::removeRetiredOdawaraGhosts({}),"An empty save root was accepted for cleanup");
    std::cout<<"PASS "<<checks<<" personal-best ghost storage/interpolation checks\n";
    return 0;
}
