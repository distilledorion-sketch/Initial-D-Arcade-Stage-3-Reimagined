// Actual exported Unity scene submission, App startup and original solver.
// Isolated saves, scene capture only; no window/audio device/network.
#include "../src/unity_bridge.cpp"
#include <iostream>
#include <fstream>
#include <iomanip>
namespace {
std::uint64_t checks=0;
void check(bool v,const char* message){++checks;if(!v)throw std::runtime_error(message);}
std::string utf8(const fs::path& p){const auto s=p.u8string();return {s.begin(),s.end()};}
struct Case {const char* name;std::vector<double> deltas;bool pause;};
}
int main(int argc,char**argv)try{
 if(argc!=3)throw std::runtime_error("Native assets and new evidence directory required");
 const auto root=fs::absolute(argv[1]),evidenceRoot=fs::absolute(argv[2]);
 fs::create_directories(evidenceRoot);
 const auto out=evidenceRoot/("run-"+std::to_string(GetCurrentProcessId())+"-"+
  std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 check(!fs::exists(out),"Preserve existing fixture evidence");fs::create_directories(out);
 const auto assets=utf8(root),saves=utf8(out/"userdata");
 check(Idas3SceneInitialize(assets.c_str(),saves.c_str(),640,480)==1,"Actual scene initialize failed");
 auto& runtime=unityRuntime();auto& a=*runtime.app;
 a.audio.enabled=false;a.diagnosticCaptureOff=true;a.replayRecordingFlags=0;a.demoPreparationStarted=true;
 a.controllerResponse=idas3::original::ControllerResponse::FlycastGamepad;
 a.frontend.car=0;a.frontend.battleProfile=idas3::original::makeOriginalFreshBattleProfile();
 a.frontend.automatic=a.automatic=false;a.frontend.night=a.night=false;
 Idas3UnityInput input{};input.size=sizeof(input);input.flags=1;input.padConnected=1;
 Idas3UnityPedals pedals{sizeof(pedals),1,3,0,0};
 constexpr double step=idas3::OriginalDrivingClock::step;
 const Case cases[]={{"source",{step},false},{"30hz",{1./30},false},{"60hz",{1./60},false},
  {"144hz",{1./144},false},{"jitter",{0,.004,.031,.009,.065,.012,.003},false},
  {"pause",{.011,.027,.005,.039},true},{"hitch",{.25,.002,.017},false},
  {"handoff-batch",{step},false}};
 std::ofstream report(out/"cadence.csv");report<<"case,submission,delta,expected_source_updates,actual_solver_frame,vs_tick,remaining,gear,accumulator\n";
 bool first=true;
 for(const auto& item:cases){
  if(!first)check(Idas3SceneReturnToCourse()==1,"Actual return to course failed");first=false;
  a.frontend.automatic=a.automatic=false;
  a.originalSession.withSharedRandom([](auto& seed){seed=1306195137;});
  input.deltaSeconds=0;input.keys[VK_F5/32]=1u<<(VK_F5%32);
  check(Idas3SceneStepWithPedals(&input,&pedals)==1,"Actual F5 start failed");
  for(auto& key:input.keys)key=0;
  check(a.vsActive&&a.originalShowcaseUsesDrivingClock(),"Actual original start did not enter source-owned showcase");
  check(a.originalSession.contactCompletion().elapsedFrames0C900E84==60,"Startup warmup count changed");
  check(a.clock.accumulator==0&&a.vsSeconds==0,"New race retained scheduler residue");
  const auto initialTicks=runtime.ticks;
  double accumulator=0;unsigned updates=0,submission=0;bool pausedOnce=false,focusChecked=false,paintChecked=false,crossedCreationSameHost=false;
  while(updates<450){
   if(item.pause&&!pausedOnce&&updates>=100){
    const auto before=a.originalSession.contactCompletion().elapsedFrames0C900E84;
    check(Idas3SceneSetPaused(1)==1,"Exported pause failed");
    for(unsigned i=0;i<4;++i){input.deltaSeconds=.08;check(Idas3SceneStepWithPedals(&input,&pedals)==1,"Paused scene step failed");}
    check(a.originalSession.contactCompletion().elapsedFrames0C900E84==before,"Pause advanced showcase solver");
    check(a.clock.accumulator==0,"Pause did not clear pending host time");
    check(Idas3SceneSetPaused(0)==1,"Exported resume failed");accumulator=0;pausedOnce=true;
   }
   if(item.pause&&!focusChecked&&updates>=300){
    const auto before=a.originalSession.contactCompletion().elapsedFrames0C900E84;
    input.flags=0;input.deltaSeconds=.1;
    check(Idas3SceneStepWithPedals(&input,&pedals)==1,"Focus-loss step failed");
    check(a.paused&&a.clock.accumulator==0&&a.originalSession.contactCompletion().elapsedFrames0C900E84==before,
     "Focus loss advanced countdown or retained pending host time");
    input.flags=1;input.deltaSeconds=0;
    check(Idas3SceneStepWithPedals(&input,&pedals)==1&&a.paused,"Focus restore bypassed explicit resume");
    check(Idas3SceneSetPaused(0)==1,"Resume after focus loss failed");accumulator=0;focusChecked=true;
   }
   input.deltaSeconds=item.deltas[submission%item.deltas.size()];++submission;
   if(std::string(item.name)=="handoff-batch"&&updates==239)input.deltaSeconds=4.25*step;
   accumulator+=std::clamp(input.deltaSeconds,0.,.1);unsigned expectedSteps=0;
   while(accumulator+1e-12>=step&&expectedSteps<6){accumulator=std::max(0.,accumulator-step);++expectedSteps;}
   const auto prior=a.originalSession.contactCompletion().elapsedFrames0C900E84;
   check(Idas3SceneStepWithPedals(&input,&pedals)==1,"Actual cadence scene submission failed");
   if(updates<241&&updates+expectedSteps>=242)crossedCreationSameHost=true;
   updates+=expectedSteps;
   const auto actual=a.originalSession.contactCompletion().elapsedFrames0C900E84;
   check(actual==60+updates&&actual-prior==expectedSteps,"Missing or duplicate solver at actual export boundary");
   check(std::abs(a.clock.accumulator-accumulator)<1e-10,"Source accumulator lost or duplicated across showcase handoff");
   check(a.vsActive==(updates<241),"Showcase child count changed with render cadence");
   check(a.vsBanner.sourceTick()==std::min(updates,241u),"Render advanced showcase child twice");
   check(a.originalRaceCountdownInitPending==(updates==241),"PreInit creation token escaped its source update");
   const auto countdownTicks=updates>242?std::min(updates-242,240u):0u;
   check(a.originalRaceStart.remaining()==240-countdownTicks,"Source countdown drifted at host handoff");
   check(a.originalRaceOwnerFrame==(updates>241?updates-241:0),"Countdown owner frame does not follow source dispatch");
   check(a.race.phase==(updates>=422?idas3::RacePhase::Running:idas3::RacePhase::Countdown),"GO did not occur at original solver482");
   check(a.originalSession.vehicle().transmission.gear00==(updates>=422?1u:0u),"Neutral/first-gear transition differs from original GO482");
   check(runtime.ticks-initialTicks==(updates>241?updates-241:0),"Legacy bridge race counter double-counted showcase");
   if(!paintChecked&&updates>=80&&updates<120){
    const auto seed=a.originalSession.sharedRandomSeed(),tick=a.vsBanner.sourceTick();
    const auto digest=a.originalSession.rollbackDigest(true);
    check(a.render(.1)&&a.render(.1),"Read-only positive-delta showcase repaint failed");
    check(a.originalSession.contactCompletion().elapsedFrames0C900E84==actual&&
     a.originalSession.sharedRandomSeed()==seed&&a.vsBanner.sourceTick()==tick&&
     a.originalSession.rollbackDigest(true)==digest,
     "Extra render advanced source-owned showcase or solver");paintChecked=true;
   }
   report<<item.name<<','<<submission<<','<<std::setprecision(17)<<input.deltaSeconds<<','<<updates<<','<<actual<<','
    <<a.vsBanner.sourceTick()<<','<<a.originalRaceStart.remaining()<<','<<a.originalSession.vehicle().transmission.gear00<<','<<a.clock.accumulator<<'\n';
   check(submission<3000,"Cadence run failed to progress");
  }
  check(paintChecked,"Cadence did not cover extra render check");
  if(std::string(item.name)=="handoff-batch")check(crossedCreationSameHost,"Fixture did not cross showcase and creation in one catch-up batch");
  if(item.pause)check(pausedOnce&&focusChecked,"Fixture missed pause/focus coverage");
  std::cout<<"PASS "<<item.name<<" submissions="<<submission<<" source_updates="<<updates<<"\n"<<std::flush;
 }
 // These are explicit shallow scope tests, not imported/online race claims.
 for(unsigned mode=0;mode<6;++mode){
  auto gate=std::make_unique<App>();gate->vsActive=true;gate->originalHandling=true;gate->courseIndex=3;
  if(mode==0)gate->importedCourse.emplace();if(mode==1)gate->multiplayer.active=true;
  if(mode==2)gate->replayPlaybackActive=true;if(mode==3)gate->originalHandling=false;
  if(mode==4)gate->courseIndex=-1;if(mode==5)gate->courseIndex=9;
  check(!gate->originalShowcaseUsesDrivingClock(),"Extension/replay entered source showcase clock");
  if(mode<2){
   gate->vsBanner=a.vsBanner;gate->vsBanner.begin({});gate->startShowcaseCamera=a.startShowcaseCamera;
   gate->advanceStartPresentation(.5/60.);check(gate->vsBanner.sourceTick()==0,"Legacy showcase consumed a half tick");
   gate->advanceStartPresentation(.5/60.);check(gate->vsBanner.sourceTick()==1,"Legacy showcase lost its60Hz contract");
   gate->advanceStartPresentation(.25);check(gate->vsBanner.sourceTick()==16,"Legacy showcase lost its250ms presentation catch-up");
  }
 }
 check(Idas3SceneShutdown()==1,"Scene shutdown failed");
 std::cout<<"PASS "<<checks<<" exported bridge source showcase/cadence/handoff checks.\n";
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<"\n";try{std::cerr<<unityRuntime().error<<'\n';}catch(...){}return 1;}
