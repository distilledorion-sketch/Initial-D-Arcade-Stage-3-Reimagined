//Actual App/rules/solver/camera/result Init. Terminal progress and an injected
//final contact are explicit fixture inputs. No window/audio device/transport.
#define IDAS3_UNITY_PLUGIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "main.cpp"
#include <iostream>
namespace {
std::uint64_t checks=0;
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
std::unique_ptr<App> fixture(const fs::path& root,const fs::path& out){
 auto a=std::make_unique<App>();a->root=root;a->saveRoot=out;a->validationMode=true;a->settings();
 a->audio.enabled=false;a->audio.configure(root);a->originalCamera=OriginalChaseCamera::load(root);
 a->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);a->frontend.initialize(root,true);
 a->hud.loadOriginal(root); //Normal App bootstrap; battle result paint consumes these banks.
 check(a->renderer.initializeSceneCapture(640,480),"Private scene-capture renderer initialization");
 a->frontend.stage=FrontendStage::Mode;a->active=true;a->menu=true;a->demoPreparationStarted=true;
 a->replayRecordingFlags=0;a->diagnosticCaptureOff=true;return a;
}
void start(App&a,unsigned mode){
 a.activeSaveSlot=-1;a.loadedProfileCar=0;a.inspectionVisit.reset();a.frontend.car=0;
 a.frontend.course=a.courseIndex=0;a.frontend.automatic=a.automatic=false;a.reverse=a.wet=a.frontend.night=a.night=false;
 a.frontend.gameMode=mode==0?original::OriginalGameMode::TimeAttack:mode==1?original::OriginalGameMode::LegendOfTheStreets:original::OriginalGameMode::BuntaChallenge;
 auto p=original::makeOriginalFreshBattleProfile();p.setu(0,mode==0?1:mode==1?0:2);p.setu(1180,p.u(1180)|1u);
 if(mode==1)original::selectOriginalRival(p,0);
 if(mode==2){p.setu(72,4000);original::selectOriginalBuntaCourse(p,0);}
 a.frontend.battleProfile=p;a.loadingActive=true;a.loadingStarted=false;a.menu=true;a.input={};a.start();
 //Declared completed loading-artwork boundary; race construction above is actual.
 a.loadingActive=false;
 a.originalSession.setEngineOutput({});unsigned ticks=0;
 while(a.race.phase==RacePhase::Countdown&&ticks++<250)a.simulate({});
 check(a.race.phase==RacePhase::Running&&a.battle==(mode!=0)&&a.bunta==(mode==2),"Actual selected mode failed to reach GO");
 DriverInput input;input.throttle=.8f;
 for(unsigned i=0;i<200;++i)a.simulate(input);
}
void terminal(App&a,bool timeout){
 auto rules=a.originalRace.state();
 if(timeout){rules.remaining.value=std::bit_cast<unsigned>(-30100);rules.extensionCounter=6;
  //The source timeout waits until the actor is stopped. This is an explicit
  //terminal fixture boundary; normal timeout gate has independent coverage.
  auto& actor=const_cast<original::OriginalActorState&>(a.originalSession.actor());
  actor.setu(0x50,actor.u(0x50)|0x4000u);
 }else rules.progress.index=a.originalRace.rules().goalIndex+100;
 a.originalRace.restoreNumericalState(rules);DriverInput in;in.throttle=.5f;a.simulate(in);
 check(a.race.phase==RacePhase::Finished&&a.race.timeUp==timeout&&a.pendingResultSetup.has_value(),"Actual terminal rule/result setup failed");
 a.updateAudioScene();
}
void route(App&a,unsigned mode,bool timeout,bool manual,bool emptyStream,bool catchup){
 std::cerr<<"begin mode="<<mode<<" timeout="<<timeout<<" manual="<<manual<<" checks="<<checks<<"\n";
 start(a,mode);std::cerr<<"startup complete checks="<<checks<<"\n";terminal(a,timeout);std::cerr<<"terminal complete checks="<<checks<<"\n";a.clock.reset();
 if(emptyStream)a.audio.stopOriginalFinishStream();
 check(a.originalOfflineFinishOwnsDriving(),"New source Finish lacks driving ownership");
 const auto goalFrame=a.originalSession.contactCompletion().elapsedFrames0C900E84;
 const auto raceTime=a.race.elapsed6000;const auto finishTime=a.originalRace.state().times.finishTime;
 const unsigned finalSolver=manual?128u:609u;
 unsigned callbacks=0,blankFrames=0,views=0;bool changedCamera=false,guarded=false,painted=false,holdExitBatch=false,exitResultBatch=false;
 DriverInput driver;driver.throttle=.6f;driver.steer=.15f;driver.shiftUp=true;
 auto step=[&]{
  ++callbacks;
  auto expected=a.originalCamera;
  const bool draw=!a.offlineFinish.active()||a.offlineFinish.drawsViews();
  const auto& drive=a.originalSession.vehicle().drive;
  if(draw){expected.update({drive.f(0),drive.f(4),drive.f(8)},{drive.f(12),drive.f(16),drive.f(20)});++views;}
  const auto before=a.originalCamera.frame().cameraWorld.elements;
  if(callbacks==finalSolver&&!mode)const_cast<original::OriginalVehicleState&>(a.originalSession.vehicle()).drive.setu(0x14c,10);
  a.simulate(driver);
  check(a.originalCamera.frame().cameraWorld.elements==expected.frame().cameraWorld.elements,"Finish camera did not update exactly once before solver / changed during Exit");
  changedCamera|=before!=a.originalCamera.frame().cameraWorld.elements;
  blankFrames+=a.offlineFinish.blank;
 };
 unsigned frames=0;
 while(a.offlineFinishParentStep<3&&frames++<5000){
  const bool sourceHold=a.offlineFinish.phase==original::OriginalOfflineFinishPresentation::Phase::Hold;
  a.input.cabinetStartHeld=manual&&sourceHold&&a.offlineFinish.counter>=120;
  if(!guarded&&sourceHold&&a.offlineFinish.counter>=40){
   const auto digest=a.originalSession.rollbackDigest(true);const auto counter=a.offlineFinish.counter;
   const auto camera=a.originalCamera.frame().cameraWorld.elements;
   for(unsigned gate=0;gate<3;++gate){
    a.paused=gate==0;a.active=gate!=1;a.setHostDrivingControlsBlocked(gate==2);
    a.advanceHostClock(.08,step);a.simulate(driver);
    check(a.originalSession.rollbackDigest(true)==digest&&a.offlineFinish.counter==counter&&a.originalCamera.frame().cameraWorld.elements==camera,"Pause/focus/modal advanced source Finish");
   }
   a.active=true;a.paused=false;a.setHostDrivingControlsBlocked(false);a.clock.reset();guarded=true;
  }
  if(!painted&&sourceHold&&a.offlineFinish.counter>=70){
   const auto digest=a.originalSession.rollbackDigest(true);const auto counter=a.offlineFinish.counter;
   check(a.render(0)&&a.render(.071)&&a.render(.071),"Source Finish private render failed");
   check(a.originalSession.rollbackDigest(true)==digest&&a.offlineFinish.counter==counter&&!a.timeAttackAnalysisPrepared,"Extra render advanced driving/Finish/capture");painted=true;
  }
  double dt=OriginalDrivingClock::step;
  if(catchup&&((sourceHold&&a.offlineFinish.counter==120)||a.offlineFinish.phase==original::OriginalOfflineFinishPresentation::Phase::Exit))dt=.1;
  else if(!manual){const double deltas[]{1./144,1./30,.043,0,.009};dt=deltas[frames%5];}
  const auto beforePhase=a.offlineFinish.phase;const auto beforeCallbacks=callbacks;const auto beforeExitCounter=a.offlineFinish.exitCounter;
  try{a.advanceHostClock(dt,step);}catch(const std::exception&e){std::cerr<<"clock failure callbacks="<<callbacks<<" parent="<<a.offlineFinishParentStep<<" counter="<<a.offlineFinish.counter<<" phase="<<int(a.offlineFinish.phase)<<"\n";throw;}
  holdExitBatch|=beforePhase==original::OriginalOfflineFinishPresentation::Phase::Hold&&
    a.offlineFinish.phase==original::OriginalOfflineFinishPresentation::Phase::Exit&&callbacks-beforeCallbacks>1;
  exitResultBatch|=beforePhase==original::OriginalOfflineFinishPresentation::Phase::Exit&&
    a.offlineFinishParentStep==3&&callbacks-beforeCallbacks==7-beforeExitCounter;
 }
 if(catchup)check(holdExitBatch&&exitResultBatch,"Legal clamped batches did not cross Hold/Exit and finalsolver/Close/Init");
 check(a.offlineFinishParentStep==3&&a.offlineFinish.done(),"Natural source handoff did not reach result Init");
 check(callbacks==finalSolver&&a.originalSession.contactCompletion().elapsedFrames0C900E84==goalFrame+finalSolver,"Wrong last car-solver ordinal / extra same-batch solver");
 check(views==finalSolver-6&&blankFrames==5&&changedCamera,"View/Exit blank count or discriminating camera fixture");
 check(!a.originalOfflineFinishOwnsDriving(),"Close/result retained driving capability");
 check(a.race.elapsed6000==raceTime&&a.originalRace.state().times.finishTime==finishTime,"Finish changed completed race timing");
 check(mode?a.resultVisit.initialized:a.timeAttackVisitActive,"Wrong actual result destination");
 if(!mode){
  const auto& v=a.originalSession.vehicle();const auto expected=a.timeAttackTelemetry.snapshotForLecture(unsigned(a.courseIndex),v.drive,v.tail,a.originalSession.contactCompletion());
  check(a.timeAttackAnalysisPrepared&&a.timeAttackSnapshot.publishedStatistics0C91FB0C==expected.publishedStatistics0C91FB0C,"Result Init did not consume last publication");
  check(a.timeAttackSnapshot.ditchCount+1==v.drive.u(0x3f8),"Final unpublished collision leaked / publication was synthesized");
 }
 const auto digest=a.originalSession.rollbackDigest(true),numerical=a.originalSession.rollbackDigest(false);
 const auto count=a.originalSession.contactCompletion().elapsedFrames0C900E84;
 for(unsigned i=0;i<15;++i){a.advanceHostClock(.1,step);a.simulate(driver);}
 check(a.originalSession.rollbackDigest(true)==digest&&a.originalSession.contactCompletion().elapsedFrames0C900E84==count,"Result owner ran a later car solver or repeated advice RNG");
 //Non-driving result Main remains the existing60Hz presentation adapter.
 //It must still advance while all car owners stay finished.
 const auto presentation=mode?a.resultVisit.animation.frame:a.timeAttackVisit.frame();
 try{check(a.render(.05),"Actual result presentation failed");}catch(const std::exception&e){std::cerr<<"result render failure mode="<<mode<<" initialized="<<a.resultVisit.initialized<<"\n";throw;}
 check((mode?a.resultVisit.animation.frame:a.timeAttackVisit.frame())>presentation,"Result screen stopped progressing");
 check(a.originalSession.rollbackDigest(false)==numerical&&a.originalSession.contactCompletion().elapsedFrames0C900E84==count,"Result rendering resumed driving");
 std::cout<<"mode="<<mode<<" timeout="<<timeout<<" manual="<<manual<<" empty_stream="<<emptyStream<<" final_solver="<<callbacks<<" views="<<views<<" retained_blank="<<blankFrames<<'\n';
}
void heldEdge(App&a){
 start(a,0);a.input.cabinetStartHeld=true;a.simulate({});terminal(a,false);a.clock.reset();
 for(unsigned i=0;i<125;++i)a.advanceHostClock(OriginalDrivingClock::step,[&]{a.simulate({});});
 check(a.offlineFinish.phase==original::OriginalOfflineFinishPresentation::Phase::Hold,"Held Start across GO/Finish bypassed minimum");
 a.input.cabinetStartHeld=false;a.advanceHostClock(OriginalDrivingClock::step,[&]{a.simulate({});});
 a.input.cabinetStartHeld=true;a.advanceHostClock(0,[&]{a.simulate({});});a.input.cabinetStartHeld=false;
 a.advanceHostClock(OriginalDrivingClock::step,[&]{a.simulate({});});
 check(a.offlineFinish.phase==original::OriginalOfflineFinishPresentation::Phase::Hold,"No-source-tick press was queued");
 a.input.cabinetStartHeld=true;a.advanceHostClock(OriginalDrivingClock::step,[&]{a.simulate({});});
 check(a.offlineFinish.phase==original::OriginalOfflineFinishPresentation::Phase::Exit,"Fresh accepted release/press failed");
}
void standaloneAndExclusions(App&a){
 HostInput input;input.sampleStandaloneCabinetStart(true,false);input.sampleStandaloneCabinetStart(false,false);
 input.sampleStandaloneCabinetStart(true,true);check(!input.cabinetStartHeld,"Standalone focus return invented Start");
 input.sampleStandaloneCabinetStart(true,false);input.sampleStandaloneCabinetStart(true,true);check(input.cabinetStartHeld,"Standalone Start did not rearm on release");
 start(a,0);terminal(a,false);check(a.originalOfflineFinishContext(),"Ordinary scope missing");
 a.importedCourse.emplace();check(!a.originalOfflineFinishContext(),"Imported captured by original owner");a.importedCourse.reset();
 a.multiplayer.active=true;check(!a.originalOfflineFinishContext(),"Online captured by original owner");a.multiplayer.active=false;
 a.replayPlaybackActive=true;check(!a.originalOfflineFinishContext(),"Replay captured by original owner");a.replayPlaybackActive=false;
 a.fullTuneActive=true;check(!a.originalOfflineFinishContext(),"Full Tune captured by original owner");a.fullTuneActive=false;
 a.hostRetireWaitForStop=true;check(!a.originalOfflineFinishContext(),"Host-retire extension captured by ordinary owner");a.hostRetireWaitForStop=false;
 for(int course:{-1,9,10}){a.courseIndex=course;check(!a.originalOfflineFinishContext(),"Special/non-original course captured");}a.courseIndex=0;
 original::OnlineRaceSetup setup;setup.profiles={original::makeOriginalFreshBattleProfile(),original::makeOriginalFreshBattleProfile()};
 a.authorityRace=std::make_unique<original::OnlineRaceSimulation>(a.root,setup);check(!a.originalOfflineFinishContext(),"Retained authority captured");
 a.authorityLink=std::make_unique<original::OnlineRaceLink>(*a.authorityRace,123,true);auto retained=std::move(a.authorityRace);
 check(!a.originalOfflineFinishContext(),"Retained authority link captured");a.authorityLink.reset();retained.reset();
}

}
int main(int argc,char**argv)try{
 if(argc!=3)throw std::runtime_error("Native assets and private evidence root required");
 const auto out=fs::absolute(argv[2])/("run-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 check(!fs::exists(out),"Preserve old test evidence");fs::create_directories(out);auto a=fixture(fs::absolute(argv[1]),out);
 for(unsigned mode=0;mode<3;++mode)for(bool timeout:{false,true}){
  route(*a,mode,timeout,false,true,false);route(*a,mode,timeout,true,false,true);
 }
 heldEdge(*a);standaloneAndExclusions(*a);
 std::cout<<"PASS "<<checks<<" actual App ordinary Finish source-clock, Start, camera, blank, final publication, Close/result Init and no later driving checks. Terminal/contact admission are explicit fixture boundaries; subsequent result Main keeps host presentation cadence.\n";
}catch(const std::exception&e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}
