#pragma once
#include <cstdint>

namespace idas3::original {
// Arithmetic/event boundaries only: source title camera/bank submission and
// maker selection/profile setters remain owned by their native callers.
struct OriginalTitleTransition {
    std::uint32_t frame84{};
};
struct OriginalTitleTransitionEvents {
    std::uint32_t blackOverlayArgb{};
    bool finishRequested{},drawCabinetMark45{};
};
//083DC0 uses the old frame, draws the overlay, requests finish only at900,
// then increments frame84. It does not supply invented title idle motion.
OriginalTitleTransitionEvents tickOriginalTitleTransition(
    OriginalTitleTransition&,std::uint32_t cabinetMode);

struct OriginalMakerTransition {
    std::uint32_t frame444{},phase448{},fade456{},overlayEnabled460{},
        timedOut484{},selected440{},sharedCountdown1176{},profileFlags1180{},
        parentEvent64{};
};
struct OriginalMakerTransitionInput {
    bool confirmPressed{};
    std::uint32_t selectedIndex{},profileMaker40{};
};
struct OriginalMakerTransitionEvents {
    bool selectionChanged{},selectionCommitted{},resetSelectedCar{},
        confirmationPhaseWritten{},parentRequested{};
    float confirmationPhase{};
};
// Sparse12C800 timing writes. Shared countdown/flags, selection and the
// parent's pre-existing event are deliberately preserved.
void initializeOriginalMakerTransition(OriginalMakerTransition&);
//12CCC0 state machine and tail. The caller supplies already-conditioned
// confirm input and1B39C0's selected index; profile mutation is an event.
OriginalMakerTransitionEvents tickOriginalMakerTransition(
    OriginalMakerTransition&,const OriginalMakerTransitionInput&);
//12CFA0/192560/222300. This consumes the current state and does not advance it.
std::uint32_t originalMakerFadeArgb(const OriginalMakerTransition&);

struct OriginalFadeOverlay {
    bool resourceAvailable{};
    std::uint32_t diffuse0{},diffuse1{};
};
struct OriginalFadeOverlayEvents {bool submitted{},controlBit2Set{};};
//0C5200's r6=0 branch used by these two screens: identical GMP diffuse
// writes, zero translation, then16D500(2) or16D440(2) according to alpha.
// Matrix stack, geometry submission and the global flag owner are explicit
// callbacks/events at the host boundary, not hidden renderer substitutes.
OriginalFadeOverlayEvents applyOriginalSimpleFadeOverlay(
    OriginalFadeOverlay&,std::uint32_t argb);
}
