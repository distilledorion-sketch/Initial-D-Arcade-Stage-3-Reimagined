#include "../src/unity_bridge.cpp"
#include <iostream>
namespace {
unsigned checks{};
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
std::string utf8(const fs::path& p){auto s=p.u8string();return {s.begin(),s.end()};}
}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("assets and isolated output root required");
    static_assert(sizeof(Idas3UnityInput)==88);
    const auto out=fs::absolute(argv[2])/("run-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    check(!fs::exists(out),"Preserve evidence");fs::create_directories(out);
    auto assets=utf8(fs::absolute(argv[1])),saves=utf8(out/"userdata");
    check(Idas3SceneInitialize(assets.c_str(),saves.c_str(),640,480)==1,"Scene initialize failed");
    auto& a=*unityRuntime().app;a.audio.enabled=false;a.replayRecordingFlags=0;
    a.diagnosticCaptureOff=true;a.demoPreparationStarted=true;a.activeSaveSlot=-1;a.loadedProfileCar=0;
    a.frontend.car=0;a.frontend.battleProfile=original::makeOriginalFreshBattleProfile();a.frontend.battleProfile.setByte(1189,1);
    Idas3UnityInput input{};input.size=sizeof(input);input.flags=1;input.padConnected=1;
    Idas3UnityPedals pedals{sizeof(pedals),1,3,0,0};
    auto submit=[&](double dt){input.deltaSeconds=dt;check(Idas3SceneStepWithPedals(&input,&pedals)==1,"Exported frame rejected");};
    auto status=[&]{Idas3UnityStatus s{};s.size=sizeof(s);check(Idas3UnityGetStatus(&s)==1,"Status export failed");return s;};
    auto key=[&](unsigned k,bool on){if(on)input.keys[k/32]|=1u<<(k%32);else input.keys[k/32]&=~(1u<<(k%32));};
    //Normal startup is necessary: validationMode intentionally skips StandBy.
    //SceneInitialize has already bound all settings/profile writes to saves.
    key(VK_F5,true);submit(0);key(VK_F5,false);submit(0);a.validationMode=true;
    check(a.vsActive&&a.sourceCabinetStartReserved()&&(status().flags&0x20000000u)!=0,"Source-card capability absent in actual StandBy");
    unsigned n=0;while((a.vsActive||a.race.phase==RacePhase::Countdown)&&n++<600)submit(OriginalDrivingClock::step);
    check(a.race.phase==RacePhase::Running,"Bridge failed to reach GO");
    // Actual canonical View comes from the mapped C/Y packet, not a second
    // physical device poll. A between-source pulse must not advance counters.
    input.flags=1|0x4000;key('C',true);submit(0);input.flags=1;key('C',false);submit(0);
    check(a.cabinetButtons.start==0&&a.cabinetButtons.view==0,"Render-only pulse reached source counters");
    const auto sourceFrame=a.originalSession.contactCompletion().elapsedFrames0C900E84;
    input.flags=0x4000;key('C',true);submit(.06);
    check(!a.input.cabinetStartHeld&&a.originalSession.contactCompletion().elapsedFrames0C900E84==sourceFrame,"Unfocused Start/solver leaked");
    input.flags=3|0x4000;submit(0);check(!a.input.cabinetStartHeld,"Modal bit retained logical Start");
    input.flags=1;key('C',false);submit(0);a.paused=false;submit(OriginalDrivingClock::step);
    check(a.cabinetButtons.start==0&&a.cabinetButtons.view==0,"Neutral release failed after modal/focus");
    // Real host input path: a modal neutral packet after one View update may
    // not turn into a short-release camera action, including after resume.
    const auto interruptedView=a.drivingView;
    key('C',true);submit(OriginalDrivingClock::step);
    check(a.cabinetButtons.view==1,"Bridge modal View fixture missed hold");
    input.flags=3;key('C',false);submit(OriginalDrivingClock::step);
    check(a.drivingView==interruptedView&&a.cabinetButtons.view==0&&a.cabinetButtons.releasedView==0,"Modal neutralization leaked View action/history");
    input.flags=1;submit(OriginalDrivingClock::step);
    check(a.drivingView==interruptedView,"Modal View deferred until resume");
    key('C',true);submit(OriginalDrivingClock::step);
    check(a.cabinetButtons.view==1,"Bridge focus View fixture missed hold");
    input.flags=0;key('C',false);submit(OriginalDrivingClock::step);
    check(a.paused&&a.cabinetButtons.view==0&&a.cabinetButtons.releasedView==0,"Focus loss retained cabinet history");
    input.flags=1;submit(0);a.paused=false;submit(OriginalDrivingClock::step);
    check(a.drivingView==interruptedView,"Focus View deferred until resume");
    // Default physical Start has a logical held bit supplied by managed input.
    // Its raw host alias must not open Pause in this source-card context.
    input.flags=1|0x4000;input.padButtons=XINPUT_GAMEPAD_START;
    submit(OriginalDrivingClock::step);submit(OriginalDrivingClock::step);
    check(!a.paused&&a.cabinetButtons.start==2&&a.cabinetButtons.view==0,"Start-first chord lost to native Pause");
    const auto view=a.drivingView;input.padButtons|=XINPUT_GAMEPAD_Y;submit(OriginalDrivingClock::step);
    check(a.cabinetButtons.start==4&&a.cabinetButtons.view==4&&a.originalRace.state().remaining.value==0,"Canonical pad View did not complete Start-first chord");
    check(a.drivingView==view&&a.race.phase==RacePhase::Running&&!a.hostRetireWaitForStop,"Bridge chord toggled View or used host Retire");
    input.flags=1;input.padButtons=0;submit(OriginalDrivingClock::step);
    check(a.originalRace.state().automaticBrake,"Bridge zero-entry brake missing");
    // No mode capability inference from originalHandling alone.
    for(unsigned exclusion=0;exclusion<3;++exclusion){
        a.multiplayer.active=exclusion==0;a.replayPlaybackActive=exclusion==1;if(exclusion==2)a.importedCourse.emplace();
        publish(unityRuntime(),0);check((status().flags&0x20000000u)==0,"Excluded mode published source-card capability");
        a.multiplayer.active=false;a.replayPlaybackActive=false;a.importedCourse.reset();
    }
    check(Idas3SceneShutdown()==1,"Shutdown failed");
    std::cout<<"PASS "<<checks<<" actual88-byte bridge/capability/Start-first/focus/modal/extension checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}
