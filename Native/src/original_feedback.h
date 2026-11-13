#pragma once
#include <cstdint>

namespace idas3ffb {
// Inputs at the original 0C15A5E0 producer boundary. Offset suffixes refer to
// the drive object at0C900F00. This contract has no hardware gain, elapsed wall
// time, synthetic impact envelope, or dependency on the emulated drive board.
struct OriginalInput {
    std::uint32_t carIndex=0;
    // Explicit host default for original0C9015D8 (configuration nibble clamped
    // to0..10). Its original menu caption is unresolved; this is NOT device gain.
    std::uint32_t globalSetting=5;
    std::uint32_t cardSteeringLevel=4; // profile byte1187 ->0C9015EC
    std::uint32_t condition=0;        // original0C9015CC,0..17
    float steering1CC=0,headingFactor27C=0,acceleration24C=0;
    float frontContact3B4=0,frontContact3B8=0;
    float normalizedFrontContact3C4=0,normalizedFrontContact3C8=0;
    std::uint32_t wallContact150=0,wallCorner154=0;
    float wallMagnitude158=0;
    std::uint32_t wet434=0,snow438=0,oil140=0,automaticBrake1A8=0;
    float drive258=0,drive25C=0,drive260=0,drive264=0;
    float normalizedSpeed248=0,filteredRpm=0;
    std::uint32_t surface164=0,surface168=0,roadFlag14C=0;
    std::uint32_t gear=1,gearSnapshot=1;
    float transmissionNormalized=0;
};
struct OriginalOutput {
    // Invalid input cannot advance state. The output owner must stop/release
    // its devices on false; no invalid packet should be sent to a drive board.
    bool valid=false;
    std::int32_t torqueCode=128,damperParameter=0,rumbleAmplitude=0;
    std::uint32_t rumbleFrequencyHalfHz=0;
    bool sendTorque=false,sendDamper=false,sendRumble=false;
    std::int32_t limit=0;
};
struct OriginalProducerState {
    std::uint32_t ramp=0;
    std::int32_t torqueCode=128,damperParameter=0;
    std::uint32_t rumbleLatch=0;
};
class OriginalProducer {
public:
    // Original159F40 resets ramp and torque/damper caches, but retains975C.
    // A newly constructed producer starts with a zero rumble latch.
    void reset() noexcept;
    // Exactly one original producer invocation. Call from the simulation owner,
    // not once per render or driver-send frame. Positive rumble is sent EVERY
    // invocation; torque and damping are sent only when their values change.
    OriginalOutput update(const OriginalInput& input) noexcept;
    const OriginalProducerState& state() const noexcept { return state_; }
private:
    OriginalProducerState state_{};
};
}
