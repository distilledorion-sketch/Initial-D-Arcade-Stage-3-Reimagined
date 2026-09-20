#pragma once
#include <cstdint>

namespace idas3::original {
// Timing fields of iSelCar. Selection, color/profile setters and showroom
// objects are owned by the caller; this helper reports their transition events.
struct OriginalCarMenuTransition {
    std::uint32_t frame452{},phase456{},fade464{},fadeRange468{},
        overlayEnabled472{},frame476{},sharedCountdown1176{},
        profileFlags1180{},parentEvent64{},previousScreen76{};
    std::uint8_t timedOut672{};
};
struct OriginalCarMenuTransitionInput {bool confirmPressed{},cancelPressed{};};
struct OriginalCarMenuTransitionEvents {
    bool selectionCommitted{},cancelAccepted{},showroomCommitRequested{},
        parentRequested{},confirmationPhaseWritten{};
    float confirmationPhase{};
};
// Sparse timing initialization from12DC60; preserves shared timer/flags and
// the parent's previous-screen/event fields.
void initializeOriginalCarMenuTransition(OriginalCarMenuTransition&);
//12E520 phase dispatch + counter/confirmation tail. Call once per original
//60Hz update. Only phase1 accepts selection/confirm/cancel input.
OriginalCarMenuTransitionEvents tickOriginalCarMenuTransition(
    OriginalCarMenuTransition&,const OriginalCarMenuTransitionInput&);
//12EB20's numeric overlay boundary; respects source unsigned conversion.
// Caller submits only when overlayEnabled472 !=0. Source submits the model,
// then this overlay, then1BBCC0; final visibility also follows native depth.
std::uint32_t originalCarMenuFadeArgb(const OriginalCarMenuTransition&);
}
