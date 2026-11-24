// Actual App startup, pose and light-request owners. All filesystem writes
// are confined to a caller-supplied NEW evidence directory. No window, audio
// device, network connection or live user profile is opened.
#define IDAS3_UNITY_PLUGIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "original_race_start.h"
#include <iostream>
namespace light_fixture {
inline unsigned checks=0,warmups=0;
inline bool expectedNight=false,expectedRival=false,expectedSpecial=false;
inline void check(bool good,const char* text){++checks;if(!good)throw std::runtime_error(text);}
inline unsigned off(unsigned flags){return flags&0x10000u;}
}
namespace idas3::original {
inline void observedLightWarmup(OriginalDrivingSession& session,const OriginalVehicleInputs& input,unsigned frame,
    std::uint8_t digital=0,const std::function<void(const OriginalDrivingStepEffects&)>& pose={}){
    using namespace light_fixture;
    check(off(session.actor().u(0x50))==0x10000,"Initial pose must feedback OFF before night LightON");
    check(off(session.publishedActors().player0C8FF388[20])==0,"Initial feedback prematurely changed published player");
    if(expectedRival){
        check(off(session.rivalPublicActor().u(0x50))==(expectedSpecial?0u:0x10000u),"Initial rival feedback did not precede global night LightON");
        check(off(session.publishedActors().secondary0C8FF430[20])==0,"Initial rival feedback prematurely changed publication");
    }
    unsigned priorPlayer=off(session.actor().u(0x50));
    unsigned priorRival=expectedRival?off(session.rivalPublicActor().u(0x50)):0;
    warmups=0;
    warmupOriginalRaceSession(session,input,frame,digital,[&](const auto& effects){
        check(off(session.publishedActors().player0C8FF388[20])==priorPlayer,"Solver did not publish prior player feedback");
        if(expectedRival)check(off(session.publishedActors().secondary0C8FF430[20])==priorRival,"Solver did not publish prior rival feedback");
        const auto published=session.publishedActors();
        if(pose)pose(effects);
        check(session.publishedActors().player0C8FF388==published.player0C8FF388&&
            session.publishedActors().secondary0C8FF430==published.secondary0C8FF430,"Pose feedback mutated current published snapshots");
        priorPlayer=off(session.actor().u(0x50));
        check(priorPlayer==(expectedNight?0u:0x10000u),"Player feedback did not use current byte81");
        if(expectedRival){
            priorRival=off(session.rivalPublicActor().u(0x50));
            check(priorRival==((expectedNight||expectedSpecial)?0u:0x10000u),"Rival feedback did not follow current request");
        }
        ++warmups;
    });
}
}
#define warmupOriginalRaceSession observedLightWarmup
#include "../src/main.cpp"
#undef warmupOriginalRaceSession
namespace {
using light_fixture::check;using light_fixture::off;
std::unique_ptr<App> fixture(const fs::path& root,const fs::path& evidence){
    auto app=std::make_unique<App>();app->root=root;app->saveRoot=evidence;
    app->validationMode=true;app->settings();app->audio.enabled=false;app->audio.configure(root);
    app->originalCamera=OriginalChaseCamera::load(root);app->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
    app->frontend.initialize(root,true);app->frontend.stage=FrontendStage::Mode;
    app->active=true;app->menu=true;app->demoPreparationStarted=true;app->replayRecordingFlags=0;
    return app;
}
void start(App& a,bool night,bool rival,bool special){
    a.originalSession=original::OriginalDrivingSession{};
    a.validationMode=true;a.activeSaveSlot=-1;a.loadedProfileCar=0;a.inspectionVisit.reset();
    a.frontend.car=0;a.frontend.course=a.courseIndex=3;a.frontend.automatic=a.automatic=false;
    a.reverse=a.wet=false;a.frontend.night=a.night=night;
    a.frontend.gameMode=rival?original::OriginalGameMode::LegendOfTheStreets:original::OriginalGameMode::TimeAttack;
    a.frontend.battleProfile=original::makeOriginalFreshBattleProfile();
    if(rival)original::selectOriginalRival(a.frontend.battleProfile,special?29:0);
    else {a.frontend.battleProfile.setu(0,1);a.frontend.battleProfile.setu(4,3);}
    // Retain each source enemy's real car/preset pair. Day/night is the
    // explicitly controlled lamp boundary, independent of its authored visit.
    a.frontend.battleProfile.setu(8,unsigned(night));
    a.loadingActive=true;a.loadingStarted=false;a.menu=true;
    light_fixture::expectedNight=night;light_fixture::expectedRival=rival;light_fixture::expectedSpecial=special;
    a.start();check(a.originalSession.ready()&&light_fixture::warmups==60,"Actual App startup skipped warmups");
    a.originalSession.setEngineOutput({});
}
void toggle(App& a,bool on){
    const auto& actor=a.originalSession.actor();
    const auto before=a.originalSession.rollbackDigest();
    const auto prior=a.originalSession.publishedActors();
    const auto priorFlags=actor.u(0x50);
    a.playerProjectedHeadlight.request(on,a.originalSession.collision(),a.playerBody.query(),{actor.f(0),actor.f(4),actor.f(8)});
    check(a.originalSession.rollbackDigest()==before,"LightON/OFF immediately mutated numerical state");
    a.projectOriginalPose(false);
    check(a.originalSession.actor().u(0x50)==((priorFlags&~0x10000u)|(on?0u:0x10000u)),"Actual App player pose failed source feedback");
    check(a.originalSession.publishedActors().player0C8FF388==prior.player0C8FF388,"Toggle feedback skipped publication latency");
    a.originalSession.tick({});
    check(off(a.originalSession.publishedActors().player0C8FF388[20])==(on?0u:0x10000u),"Next solver did not publish toggled light flag");
    a.projectOriginalPose(false);
}
void exclusions(App& a){
    // Deliberately make lamp and actor disagree. Every excluded owner must
    // leave the complete numerical digest unchanged, not merely this bit.
    a.originalSession.feedbackActorHeadlights(false,true);
    a.playerProjectedHeadlight.reset();
    const auto unchanged=[&](){const auto before=a.originalSession.rollbackDigest();a.feedbackOriginalActorHeadlights(false);
        check(a.originalSession.rollbackDigest()==before,"Excluded presentation owner mutated numerical checkpoint");};
    a.multiplayer.active=true;unchanged();a.multiplayer.active=false;
    a.replayPlaybackActive=true;unchanged();a.replayPlaybackActive=false;
    a.importedCourse.emplace();unchanged();a.importedCourse.reset();
    a.originalHandling=false;unchanged();a.originalHandling=true;
    for(int course:{-1,9,18}){a.courseIndex=course;unchanged();}a.courseIndex=3;
    a.feedbackOriginalActorHeadlights(false);check(off(a.originalSession.actor().u(0x50))==0x10000,"Original offline owner did not resume feedback");
}
}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("Native asset root and private evidence root required");
    const auto root=fs::absolute(argv[1]),evidenceRoot=fs::absolute(argv[2]);
    fs::create_directories(evidenceRoot);
    const auto runId=std::to_string(GetCurrentProcessId())+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto evidence=evidenceRoot/("run-"+runId);
    if(fs::exists(evidence))throw std::runtime_error("Preserve existing evidence");fs::create_directories(evidence);
    auto app=fixture(root,evidence);
    for(bool rival:{false,true})for(bool night:{false,true})for(bool special:{false,true}){
        if(special&&!rival)continue;
        start(*app,night,rival,special);toggle(*app,!night);toggle(*app,night);
    }
    start(*app,false,false,false);exclusions(*app);
    // A bare numerical session has no presentation owner, even after ticks.
    original::OriginalDrivingSession bare;auto selection=app->originalSession.selection();
    const auto spawn=original::originalStartPose(selection.physics.conditionCode,0);
    bare.reset(root,selection,spawn.position,spawn.angles);
    check(off(bare.actor().u(0x50))==0,"Bare reset acquired an invented presentation owner");
    for(unsigned frame=0;frame<3;++frame){bare.tick({});check(off(bare.actor().u(0x50))==0&&off(bare.publishedActors().player0C8FF388[20])==0,"Bare tick invented lamp feedback");}
    const auto checkpoint=bare.checkpoint();const auto before=bare.rollbackDigest();
    bare.feedbackActorHeadlights(false,false);check(bare.rollbackDigest()!=before,"Explicit feedback missing from checkpoint digest");
    bare.restore(checkpoint);check(bare.rollbackDigest()==before,"Checkpoint did not restore live feedback word");
    std::cout<<"PASS "<<light_fixture::checks<<" actual App light feedback checks; startup day/night and rival request ordering,60 warmups,toggles,exclusions,and bare-session ownership. No network/window/audio opened.\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
