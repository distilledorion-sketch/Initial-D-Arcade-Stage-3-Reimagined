// Actual App initialization/simulate with explicit synthetic inherited-index,
// rules-progress and contact-admission boundaries. No window/audio/network.
#define IDAS3_UNITY_PLUGIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "original_race_start.h"
#include <iostream>
#include <tuple>
namespace record_fixture {
inline unsigned checks=0,warmups=0;inline unsigned inherited=0;
inline void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
}
namespace idas3::original {
inline void observedRecordingWarmup(OriginalDrivingSession& session,const OriginalVehicleInputs& inputs,unsigned frame,
    std::uint8_t digital=0,const std::function<void(const OriginalDrivingStepEffects&)>& pose={}){
    using namespace record_fixture;
    check(session.recordingProgress()==inherited,"App reset discarded inherited source global");
    warmups=0;warmupOriginalRaceSession(session,inputs,frame,digital,[&](const auto& e){
        check(session.recordingProgress()==inherited,"Warmup ran a recorder update");
        check(e.frame0C92DE30==(session.usesRecordingProgress()?inherited:frame),"Warmup mixed recorder and host frame");
        if(pose)pose(e);++warmups;
    });
    check(session.platformFrame()==frame,"Warmup escaped host frame boundary");
}
}
#define warmupOriginalRaceSession observedRecordingWarmup
#include "main.cpp"
#undef warmupOriginalRaceSession
using record_fixture::check;
namespace {
std::unique_ptr<App> fixture(const fs::path& root,const fs::path& evidence){
    auto a=std::make_unique<App>();a->root=root;a->saveRoot=evidence;a->validationMode=true;a->settings();
    a->audio.enabled=false;a->audio.configure(root);a->originalCamera=OriginalChaseCamera::load(root);
    a->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);a->frontend.initialize(root,true);
    a->frontend.stage=FrontendStage::Mode;a->active=true;a->menu=true;a->demoPreparationStarted=true;a->replayRecordingFlags=0;
    return a;
}
void start(App& a,unsigned course,bool reverse,bool battle=false){
    record_fixture::inherited=a.originalSession.ready()?a.originalSession.recordingProgress():0;
    a.validationMode=true;a.activeSaveSlot=-1;a.loadedProfileCar=0;a.inspectionVisit.reset();
    a.frontend.car=0;a.frontend.course=a.courseIndex=int(course);a.frontend.automatic=a.automatic=false;
    a.reverse=reverse;a.wet=false;a.frontend.night=a.night=false;
    a.frontend.gameMode=battle?original::OriginalGameMode::LegendOfTheStreets:original::OriginalGameMode::TimeAttack;
    a.frontend.battleProfile=original::makeOriginalFreshBattleProfile();
    if(battle)original::selectOriginalRival(a.frontend.battleProfile,0);
    else {a.frontend.battleProfile.setu(0,1);a.frontend.battleProfile.setu(4,course);}
    a.loadingActive=true;a.loadingStarted=false;a.menu=true;a.start();
    check(a.originalSession.ready()&&record_fixture::warmups==60,"Actual App startup missing60 warmups");
    check(a.originalSession.usesRecordingProgress()==(!battle&&!a.replayPlaybackActive),"App assigned recorder to wrong race mode");
    if(!battle)check(a.courseIndex==int(course)&&a.reverse==reverse,"Actual App did not use requested course/direction");
    a.originalSession.setEngineOutput({});
}
void armDitch(App& a){
    // A source completion timer10 admits a ditch on this solver. It is an
    // explicit event boundary, not a claim to recreate the wheel geometry.
    auto& drive=const_cast<original::OriginalVehicleState&>(a.originalSession.vehicle()).drive;
    drive.setu(0x14c,10);
}
auto snapshot(App& a){const auto& s=a.originalSession;return a.timeAttackTelemetry.snapshot(unsigned(a.courseIndex),
    s.vehicle().drive,s.vehicle().tail,s.contactCompletion(),0,false);}
void countdown(App& a){
    const auto course=unsigned(a.courseIndex),direction=unsigned(a.reverse);
    original::OriginalTimeAttackTelemetry reference;
    for(unsigned raw:{1300u,1600u,1630u,1631u}){
        auto rules=a.originalRace.state();rules.progress.index=int(raw);a.originalRace.restoreNumericalState(rules);
        const auto old=a.originalSession.recordingProgress(),frame=a.originalSession.platformFrame();
        const auto cursor=a.originalSession.contactCompletion().frameCursor;armDitch(a);
        a.simulate({});
        check(a.race.phase==RacePhase::Countdown,"Countdown fixture accidentally ran GO");
        check(a.originalSession.contactCompletion().frameCursor==cursor+1,"Ditch admission failed");
        check(a.originalSession.contactCompletion().impactFrames[cursor]==old,"Actual App solver used new recorder progress");
        const auto expected=reference.recordProgress(course,direction,raw,original::originalTimeAttackRecordingLength(course,direction),a.originalRace.displayedElapsed());
        check(a.originalSession.recordingProgress()==expected,"Countdown failed mode2 recorder update");
        check(a.originalSession.platformFrame()==frame+1,"Recorder changed host/authority frame");
        const auto saved=snapshot(a);check(saved.ditchEvents.back().progressIndex==old,"Actual App restamped the old ditch event");
        check(saved.drivingPath.empty(),"Countdown changed current path-rendering policy");
    }
}
void showcase(App& a){
    // Explicit inherited-global boundary, not a reset claim or capture seed.
    a.originalSession.setRecordingProgress(424242);original::OriginalTimeAttackTelemetry reference;
    const auto normalized=reference.recordProgress(unsigned(a.courseIndex),unsigned(a.reverse),0,
        original::originalTimeAttackRecordingLength(unsigned(a.courseIndex),unsigned(a.reverse)),0);
    for(unsigned tick=0;tick<2;++tick){
        const auto old=a.originalSession.recordingProgress(),frame=a.originalSession.platformFrame();
        const auto cursor=a.originalSession.contactCompletion().frameCursor;armDitch(a);
        check(a.advanceShowcaseControls({}),"Actual showcase solver did not advance");
        check(a.originalSession.roadContact().tick0C92DE30==old&&a.originalSession.contactCompletion().impactFrames[cursor]==old,
            "Showcase solver failed inherited/prior stamp");
        check(a.originalSession.recordingProgress()==normalized&&a.originalSession.platformFrame()==frame+1,
            "Showcase mode2 failed recorder-end normalization");
        const auto saved=snapshot(a);check(saved.ditchEvents.back().progressIndex==old&&saved.drivingPath.empty(),
            "Showcase restamped contact or changed path sample policy");
    }
}
void running(App& a){
    unsigned ticks=0;
    while(a.race.phase==RacePhase::Countdown&&ticks++<250){
        const auto old=a.originalSession.recordingProgress();a.simulate({});
        check(a.originalSession.roadContact().tick0C92DE30==old,"Actual countdown/GO solver did not sample previous latch");
    }
    check(a.race.phase==RacePhase::Running,"Actual countdown did not reach Running");
    const auto old=a.originalSession.recordingProgress(),cursor=a.originalSession.contactCompletion().frameCursor;
    const auto paths=snapshot(a).drivingPath.size();armDitch(a);a.simulate({});
    check(a.originalSession.contactCompletion().frameCursor==cursor+1&&
        a.originalSession.contactCompletion().impactFrames[cursor]==old,"Running solver did not stamp prior recorder progress");
    const auto saved=snapshot(a);check(saved.ditchEvents.back().progressIndex==old,"Running App restamped contact history");
    check(saved.drivingPath.size()>=paths&&saved.drivingPath.back().progressIndex==a.originalSession.recordingProgress(),
        "Driving path did not retain current rules progress");
}
void wallAndSection(){
    using namespace original;
    OriginalDriveState drive;drive.setf(0,-130);drive.setf(4,17);drive.setf(8,-285);
    OriginalRoadContactState road;OriginalRoadContactServices services;
    services.swept022D20=[](auto& query){query.setu(28,0x8000);query.setf(24,0.04f);query.setf(0,1);query.setf(4,0);query.setf(8,0);};
    const auto range=originalTimeAttackMapProgressRange(1,0);road.tick0C92DE30=range[1];
    queryOriginalWheelSweep(drive,road,0,{},{},services);
    check(road.impactRecords.size()==1&&road.impactRecords[0].tick==range[1],"Actual wall writer lost retained stamp");
    OriginalContactCompletionState completion;completion.positionCursor=completion.frameCursor=1;
    completion.impactPositions[0]={drive.u(0),drive.u(4),drive.u(8)};completion.impactFrames[0]=range[1];drive.setu(0x3f8,1);
    OriginalTimeAttackTelemetry records;records.record(road.impactRecords);records.recordDitches(completion);
    const auto saved=records.snapshot(1,drive,{},completion);
    check(saved.sectionWallMarkers[0].size()==1&&saved.sectionDitchMarkers[0].size()==1,"Retained boundary marker missing from its section");
    auto wronglyRestamped=saved;wronglyRestamped.wallEvents[0].tick=range[1]+1;wronglyRestamped.ditchEvents[0].progressIndex=range[1]+1;
    convertOriginalTimeAttackEvents(1,wronglyRestamped);
    check(wronglyRestamped.sectionWallMarkers[0].empty()&&wronglyRestamped.sectionDitchMarkers[0].empty(),"Fixture does not discriminate old/new stamp at range boundary");
}
void physicsAndRollback(App& a){
    auto& s=a.originalSession;s.setEngineOutput({});s.useRecordingProgress(true);
    const auto base=s.checkpoint();
    auto run=[&](unsigned index){s.restore(base);s.setRecordingProgress(index);
        const_cast<original::OriginalVehicleState&>(s.vehicle()).drive.setu(0x14c,10);
        original::OriginalVehicleInputs input;input.gearEnabled=true;input.analog.throttle=25000;input.analog.brake=17000;
        const auto e=s.tick(input);return std::tuple{s.vehicle().drive.words,s.publishedActors(),s.contactCompletion(),e};};
    const auto [driveA,publishedA,completionA,eA]=run(1630);const auto [driveB,publishedB,completionB,eB]=run(17);
    check(driveA==driveB&&publishedA.player0C8FF388==publishedB.player0C8FF388&&publishedA.secondary0C8FF430==publishedB.secondary0C8FF430,
        "Recording stamp changed force/pose state");
    check(completionA.randomSeed0C37C778==completionB.randomSeed0C37C778&&completionA.steeringMask0C900EBC==completionB.steeringMask0C900EBC&&
        completionA.elapsedFrames0C900E84==completionB.elapsedFrames0C900E84&&eA.acceptedContactCues==eB.acceptedContactCues,
        "Recording stamp changed RNG, input memory or elapsed frames");
    check(completionA.impactFrames[completionA.frameCursor-1]==1630&&completionB.impactFrames[completionB.frameCursor-1]==17,"Numerical fixture did not emit both index values");
    s.restore(base);s.setRecordingProgress(789);s.setPlatformFrame(12345);const auto cp=s.checkpoint();const auto digest=s.rollbackDigest();
    s.setRecordingProgress(987);s.setPlatformFrame(65432);s.useRecordingProgress(false);s.tick({});s.restore(cp);
    check(s.recordingProgress()==789&&s.platformFrame()==12345&&s.usesRecordingProgress()&&s.rollbackDigest()==digest,"Rollback mixed independent latch/authority clock");
    s.useRecordingProgress(false);const auto e=s.tick({});
    check(e.frame0C92DE30==12345&&s.recordingProgress()==789&&s.platformFrame()==12346,"Unowned/online/imported host history policy changed");
    const auto unowned=s.rollbackDigest();s.setRecordingProgress(123456789);
    check(s.rollbackDigest()==unowned,"Disabled source latch changed legacy authority digest");
}
void exclusions(App& a){
    const auto before=a.originalSession.recordingProgress();original::OriginalDrivingStepEffects synthetic;
    synthetic.newImpactRecords.push_back({{-130,17,-285},444,0.04f});
    a.multiplayer.active=true;a.recordTimeAttackDrivingFrame(synthetic,{},true);a.multiplayer.active=false;
    check(a.originalSession.recordingProgress()==before,"Online adapter ran solo recorder");
    a.battle=true;a.recordTimeAttackDrivingFrame(synthetic,{},true);a.battle=false;
    check(a.originalSession.recordingProgress()==before,"Battle adapter ran solo recorder");
    a.replayPlaybackActive=true;a.recordTimeAttackDrivingFrame(synthetic,{},true);a.replayPlaybackActive=false;
    check(a.originalSession.recordingProgress()==before,"Replay adapter ran solo recorder");
    original::OnlineRaceSetup setup;setup.profiles={original::makeOriginalFreshBattleProfile(),original::makeOriginalFreshBattleProfile()};
    a.authorityRace=std::make_unique<original::OnlineRaceSimulation>(a.root,setup);
    a.recordTimeAttackDrivingFrame(synthetic,{},true);
    check(a.originalSession.recordingProgress()==before,"Retained authority simulation ran solo recorder");
    a.authorityLink=std::make_unique<original::OnlineRaceLink>(*a.authorityRace,987,true);
    auto retainedSimulation=std::move(a.authorityRace);a.recordTimeAttackDrivingFrame(synthetic,{},true);
    check(a.originalSession.recordingProgress()==before,"Retained authority link ran solo recorder");
    a.authorityLink.reset();retainedSimulation.reset();
    auto rules=a.originalRace.state();rules.progress.index=1777;a.originalRace.restoreNumericalState(rules);
    a.importedCourse.emplace();a.recordTimeAttackDrivingFrame(synthetic,{},false);
    check(a.originalSession.recordingProgress()==before,"Imported countdown ran recorder");
    a.recordTimeAttackDrivingFrame(synthetic,{},true);a.importedCourse.reset();
    check(a.originalSession.recordingProgress()==before&&snapshot(a).wallEvents.back().tick==1777,"Imported current-progress adapter changed");
}
}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("Native asset root and private evidence root required");
    const auto root=fs::absolute(argv[1]),evidenceRoot=fs::absolute(argv[2]);
    fs::create_directories(evidenceRoot);
    const auto runId=std::to_string(GetCurrentProcessId())+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto out=evidenceRoot/("run-"+runId);
    if(fs::exists(out))throw std::runtime_error("Preserve existing evidence");fs::create_directories(out);
    auto app=fixture(root,out);start(*app,0,false);showcase(*app);countdown(*app);running(*app);physicsAndRollback(*app);
    for(const auto [course,reverse]:{std::pair{2u,true},std::pair{1u,true}}){start(*app,course,reverse);showcase(*app);countdown(*app);}
    const auto retained=app->originalSession.recordingProgress();start(*app,0,false,true);
    app->simulate({});check(app->originalSession.recordingProgress()==retained,"Nonmode2 App updated global recorder");
    app->replayPlaybackActive=true;start(*app,0,false);app->replayPlaybackActive=false;
    start(*app,0,false);countdown(*app);exclusions(*app);wallAndSection();
    app->race.phase=RacePhase::Finished;const auto old=app->originalSession.recordingProgress();armDitch(*app);
    const auto cursor=app->originalSession.contactCompletion().frameCursor;
    const auto frame=app->originalSession.platformFrame();
    // start() above deliberately leaves loadingActive at a synthetic artwork
    // boundary. Finished source ownership must freeze there, not tick cars.
    check(app->loadingActive,"Finished loading-gate fixture boundary missing");
    app->simulate({});
    check(app->originalSession.platformFrame()==frame&&
        app->originalSession.contactCompletion().frameCursor==cursor&&
        app->originalSession.recordingProgress()==old&&!app->offlineFinish.active(),
        "Finished loading gate advanced solver, recorder or presentation");
    // Explicit completion of that artwork boundary; retain the already armed
    // contact and original stamp assertions, now under an eligible owner.
    app->loadingActive=false;
    app->simulate({});
    check(app->originalSession.platformFrame()==frame+1&&
        app->originalSession.contactCompletion().frameCursor==cursor+1,
        "Eligible Finished callback did not advance exactly one solver/contact");
    check(app->originalSession.contactCompletion().impactFrames[cursor]==old&&snapshot(*app).ditchEvents.back().progressIndex==old,
        "Finished solver/restamping lost retained source progress");
    check(app->originalSession.recordingProgress()==1777,"Finished mode2 failed recorder-end update");
    std::cout<<"PASS "<<record_fixture::checks<<" actual App recording-progress checks. Synthetic inherited index/rules progress/contact admission; actual startup60 warmups, countdown/Finish solver, recorder, history/map, reset, rollback. No capture resync/window/audio/network.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
