#pragma once
#include "original_battle_profile.h"
#include "original_driving_session.h"
#include "original_engine_control.h"
#include "original_race_rules.h"
#include "original_race_start.h"
#include "original_tire_audio.h"
#include "car_smoke_state.h"
#include "replay_car_smoke.h"
#include "race.h"

namespace idas3 {struct ImportedCourse;}
namespace idas3::original {
struct OnlineRaceSetup {
    std::shared_ptr<const idas3::ImportedCourse> imported;
    std::uint32_t condition=0;
    bool wet=false,collisions=true,boost=false;
    // The presentation owner requests these neutral input frames before the
    // countdown. They belong to the shared rollback timeline on both peers.
    std::uint32_t showcaseFrames=0;
    std::array<OriginalBattleProfile,2> profiles;
    std::array<bool,2> automatic{true,true};
    bool night=false;
};
struct OnlineRaceFrame {
    std::uint64_t frame=0;
    OriginalRaceStartFrame start;
    std::array<OriginalDrivingStepEffects,2> driving;
    std::array<OriginalRaceEvents,2> rules;
    std::array<OriginalRaceRuleState,2> states;
    std::array<std::vector<OriginalEngineCommand>,2> engine;
    std::array<std::vector<OriginalTireCommand>,2> tires;
    // Regenerated on rollback; consumers persist only confirmed outputs.
    std::array<ReplayCarSmokeFrame,2> smokeInputs;
    bool smokeAvailable=false;
    // Numerical pose/telemetry for this exact tick, regenerated on rollback.
    // Body placement, lamps and presentation timers are bound by the recorder.
    std::array<ReplayFrame,2> replayPoses{};
};
// Keep source sub-frame crossing precision when deciding a photo finish.
inline int resolveOnlineRaceWinner(const std::array<OriginalRaceRuleState,2>& states){
    const bool firstFinished=states[0].phase==OriginalRacePhase::Finished,secondFinished=states[1].phase==OriginalRacePhase::Finished;
    if(firstFinished!=secondFinished)return firstFinished?0:1;
    if(!firstFinished)return states[0].phase==OriginalRacePhase::TimeUp&&states[1].phase==OriginalRacePhase::TimeUp?-1:-3;
    const auto first=states[0].times.finishTime,second=states[1].times.finishTime;
    return first==second?2:first<second?0:1;
}
// One canonical slot-ordered simulation for BOTH human cars. The same core
// can run on the authoritative host and a predicting client. It owns no
// renderer, devices, transport, persistence, or result awards.
class OnlineRaceSimulation {
public:
    class Checkpoint {
        struct Data;
        std::shared_ptr<const Data> data_;
        friend class OnlineRaceSimulation;
    public:
        Checkpoint()=default;
        std::size_t memoryBytes()const;
    };
    OnlineRaceSimulation(const std::filesystem::path& root,const OnlineRaceSetup& setup);
    ~OnlineRaceSimulation();
    OnlineRaceSimulation(OnlineRaceSimulation&&)noexcept;
    OnlineRaceSimulation& operator=(OnlineRaceSimulation&&)noexcept;
    OnlineRaceSimulation(const OnlineRaceSimulation&)=delete;
    OnlineRaceSimulation& operator=(const OnlineRaceSimulation&)=delete;
    OnlineRaceFrame step(const std::array<OriginalVehicleInputs,2>& controls);
    Checkpoint checkpoint()const;
    void restore(const Checkpoint& checkpoint);
    std::uint64_t digest()const;
    std::uint64_t frame()const;
    std::uint64_t contactFrames()const;
    const OriginalDrivingSession& car(unsigned slot)const;
    const OriginalRaceRules& rules(unsigned slot)const;
    const OnlineRaceSetup& setup()const;
    const OnlineRaceFrame& lastFrame()const;
    const OriginalRaceStart& start()const;
    const CarSmokeState& smoke(unsigned view)const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
