#pragma once
#include "original_battle_profile.h"

namespace idas3::original {
enum class OriginalLegendResult { NotLegend,Loss,Win };
// Complete1343A0/134480 profile mutations; the caller invokes these once per
// settled result. Counts are saturating nibbles, not boolean clear flags.
void recordOriginalLegendWin(OriginalBattleProfile& profile,std::uint32_t enemy);
void recordOriginalLegendLoss(OriginalBattleProfile& profile,std::uint32_t enemy);
//05CFAC..05D026: only profile mode0 records a Legend result. A win requires
// both the original finish byte1572 and outcome1644==0. Timeout/retire while
// ahead therefore records a loss. No persistence or repeated-call guard is
// silently introduced; the native result lifecycle owns that boundary.
OriginalLegendResult recordOriginalLegendResult(OriginalBattleProfile& profile,
    std::uint8_t finishedByte1572,std::uint32_t outcome1644);

struct OriginalLegendPoints {
    std::uint32_t participation{},win{},advantage{},total{},balanceBeforeCap{};
};
//091714..0917FA: call AFTER the result counter update. The source result
// status is zero for a win; every other status receives participation only.
// advantage84 is the original signed metre advantage, not path-index units.
OriginalLegendPoints calculateOriginalLegendPoints(const OriginalBattleProfile& profile,
    std::uint32_t resultStatus80,float advantage84);
//06FC48..06FCA4: commit the source unsigned balance with its999,999,999 cap.
// The surrounding result screen and persistent-profile lifecycle call once.
OriginalLegendPoints awardOriginalLegendPoints(OriginalBattleProfile& profile,
    std::uint32_t resultStatus80,float advantage84);
//0F1AD8..0F1B34, iRivalWon dialog initialization AFTER the results screen.
// Recomputes the selected-course defaults from leading cleared rivals. The
// source limits are surprisingly{4,4,4,4,4,4,0,0,0}; preserve them exactly.
// This is a separate lifecycle operation, not part of1343A0's counter update.
void refreshOriginalLegendCourseProgress(OriginalBattleProfile& profile);
//18A940 and its three pure profile components, then05D026..05D084. This
// common post-result rank update also applies to the other original modes.
std::uint32_t calculateOriginalDriverRank(const OriginalBattleProfile& profile);
void updateOriginalPostRaceRank(OriginalBattleProfile& profile);
}
