#pragma once
#include "original_battle_profile.h"

namespace idas3::original {
struct OriginalTimeAttackPointsInput {
    // AResultTA's source race-result field+92. Signed values<=1 finished;
    // values>1 follow the time-up branch. This is not the host race enum.
    std::int32_t resultStatus92=2;
    std::uint32_t elapsed6000=0;
    // Snapshot BEFORE inserting this finish. Original course/model tables
    // have factory defaults; personal0 alone denotes no previous record.
    std::uint32_t previousCourse6000=0,previousModel6000=0,previousPersonal6000=0;
};
struct OriginalTimeAttackPoints {
    std::uint32_t participation=0,finish=0,recordBonus=0;
    std::uint32_t courseBonus=0,modelBonus=0,personalBonus=0;
    std::uint32_t total=0,balanceBeforeCap=0;
};
//07DE00..07DF32,192380/1923C0/192400. Pure calculation: the common result
// owner commits/caps once, then creates the tuning child. Never updates times.
OriginalTimeAttackPoints calculateOriginalTimeAttackPoints(
    const OriginalBattleProfile&,const OriginalTimeAttackPointsInput&);
}
