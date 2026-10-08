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
    std::cout<<"PASS "<<checks<<" personal-best ghost storage/interpolation checks\n";
    return 0;
}
