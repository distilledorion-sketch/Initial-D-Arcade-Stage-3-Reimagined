#pragma once
#include <bit>
#include <cstdint>

namespace idas3::original {
// ARace +068C/+0690/+0694/+0698. These count accepted parent updates,
// including StandBy and Countdown, not rendered frames or only GO updates.
struct OriginalCabinetButtonCounters {
    std::uint32_t start{},view{},releasedStart{},releasedView{};
};

// 05FC7A..05FC94: one race initialization, not one menu render.
inline void resetOriginalCabinetButtons(OriginalCabinetButtonCounters& buttons,
    std::uint32_t& profileFlags1180) {
    buttons={};
    profileFlags1180&=0xfffdffffu;
}

// 05FDDC..05FE18: P1 accepted level byte 0C92ECE0, Start bit7/View bit4.
// Unsigned arithmetic deliberately retains the source's 32-bit wrap.
// Release history is overwritten on EVERY released update (including zero).
inline void advanceOriginalCabinetButtons(OriginalCabinetButtonCounters& buttons,
    std::uint8_t acceptedLevelByte) {
    if(acceptedLevelByte&0x80u)++buttons.start;
    else {buttons.releasedStart=buttons.start;buttons.start=0;}
    if(acceptedLevelByte&0x10u)++buttons.view;
    else {buttons.releasedView=buttons.view;buttons.view=0;}
}

// 067F2A..067FAC, reachable only if timer was positive at 067D16 ENTRY.
// The ordinary decrement/synchronization has ALREADY happened at this site;
// remainingAfterDecrement may be zero or negative and must not be re-gated.
// configWord12 is the actual word at getter04FCC0()+12; its bits14..15 are
// retained numerically because no user-facing operator-setting label is proved.
// This translates the original numeric1 gate but supplies no host online policy.
inline bool applyOriginalForcedQuitInPositiveBranch(
    OriginalCabinetButtonCounters& buttons,bool positiveAtTimerEntry,
    std::uint32_t numericRaceMode,std::uint32_t configWord12,std::uint8_t cardByte1189,
    std::uint32_t& remainingAfterDecrement,std::uint32_t& profileFlags1180) {
    if(!positiveAtTimerEntry||buttons.start-1u>2u||
       std::bit_cast<std::int32_t>(buttons.view)<=0||
       std::bit_cast<std::int32_t>(buttons.view)>3)return false;
    const auto config=(configWord12>>14)&3u;
    const bool allowsWithoutCard=config==1||config==2;
    if(numericRaceMode==1&&!allowsWithoutCard)return false;
    if(cardByte1189==0&&!allowsWithoutCard)return false;
    buttons.view=4;
    buttons.start=4;
    remainingAfterDecrement=0;
    profileFlags1180|=0x20000u;
    return true;
}

// Bounded nonpositive-entry branch 067D24..067DEC. Published actor selection
// (car+095C fallback / +0960 preferred) is the caller's explicit boundary.
// Neither remaining/elapsed timers nor goal/outcome/challenger are owned here.
// Return value denotes the original diagnostic-log transition, not race end.
inline bool applyOriginalExpiredRaceTimer(std::uint32_t remainingAtEntry,
    std::uint32_t numericRaceMode,std::uint32_t publishedPlayerFlags,
    std::uint32_t publishedRivalFlags,std::uint32_t& timeUpFlag,
    std::uint32_t& livePlayerFlags,std::uint8_t& forcedBrakeGlobal) {
    const auto remaining=std::bit_cast<std::int32_t>(remainingAtEntry);
    if(remaining>0)return false;
    bool stopped=(publishedPlayerFlags&0x4000u)!=0;
    if(numericRaceMode==1)stopped=stopped&&(publishedRivalFlags&0x4000u)!=0;
    const bool changed=(stopped||remaining<=-30000)&&timeUpFlag!=1;
    if(changed)timeUpFlag=1;
    livePlayerFlags|=0x2000u;
    forcedBrakeGlobal=1;
    return changed;
}
} // namespace idas3::original
