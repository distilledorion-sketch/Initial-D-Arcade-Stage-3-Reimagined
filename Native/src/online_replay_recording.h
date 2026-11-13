#pragma once
#include "online_race_simulation.h"
#include "online_race_timeline.h"
#include "original_car_body_position.h"
#include "original_car_dimensions.h"
#include "original_headlights.h"
#include "original_race_feedback.h"
#include "imported_course.h"
#include "imported_road_presentation.h"

namespace idas3 {
// Presentation inputs are sampled before the local tick, not when a delayed
// network packet confirms it. Numerical poses and smoke come from confirmation.
class OnlineReplayRecording {
public:
    struct Lamps {OriginalHeadlightState state;bool enabled=false;};
    struct Steering {unsigned source=0;int response=0;float deadzone=0,smoothing=0;unsigned shiftHeld=0;};
    const std::array<std::uint32_t,5>& steeringTicks()const{return steeringTicks_;}
    const Steering& steeringSettings()const{return steeringSettings_;}
    bool steeringSettingsChanged()const{return steeringChanged_;}
    void reset(){*this=OnlineReplayRecording{};}
    void begin(const original::OnlineRaceSimulation& sim,unsigned local){
        if(local>1||sim.frame()!=0)throw std::invalid_argument("Replay requires a fresh online race");
        reset();local_=local;active_=true;
        for(unsigned slot=0;slot<2;++slot){const auto& car=sim.car(slot);const auto& d=car.vehicle().drive;
            bodies_[slot].update(car.collision(),sim.setup().profiles[slot].u(16),{d.f(0),d.f(4),d.f(8)});
            feedback_[slot].reset(std::bit_cast<std::int32_t>(sim.rules(slot).state().remaining.value));}
    }
    bool active()const{return active_&&!finished_;}
    void sampleLamps(std::uint64_t frame,const std::array<Lamps,2>& lamps,const Steering& steering){
        if(!active())return;
        if(frame<next_||frame-next_>original::OnlineRaceTimeline::capacity)
            throw std::logic_error("Replay lamp sample outside bounded prediction window");
        auto& entry=lamps_[frame%lamps_.size()];
        if(entry.frame!=absent&&entry.frame!=frame)throw std::logic_error("Unconfirmed replay lamps would be overwritten");
        entry={frame,lamps,steering};
    }
    void confirm(const original::OnlineRaceFrame& frame,const original::OnlineRaceSimulation& sim,
            Replay& player,Replay& opponent){
        if(!active())return;
        auto& entry=lamps_[frame.frame%lamps_.size()];
        if(frame.frame!=next_||entry.frame!=frame.frame)throw std::logic_error("Online replay confirmation has no matching presentation sample");
        auto poses=frame.replayPoses;
        for(unsigned slot=0;slot<2;++slot){auto& pose=poses[slot];auto& d=pose.detail;
            auto& flash=backfireFrames_[slot];if(flash)--flash;
            for(const auto& cue:frame.engine[slot])
                if(cue.target==original::OriginalEngineCommandTarget::RaceCue1424A0&&cue.value==7&&flash==0)flash=2;
            pose.backfire=flash?int(2-flash):-1;
            const auto car=sim.setup().profiles[slot].u(16);
            d.bodyPosition=bodies_[slot].update(sim.car(slot).collision(),car,pose.position);
            if(slot==local_&&sim.setup().imported&&!importedCourseDefinition(sim.setup().imported->id).specialStage){
                const auto& road=bodies_[slot].query();const auto id=sim.setup().imported->id;
                if(id==10)localRoad_.anchor(pose.position,d.pitch,d.roll,road.f(16),{road.f(0),road.f(4),road.f(8)},pose.yaw,bodies_[slot].surfaceFound(),1./60.);
                else localRoad_.update(pose.position,d.pitch,d.roll,road.f(16),bodies_[slot].surfaceFound()&&road.f(4)>.6f,1./60.);
                if(id==10&&localRoad_.ready())d.bodyPosition=Vec3{pose.position.x,road.f(16),pose.position.z}+Vec3{road.f(0),road.f(4),road.f(8)}*originalCarRideHeight(car);
                else d.bodyPosition.y+=localRoad_.position().y-pose.position.y;
                d.pitch=localRoad_.pitch();d.roll=localRoad_.roll();
            }
            auto& feedback=feedback_[slot];
            if(frame.rules[slot].timeExtension)feedback.extend(d.remaining-frame.rules[slot].secondsAdded*6000);
            feedback.tick(d.remaining);d.remaining=feedback.displayedRemaining;d.extension=feedback.extensionTicks;
            const auto& lamps=entry.lamps[slot];
            d.lightCounter=lamps.state.counter;d.lightMaximum=lamps.state.maximumPhase;d.lightPhase=lamps.state.phase;
            d.lightVisible=unsigned(lamps.state.visible);d.lightFraction=lamps.state.fraction;d.lights=unsigned(lamps.enabled);
        }
        entry.frame=absent;++next_;
        if(frame.smokeAvailable)player.recordSmoke(frame.smokeInputs[local_]);
        // Keep showcase/countdown effect history, but start replay pose time at GO.
        if(!frame.start.runRules)return;
        const auto tick=std::uint64_t(player.frames.size()+1);
        auto& local=poses[local_];auto& remote=poses[1-local_];
        const auto& steering=entry.steering;
        local.telemetry.flags|=(std::min(steering.source,4u)<<5)|((steering.shiftHeld&3u)<<3);
        if(player.frames.empty())steeringSettings_=steering;
        else if(steering.response!=steeringSettings_.response||steering.deadzone!=steeringSettings_.deadzone||steering.smoothing!=steeringSettings_.smoothing)steeringChanged_=true;
        ++steeringTicks_[std::min(steering.source,4u)];
        remote.detail.progress=metrics_.advantage(frame.states[local_].progress,frame.states[1-local_].progress);
        player.record(tick,local.position,local.yaw,local.speed,local.gear,&local.detail,local.backfire,local.telemetryAvailable?&local.telemetry:nullptr);
        opponent.record(tick,remote.position,remote.yaw,remote.speed,remote.gear,&remote.detail,remote.backfire);
        for(unsigned slot=0;slot<2;++slot)if(frame.states[slot].phase==original::OriginalRacePhase::Finished)
            (slot==local_?player:opponent).finishTicks6000=frame.states[slot].times.finishTime;
        finished_=original::resolveOnlineRaceWinner(frame.states)!=-3;
    }
    void setMetrics(original::OriginalBattleMetrics metrics){metrics_=std::move(metrics);}
    std::uint64_t confirmed()const{return next_;}
private:
    static constexpr std::uint64_t absent=~std::uint64_t(0);
    struct Entry {std::uint64_t frame=absent;std::array<Lamps,2> lamps;Steering steering;};
    // One extra candidate slot permits a step that reconciles old frames but
    // subsequently stalls. Repeating that attempt replaces only its candidate.
    std::array<Entry,original::OnlineRaceTimeline::capacity+1> lamps_{};
    std::array<OriginalCarBodyPosition,2> bodies_;
    std::array<OriginalRaceFeedback,2> feedback_;
    std::array<unsigned,2> backfireFrames_{};
    std::array<std::uint32_t,5> steeringTicks_{};
    Steering steeringSettings_;bool steeringChanged_=false;
    ImportedRoadPresentation localRoad_;
    original::OriginalBattleMetrics metrics_;
    std::uint64_t next_=0;unsigned local_=0;bool active_=false,finished_=false;
};
}
