// Numerical App integration test. No window, renderer/audio initialization,
// controller discovery, device acquisition or force-output API is invoked.
#include "original_feedback_presentation.h"
namespace idas3ffb {
inline unsigned capturedCar=0,capturedCondition=0,capturedCard=0;
inline OriginalInput checkedFeedbackInput(const idas3::original::OriginalVehicleState& vehicle,
        unsigned car,unsigned condition,unsigned card){
    capturedCar=car;capturedCondition=condition;capturedCard=card;
    return feedbackInput(vehicle,car,condition,card);
}
}
#define feedbackInput checkedFeedbackInput
#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#undef feedbackInput
#include <iostream>

namespace {
unsigned checks=0,soloSteps=0,onlineSteps=0,activeSteps=0;
void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
bool same(const idas3ffb::OriginalOutput& a,const idas3ffb::OriginalOutput& b){
    return a.valid==b.valid&&a.torqueCode==b.torqueCode&&a.damperParameter==b.damperParameter&&
        a.rumbleAmplitude==b.rumbleAmplitude&&a.rumbleFrequencyHalfHz==b.rumbleFrequencyHalfHz&&
        a.sendTorque==b.sendTorque&&a.sendDamper==b.sendDamper&&a.sendRumble==b.sendRumble&&a.limit==b.limit;
}
idas3ffb::OriginalProducer referenceAtReset(const App& app){
    idas3ffb::OriginalProducer result;
    // Original reset keeps the prior rumble latch. App is reused across races;
    // start the independent producer with the same documented retained state.
    if(app.originalFeedback.producerState().rumbleLatch==1){
        idas3ffb::OriginalInput prior;prior.normalizedSpeed248=1;
        result.update(prior);result.reset();
    }
    const auto& expected=app.originalFeedback.producerState();const auto& actual=result.state();
    check(actual.ramp==expected.ramp&&actual.torqueCode==expected.torqueCode&&
        actual.damperParameter==expected.damperParameter&&actual.rumbleLatch==expected.rumbleLatch,
        "Direct reference producer did not start at the same reset boundary");
    return result;
}
void running(App& app){
    app.originalHandling=true;app.menu=false;app.paused=false;app.active=true;
    app.legendVisitActive=false;app.preRaceDialogueActive=false;app.loadingActive=false;
    app.endingActive=false;app.timeAttackVisitActive=false;app.buntaVisitActive=false;
    app.replayPlaybackActive=false;app.vsActive=false;app.race.phase=RacePhase::Running;
    app.multiplayer.disconnected=false;app.multiplayer.waiting=false;app.authorityStalled=false;
}
void assertReadOnly(App& app,const idas3ffb::OriginalOutput& expected){
    const auto before=app.presentedSession().rollbackDigest();
    check(app.originalFeedback.active()&&same(app.originalFeedback.latest(),expected),
        "App source adapter differs from direct original producer");
    const auto ramp=app.originalFeedback.producerState().ramp;
    const auto tick=app.originalFeedback.tick();
    app.advanceOriginalFeedback(true); // Repeated renderer/read-side invocation.
    check(app.originalFeedback.producerState().ramp==ramp&&app.originalFeedback.tick()==tick&&
        same(app.originalFeedback.latest(),expected),"Repeated local tick advanced feedback producer");
    const int peak=app.originalFeedback.consumePeak();
    check(peak>=expected.rumbleAmplitude&&app.originalFeedback.consumePeak()==0,
        "Reading feedback lost a transient or did not consume its peak");
    check(same(app.originalFeedback.latest(),expected),"Peak consumption changed latest original command");
    check(app.presentedSession().rollbackDigest()==before,"Feedback observer/read changed physics state");
}
void gateChecks(App& app){
    using Gate=std::pair<const char*,std::function<void(bool)>>;
    const std::vector<Gate> gates{
        {"menu",[&](bool on){app.menu=on;}},
        {"pause",[&](bool on){app.paused=on;}},
        {"focus",[&](bool on){app.active=!on;}},
        {"loading",[&](bool on){app.loadingActive=on;}},
        {"dialogue",[&](bool on){app.preRaceDialogueActive=on;}},
        {"legend visit",[&](bool on){app.legendVisitActive=on;}},
        {"ending",[&](bool on){app.endingActive=on;}},
        {"time attack visit",[&](bool on){app.timeAttackVisitActive=on;}},
        {"Bunta visit",[&](bool on){app.buntaVisitActive=on;}},
        {"replay",[&](bool on){app.replayPlaybackActive=on;}},
        {"showcase",[&](bool on){app.vsActive=on;}},
        {"finish",[&](bool on){app.race.phase=on?RacePhase::Finished:RacePhase::Running;}},
        {"countdown",[&](bool on){app.race.phase=on?RacePhase::Countdown:RacePhase::Running;}},
        {"ready",[&](bool on){app.race.phase=on?RacePhase::Ready:RacePhase::Running;}},
        {"waiting",[&](bool on){app.multiplayer.waiting=on;}},
        {"stall",[&](bool on){app.authorityStalled=on;}},
        {"disconnect",[&](bool on){app.multiplayer.active=true;app.multiplayer.disconnected=on;}},
        {"non-source handling",[&](bool on){app.originalHandling=!on;}}
    };
    const bool wasOnline=app.multiplayer.active;
    for(const auto& [name,toggle]:gates){
        running(app);app.originalFeedback.stop();++app.vehicle.tick;
        app.advanceOriginalFeedback(true);check(app.originalFeedback.active(),"Gate fixture did not activate feedback");
        const auto digest=app.presentedSession().rollbackDigest();
        toggle(true);check(!app.feedbackDriving(),name);app.advanceOriginalFeedback(true);
        check(!app.originalFeedback.active()&&app.originalFeedback.producerState().ramp==0&&
            app.originalFeedback.consumePeak()==0,"Blocked state retained active feedback or pending peak");
        check(app.presentedSession().rollbackDigest()==digest,"Feedback stop changed numerical physics");
        toggle(false);app.multiplayer.active=wasOnline;
        app.advanceOriginalFeedback(true);
        check(app.originalFeedback.active()&&app.originalFeedback.producerState().ramp==1,
            "Resuming after blocked state failed to restart the original ramp");
    }
    const auto digest=app.presentedSession().rollbackDigest(),tick=app.vehicle.tick;
    app.projectOriginalPose(false);
    check(!app.originalFeedback.active()&&app.vehicle.tick==tick&&app.presentedSession().rollbackDigest()==digest,
        "Non-advancing pose projection retained feedback or changed physics");
}
}

int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("Usage: original_feedback_application_tests native_asset_root NEW_evidence_directory");
    const auto root=fs::absolute(argv[1]),out=fs::absolute(argv[2]);
    if(fs::exists(out))throw std::runtime_error("Preserve existing evidence; choose a new directory");
    fs::create_directories(out);
    std::ofstream evidence(out/"commands.csv");
    evidence<<"mode,case,step,local_slot,car,condition,card,tick,torque,damper,rumble,ramp,physics_digest\n";
    auto app=std::make_unique<App>();app->root=root;app->saveRoot=out/"userdata";
    app->validationMode=true;app->audio.enabled=false;
    running(*app);
    check(!app->feedbackDriving(),"Uninitialized driving session admitted feedback");
    app->advanceOriginalFeedback(true);check(!app->originalFeedback.active(),"Uninitialized session created output");
    const std::array<unsigned,3> cars{0,7,34},conditions{6,7,16},cards{0,4,8};
    // Reset preserves original unwritten histories and the platform frame.
    // Reuse both owners so each scenario compares the same warm-reset state.
    original::OriginalDrivingSession soloControl;
    for(unsigned scenario=0;scenario<cars.size();++scenario){
        app->clearAuthority();app->multiplayer={};running(*app);app->originalFeedback.stop();app->vehicle={};
        app->frontend.car=int(cars[scenario]);app->frontend.battleProfile=original::makeOriginalFreshBattleProfile();
        app->frontend.battleProfile.setByte(1187,std::uint8_t(cards[scenario]));
        original::OriginalDrivingSelection selection;
        selection.physics=original::makeOriginalFreshTimeAttackSelection(cars[scenario],conditions[scenario],
            scenario?original::OriginalWeather::Wet:original::OriginalWeather::Dry);
        selection.collisionVariant=conditions[scenario]&1u;selection.bodyContactEnabled=false;
        const auto spawn=original::originalStartPose(conditions[scenario],0);
        app->originalSession.reset(root,selection,spawn.position,spawn.angles);
        soloControl.reset(root,selection,spawn.position,spawn.angles);
        check(app->originalSession.rollbackDigest()==soloControl.rollbackDigest(),"Solo control fixture differs before observation");
        app->playerBody.reset();app->projectOriginalPose(false);
        auto producer=referenceAtReset(*app);original::OriginalHostInputState host;
        for(unsigned step=0;step<300;++step){
            const float steering=step<80?0.f:step<160?.15f:step<240?-.2f:0.f;
            const auto input=original::adaptOriginalHostInput(host,{steering,step<240?.8f:0.f,step>=240?.3f:0.f,false,false},true,true,0);
            app->originalSession.tick(input);soloControl.tick(input);++soloSteps;
            const auto before=app->originalSession.rollbackDigest();
            check(before==soloControl.rollbackDigest(),"Feedback observer changed next solo physics step");
            const auto expected=producer.update(idas3ffb::feedbackInput(app->originalSession.vehicle(),cars[scenario],conditions[scenario],cards[scenario]));
            const auto tick=app->vehicle.tick;app->projectOriginalPose(true);++activeSteps;
            check(idas3ffb::capturedCar==cars[scenario]&&idas3ffb::capturedCondition==conditions[scenario]&&
                idas3ffb::capturedCard==cards[scenario],"App changed selected car/course/card at the feedback boundary");
            check(app->vehicle.tick==tick+1&&app->originalFeedback.tick()==app->vehicle.tick,
                "Pose projection did not sample exactly one committed driving tick");
            check(app->originalFeedback.producerState().ramp==producer.state().ramp,"Solo feedback cadence differs from original producer");
            assertReadOnly(*app,expected);
            check(app->originalSession.rollbackDigest()==before,"Pose/feedback presentation changed solo numerical state");
            evidence<<"solo,"<<scenario<<','<<step<<",0,"<<cars[scenario]<<','<<conditions[scenario]<<','<<cards[scenario]<<','
                <<app->vehicle.tick<<','<<expected.torqueCode<<','<<expected.damperParameter<<','<<expected.rumbleAmplitude<<','
                <<producer.state().ramp<<','<<before<<'\n';
        }
        gateChecks(*app);
        app->frontend.battleProfile.setByte(1187,255);++app->vehicle.tick;app->advanceOriginalFeedback(true);
        check(idas3ffb::capturedCard==8&&app->originalFeedback.active(),"Invalid saved steering byte escaped the existing card clamp");
    }
    for(unsigned local=0;local<2;++local){
        running(*app);app->multiplayer={};app->multiplayer.active=true;app->multiplayer.config.localSlot=local;
        app->originalFeedback.stop();app->vehicle={};app->playerBody.reset();
        original::OnlineRaceSetup setup;setup.condition=14;setup.collisions=false;
        for(unsigned slot=0;slot<2;++slot){
            setup.profiles[slot]=original::makeOriginalFreshBattleProfile();
            setup.profiles[slot].setu(16,slot?7u:0u);setup.profiles[slot].setByte(1187,slot?8u:0u);
        }
        app->frontend.battleProfile=setup.profiles[local];app->frontend.car=int(setup.profiles[local].u(16));
        app->authorityRace=std::make_unique<original::OnlineRaceSimulation>(root,setup);
        original::OnlineRaceSimulation unobserved(root,setup);
        check(&app->presentedSession()==&app->authorityRace->car(local),"App authority handoff selected the wrong local car");
        check(&app->presentedSession()!=&app->originalSession,"Authority feedback retained the old solo session");
        auto producer=referenceAtReset(*app);std::array<original::OriginalHostInputState,2> hosts;
        for(unsigned step=0;step<330;++step){
            std::array<original::OriginalVehicleInputs,2> inputs;
            for(unsigned slot=0;slot<2;++slot)inputs[slot]=original::adaptOriginalHostInput(hosts[slot],
                {step<270?0.f:slot?.2f:-.15f,slot?.9f:.7f,0,false,false},true,true,0);
            const auto frame=app->authorityRace->step(inputs);unobserved.step(inputs);++onlineSteps;
            const auto before=app->authorityRace->digest();
            check(before==unobserved.digest(),"Feedback observer changed next authority physics step");
            app->race.phase=frame.start.runRules?RacePhase::Running:RacePhase::Countdown;
            app->projectOriginalPose(true);
            if(frame.start.runRules){
                check(idas3ffb::capturedCar==setup.profiles[local].u(16)&&idas3ffb::capturedCondition==setup.condition&&
                    idas3ffb::capturedCard==setup.profiles[local].byte(1187),"Authority handoff used another session's car/course/card");
                const auto expected=producer.update(idas3ffb::feedbackInput(app->authorityRace->car(local).vehicle(),
                    setup.profiles[local].u(16),setup.condition,setup.profiles[local].byte(1187)));
                check(app->originalFeedback.producerState().ramp==producer.state().ramp,"Authority feedback cadence differs from local driving steps");
                assertReadOnly(*app,expected);++activeSteps;
                evidence<<"authority,"<<local<<','<<step<<','<<local<<','<<setup.profiles[local].u(16)<<','<<setup.condition<<','
                    <<unsigned(setup.profiles[local].byte(1187))<<','<<app->vehicle.tick<<','<<expected.torqueCode<<','
                    <<expected.damperParameter<<','<<expected.rumbleAmplitude<<','<<producer.state().ramp<<','<<before<<'\n';
            }else check(!app->originalFeedback.active(),"Authority countdown emitted driving feedback");
            check(app->authorityRace->digest()==before,"Feedback/pose observation changed shared authority physics");
        }
        gateChecks(*app);
    }
    check(bool(evidence),"Could not write feedback command evidence");
    std::ofstream result(out/"result.txt");
    result<<"PASS "<<checks<<" actual App feedback checks; "<<soloSteps<<" solo steps, "<<onlineSteps
        <<" shared-authority steps, "<<activeSteps<<" active local feedback samples. Three car/course/card selections, both authority slots, direct-producer parity, parallel unobserved physics, duplicate-tick/read checks and lifecycle gates. No renderer/audio/device initialization.\n";
    std::cout<<"PASS "<<checks<<" actual App feedback checks; "<<soloSteps<<" solo, "<<onlineSteps<<" authority, "<<activeSteps<<" active feedback samples. No game window or hardware output.\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
