//Actual88-byte scene export/input mapper/capability publication. No window,
//audio device or network. Terminal progress is an explicit test boundary.
#include "unity_bridge.cpp"
#include <iostream>
namespace {
unsigned checks=0;
void check(bool v,const char* why){++checks;if(!v)throw std::runtime_error(why);}
std::string utf8(const fs::path&p){auto s=p.u8string();return {s.begin(),s.end()};}
}
int main(int argc,char**argv)try{
 if(argc!=3)throw std::runtime_error("Native assets and isolated evidence root required");
 static_assert(sizeof(Idas3UnityInput)==88);check(sizeof(Idas3UnityInput)==88,"Input ABI changed");
 const auto out=fs::absolute(argv[2])/("run-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 check(!fs::exists(out),"Preserve fixture evidence");fs::create_directories(out);
 const auto assets=utf8(fs::absolute(argv[1])),saves=utf8(out/"userdata");
 check(Idas3SceneInitialize(assets.c_str(),saves.c_str(),640,480)==1,"Exported scene initialization failed");
 auto& runtime=unityRuntime();auto&a=*runtime.app;a.audio.enabled=false;a.replayRecordingFlags=0;a.diagnosticCaptureOff=true;a.demoPreparationStarted=true;
 a.frontend.car=0;a.frontend.battleProfile=idas3::original::makeOriginalFreshBattleProfile();
 Idas3UnityInput input{};input.size=sizeof(input);input.flags=1;input.padConnected=1;
 Idas3UnityPedals pedals{sizeof(pedals),1,3,0,0};
 const auto submit=[&](double dt){input.deltaSeconds=dt;check(Idas3SceneStepWithPedals(&input,&pedals)==1,"Actual exported submission failed");};
 const auto status=[&]{Idas3UnityStatus s{};s.size=sizeof(s);check(Idas3UnityGetStatus(&s)==1,"Actual exported status failed");return s;};
 input.keys[VK_F5/32]=1u<<(VK_F5%32);submit(0);for(auto&k:input.keys)k=0;
 check(a.vsActive&&a.originalShowcaseUsesDrivingClock(),"Exported startup missed source StandBy");
 //Sample the new held bit while still in StandBy/countdown; no host edge queue.
 input.flags=1|0x4000u;submit(0);check(a.input.cabinetStartHeld,"Held bit14 lost at actual bridge");
 input.flags=1;unsigned pre=0;while(a.race.phase!=RacePhase::Running&&pre++<600)submit(OriginalDrivingClock::step);
 check(a.race.phase==RacePhase::Running,"Exported startup did not reach GO");
 auto rules=a.originalRace.state();rules.progress.index=a.originalRace.rules().goalIndex+100;a.originalRace.restoreNumericalState(rules);
 submit(OriginalDrivingClock::step);check(a.race.phase==RacePhase::Finished&&a.pendingResultSetup,"Terminal producer failed");
 check((status().flags&0x10000000u)!=0,"Source Finish capability absent");
 const auto goalFrame=a.originalSession.contactCompletion().elapsedFrames0C900E84;
 //Pause/focus blockers clear the logical held bit even if an untrusted caller
 //retains it. The managed binding layer independently guards physical release.
 input.flags=3|0x4000u;submit(.06);check(!a.input.cabinetStartHeld&&a.originalSession.contactCompletion().elapsedFrames0C900E84==goalFrame,"Blocked bit14 or solver leaked");
 input.flags=0x4000u;submit(.06);check(!a.input.cabinetStartHeld&&a.originalSession.contactCompletion().elapsedFrames0C900E84==goalFrame,"Unfocused bit14 or solver leaked");
 input.flags=1;submit(0);
 //A press/release entirely between source callbacks cannot be queued at120.
 input.flags=1|0x4000u;submit(0);input.flags=1;submit(0);
 unsigned n=0;while((!a.offlineFinish.active()||a.offlineFinish.counter<119)&&n++<130)submit(OriginalDrivingClock::step);
 check(a.offlineFinish.counter==119,"Minimum boundary fixture drifted");
 //Raw menu/control aliases have no logicalStart bit; they cannot manually
 //skip the new owner even when the actual native commands see them.
 input.padButtons=XINPUT_GAMEPAD_START|XINPUT_GAMEPAD_A|XINPUT_GAMEPAD_B|XINPUT_GAMEPAD_X;
 for(int key:std::array<int,4>{VK_RETURN,VK_ESCAPE,'E','Q'})input.keys[key/32]|=1u<<(key%32);
 submit(OriginalDrivingClock::step);submit(OriginalDrivingClock::step);
 check(!a.input.cabinetStartHeld&&a.offlineFinish.phase==original::OriginalOfflineFinishPresentation::Phase::Hold&&a.offlineFinish.counter==121,"Raw Enter/Escape/pad/shift alias skipped Finish");
 input.padButtons=0;for(auto&k:input.keys)k=0;input.flags=1|0x4000u;
 submit(OriginalDrivingClock::step);check(a.offlineFinish.phase==original::OriginalOfflineFinishPresentation::Phase::Exit,"Fresh logical Start did not exit after120");
 for(unsigned i=0;i<5;++i)submit(OriginalDrivingClock::step);
 check((status().flags&0x10000000u)!=0&&a.offlineFinish.blank,"Exit dropped controls before final solver");
 submit(OriginalDrivingClock::step);
 check(a.offlineFinishParentStep==1&&!a.offlineFinish.blank&&(status().flags&0x10000000u)==0,"Close retained driving capability / blank");
 const auto finalFrame=a.originalSession.contactCompletion().elapsedFrames0C900E84;
 check(finalFrame==goalFrame+129,"Actual export final solver ordinal differs");
 input.flags=1;submit(.1);
 check(a.offlineFinishParentStep==3&&a.timeAttackVisitActive&&a.timeAttackAnalysisPrepared,"Same-batch Close/result Init missing");
 check(a.originalSession.contactCompletion().elapsedFrames0C900E84==finalFrame&&(status().flags&0x10000000u)==0,"Result dispatch/excess batch callbacks resumed solver");
 const auto digest=a.originalSession.rollbackDigest(true);submit(0);submit(.1);
 check(a.originalSession.rollbackDigest(true)==digest&&a.originalSession.contactCompletion().elapsedFrames0C900E84==finalFrame,"Later result frames advanced car/advice RNG");
 check(Idas3SceneShutdown()==1,"Exported scene shutdown failed");
 std::cout<<"PASS "<<checks<<" actual88-byte exported Start/focus/block/alias/source-edge and Finish/Close capability checks. No source resultMain-cadence claim.\n";
}catch(const std::exception&e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}
