#pragma once
#include "original_dynamics.h"
#include "original_transmission.h"
#include <functional>

namespace idas3::original {
struct OriginalContactCompletionState {
    std::uint32_t previousFlag0CAA94C4=0;
    std::array<std::uint32_t,5> cues0C900E5C{};
    std::uint32_t elapsedFrames0C900E84=0,steeringMask0C900EBC=0;
    std::uint32_t randomSeed0C37C778=0;
    std::array<std::uint32_t,8> snapshot0CAA9718{};
    // Native backing for the two original append cursors at91FB0C+36/+40.
    // 157AE0 records only while the incremented drive+3F8 count is <=98.
    std::array<std::array<std::uint32_t,3>,128> impactPositions{};
    std::array<std::uint32_t,128> impactFrames{};
    std::uint32_t positionCursor=0,frameCursor=0;
};
struct OriginalContactCompletionInputs {
    std::uint32_t frame0C92DE30=0;
    std::uint8_t digitalByte0C92ED00=0;
};
struct OriginalContactCompletionEffects {
    // Every invocation requests the original142860 platform audio update.
    std::uint32_t engineChannel=0xffffffffu,gear=0;
    float engineValue=0,engineScale=1,throttle=0;
    bool requestCue4=false;
};
using OriginalContactEngineOutput=std::function<void(const OriginalContactCompletionEffects&,std::uint32_t&)>;
// [157D6C,157E6E): original contact timer/cues/history, frame counter, RNG,
// steering mask and optional159A20 snapshot. Call after158200 and157A80.
// Platform audio/cue calls are explicit effects, not replacements for forces.
OriginalContactCompletionEffects finishOriginalContactFrame(OriginalDriveState& drive,
    const idas3::OriginalTransmissionState& transmission,
    OriginalContactCompletionState& state,const OriginalContactCompletionInputs& inputs,
    const OriginalContactEngineOutput& engineOutput={});
}
