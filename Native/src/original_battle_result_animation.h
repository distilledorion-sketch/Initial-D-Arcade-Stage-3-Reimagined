#pragma once
#include <array>
#include <cstdint>

namespace idas3::original {
struct OriginalBattleResultCommit {
    // Consume this once when the result owner is created. The original commits
    // before its visible count; animation never awards or persists points.
    std::uint32_t balance=0;
    std::int32_t signedDelta=0;
};
struct OriginalBattleResultAnimationSetup {
    std::uint32_t earnedMagnitude=0,balanceBeforeCap=0;
    bool deduction=false;
    // Explicit source tuning-child boundary. A native card/tuning owner must
    // provide this state; absence follows the actual no-child branch.
    bool upgradeChildPresent=false,upgradeNotice=false;
    std::uint32_t upgradeThreshold=0x7fffffffu;
};
struct OriginalBattleResultAnimationState {
    std::uint32_t frame=0,phase=0,delta=0,startingBalance=0,committedBalance=0;
    std::uint32_t displayedBalance=0,upgradeThreshold=0x7fffffffu;
    std::uint32_t balanceBlinkFrame=0;
    bool highlightBalance=false,upgradeNotice=false,upgradeChildPresent=false;
    bool showUpgradeChild=false,complete=false;
};
struct OriginalBattleResultAnimationInput {
    bool confirmEdge=false;
    // Original child command14. Commands that apply tuning/profile changes
    // belong to that child and are not synthesized by this component.
    bool upgradeChildFinished=false;
};
struct OriginalBattleResultAnimationFrame {
    std::uint32_t displayedBalance=0,fadeAlpha=0,sourcePhase=0;
    bool highlightBalance=false,balanceVisible=true,upgradeNotice=false;
    bool showUpgradeChild=false,finished=false;
    std::array<std::uint8_t,2> cueIds{};
    unsigned cueCount=0;
};
OriginalBattleResultCommit initializeOriginalBattleResultAnimation(
    OriginalBattleResultAnimationState&,const OriginalBattleResultAnimationSetup&);
// One original60Hz update. Output describes the image drawn before confirm is
// processed at070ECE; skip affects the following image, just like the source.
OriginalBattleResultAnimationFrame advanceOriginalBattleResultAnimation(
    OriginalBattleResultAnimationState&,OriginalBattleResultAnimationInput={});
}
