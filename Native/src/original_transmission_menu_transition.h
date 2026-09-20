#pragma once
#include <cstdint>

namespace idas3::original {
// Original iSelMission (transmission choice), distinct from the powertrain.
struct OriginalTransmissionMenuTransition {
    std::uint32_t frame452{},exitHold456{},fade460{},overlayEnabled464{},
        committed468{},phase476{},selected512{},sharedCountdown1176{},
        profileFlags1180{},profileTransmission68{},profileCar16{},
        parentEvent64{},previousScreen76{},alternateScreen80{};
    std::uint8_t timedOut516{},profileByte1192{},profileByte152{};
};
struct OriginalTransmissionMenuTransitionInput {
    bool confirmPressed{};
    std::uint32_t selectedIndex{}; //1B39C0 return: AT0, MT1.
};
struct OriginalTransmissionMenuTransitionEvents {
    bool selectionChanged{},selectionCommitted{},parentRequested{},
        profileByte152Cleared{},confirmationPhaseWritten{};
    float confirmationPhase{};
};
// Sparse124440 writes; selection copies profile+68. Shared timer, profile
// state and caller-supplied routing fields survive initialization.
void initializeOriginalTransmissionMenuTransition(OriginalTransmissionMenuTransition&);
// Full1249A0 timing/selection/profile/routing arithmetic at60Hz. The original
// owner polls confirm only: it has no cancel button branch.
OriginalTransmissionMenuTransitionEvents tickOriginalTransmissionMenuTransition(
    OriginalTransmissionMenuTransition&,const OriginalTransmissionMenuTransitionInput&);
//124D88..124DBE. Caller draws only when overlayEnabled464 is nonzero.
std::uint32_t originalTransmissionMenuFadeArgb(const OriginalTransmissionMenuTransition&);
}
