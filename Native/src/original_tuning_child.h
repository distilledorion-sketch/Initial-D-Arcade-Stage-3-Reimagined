#pragma once
#include "original_tuning.h"

namespace idas3::original {
struct OriginalTuningChild {
    OriginalTuningChildKind kind=OriginalTuningChildKind::none;
    std::uint32_t frame=0,phase=0,car=0,package=0,selected=0,skip=1;
    std::uint32_t balance=0,count=0,picture=0,extraIndex=0,nextThreshold=0;
    std::int32_t current=-1,next=-1;
    std::uint32_t flags=1;
    float completionX=0,completionY=0,completionZ=0;
    std::uint32_t choice=0xffffffffu,choiceCursor=1,optionalIndex=0,balanceBeforeSpend=0;
};
struct OriginalTuningChildInput {
    // Original0D43A0 normalized menu axis, with the neutral cursor at0.5.
    float selectionAxis=0.5f;
    bool confirm=false;
};
struct OriginalTuningChildFrame {
    // Return value of115F60/116EA0/117F40. Apply1..6 once using
    // applyOriginalTuningCommand;14 releases the owning results screen.
    std::uint32_t command=0;
    bool finished=false,descriptionChanged=false;
    std::uint32_t descriptionAddress=0;
    float descriptionX=0,descriptionY=388;
    std::vector<std::uint32_t> directCueIds;
};
// Numeric constructor state only. Bank/text allocation remains presentation.
// prepareOriginalResultTuning must already have selected and initialized the
// child (including optional timeout879 in the profile).
OriginalTuningChild beginOriginalTuningChild(const OriginalBattleProfile&,const OriginalTuningData&,OriginalTuningChildKind);
// One original60Hz update. Basic/performance input is deliberately ignored.
// Optional choice timeout is a profile mutation; rewards/purchases occur only
// when the caller dispatches the returned command, in original owner order.
OriginalTuningChildFrame advanceOriginalTuningChild(OriginalTuningChild&,OriginalBattleProfile&,const OriginalTuningData&,const OriginalTuningChildInput& = {});
}
