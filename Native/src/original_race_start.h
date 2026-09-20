#pragma once
#include "original_driving_session.h"

namespace idas3::original {
struct OriginalRaceStartFrame {
    std::uint32_t countdownRemaining{};
    //05AE20 updates the HUD while its remaining counter is nonzero.
    std::int32_t countdownDigit{-1};
    bool go{},gearEnabled{},runRules{},cue2{},cue3{},requestService1{};
};
// Local ARace countdown child plus its same-frame GO handoff. One step is
// one owner update, not one paint. The auxiliary child survives GO for60 ticks.
class OriginalRaceStart {
public:
    void reset(std::uint32_t numericRaceMode);
    OriginalRaceStartFrame step();
    bool started() const noexcept {return started_;}
    std::uint32_t remaining() const noexcept {return remaining_;}
    std::uint32_t mode() const noexcept {return mode_;}
private:
    std::uint32_t remaining_{240},mode_{2};
    bool started_{};
};

//0620FA..06212A: sixty159920/063CE0 pairs before067A00 initializes the engine.
// The platform frame and physical input snapshot stay fixed. The elapsed
// driving counter/RNG/contact histories still advance on EVERY solver call.
// Set the source progress correction beforehand; no path/race-rule update
// occurs inside this loop. Bind the engine-output callback after this call.
// The optional callback is the host's ACar publication adapter, not an audio
// service. It observes each completed step. No digital edge re-adaptation.
void warmupOriginalRaceSession(OriginalDrivingSession& session,
    const OriginalVehicleInputs& frozenInputs,std::uint32_t platformFrame,
    std::uint8_t digitalByte=0,
    const std::function<void(const OriginalDrivingStepEffects&)>& publish={});
} // namespace idas3::original
