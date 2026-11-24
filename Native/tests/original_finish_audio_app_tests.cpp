#define IDAS3_UNITY_PLUGIN
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "../src/main.cpp"
#include <iostream>
namespace {
unsigned checks=0;
void check(bool ok,const char* what){++checks;if(!ok)throw std::runtime_error(what);}
std::unique_ptr<App> fixture(const fs::path& root){
 auto a=std::make_unique<App>();a->validationMode=true;a->root=fs::absolute(root);a->audio.enabled=false;a->active=true;a->menu=false;a->originalHandling=true;a->courseIndex=0;
 a->frontend.battleProfile=original::makeOriginalFreshBattleProfile();a->battleProfile=a->frontend.battleProfile;a->frontend.car=0;
 original::OriginalDrivingSelection selection;selection.physics=original::makeOriginalFreshTimeAttackSelection(0,0,original::OriginalWeather::Dry);selection.bodyContactEnabled=false;
 const auto pose=original::originalStartPose(0,0);a->originalSession.reset(a->root,selection,pose.position,pose.angles);
 a->audio.configure(a->root);a->audio.selectOriginalEngine(a->root,a->battleProfile);
 auto* app=a.get();a->originalSession.setEngineOutput([app](const original::OriginalContactCompletionEffects& effect,std::uint32_t& seed){
  original::OriginalEngineControlInput in;in.rpm=effect.engineValue;in.throttle=effect.throttle;in.gear=std::bit_cast<int>(effect.gear);in.suppressShiftRelease=app->originalSession.raceAutomaticBrakeByte()!=0;
  for(unsigned k=0;k<4;++k)in.wheelSurface[k]=std::uint8_t(app->presentedSession().actor().u(116)>>(k*8));
  app->audio.stepOriginalEngine(in,seed);
 });
 a->courseLightFsca=original::OriginalFscaTable::load(a->root/"data/original_physics/fsca_table.bin");a->originalPath=original::OriginalRacePath::load(a->root,0);a->originalCoordinate={0,0};
 a->originalCamera=OriginalChaseCamera::load(a->root);a->bumperCamera=OriginalChaseCamera::load(a->root,OriginalDrivingView::Bumper);a->playerBody.reset();a->projectOriginalPose(false);
 a->race.phase=RacePhase::Finished;a->race.timeUp=true;a->finishBannerDone=false;a->finishAudioSkipped=false;a->updateAudioScene();return a;
}
unsigned randomAdvance(unsigned x){return x*1103515245u+12345u;}
void finishDrivingControls(const fs::path& root){
 // Declared boundary state: live original MT capture retains gear3 across
 //05C040->05C280 with raw accelerator held and brake released. The separate
 //original-instruction owner test proves the unchanged ADC/gear-enable path.
 for(bool moving:{false,true})for(float brake:{0.f,.4f}){
  auto a=fixture(root);auto& v=const_cast<original::OriginalVehicleState&>(a->originalSession.vehicle());
  v.transmission.gear00=3;v.transmission.filtered18=1406.f;v.transmission.tach1c=1406.f;
  if(moving)v.drive.setf(0x238,30.f);
  a->originalSession.enableRaceStart(2);
  DriverInput input;input.automatic=false;input.throttle=1.f;input.brake=brake;input.steer=.15f;
  original::OriginalHostInputState expectedHost;
  const auto expected=original::adaptOriginalHostInput(expectedHost,{input.steer,input.throttle,input.brake,false,false},false,true,0);
  for(unsigned tick=0;tick<180;++tick){
   a->simulate(input);
   check(v.transmission.gear00==3,"Finish owner forced neutral despite original GO engagement");
   check((a->originalSession.actor().u(0x50)&0x8000u)!=0,"Finish owner cleared original gear-enable bit");
   check(v.controls.throttleHighByte==(expected.analog.throttle>>8)&&v.controls.brakeHighByte==(expected.analog.brake>>8),"Finish owner rewrote accepted pedal inputs");
   check(v.controls.throttleAlias==1.f,"Finish owner forced the raw throttle-suppression word");
   check(a->originalSession.raceAutomaticBrakeByte()==1,"Finish child omitted original automatic brake");
  }
  check(a->originalSession.stoppedForRace()&&v.drive.f(0x238)==0.f,"Source automatic brake did not stop the finished car");
 }
 // Actual App rule handoff, with the goal progress as an explicit fixture
 //boundary: the crossing's final solver precedes the finish child's brake.
 auto a=fixture(root);auto& v=const_cast<original::OriginalVehicleState&>(a->originalSession.vehicle());
 a->course=Course::load(a->root/"data"/"courses",courseIds[0],courseNames[0],false);
 v.transmission.gear00=3;v.transmission.filtered18=5000.f;v.transmission.tach1c=5000.f;
 a->originalSession.enableRaceStart(2);a->race.phase=RacePhase::Running;
 a->originalRaceStart.reset(2);for(unsigned tick=0;tick<180;++tick)a->originalRaceStart.step();
 const original::OriginalRacePoint position{a->vehicle.position.x,a->vehicle.position.y,a->vehicle.position.z};
 a->originalPath.project(position,a->originalCoordinate);
 a->originalRace.reset(a->root,{0,2,0},a->originalCoordinate,position);a->originalRace.start();
 auto rules=a->originalRace.state();rules.progress.index=a->originalRace.rules().goalIndex;
 rules.previousCoordinate=a->originalCoordinate;rules.previousPosition=position;
 a->originalRace.restoreNumericalState(rules);
 DriverInput input;input.automatic=false;input.throttle=1.f;
 a->simulate(input);
 check(a->race.phase==RacePhase::Finished&&!a->race.timeUp,"App fixture did not cross the actual rules finish predicate");
 check(a->originalSession.raceAutomaticBrakeByte()==0&&v.transmission.gear00==3,"App braked or forced neutral on the crossing's final racing solver");
 a->simulate(input);
 check(a->originalSession.raceAutomaticBrakeByte()==1&&v.transmission.gear00==3,"App failed to apply finish braking on the following solver while retaining gear");
}
}
int main(int argc,char**argv)try{
 if(argc!=2)throw std::runtime_error("Native asset root required");
 auto a=fixture(argv[1]);DriverInput input;auto expected=a->originalSession.sharedRandomSeed();
 a->audio.playRaceCue(2,7);
 check(a->audio.backfireFrame()==0,"Final racing cue did not start its flash");
 for(unsigned i=0;i<4;++i){a->simulate(input);expected=randomAdvance(expected);
  check(a->audio.backfireFrame()==(i==0?1:-1),"Finish engine gate froze the final racing flash");
  check(a->audio.originalDrivingStopped(),"Local finish child did not stop driving audio");
  check(a->audio.engineStatistics().simulationFrames==0,"Finished engine controller consumed an extra tick");
  check(a->originalSession.sharedRandomSeed()==expected,"Finished idle engine/tire consumed RNG beyond source completion");}
 check(a->offlineFinish.phase==original::OriginalOfflineFinishPresentation::Phase::Hold&&a->offlineFinish.counter==3&&
  a->offlineFinish.visibleHudCode==6&&!a->finishBannerDone,"Audio gate changed source readiness/three Hold calls");
 a->audio.selectOriginalEngine(a->root,a->battleProfile);
 check(!a->audio.originalDrivingStopped(),"New engine selection retained finish stop latch");
 // Automatic brake alone is not a mute event. This same real engine path
 // consumes idle randomness while a source car is braking to a timeout.
 original::OriginalEngineControlInput idle;idle.rpm=800;idle.suppressShiftRelease=true;
 unsigned seed=19;a->audio.stepOriginalEngine(idle,seed);a->audio.finishSoundFrame(seed);
 check(a->audio.engineStatistics().simulationFrames==1&&seed==randomAdvance(19),"Automatic braking incorrectly disabled engine RNG");
 auto retired=fixture(argv[1]);retired->hostRetireWaitForStop=true;
 bool sawUnstopped=false,sawStopped=false;
 for(unsigned i=0;i<6;++i){const bool stopped=retired->originalSession.stoppedForRace();const auto before=retired->audio.engineStatistics().simulationFrames;
  retired->simulate(input);const auto after=retired->audio.engineStatistics().simulationFrames;
  if(!stopped){sawUnstopped=true;check(after==before+1&&!retired->audio.originalDrivingStopped(),"Retirement disabled before source actorbit14");}
  else{sawStopped=true;check(after==before&&retired->audio.originalDrivingStopped(),"Retirement failed to stop at source actorbit14");}}
 check(sawUnstopped&&sawStopped,"Retirement fixture failed to cross actual stopped boundary");
 auto online=fixture(argv[1]);online->multiplayer.active=true;
 for(unsigned i=0;i<4;++i)online->simulate(input);
 check(!online->audio.originalDrivingStopped()&&online->audio.engineStatistics().simulationFrames==4,"Local finish gate contaminated multiplayer audio ownership");
 finishDrivingControls(argv[1]);
 std::cout<<"PASS "<<checks<<" actual App local finish/retire/audio reset/RNG/authority-isolation checks. No window, audio device or user settings opened.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
