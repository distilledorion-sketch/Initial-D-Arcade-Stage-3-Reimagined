#include "online_race_simulation.h"
#include "imported_course.h"
#include "original_engine_playback.h"
#include "original_host_input.h"
#include "original_race_path.h"
#include "original_start_grid.h"
#include "original_tuning.h"
#include "original_battle_metrics.h"
#include "car_catalog.h"
#include "replay_car_smoke_playback.h"
#include <algorithm>
#include <bit>
#include <stdexcept>

namespace idas3::original {
namespace {
struct SoundState {
    OriginalEngineControlState engine;
    OriginalSoundCueQueue queue;
    OriginalTireAudioState tire;
};
struct RaceMutable {
    OriginalRaceStart start;
    std::array<OriginalPathCoordinate,2> coordinates;
    std::array<SoundState,2> sound;
    std::array<CarSmokeState,2> smoke;
    OriginalBodyContactState contact;
    std::uint64_t frame=0,contactFrames=0;
    int winner=-3;
};
}
struct OnlineRaceSimulation::Impl {
    OnlineRaceSetup setup;
    std::array<OriginalDrivingSession,2> cars;
    std::array<OriginalRaceRules,2> rules;
    OriginalRacePath path;
    OriginalBattleMetrics metrics;
    OriginalRivalData geometry;
    OriginalFscaTable fsca;
    OriginalEngineTables engineTables;
    std::array<OriginalEngineConfiguration,2> engines;
    std::array<std::array<std::array<float,3>,4>,2> smokeOrigins;
    RaceMutable state;
    OnlineRaceFrame lastFrame;
    std::shared_ptr<const int> epoch=std::make_shared<const int>(0);
};
struct OnlineRaceSimulation::Checkpoint::Data {
    std::array<OriginalDrivingSession::Checkpoint,2> cars;
    std::array<OriginalRaceRuleState,2> rules;
    RaceMutable state;
    std::shared_ptr<const int> epoch;
};
std::size_t OnlineRaceSimulation::Checkpoint::memoryBytes()const {
    return data_?sizeof(Data)+data_->cars[0].memoryBytes()+data_->cars[1].memoryBytes()+
        data_->state.smoke[0].dynamicBytes()+data_->state.smoke[1].dynamicBytes():0;
}
OnlineRaceSimulation::~OnlineRaceSimulation()=default;
OnlineRaceSimulation::OnlineRaceSimulation(OnlineRaceSimulation&&)noexcept=default;
OnlineRaceSimulation& OnlineRaceSimulation::operator=(OnlineRaceSimulation&&)noexcept=default;
OnlineRaceSimulation::OnlineRaceSimulation(const std::filesystem::path& root,const OnlineRaceSetup& setup):impl_(std::make_unique<Impl>()){
    if(setup.condition>=18||(setup.condition>=16&&!setup.wet))throw std::invalid_argument("Invalid online course/weather");
    auto& s=*impl_;s.setup=setup;
    const bool reverse=(setup.condition&1)!=0;
    s.path=setup.imported?setup.imported->racePath(reverse):OriginalRacePath::load(root,setup.condition);
    s.metrics=setup.imported?setup.imported->battleMetrics(reverse):OriginalBattleMetrics::load(root,setup.condition);
    auto importedRoad=setup.imported?std::optional(setup.imported->drivingRoad(reverse)):std::nullopt;
    s.geometry=OriginalRivalData::load(root/"data/original_rival");
    s.fsca=OriginalFscaTable::load(root/"data/original_physics/fsca_table.bin");
    s.engineTables=OriginalEngineTables::load(root);
    s.state.start.reset(2);
    for(unsigned slot=0;slot<2;++slot){
        auto& car=s.cars[slot];const auto& profile=setup.profiles[slot];
        s.state.smoke[slot].reset(setup.wet?1u:0u);
        if(!setup.imported)s.smokeOrigins[slot]=originalCarEffectOrigins(OriginalCarParts::load(
            root/"data/original_models"/originalCarFolders.at(profile.u(16))/"assembly_parts.bin"));
        OriginalDrivingSelection selection;
        selection.physics=makeOriginalFreshTimeAttackSelection(profile.u(16),setup.imported?setup.imported->handlingCondition(reverse):setup.condition,setup.wet?OriginalWeather::Wet:OriginalWeather::Dry);
        applyOriginalProfilePhysicsSelection(selection.physics,profile);
        selection.physics.progressEnabled0C9015E4=setup.boost?1u:0u;
        selectOriginalWeather(selection.physics,setup.wet?OriginalWeather::Wet:OriginalWeather::Dry);
        selection.collisionVariant=setup.condition&1;
        const auto grid=setup.imported?setup.imported->onlineSpawn(reverse,slot):originalStartPose(setup.condition,slot);
        car.reset(root,selection,grid.position,grid.angles,importedRoad?&*importedRoad:nullptr);
        OriginalHostInputState host;
        const auto frozen=adaptOriginalHostInput(host,{},setup.automatic[slot],false,0);
        car.setProgressCorrection(0,0);
        warmupOriginalRaceSession(car,frozen,0);
        const auto& actor=car.actor();
        s.state.coordinates[slot]={setup.imported?setup.imported->rules(reverse).startIndex:originalRaceRuleRow(originalRaceRuleRowIndex(setup.condition,2)).startIndex,0};
        if(setup.imported)setup.imported->resetRules(s.rules[slot],reverse,{actor.f(0),actor.f(4),actor.f(8)});
        else s.rules[slot].reset(root,{setup.condition,2,setup.wet?1u:0u},s.state.coordinates[slot],{actor.f(0),actor.f(4),actor.f(8)});
        s.engines[slot]=configureProfileEngineSound(profile);
        resetOriginalEngineControl(s.state.sound[slot].engine);
        resetOriginalTireAudio(s.state.sound[slot].tire);
        // The numerical owner keeps contact-queue/RNG state inside its car
        // checkpoints; accepted sounds are played only after confirmation.
        car.setContactSoundQueueActive(true);
    }
}
OnlineRaceFrame OnlineRaceSimulation::step(const std::array<OriginalVehicleInputs,2>& incoming){
    auto& s=*impl_;OnlineRaceFrame out;out.frame=s.state.frame;
    if(s.state.frame<s.setup.showcaseFrames)out.start.countdownRemaining=s.state.start.remaining();
    else out.start=s.state.start.step();
    // Sample both cars before either solver advances. Replays use the same
    // original distance correction: trailing gap capped at 100 metres.
    const float gap=s.metrics.advantage(s.rules[0].state().progress,s.rules[1].state().progress);
    for(unsigned slot=0;slot<2;++slot)s.cars[slot].setProgressCorrection(s.setup.boost?(slot==0?gap:-gap):0.f,0);
    OriginalPublishedActors poses;
    poses.player0C8FF388=s.cars[0].publishedActors().player0C8FF388;
    poses.secondary0C8FF430=s.cars[1].publishedActors().player0C8FF388;
    std::array<OriginalBodyCollisionResult,2> contacts{};
    // Finished cars cease colliding. Never run the pair solve against a
    // delayed render pose or a car already advanced to the next frame.
    if(s.state.winner==-3&&s.setup.collisions&&s.state.start.started()&&
        s.rules[0].state().phase!=OriginalRacePhase::Finished&&s.rules[0].state().phase!=OriginalRacePhase::TimeUp&&
        s.rules[1].state().phase!=OriginalRacePhase::Finished&&s.rules[1].state().phase!=OriginalRacePhase::TimeUp){
        contacts=produceOriginalBodyPairContact(poses,s.state.contact,s.geometry,s.fsca);
        if(contacts[0].active)++s.state.contactFrames;
    }
    for(unsigned slot=0;slot<2;++slot){
        auto& car=s.cars[slot];auto& rules=s.rules[slot];auto& sound=s.state.sound[slot];
        if(out.start.go){car.enableRaceStart(2);rules.start();}
        if(out.start.runRules&&s.state.winner==-3){const auto& actor=car.actor();
            const OriginalRacePoint position{actor.f(0),actor.f(4),actor.f(8)};
            s.path.project(position,s.state.coordinates[slot]);
            out.rules[slot]=rules.tick(s.state.coordinates[slot],position,car.stoppedForRace());
            car.setRaceAutomaticBrake(rules.state().automaticBrake);
        }
        auto controls=incoming[slot];controls.automaticMode=s.setup.automatic[slot];controls.gearEnabled=out.start.gearEnabled;
        if(s.state.winner!=-3||rules.state().phase==OriginalRacePhase::Finished||rules.state().phase==OriginalRacePhase::TimeUp){
            car.setRaceAutomaticBrake(true);
            // Preserve each driver's steering while the finish owner brakes.
            controls.analog.throttle=std::uint16_t(64u<<8);controls.analog.brake=std::uint16_t(171u<<8);
            controls.calibration.throttleWord=controls.calibration.brakeWord=32;controls.suppressRawThrottle0C2F4BC8=1;
            controls.pressedByte=0;controls.gearEnabled=false;
        }
        car.setPlatformFrame(std::uint32_t(s.state.frame));
        out.driving[slot]=car.tick(controls,&contacts[slot],[&](const OriginalContactCompletionEffects& effect,std::uint32_t& seed){
            OriginalEngineControlInput input;
            input.rpm=effect.engineValue;input.throttle=effect.throttle;input.gear=std::bit_cast<std::int32_t>(effect.gear);
            input.suppressShiftRelease=car.raceAutomaticBrakeByte()!=0;
            for(unsigned i=0;i<4;++i)input.wheelSurface[i]=std::uint8_t(car.actor().u(116)>>(8*i));
            const auto commands=stepOriginalEngineControl(s.engineTables,s.engines[slot],sound.engine,input,seed,
                [&](const OriginalEngineCommand& cue,std::uint32_t& shared){
                    if(cue.target==OriginalEngineCommandTarget::RaceCue1424A0||sound.queue.request(false,shared))out.engine[slot].push_back(cue);
                });
            for(const auto& command:commands)if(command.target==OriginalEngineCommandTarget::Continuous)out.engine[slot].push_back(command);
        });
        const auto& angular=out.driving[slot].vehicle.feedback;
        requestOriginalTireAudio(sound.tire,0,angular.feedbackSpeed,0,angular.feedbackArgument,angular.feedbackStrength);
        out.states[slot]=rules.state();
        auto& pose=out.replayPoses[slot];const auto& drive=car.vehicle().drive;const auto& actor=car.actor();
        pose.tick=out.frame;pose.position={drive.f(0),drive.f(4),drive.f(8)};
        pose.yaw=wrapAngle(drive.f(0x10)+pi);pose.speed=drive.f(0x238);pose.gear=int(car.vehicle().transmission.gear00);
        auto& detail=pose.detail;detail.rpm=car.vehicle().transmission.tach1c;
        detail.pitch=-drive.f(0x0c);detail.roll=-drive.f(0x14);detail.steering=actor.f(0x3c);
        for(unsigned i=0;i<4;++i){detail.suspension[i]=actor.f(0x40+i*4);detail.rotation[i]=actor.f(0x60+i*4);}
        detail.throttle=drive.f(0x1b8);detail.brake=drive.f(0x1c4);
        detail.elapsed=rules.displayedElapsed();detail.remaining=std::bit_cast<std::int32_t>(out.states[slot].remaining.value);
        detail.sections=out.states[slot].times.sectionTimes;detail.sector=out.states[slot].times.sectionCount;
        detail.capacity=1;for(auto index:rules.rules().sectionIndices)if(index>=0)++detail.capacity;
        detail.capacity=std::min(detail.capacity,4u);
        detail.progress=float(out.states[slot].progress.index)+out.states[slot].progress.fraction;
        if(out.driving[slot].invalidScalarDiagnostics)throw std::runtime_error("Online pair contact produced invalid physics");
    }
    if(!s.setup.imported){
        const std::array<OriginalCarSmokeInput,2> cars{
            makeCarSmokeInput(s.cars[0].publishedActors().player0C8FF388,s.smokeOrigins[0],s.fsca),
            makeCarSmokeInput(s.cars[1].publishedActors().player0C8FF388,s.smokeOrigins[1],s.fsca)};
        for(unsigned view=0;view<2;++view){
            const auto& profile=s.setup.profiles[view];
            const OriginalCarSmokeScene scene{s.setup.condition/2,unsigned(s.setup.night),unsigned(s.setup.wet),
                profile.u(4),0,(profile.u(1180)&128u)!=0};
            const std::array<OriginalCarSmokeInput,2> viewed{cars[view],cars[1-view]};
            s.cars[view].withSharedRandom([&](auto& seed){
                out.smokeInputs[view]=captureReplayCarSmoke(scene,viewed,seed);
                s.state.smoke[view].advanceShared(scene,viewed,seed);
            });
        }
        out.smokeAvailable=true;
    }
    // The source outer dispatcher completes the race's smoke owner before
    //1415E0 chooses tire sound using the same random state. Both current poses
    // must exist before either view advances its effects; rendering never owns
    // this step. Each player's stream and smoke state are rollback checkpoints.
    for(unsigned slot=0;slot<2;++slot){
        auto& sound=s.state.sound[slot];
        s.cars[slot].finishSoundFrame([&](auto& seed){out.tires[slot]=stepOriginalTireAudio(sound.tire,0,seed);});
        sound.queue.tick();
    }
    // Evaluate both crossings in the same source tick before settling a photo
    // finish. Freeze race clocks/progress after this tick; the trailing car is
    // not assigned a fictional finish time. Both cars brake on following ticks.
    if(s.state.winner==-3)s.state.winner=resolveOnlineRaceWinner(out.states);
    ++s.state.frame;s.lastFrame=out;return out;
}
OnlineRaceSimulation::Checkpoint OnlineRaceSimulation::checkpoint()const {
    const auto& s=*impl_;Checkpoint out;
    out.data_=std::make_shared<Checkpoint::Data>(Checkpoint::Data{{s.cars[0].checkpoint(),s.cars[1].checkpoint()},
        {s.rules[0].state(),s.rules[1].state()},s.state,s.epoch});return out;
}
void OnlineRaceSimulation::restore(const Checkpoint& checkpoint){
    auto& s=*impl_;if(!checkpoint.data_||checkpoint.data_->epoch!=s.epoch)throw std::invalid_argument("Online checkpoint belongs to another race");
    for(unsigned slot=0;slot<2;++slot){s.cars[slot].restore(checkpoint.data_->cars[slot]);s.rules[slot].restoreNumericalState(checkpoint.data_->rules[slot]);}
    s.state=checkpoint.data_->state;
}
std::uint64_t OnlineRaceSimulation::digest()const {
    const auto& s=*impl_;std::uint64_t hash=14695981039346656037ull;
    auto add=[&](std::uint64_t value){for(unsigned i=0;i<8;++i){hash^=(value>>(8*i))&255;hash*=1099511628211ull;}};
    auto f=[&](float value){add(std::bit_cast<std::uint32_t>(value));};
    add(s.setup.showcaseFrames);
    for(unsigned slot=0;slot<2;++slot){
        add(s.cars[slot].rollbackDigest());const auto& r=s.rules[slot].state();
        add(unsigned(r.phase));add(r.elapsed.value);add(r.elapsed.status);add(r.remaining.value);add(r.remaining.status);
        add(r.progress.index);f(r.progress.fraction);add(r.previousCoordinate.index);f(r.previousCoordinate.fraction);
        for(float v:r.previousPosition)f(v);
        add(r.previousSampleTime);add(r.lapCounter);add(r.sectionCounter);add(r.extensionCounter);add(r.automaticBrake);add(r.timeUpFlag);
        for(auto v:r.times.lapTimes)add(v);for(auto v:r.times.sectionTimes)add(v);add(r.times.lapCount);add(r.times.sectionCount);add(r.times.finishTime);
        const auto& a=s.state.sound[slot];const auto& e=a.engine;
        f(e.previousThrottle);f(e.shiftOffset);f(e.previousRpm);add(e.fallingRpmFrames);add(e.shiftFrames);add(e.previousGear);add(e.auxiliaryLevel);
        add(e.decayFrames);add(e.heldVolume);add(e.recoveryFrames);add(e.decayLatched);add(e.roadFrames);add(e.backfireFrames);add(e.previousRoadFrames);add(e.backfirePattern);
        for(auto v:e.lastVolume)add(v);for(auto v:e.lastPitch)add(v);add(a.queue.cooldown);
        f(a.tire.strength);f(a.tire.volume);add(a.tire.kind);add(a.tire.surface);add(a.tire.phase);add(a.tire.frames);add(a.tire.requested);
        // Emitter/particle lifetime determines future shared RNG consumption.
        // A matching car seed alone cannot certify the next frame anymore.
        add(s.state.smoke[slot].digest());
        add(s.state.coordinates[slot].index);f(s.state.coordinates[slot].fraction);
    }
    for(const auto& shape:s.state.contact.shapes0C401B04)for(auto word:shape.words)add(word);
    add(s.state.contact.count0CA9B360);for(const auto& point:s.state.contact.intersections0CA9B364)for(float v:point)f(v);
    add(s.state.winner);add(s.state.start.started());add(s.state.start.remaining());add(s.state.frame);add(s.state.contactFrames);return hash;
}
std::uint64_t OnlineRaceSimulation::frame()const{return impl_->state.frame;}
std::uint64_t OnlineRaceSimulation::contactFrames()const{return impl_->state.contactFrames;}
const OriginalDrivingSession& OnlineRaceSimulation::car(unsigned slot)const{return impl_->cars.at(slot);}
const OriginalRaceRules& OnlineRaceSimulation::rules(unsigned slot)const{return impl_->rules.at(slot);}
const OnlineRaceSetup& OnlineRaceSimulation::setup()const{return impl_->setup;}
const OnlineRaceFrame& OnlineRaceSimulation::lastFrame()const{return impl_->lastFrame;}
const OriginalRaceStart& OnlineRaceSimulation::start()const{return impl_->state.start;}
const CarSmokeState& OnlineRaceSimulation::smoke(unsigned view)const{return impl_->state.smoke.at(view);}
}
