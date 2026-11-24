// Actual App startup, goal/timeout, Finish solver and HLecture entry.
// Race-progress/contact admission are explicit synthetic boundaries.
// No window, renderer, audio device, transport, or user save directory.
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
unsigned checks=0;
void check(bool v,const char* s){++checks;if(!v)throw std::runtime_error(s);}
auto bits(float x){return std::bit_cast<std::uint32_t>(x);}
std::unique_ptr<App> fixture(const fs::path& root,const fs::path& out){
 auto a=std::make_unique<App>();a->root=root;a->saveRoot=out;a->validationMode=true;a->settings();
 a->audio.enabled=false;a->audio.configure(root);a->originalCamera=OriginalChaseCamera::load(root);
 a->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);a->frontend.initialize(root,true);
 a->frontend.stage=FrontendStage::Mode;a->active=true;a->menu=true;a->demoPreparationStarted=true;a->replayRecordingFlags=0;return a;
}
void start(App& a){
 a.activeSaveSlot=-1;a.loadedProfileCar=0;a.inspectionVisit.reset();a.frontend.car=0;a.frontend.course=a.courseIndex=1;
 a.frontend.automatic=a.automatic=false;a.reverse=a.wet=a.frontend.night=a.night=false;
 a.frontend.gameMode=original::OriginalGameMode::TimeAttack;a.frontend.battleProfile=original::makeOriginalFreshBattleProfile();
 a.frontend.battleProfile.setu(0,1);a.frontend.battleProfile.setu(4,1);
 a.loadingActive=true;a.loadingStarted=false;a.menu=true;a.start();a.loadingActive=false; /* declared loading-artwork completion */ a.originalSession.setEngineOutput({});
 unsigned ticks=0;while(a.race.phase==RacePhase::Countdown&&ticks++<250)a.simulate({});
 check(a.race.phase==RacePhase::Running&&a.originalSession.usesRecordingProgress(),"Actual App did not reach solo GO");
 for(unsigned i=0;i<200;++i)a.simulate({});
 check(a.originalSession.contactCompletion().elapsedFrames0C900E84>360,"App averaging fixture before source gate");
}
void armDitch(App& a){const_cast<original::OriginalVehicleState&>(a.originalSession.vehicle()).drive.setu(0x14c,10);}
void equivalent(const original::OriginalTimeAttackTelemetrySnapshot& a,const original::OriginalTimeAttackTelemetrySnapshot& b){
 check(a.valid&&b.valid&&a.publishedStatistics0C91FB0C==b.publishedStatistics0C91FB0C,"Entry lost final published stats");
 check(bits(a.maxSpeedKph)==bits(b.maxSpeedKph)&&bits(a.acceleratorFraction)==bits(b.acceleratorFraction)&&
  bits(a.brakeFraction)==bits(b.brakeFraction)&&bits(a.maxSteeringDelta)==bits(b.maxSteeringDelta),"Entry scalar snapshot mismatch");
 check(a.wallCount==b.wallCount&&a.ditchCount==b.ditchCount&&a.ditchWallCount==b.ditchWallCount&&a.maxGearUsed==b.maxGearUsed,"Entry counter snapshot mismatch");
 check(a.wallEvents.size()==b.wallEvents.size()&&a.ditchEvents.size()==b.ditchEvents.size()&&
  a.drivingPath.size()==b.drivingPath.size()&&a.convertedEventCount==b.convertedEventCount,"Entry history snapshot mismatch");
 for(unsigned i=0;i<3;++i)check(a.sectionWallMarkers[i].size()==b.sectionWallMarkers[i].size()&&
  a.sectionDitchMarkers[i].size()==b.sectionDitchMarkers[i].size(),"Entry marker admission mismatch");
}
void route(App& a,bool timeout){
 start(a);auto state=a.originalRace.state();
 // Explicit rule boundary, followed by the actual rule owner and App finish route.
 if(timeout){state.remaining.value=std::bit_cast<unsigned>(-30100);state.extensionCounter=6;}
 else state.progress.index=a.originalRace.rules().goalIndex+100;
 a.originalRace.restoreNumericalState(state);armDitch(a);DriverInput input;input.throttle=.75f;input.brake=.1f;input.steer=.1f;
 a.simulate(input);check(a.race.phase==RacePhase::Finished&&a.race.timeUp==timeout,"Actual terminal rule route failed");
 check(a.timeAttackSnapshot.valid&&!a.timeAttackAnalysisPrepared,"Goal did not retain compatibility snapshot");
 const auto goal=a.timeAttackSnapshot;const auto raceTime=a.race.elapsed6000,finishTime=a.originalRace.state().times.finishTime;
 const auto resultFlags=a.results.recordFlags;const auto profile=a.battleProfile.words;
 const auto beforeSamples=a.originalSession.vehicle().drive.u(0x3f0);
 // Finish is still allowed to append contacts. Each source publication must
 // precede its own append; the last contact is deliberately left unpublished.
 for(unsigned i=0;i<4;++i){armDitch(a);input.throttle=0;input.brake=.8f;input.steer=.9f;a.simulate(input);}
 const auto& vehicle=a.originalSession.vehicle();
 check(vehicle.drive.u(0x3f0)==beforeSamples+(timeout?0u:1u),"Finish automatic-brake latch sample order differs");
 check(a.timeAttackSnapshot.ditchCount==goal.ditchCount,"App refreshed goal snapshot before result entry");
 check(vehicle.tail.statistics0C91FB0C[8]+1==vehicle.drive.u(0x3f8),"Actual final collision is not unpublished");
 const auto digest=a.originalSession.rollbackDigest(true),numerical=a.originalSession.rollbackDigest(false);
 const auto seed=a.originalSession.sharedRandomSeed();
 const auto expected=a.timeAttackTelemetry.snapshotForLecture(unsigned(a.courseIndex),vehicle.drive,vehicle.tail,a.originalSession.contactCompletion());
 check(a.originalSession.rollbackDigest(true)==digest&&a.originalSession.sharedRandomSeed()==seed,"Pure capture changed solver/RNG");
 check(expected.ditchCount>goal.ditchCount&&expected.ditchEvents.size()==expected.ditchCount,"Late data did not discriminate old snapshot");
 check(a.beginTimeAttackVisit(true),"Actual HLecture entry failed");
 check(a.timeAttackAnalysisPrepared&&a.timeAttackVisitActive,"Actual HLecture preparation missing");
 equivalent(a.timeAttackSnapshot,expected);equivalent(a.timeAttackVisit.setup().telemetry,expected);
 check(a.timeAttackAnalysisInput.ditchCount==expected.ditchCount&&a.timeAttackAnalysisInput.wallCount==expected.wallCount&&
  bits(a.timeAttackAnalysisInput.acceleratorFraction)==bits(expected.acceleratorFraction)&&
  bits(a.timeAttackAnalysisInput.maxSteeringDelta)==bits(expected.maxSteeringDelta),"Classifier received goal-time data");
 auto expectedSeed=seed;const auto expectedAdvice=original::analyzeOriginalTimeAttack(a.timeAttackAnalysisInput,expectedSeed);
 check(a.originalSession.sharedRandomSeed()==expectedSeed&&a.timeAttackAnalysis.adviceIndex==expectedAdvice.adviceIndex&&
  a.timeAttackAnalysis.kind==expectedAdvice.kind,"Entry consumed unexpected advice RNG");
 check(a.originalSession.rollbackDigest(false)==numerical,"Lecture entry changed direct numerical state");
 check(a.race.elapsed6000==raceTime&&a.originalRace.state().times.finishTime==finishTime&&
  a.timeAttackVisit.setup().ticks6000==raceTime&&a.results.recordFlags==resultFlags&&a.battleProfile.words==profile,
  "Late analysis changed completed timing, records or profile");
 const auto once=a.originalSession.rollbackDigest(true);a.beginTimeAttackVisit(true);
 equivalent(a.timeAttackSnapshot,expected);check(a.originalSession.rollbackDigest(true)==once,"Repeated lecture prepared/RNG twice");
 // Once HLecture has entered, another App simulate request cannot manufacture
 // a publication. The source fixture separately proves the final append lag.
 const auto entryFrame=a.originalSession.contactCompletion().elapsedFrames0C900E84;
 a.simulate({});
 check(a.originalSession.contactCompletion().elapsedFrames0C900E84==entryFrame&&
  a.originalSession.rollbackDigest(true)==once,"Lecture owner allowed an extra car publication");
 equivalent(a.timeAttackSnapshot,expected);
}
void exclusions(App& a){
 start(a);a.race.phase=RacePhase::Finished;
 original::OriginalDriveState d;d.setu(0x3f4,777);original::OriginalTailState t;original::OriginalContactCompletionState c;
 const auto sentinel=a.timeAttackTelemetry.snapshot(1,d,t,c);
 const auto reset=[&]{a.timeAttackAnalysisPrepared=false;a.timeAttackSnapshot=sentinel;};
 const auto unchanged=[&]{check(a.timeAttackSnapshot.wallCount==777,"Excluded result path captured stock stats");};
 reset();a.battle=true;a.prepareTimeAttackAnalysis();a.battle=false;unchanged();check(!a.timeAttackAnalysisPrepared,"Battle prepared TA advice");
 reset();a.multiplayer.active=true;a.prepareTimeAttackAnalysis();a.multiplayer.active=false;unchanged();check(!a.timeAttackAnalysisPrepared,"Online prepared TA advice");
 reset();a.replayPlaybackActive=true;a.prepareTimeAttackAnalysis();a.replayPlaybackActive=false;unchanged();
 reset();a.originalSession.useRecordingProgress(false);a.prepareTimeAttackAnalysis();a.originalSession.useRecordingProgress(true);unchanged();
 reset();a.originalHandling=false;a.prepareTimeAttackAnalysis();a.originalHandling=true;unchanged();
 reset();a.race.phase=RacePhase::Running;a.prepareTimeAttackAnalysis();a.race.phase=RacePhase::Finished;unchanged();
 reset();a.importedCourse.emplace();a.prepareTimeAttackAnalysis();a.importedCourse.reset();unchanged();
 original::OnlineRaceSetup setup;setup.profiles={original::makeOriginalFreshBattleProfile(),original::makeOriginalFreshBattleProfile()};
 a.authorityRace=std::make_unique<original::OnlineRaceSimulation>(a.root,setup);reset();a.prepareTimeAttackAnalysis();unchanged();
 a.authorityLink=std::make_unique<original::OnlineRaceLink>(*a.authorityRace,12345,true);auto retained=std::move(a.authorityRace);
 reset();a.prepareTimeAttackAnalysis();unchanged();a.authorityLink.reset();retained.reset();
 reset();a.battleProfile.setu(1180,a.battleProfile.u(1180)|0x20000u);a.prepareTimeAttackAnalysis();unchanged();
 check(!a.timeAttackAnalysisPrepared,"Suppressed lecture prepared advice");
 a.battleProfile.setu(1180,a.battleProfile.u(1180)&~0x20000u);
 reset();a.timeAttackSnapshot.valid=false;a.prepareTimeAttackAnalysis();check(!a.timeAttackAnalysisPrepared,"Invalid capture prepared advice");
}
}
int main(int argc,char** argv)try{
 if(argc!=3)throw std::runtime_error("Native assets and private output root required");
 const auto out=fs::absolute(argv[2])/("run-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 if(fs::exists(out))throw std::runtime_error("Retain previous evidence");fs::create_directories(out);
 auto a=fixture(fs::absolute(argv[1]),out);route(*a,false);route(*a,true);exclusions(*a);
 std::cout<<"PASS "<<checks<<" actual App late-result checks. Actual startup, GO/timeout rules, Finish solver, HLecture entry, publication lag, repeat-entry RNG, unchanged timing/profile and exclusions. Synthetic terminal progress/contact admission; no emulator/image/full Finish-timing claim.\n";
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}
