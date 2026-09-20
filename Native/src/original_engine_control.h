#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <vector>

namespace idas3::original {
// Native control side of the continuous sound path (0C3B60/0C3C60/0C4360).
// Commands retain their unpacked AM2 driver arguments. Sample playback and DSP
// are separate: this controller neither synthesizes audio nor runs game opcodes.
struct OriginalEngineCurve {
    std::int32_t scale=0,mode=0,shiftWeight=0,reserved=0;
    float constant=0,linear=0,nonlinear=0;
};
struct OriginalEngineFamily {
    std::array<OriginalEngineCurve,4> curves{};
    float maximumRpm=0;
    std::array<std::int32_t,2> pitchLimits{};
};
struct OriginalEngineTables {
    std::array<OriginalEngineFamily,36> families{};
    std::array<std::uint16_t,3> backfirePatterns{};
    static OriginalEngineTables load(const std::filesystem::path& root);
};
std::int32_t evaluateOriginalEngineCurve(const OriginalEngineCurve& curve,float input);
struct OriginalEngineConfiguration {
    unsigned originalCar=0,family=0;
    float volume0=0,pitch0=0,volumePitch1=0;
    bool auxiliaryLoop=false,releaseCue=false,backfire=false;
};
OriginalEngineConfiguration configureOriginalEngine(unsigned family,int tuningLevel,
    std::uint8_t option34,std::uint8_t option3e,std::uint8_t option42);
struct OriginalEngineControlState {
    float previousThrottle=0,shiftOffset=0,previousRpm=0;
    std::int32_t fallingRpmFrames=0,shiftFrames=0,previousGear=0,auxiliaryLevel=0;
    std::int32_t decayFrames=30,heldVolume=0,recoveryFrames=0;
    bool decayLatched=true;
    std::int32_t roadFrames=0,backfireFrames=0,previousRoadFrames=0;
    std::uint16_t backfirePattern=0;
    std::array<std::int32_t,4> lastVolume{},lastPitch{};
};
//0C4114..0C4160 resets only these fields; backfire/past-road globals survive.
void resetOriginalEngineControl(OriginalEngineControlState& state);
enum class OriginalEngineCommandTarget { Continuous, RaceCue1424A0, RaceCue1424E0 };
struct OriginalEngineCommand {
    OriginalEngineCommandTarget target=OriginalEngineCommandTarget::Continuous;
    std::uint32_t handle=0,command=0;
    std::int32_t value=0;
    bool operator==(const OriginalEngineCommand&)const=default;
};
struct OriginalEngineControlInput {
    float rpm=0,throttle=0;
    std::int32_t gear=0;
    bool suppressShiftRelease=false; // byte2F4DE0
    std::array<std::uint8_t,4> wheelSurface{}; // actor116..119
    std::array<std::uint32_t,4> handles{0,1,2,3};
};
// The source shares RNG37C778 with driving. Call at the142860 boundary, before
// contact completion consumes its next random value; never use a private seed.
std::vector<OriginalEngineCommand> stepOriginalEngineControl(const OriginalEngineTables& tables,
    const OriginalEngineConfiguration& configuration,OriginalEngineControlState& state,
    const OriginalEngineControlInput& input,std::uint32_t& sharedRandomSeed,
    const std::function<void(const OriginalEngineCommand&,std::uint32_t&)>& cueOutput={});
}
