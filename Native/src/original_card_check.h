#pragma once
#include "original_battle_profile.h"
#include <array>
#include <cstdint>
#include <vector>

namespace idas3::original {
// Source iSelCheck fields. This is presentation/input state, not a physical
// card reader and not a second save format. Host save slots remain independent.
struct OriginalCardCheckState {
    std::uint32_t parentEvent64{},frame448{},phase452{},overlay460{},fade464{};
    std::uint32_t page492{},editing496{},confirm516{},view520{};
    std::uint8_t canConfirm524{1};
    std::uint32_t selectedValue1452{},editPhase1456{},row1460{};
    std::array<std::uint32_t,5> committed1524{},draft1544{};
};
struct OriginalCardCheckInput {
    bool confirm{},cancel{},nextPage{};
    // Original gear edges after sign inversion: +1 moves to the previous row,
    // -1 to the next. The input adapter maps controller/wheel navigation here.
    int rowDirection{};
    // Value returned by the original row selector (09BC40), not raw steering.
    std::uint32_t selectorValue{};
};
struct OriginalCardCheckEvents {
    // 141F80(group,1): group 7 for confirm/row movement, 2 for value changes.
    std::vector<unsigned> soundGroups;
    bool profileWritten{},parentRequested{};
};
// 10C1AE..10C1F4: copies only the original five configuration bytes.
void loadOriginalCardCheckOptions(OriginalCardCheckState&,const OriginalBattleProfile&);
// 109520..10963E, before the virtual page-specific update callback.
OriginalCardCheckEvents tickOriginalCardCheckTransition(OriginalCardCheckState&,OriginalBattleProfile&);
// 10C6C0..10C9E6: options edit/commit/cancel, then the common transition.
// The actual selector interpolation and drawing belong to the presentation.
OriginalCardCheckEvents tickOriginalCardCheck(OriginalCardCheckState&,OriginalBattleProfile&,const OriginalCardCheckInput&);
}
