// Actual offline App showcase-to-countdown handoff. Private repeat-safe save roots;
// no window, audio device, network or live user profiles are opened.
#define IDAS3_UNITY_PLUGIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "../src/main.cpp"
#include <iostream>
#include <stdexcept>
using namespace idas3;
unsigned checks=0;
void check(bool v,const char* message){++checks;if(!v)throw std::runtime_error(message);}
std::unique_ptr<App> makeApp(const std::filesystem::path& root,const std::filesystem::path& dir,bool validate,bool fullThrottle=false){
 auto a=std::make_unique<App>();a->root=root;a->saveRoot=dir;a->validationMode=true;a->settings();
 a->audio.enabled=false;a->audio.configure(root);a->originalCamera=OriginalChaseCamera::load(root);a->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
 a->frontend.initialize(root,true);a->frontend.gameMode=original::OriginalGameMode::TimeAttack;a->frontend.car=0;a->frontend.course=a->courseIndex=0;
 a->frontend.battleProfile=original::makeOriginalFreshBattleProfile();a->frontend.automatic=a->automatic=false;
 a->frontend.reverse=a->reverse=false;a->frontend.night=a->night=false;a->frontend.wet=a->wet=false;
 a->input.connected=true;a->input.originalThrottleAnalog=fullThrottle?65407:0;a->input.originalBrakeAnalog=0;a->controllerResponse=original::ControllerResponse::FlycastGamepad;
 a->active=true;a->menu=true;a->loadingActive=false;a->replayRecordingFlags=0;a->diagnosticCaptureOff=true;a->demoPreparationStarted=true;
 a->loadStartPresentation();a->validationMode=validate;a->originalSession.withSharedRandom([](auto& seed){seed=1306195137;});a->start();return a;
}
int main(int argc,char**argv)try{
 if(argc!=3)throw std::runtime_error("root evidenceRoot required");
 const auto root=std::filesystem::absolute(argv[1]),evidenceRoot=std::filesystem::absolute(argv[2]);
 std::filesystem::create_directories(evidenceRoot);
 const auto runId=std::to_string(GetCurrentProcessId())+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
 const auto out=evidenceRoot/("run-"+runId);
 if(std::filesystem::exists(out))throw std::runtime_error("Preserve existing test evidence");std::filesystem::create_directories(out);
 auto a=makeApp(root,out/"full",false);check(a->vsActive,"Actual start must enter showcase");check(!a->originalRaceCountdownInitPending,"Pending before showcase exit");
 unsigned showcase=0;while(a->vsActive){a->advanceHostClock(OriginalDrivingClock::step,[&]{a->simulate(a->driver());});++showcase;if(showcase>242)throw std::runtime_error("showcase did not finish");}
 check(showcase==241,"source showcase241 updates");check(a->originalSession.contactCompletion().elapsedFrames0C900E84==301,"showcase owns one solver each");
 check(a->originalRaceCountdownInitPending,"source handoff creation update missing");
 DriverInput neutral;neutral.automatic=false;neutral.originalSteeringAnalog=32768;neutral.originalThrottleAnalog=neutral.originalBrakeAnalog=0;
 a->simulate(neutral);check(!a->originalRaceCountdownInitPending,"creation update consumed once");check(a->originalRaceStart.remaining()==240,"creation update decremented auxiliary");
 check(a->originalSession.contactCompletion().elapsedFrames0C900E84==302,"creation update must own one solver");check(a->originalSession.vehicle().transmission.gear00==0,"creation update enabled gear");
 for(unsigned update=1;update<=180;++update){a->simulate(neutral);check(a->originalSession.contactCompletion().elapsedFrames0C900E84==302+update,"countdown extra/missing solver");check(a->originalRaceStart.remaining()==240-update,"countdown decrement");check(a->originalSession.vehicle().transmission.gear00==(update==180?1u:0u),"first GO must be solver482");}
 check(a->race.phase==RacePhase::Running,"GO did not start actual race");
 for(unsigned mode=0;mode<5;++mode){auto guard=std::make_unique<App>();guard->audio.enabled=false;guard->vsBanner=a->vsBanner;guard->vsActive=true;guard->originalHandling=mode!=4;
  if(mode==1)guard->importedCourse.emplace();if(mode==2)guard->multiplayer.active=true;if(mode==3)guard->replayPlaybackActive=true;
  if(guard->originalShowcaseUsesDrivingClock()){
   // This shallow scope fixture starts with App's default open menu. The
   // production source callback must respect that gate before testing exit.
   check(guard->menu&&!guard->hostDrivingClockMayAdvance(),"default open-menu gate missing");
   const auto before=guard->vsBanner.sourceTick();
   guard->advanceHostClock(OriginalDrivingClock::step,[]{});
   check(guard->vsActive&&guard->vsBanner.sourceTick()==before&&!guard->originalRaceCountdownInitPending,
       "open-menu callback advanced source showcase");
  }
  // Explicit usable-window/menu-closed boundary; this is not a new App
  // startup or a claim to execute frontend navigation in the shallow guard.
  guard->active=true;guard->menu=false;
  if(guard->originalShowcaseUsesDrivingClock())guard->advanceHostClock(OriginalDrivingClock::step,[]{});else guard->advanceStartPresentation(1./60.);check(!guard->vsActive,"guard fixture did not exit showcase");check(guard->originalRaceCountdownInitPending==(mode==0),"source-only handoff guard");
 }
 // These are actual App starts, with the same explicit seed/profile and
 // accepted raw pedals. Full throttle must not alter the60 warmup solvers.
 auto held=makeApp(root,out/"held-throttle",false,true);
 auto idle=makeApp(root,out/"idle-throttle",false);
 check(held->originalSession.contactCompletion().elapsedFrames0C900E84==60,"held warmup solver count");
 check(held->originalSession.vehicle().controls.throttleHighByte==255&&held->originalSession.vehicle().controls.throttleHighByteAfterMode==0,
     "warmup must preserve raw full throttle and apply its constructor gate");
 check(held->originalSession.vehicle().controls.throttleAlias==0.f,"warmup allowed full throttle");
 check(held->originalSession.vehicle().drive.words==idle->originalSession.vehicle().drive.words&&
     held->originalSession.actor().words==idle->originalSession.actor().words&&
     held->originalSession.vehicle().transmission.filtered18==idle->originalSession.vehicle().transmission.filtered18&&
     held->originalSession.sharedRandomSeed()==idle->originalSession.sharedRandomSeed(),"held warmup altered driving/actor/engine/RNG versus idle");
 for(unsigned repeat=0;repeat<2;++repeat){
  if(repeat){held->start();check(held->originalSession.vehicle().controls.throttleAlias==0.f,"second race retained cleared warmup gate");}
  check(held->vsActive&&held->vsBanner.sourceTick()==0,"fresh showcase counter missing");
  for(unsigned i=0;i<19;++i){
   held->advanceHostClock(OriginalDrivingClock::step,[&]{held->simulate(held->driver());});
   const auto& v=held->originalSession.vehicle();
   check(held->vsBanner.sourceTick()==i+1&&held->originalSession.contactCompletion().elapsedFrames0C900E84==61+i,"early showcase source ownership");
   check(v.controls.throttleHighByte==255&&v.controls.throttleHighByteAfterMode==0&&v.controls.throttleAlias==0.f,"early showcase lost suppression or accepted ADC");
   check(v.transmission.gear00==0,"suppression changed neutral gear ownership");
  }
  const auto before=held->originalSession.rollbackDigest(true);
  held->advanceStartPresentation(.1);held->advanceStartPresentation(0);
  held->advanceHostClock(0,[&]{held->simulate(held->driver());});
  held->paused=true;held->advanceHostClock(OriginalDrivingClock::step,[&]{held->simulate(held->driver());});held->paused=false;
  check(held->vsBanner.sourceTick()==19&&held->originalSession.rollbackDigest(true)==before,"render-only/zero-tick/paused frame advanced suppression owner");
  if(!repeat){
   held->advanceHostClock(OriginalDrivingClock::step,[&]{held->simulate(held->driver());});
   check(held->vsBanner.sourceTick()==20&&held->originalSession.vehicle().controls.throttleAlias==0.f,"20th showcase solver released gate early");
   held->advanceHostClock(OriginalDrivingClock::step,[&]{held->simulate(held->driver());});
  }else{
   // One accepted host batch crosses OLD counters19 and20. No render is
   // needed to release the gate on its second source update.
   held->advanceHostClock(2*OriginalDrivingClock::step,[&]{held->simulate(held->driver());});
  }
  check(held->vsBanner.sourceTick()==21&&held->originalSession.contactCompletion().elapsedFrames0C900E84==81,"release batch source count");
  check(held->originalSession.vehicle().controls.throttleHighByteAfterMode==255&&held->originalSession.vehicle().controls.throttleAlias==1.f,"21st showcase solver did not permit revving");
  check(held->originalSession.vehicle().transmission.gear00==0,"release enabled gear early");
  while(held->vsActive)held->advanceHostClock(OriginalDrivingClock::step,[&]{held->simulate(held->driver());});
  held->simulate(held->driver());
  check(held->originalSession.vehicle().controls.throttleAlias==1.f&&held->originalRaceStart.remaining()==240,"creation update retained suppression");
  for(unsigned i=0;i<180;++i)held->simulate(held->driver());
  check(held->race.phase==RacePhase::Running&&held->originalSession.vehicle().transmission.gear00==1&&held->originalSession.vehicle().controls.throttleAlias==1.f,
      "normal countdown/GO revving or gear ownership changed");
 }
 // A diagnostic start deliberately omits StandBy. Its source warmup still
 // suppresses the pedal, but no absent showcase counter can trap countdown.
 auto directHeld=makeApp(root,out/"direct-held",true,true);
 check(!directHeld->vsActive&&directHeld->originalSession.vehicle().controls.throttleAlias==0.f,"direct warmup gate missing");
 directHeld->simulate(directHeld->driver());
 check(directHeld->originalSession.vehicle().controls.throttleAlias==1.f,"direct no-showcase countdown retained gate");
 auto direct=makeApp(root,out/"direct-validation",true);check(!direct->vsActive&&!direct->originalRaceCountdownInitPending,"direct validation start changed contract");
 for(unsigned i=1;i<=180;++i){direct->simulate(neutral);check(direct->originalSession.vehicle().transmission.gear00==(i==180?1u:0u),"direct isolated countdown changed");}
 direct->originalRaceCountdownInitPending=true;direct->start();check(!direct->originalRaceCountdownInitPending,"new start retained abandoned pending creation update");
 std::cout<<"PASS "<<checks<<" actual App showcase/handoff/first-GO/direct-start/exclusion checks. No window, audio device or network.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
