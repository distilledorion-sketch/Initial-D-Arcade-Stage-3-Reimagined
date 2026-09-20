#pragma once
#include <array>
#include <cstdint>

namespace idas3::original {
// Exact current-profile extent at31C99C. Offsets are original byte offsets,
// including course clear bytes116..146 and Bunta progress1080..1108.
struct OriginalBattleProfile {
    std::array<std::uint32_t,307> words{};
    std::uint32_t u(std::uint32_t offset)const;
    void setu(std::uint32_t offset,std::uint32_t value);
    std::uint8_t byte(std::uint32_t offset)const;
    void setByte(std::uint32_t offset,std::uint8_t value);
};
//134A60/1348A0(1,FFFF). Mode/car/transmission remain original zero defaults.
OriginalBattleProfile makeOriginalFreshBattleProfile();
struct OriginalRivalRecord {
    std::uint32_t night,direction,car,course,scene;
    std::array<std::uint32_t,3> weather; // fresh, low nibble>0, high nibble>0
};
//31 authored records at31D8A8. Solver profile31 is NOT a roster entry.
const OriginalRivalRecord& originalRival(std::uint32_t enemy);
std::uint32_t originalLegendRivalId(std::uint32_t course,std::uint32_t choice);
struct OriginalLegendChoices {std::uint32_t count{},selected{};};
OriginalLegendChoices originalLegendChoices(const OriginalBattleProfile&,std::uint32_t course);
//133CA0. Mode2 intentionally preserves selected course/scene/route/time/weather.
void selectOriginalRival(OriginalBattleProfile&,std::uint32_t enemy);
//1347C0. The eighth menu entry is Tsuchisaka; Akina becomes Snow above10.
std::uint32_t originalBuntaCourse(std::uint32_t menuIndex,std::int32_t level);
//184374..184566 profile portion. Caller must have chosen mode2. No card,
// charging, animation or additional race object is silently manufactured.
void selectOriginalBuntaCourse(OriginalBattleProfile&,std::uint32_t menuIndex);
struct OriginalBattleSelection {
    std::uint32_t gameMode{},playerCar{},opponentCar{},enemy{},course{},scene{},direction{},night{},weather{};
    std::uint32_t condition()const{return course*2+direction;}
};
OriginalBattleSelection originalBattleSelection(const OriginalBattleProfile&);
} // namespace idas3::original
