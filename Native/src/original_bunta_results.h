#pragma once
#include "original_battle_profile.h"

namespace idas3::original {
// Shared race-result05CE60: clear transient progress flags, then apply its
// mode2 winning branch. Call once on result entry, before scoring below.
// A finished loss/time-up does not advance the course level. Snow aliases3.
bool recordOriginalBuntaResult(OriginalBattleProfile&,
    std::uint8_t finishedByte1572,std::uint32_t outcome1644);

struct OriginalBuntaPoints {
    std::uint32_t participation{},win{},advantage{},total{},balanceBeforeCap{};
    bool deduction{};
};
// Live HResultBunta187FA0, called through the result child constructed by
// HBunta1848C0. Level is read AFTER the shared result mutation above.
// Result status0 means win; other values use the source point deduction.
OriginalBuntaPoints calculateOriginalBuntaPoints(const OriginalBattleProfile&,
    std::uint32_t resultStatus80,float advantage84);
// Common06FC48..06FCA2 cap/commit, preserving unsigned arithmetic.
OriginalBuntaPoints awardOriginalBuntaPoints(OriginalBattleProfile&,
    std::uint32_t resultStatus80,float advantage84);
}
