#pragma once
#include "original_battle_profile.h"
#include "original_tuning.h"
#include <cstdint>
#include <vector>

namespace idas3::original {
// iSelOptCrs129240, Init129CC0/Main12AD80. Resource creation, the analog
// selector1B39C0 and preview drawing remain explicit external boundaries.
struct OriginalTuningCourseMenu {
    std::uint32_t confirmationFrames460{},phase464{},fade472=7,fadeMaximum476=7,
        fadeEnabled480=1,car484{},selectionFrames488{},frame492{},selected496{},
        previous500=0xffffffffu,count504{},parentEvent64{},
        previousScreen76=0x0802,alternateScreen80=0x0700;
    std::uint8_t enabled604{},enteredOnLast628{},lastLatched629{},leftLast630{},timedOut636{};
    std::uint32_t leftLastFrames632{};
};
struct OriginalTuningCourseMenuInput {
    std::uint32_t selectedIndex{}; // Actual1B39C0 result; native navigation adapter.
    bool confirmPressed{}; //0D4300(1) OR signed digital-byte92ED00<0.
    // Init12A150..12A1C8: count3->2,count4->3,otherwise4. Despite its legacy
    // field name this is a layout selector, not a changing movement phase.
    std::uint32_t selectorPhase620{};
};
struct OriginalTuningCourseMenuEvents {
    bool selectionChanged{},profileCommitted{},parentRequested{},previewCommit{};
    std::vector<unsigned> cueIds;
};
// Resets the actual source menu timer to1679 and controller fields. It does
// not alter the selected package, parts, points or source routing flags.
void initializeOriginalTuningCourseMenu(OriginalTuningCourseMenu&,
    OriginalBattleProfile&,const OriginalTuningData&);
OriginalTuningCourseMenuEvents tickOriginalTuningCourseMenu(OriginalTuningCourseMenu&,
    OriginalBattleProfile&,const OriginalTuningCourseMenuInput&);
std::uint32_t originalTuningCourseFadeArgb(const OriginalTuningCourseMenu&);
//1AF660 receives this value in phase2 (the source does not clamp it).
float originalTuningCourseConfirmationPhase(const OriginalTuningCourseMenu&);
}
