#pragma once
#include "native_multiplayer.h"
#include <array>
#include <limits>

namespace idas3 {
inline bool validBattleRecord(const Idas3BattleRecord& value){
    return value.wins<=value.battles&&value.streak<=value.wins&&value.streak<=99&&value.level>=1&&value.level<=99;
}
struct BattleRecordAdvance { Idas3BattleRecord record;std::uint32_t experience; };
// Original 0C16E020: profile+472=current level,+480=experience,+488=streak.
// Win/loss history is retained as full counters instead of the card's999 cap.
// No original single-player profile or points balance is mutated here.
inline BattleRecordAdvance advanceBattleRecord(const Idas3BattleRecord& before,bool won,
    const Idas3BattleRecord& opponent,std::uint32_t experience){
    if(!validBattleRecord(before)||!validBattleRecord(opponent)||experience>99)
        throw std::invalid_argument("Invalid battle progression input");
    static constexpr std::array<std::array<int,7>,4> win{{
        {{20,20,20,20,20,20,40}},{{5,10,10,20,20,20,20}},
        {{5,10,10,20,20,20,20}},{{1,5,5,10,10,10,10}}}};
    static constexpr std::array<std::array<int,7>,4> loss{{
        {{2,2,2,2,2,2,2}},{{-7,-7,-7,-5,-5,-5,-2}},
        {{-10,-10,-10,-7,-7,-7,-2}},{{-10,-10,-10,-10,-5,-5,-2}}}};
    const unsigned tier=before.level<=10?0:before.level<=20?1:before.level<=30?2:3;
    const unsigned difference=unsigned(std::clamp(int(opponent.level)-int(before.level),-3,3)+3);
    int level=int(before.level),points=int(experience)+(won?win:loss)[tier][difference];
    if(points>99){
        ++level;
        if(level>99){level=99;points=99;}else points-=100;
    }else if(points<0){--level;points+=100;}
    auto next=before;
    if(next.battles<std::numeric_limits<std::uint32_t>::max())++next.battles;
    if(won&&next.wins<std::numeric_limits<std::uint32_t>::max())++next.wins;
    next.streak=won?std::min(before.streak+1,99u):0;
    next.level=unsigned(std::clamp(level,1,99));
    return {next,unsigned(std::clamp(points,0,99))};
}
}
