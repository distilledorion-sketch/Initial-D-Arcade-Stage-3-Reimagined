#pragma once
#include <cstdint>
#include <array>

namespace idas3::original {
// iSelGameMode owner1259C0/125F40. Profile mode0 uses Legend0,TA1,Bunta2;
// those values are independent of the later race-mode enum.
struct OriginalModeMenuTransition {
    std::uint32_t drawFrame444{},fade448{},confirmationFrame452{},
        overlayEnabled456{},committed460{},phase468{},
        pointsWarning520{},pointsWarningAge524{},cardWarning528{},cardWarningAge532{},
        selected564{},sharedCountdown1176{},profileMode0{},profilePoints72{},
        profileFlags1180{},parentEvent64{};
    std::uint8_t timedOut572{},profileByte1191{};
};
struct OriginalModeMenuTransitionInput {
    bool confirmPressed{}; //0D4300 button1 or cabinet92ED00 bit7.
    std::uint32_t selectedIndex{}; //09C580 return; only consumed in phase1.
    //16DE80's return. Its hidden-code/global-flag machinery remains an
    // external owner; the ordinary native menu supplies false.
    bool hiddenExitRequested{};
};
struct OriginalModeMenuTransitionEvents {
    bool readyRequested{},selectionChanged{},selectionCommitted{},
        pointsRejected{},cardRejected{},parentRequested{},confirmationPhaseWritten{};
    float confirmationPhase{};
    std::int32_t timerDisplayValue{}; //signed shared countdown /1926A0()==80.
};
// Exact sparse lifecycle stores from Init, including new mode timer1279.
// confirmationFrame452, parent event and pre-existing profile selections,
// points, flags and byte1191 are preserved until the source writes them.
void initializeOriginalModeMenuTransition(OriginalModeMenuTransition&);
// Full125E20 input/warning ordering and125F40 lifecycle arithmetic at60Hz.
// There is no cancel-button branch. This does not implement09C580's analog
// selector, draw a warning dialog or invent a higher-level next-screen route.
OriginalModeMenuTransitionEvents tickOriginalModeMenuTransition(
    OriginalModeMenuTransition&,const OriginalModeMenuTransitionInput&);
//126228..12625A. Draw only when overlayEnabled456 is nonzero.
std::uint32_t originalModeMenuFadeArgb(const OriginalModeMenuTransition&);
//12626C..126274 increments this independent source draw counter; call once
// per original presentation frame, rather than once per host repaint.
void advanceOriginalModeMenuDraw(OriginalModeMenuTransition&);
//1261C0: select0402 points panel, then mode_bunta card panel. -1 skips
// a bank. These legacy resources are separate from the V3 selection artwork.
std::array<int,2> originalModeWarningChunks(const OriginalModeMenuTransition&);
}
