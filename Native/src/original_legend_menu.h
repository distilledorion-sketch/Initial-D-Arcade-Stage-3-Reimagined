#pragma once
#include "original_choice_menu.h"
#include "original_battle_profile.h"

namespace idas3::original {
struct OriginalLegendStartMetadata {std::uint32_t direction=0,race=1;bool extra=false;};
// Shared with the opponent screen: source row direction and race ordinal.
OriginalLegendStartMetadata originalLegendStartMetadata(std::uint32_t enemy);

// Original196C00/196CE0. Profile progress controls secret portraits, clear
// markers, and the selected opponent's weather. Course/choice remain explicit
// controller fields; the caller supplies the selected profile's time/direction.
// The caller owns frame36 and advances it once per original menu draw.
std::vector<OriginalChoiceDraw> originalLegendMenuDraws(const OriginalBattleProfile& profile,
    std::uint32_t course,std::uint32_t choice,float confirmationPhase,std::uint32_t frame);
// Fresh-profile compatibility overload, including original rival selection.
std::vector<OriginalChoiceDraw> originalLegendMenuDraws(std::uint32_t course,
    std::uint32_t choice,float confirmationPhase,std::uint32_t frame);
const char* originalLegendMenuBankName(std::uint32_t bank);
//196710..196746 uses the same alternating0/CCCCCCCC four-entry buffer as
// the existing InactiveTransmission mode, not the route gradient.
NativeModelChunk materializeOriginalLegendColors(const NativeModelChunk& chunk,OriginalChoiceColors colors);
}
