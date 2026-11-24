#define IDAS3_UNITY_PLUGIN
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "../src/main.cpp"
#include <iostream>
namespace {
unsigned checks{};
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
std::unique_ptr<App> fixture(const fs::path& assets,const fs::path& out){
    auto a=std::make_unique<App>();a->root=assets;a->saveRoot=out;a->validationMode=true;a->settings();
    a->audio.enabled=false;a->audio.configure(assets);a->originalCamera=OriginalChaseCamera::load(assets);
    a->bumperCamera=OriginalChaseCamera::load(assets,OriginalDrivingView::Bumper);
    a->frontend.initialize(assets,true);a->hud.loadOriginal(assets);a->loadStartPresentation();
    check(a->renderer.initializeSceneCapture(640,480),"Hidden scene renderer");
    a->replayRecordingFlags=0;a->diagnosticCaptureOff=true;a->demoPreparationStarted=true;return a;
}
void start(App& a,unsigned mode,std::uint8_t card,bool showcase=false){
    a.activeSaveSlot=-1;a.loadedProfileCar=0;a.inspectionVisit.reset();a.frontend.car=0;
    a.frontend.course=a.courseIndex=0;a.frontend.automatic=a.automatic=false;
    a.reverse=a.wet=a.frontend.night=a.night=false;a.active=true;a.menu=true;a.input={};
    a.frontend.gameMode=mode==0?original::OriginalGameMode::TimeAttack:mode==1?original::OriginalGameMode::LegendOfTheStreets:original::OriginalGameMode::BuntaChallenge;
    auto p=original::makeOriginalFreshBattleProfile();p.setu(0,mode==0?1:mode==1?0:2);
    p.setu(1180,p.u(1180)|0x20001u);p.setByte(1189,card);
    if(mode==1)original::selectOriginalRival(p,0);
    if(mode==2){p.setu(72,4000);original::selectOriginalBuntaCourse(p,0);}
    a.frontend.battleProfile=p;a.loadingActive=!showcase;a.loadingStarted=false;
    //Validation mode deliberately skips StandBy. All profile/settings paths
    //are already bound to this fixture's isolated saveRoot before normal Init.
    a.validationMode=!showcase;a.start();a.validationMode=true;
    check((a.battleProfile.u(1180)&0x20000u)==0,"Normal race Init did not clear marker");
    check(a.cabinetButtons.start==0&&a.cabinetButtons.view==0,"Normal race Init retained counters");
    a.loadingActive=false;
}
void held(App& a,bool start,bool view){a.input.cabinetStartHeld=start;a.input.down['C']=view;}
void step(App& a,const DriverInput& d={}){a.advanceHostClock(OriginalDrivingClock::step,[&]{a.simulate(d);});}
void reachGo(App& a){unsigned updates=0;while((a.vsActive||a.race.phase==RacePhase::Countdown)&&updates++<600)step(a);check(a.race.phase==RacePhase::Running,"Actual startup did not reach GO");}
void release(App& a){held(a,false,false);step(a);check(a.cabinetButtons.start==0&&a.cabinetButtons.view==0,"Accepted release did not zero counters");}
void run(App& a,unsigned mode,int stagger){
    std::cerr<<"case mode="<<mode<<" stagger="<<stagger<<" checks="<<checks<<'\n';
    start(a,mode,mode==2?255:1,true);
    held(a,true,true);unsigned updates=0;
    while(a.vsActive&&updates++<250){step(a);check(a.cabinetButtons.start==updates&&a.cabinetButtons.view==updates,"Showcase missed/doubled counter update");}
    check(updates==241,"Source StandBy update count");reachGo(a);
    check(a.race.phase==RacePhase::Running&&(a.battleProfile.u(1180)&0x20000u)==0,"Long pre-GO hold became fresh GO chord");
    release(a);DriverInput drive;drive.throttle=.8f;
    for(unsigned i=0;i<160;++i)step(a,drive);
    check(a.vehicle.speed>1,"Moving fixture failed to accelerate");
    check(a.sourceCabinetStartReserved(),"Eligible card mode did not reserve Start");
    // Host rendering/command sampling alone cannot advance source counters.
    const auto before=a.cabinetButtons.start;held(a,true,false);a.commands(0);a.commands(0);
    check(a.cabinetButtons.start==before&&!a.paused,"Logical Start became host Pause or render-ticked counter");
    held(a,false,false);
    // Four held Start updates invalidate a later View press; normal View still
    // changes once at its third accepted update when the chord is rejected.
    const auto view=a.drivingView;
    for(unsigned i=0;i<4;++i){held(a,true,false);step(a,drive);}
    for(unsigned i=0;i<3;++i){held(a,true,true);step(a,drive);}
    check((a.battleProfile.u(1180)&0x20000u)==0&&a.race.phase==RacePhase::Running,"Long hold admitted forced quit");
    check(unsigned(a.drivingView)==(unsigned(view)+1)%3,"Rejected chord suppressed normal source View trigger");
    release(a);release(a);
    const auto quitView=a.drivingView;
    // Start-first or View-first within the source1..3 window, plus simultaneous.
    for(unsigned tick=0;tick<unsigned(std::abs(stagger))+1;++tick){
        held(a,stagger>=0||tick>=unsigned(-stagger),stagger<=0||tick>=unsigned(stagger));
        step(a,drive);
    }
    check((a.battleProfile.u(1180)&0x20000u)!=0&&a.cabinetButtons.start==4&&a.cabinetButtons.view==4,"Fresh staggered chord was not consumed");
    check(a.originalRace.state().remaining.value==0&&a.race.phase==RacePhase::Running&&!a.originalRace.state().timeUpFlag,"Chord became immediate Finished/timeout");
    check(!a.originalRace.state().automaticBrake&&!a.hostRetireWaitForStop,"Chord incorrectly used host Retire brake path");
    check(a.drivingView==quitView,"Accepted chord also cycled View");
    held(a,false,false);step(a,drive);
    check(a.originalRace.state().automaticBrake&&!a.hostRetireWaitForStop,"Next zero-entry failed to apply normal brake");
    unsigned stopping=0;while(a.race.phase==RacePhase::Running&&stopping++<1800)step(a,drive);
    check(a.race.phase==RacePhase::Finished&&a.race.timeUp&&!a.hostRetireWaitForStop,"Natural stop did not enter ordinary timeout Finish");
    check(a.originalOfflineFinishContext()&&a.pendingResultSetup.has_value(),"Forced quit bypassed ordinary Finish/result owner");
    const auto frame=a.originalSession.contactCompletion().elapsedFrames0C900E84;
    step(a,drive);check(a.offlineFinish.phase==original::OriginalOfflineFinishPresentation::Phase::Hold&&a.offlineFinish.counter==0,
        "Normal Finish lost its initial AwaitOutcome dispatch");
    check(a.originalSession.contactCompletion().elapsedFrames0C900E84==frame+1,"Normal Finish lost its solver");
    step(a,drive);check(a.offlineFinish.counter==1&&a.originalSession.contactCompletion().elapsedFrames0C900E84==frame+2,
        "Normal Finish Hold did not advance after initial dispatch");
    std::cout<<"mode="<<mode<<" stagger="<<stagger<<" normalStopUpdates="<<stopping<<" acceptedCount=4 nextTimer=0 firstFinishCounter=0\n";
}
void guards(App& a){
    start(a,0,0);reachGo(a);held(a,true,true);step(a);
    check(!a.sourceCabinetStartReserved()&&(a.battleProfile.u(1180)&0x20000u)==0,"Disabled card admitted chord");release(a);
    a.battleProfile.setByte(1189,2);check(a.sourceCabinetStartReserved(),"Nonzero card byte treated as exactly1");
    const auto before=a.cabinetButtons.start;
    for(unsigned gate=0;gate<5;++gate){
        a.multiplayer.active=gate==0;a.replayPlaybackActive=gate==1;a.originalHandling=gate!=2;
        a.fullTuneActive=gate==3;if(gate==4)a.importedCourse.emplace();
        check(!a.originalOfflineCabinetContext()&&!a.sourceCabinetStartReserved(),"Extension gained source cabinet ownership");
        a.advanceOriginalCabinetButtons();check(a.cabinetButtons.start==before,"Excluded extension advanced cabinet counters");
        a.importedCourse.reset();a.multiplayer.active=a.replayPlaybackActive=a.fullTuneActive=false;a.originalHandling=true;
    }
    // Explicit host Pause remains separate while source Start is reserved.
    a.input.pressed[VK_ESCAPE]=true;a.commands(0);a.input.pressed[VK_ESCAPE]=false;
    check(a.paused,"Explicit Escape no longer pauses");a.paused=false;a.clock.reset();
    // Host Retire remains its existing extension, with distinct instant result.
    start(a,1,1);reachGo(a);check(a.retireLegendRace()&&a.hostRetireWaitForStop&&a.race.phase==RacePhase::Finished,"Explicit pause Retire changed");
    check((a.battleProfile.u(1180)&0x20000u)==0,"Host Retire acquired source chord marker");
}
void countdownView(App& a,unsigned mode){
    start(a,mode,1,true);
    for(unsigned i=0;i<239;++i)step(a);
    check(a.vsActive&&a.vsBanner.sourceTick()==239,"Countdown View fixture missed last StandBy frames");
    const auto first=a.drivingView;
    held(a,false,true);step(a);step(a);
    check(!a.vsActive&&a.originalRaceCountdownInitPending&&a.cabinetButtons.view==2&&a.drivingView==first,
        "StandBy unexpectedly invoked normal View owner");
    const auto remaining=a.originalRace.state().remaining.value;
    step(a);
    check(a.cabinetButtons.view==3&&unsigned(a.drivingView)==(unsigned(first)+1)%3&&a.originalRaceStart.remaining()==240,
        "PreInit creation Main lost source View after third accepted hold");
    check(a.originalRace.state().remaining.value==remaining,"PreInit creation ran GO timer");
    release(a);release(a);
    for(unsigned length:{1u,2u,3u}){
        const auto before=a.drivingView;
        for(unsigned n=1;n<=length;++n){held(a,false,true);step(a);
            check(unsigned(a.drivingView)==(unsigned(before)+(n==3?1u:0u))%3,"Countdown View hold predicate");}
        release(a);
        check(unsigned(a.drivingView)==(unsigned(before)+1u)%3,"Countdown View short-release predicate");
        release(a);check(unsigned(a.drivingView)==(unsigned(before)+1u)%3,"Countdown View repeated release toggled twice");
    }
    const auto before=a.drivingView;
    for(unsigned n=0;n<3;++n){held(a,true,true);step(a);}
    check(a.race.phase==RacePhase::Countdown&&(a.battleProfile.u(1180)&0x20000u)==0&&
        a.originalRace.state().remaining.value==remaining&&a.cabinetButtons.start==3&&a.cabinetButtons.view==3,
        "PreInit incorrectly ran GO Forced Quit/timer owner");
    check(unsigned(a.drivingView)==(unsigned(before)+1)%3,"PreInit chord incorrectly suppressed ordinary View");
    release(a);release(a);
}
void interruptedView(App& a){
    start(a,0,1);reachGo(a);
    for(bool countdown:{false,true}){
        if(countdown)start(a,0,1);
        const auto phase=a.race.phase;
        const auto view=a.drivingView;
        // Direct accepted blocked source tick consumes its synthetic release
        // but cannot perform the corresponding View action or defer it.
        held(a,false,true);step(a);check(a.cabinetButtons.view==1,"Short View hold fixture");
        a.setHostDrivingControlsBlocked(true);held(a,false,false);step(a);
        check(a.drivingView==view&&a.cabinetButtons.view==0&&a.cabinetButtons.releasedView==1,"Blocked source tick leaked short View release");
        a.setHostDrivingControlsBlocked(false);step(a);
        check(a.drivingView==view&&a.cabinetButtons.releasedView==0,"Blocked View action deferred to resume");
        for(unsigned suspension=0;suspension<3;++suspension){
            held(a,false,true);step(a);check(a.cabinetButtons.view==1,"Interrupted View fixture");
            const auto frame=a.originalSession.contactCompletion().elapsedFrames0C900E84;
            a.active=suspension!=0;a.paused=suspension==1;a.setHostDrivingControlsBlocked(suspension==2);
            held(a,false,false);a.commands(0);
            check(a.cabinetButtons.view==0&&a.cabinetButtons.releasedView==0&&a.cabinetButtons.start==0,"Host suspension retained cabinet history");
            check(a.originalSession.contactCompletion().elapsedFrames0C900E84==frame,"Host suspension advanced solver");
            a.active=true;a.paused=false;a.setHostDrivingControlsBlocked(false);a.commands(0);step(a);
            check(a.drivingView==view&&a.race.phase==phase,"Interrupted View leaked on resume");
        }
    }
}
void preservePositiveBrake(App& a){
    for(bool chord:{false,true}){
        start(a,0,1);reachGo(a);
        // Explicit state boundary: source067D00 leaves preexisting auto-brake
        // untouched in its positive-entry branch, whether chord is accepted.
        auto rules=a.originalRace.state();rules.automaticBrake=true;
        const auto remaining=rules.remaining.value;check(remaining>100,"Positive brake fixture timer");
        a.originalRace.restoreNumericalState(rules);held(a,chord,chord);step(a);
        check(a.originalRace.state().automaticBrake,"Positive source timer cleared preexisting automaticBrake");
        check(a.race.phase==RacePhase::Running&&!a.originalRace.state().timeUpFlag,"Positive brake fixture became immediate timeout");
        check(a.originalRace.state().remaining.value==(chord?0:remaining-100),"Positive brake/chord timer ordering");
        check(((a.battleProfile.u(1180)&0x20000u)!=0)==chord,"Positive brake changed chord eligibility");
    }
}
void goal(App& a,bool chord){
    start(a,0,1);reachGo(a);
    // Explicit goal-trigger fixture boundary, no captured vehicle injection.
    auto rules=a.originalRace.state();rules.automaticBrake=true;rules.progress.index=a.originalRace.rules().goalIndex+100;a.originalRace.restoreNumericalState(rules);
    a.battleProfile.setu(1180,a.battleProfile.u(1180)|0x20000);held(a,chord,chord);step(a);
    check(a.race.phase==RacePhase::Finished&&!a.race.timeUp,"Same-goal chord changed normal finish outcome");
    check(!a.originalRace.state().automaticBrake,"Goal did not clear preexisting automaticBrake before chord");
    check(((a.battleProfile.u(1180)&0x20000u)!=0)==chord,"Goal clear / later chord set ordering differs");
}
}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("assets and isolated output root required");
    const auto out=fs::absolute(argv[2])/("run-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    check(!fs::exists(out),"Evidence root already exists");fs::create_directories(out);auto a=fixture(fs::absolute(argv[1]),out);
    guards(*a);for(unsigned mode=0;mode<3;++mode)countdownView(*a,mode);
    for(unsigned mode=0;mode<3;++mode)for(int stagger:{-2,0,2})run(*a,mode,stagger);
    interruptedView(*a);preservePositiveBrake(*a);goal(*a,false);goal(*a,true);
    std::cout<<"PASS "<<checks<<" actual App cabinet counters/GO chord/normal stop/Finish/view/extension checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}
