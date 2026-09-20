#pragma once
#include "original_battle_profile.h"
#include <array>
#include <cstdint>

namespace idas3::original {
enum class OriginalBuntaEligibility { Eligible, InsufficientPoints, CardRequired };
// Manual-confirm filter125E20: points warning takes precedence over card
// warning. 31CE38 is the profile's own +1180 word, not a separate host flag.
OriginalBuntaEligibility originalBuntaEligibility(const OriginalBattleProfile&);
// Accepted-card flag write0FECAE..0FECB6 from iSelCardIn. A native virtual
// card owner may call this after it has successfully loaded/created its
// persistent profile. Only flag1 changes; no points, levels or card-use data
// are invented. This is not a port of the physical card reader protocol.
void applyOriginalAcceptedCardFlag(OriginalBattleProfile&);
// Timeout confirmation126006..12606A. Field572 is set after the menu
// countdown reaches zero. Ineligible choice2 then falls back to TimeAttack1.
std::uint32_t originalModeAfterBuntaEligibility(std::uint32_t selectedMode,
    bool menuTimedOut572,std::int32_t points72,std::uint32_t flags0C31CE38);

// Fields shared by the original common ARace initialization paths.
struct OriginalBuntaCommonRaceFields {
    std::uint32_t numericRaceMode1640{},course1652{},scene1656{},direction1660{},
        night1664{},weather1668{},condition1568{},ruleRow1564{};
    std::uint32_t playerGridSlot{},rivalGridSlot{};
    std::int32_t ordinaryRivalControl{-1};
    std::uint32_t timerMode{2},initialTime6000{};
    std::array<std::int32_t,6> extensionSeconds{};
};
// Alternate HRaceBunta::Create1871C0, not the live HBunta1848C0 race owner.
// Retained only to make those original contracts explicit; do not use this
// factory for the live Bunta game. Course9 is its special scene/2 branch.
OriginalBuntaCommonRaceFields originalLegacyBuntaCommonRaceSetup(
    const OriginalBattleProfile&,std::uint32_t cabinetDifficultyCode);

// Live076B80 -> HBunta1848C0 -> HRace0903C0/090960/0907E0. The common race
// uses numeric mode0, while the profile/solver mode remains2. Timers still
// use the profile2 override. The ordinary rival is initialized normally.
struct OriginalBuntaRaceSetup : OriginalBuntaCommonRaceFields {
    std::uint32_t profileMode0C901648{2},enemyId0C9015E0{},level0C9015D0{},
        geometryCar0C9015F8{},opponentProgress0C901644{};
    std::array<std::uint32_t,8> progress0C901604{};
};
OriginalBuntaRaceSetup makeOriginalBuntaRaceSetup(
    const OriginalBattleProfile&,std::uint32_t cabinetDifficultyCode);
}
