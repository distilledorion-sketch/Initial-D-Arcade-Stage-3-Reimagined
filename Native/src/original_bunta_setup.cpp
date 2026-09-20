#include "original_bunta_setup.h"
#include "original_race_rules.h"
#include <bit>
#include <stdexcept>

namespace idas3::original {
OriginalBuntaEligibility originalBuntaEligibility(const OriginalBattleProfile& p){
    if(std::bit_cast<std::int32_t>(p.u(72))<=3999)return OriginalBuntaEligibility::InsufficientPoints;
    if((p.u(1180)&3u)==0)return OriginalBuntaEligibility::CardRequired;
    return OriginalBuntaEligibility::Eligible;
}
void applyOriginalAcceptedCardFlag(OriginalBattleProfile& p){p.setu(1180,p.u(1180)|1u);}
std::uint32_t originalModeAfterBuntaEligibility(std::uint32_t mode,bool timedOut,
    std::int32_t points,std::uint32_t flags){
    return timedOut&&mode==2&&(points<=3999||(flags&3u)==0)?1u:mode;
}
OriginalBuntaCommonRaceFields originalLegacyBuntaCommonRaceSetup(
    const OriginalBattleProfile& p,std::uint32_t difficulty){
    if(p.u(0)!=2)throw std::invalid_argument("Original Bunta profile mode must be2");
    if(p.u(4)>9||p.u(28)>17||p.u(12)>1||p.u(8)>1||p.u(32)>1)
        throw std::invalid_argument("Original Bunta common course selection is outside supported authored data");
    OriginalBuntaCommonRaceFields out;
    out.numericRaceMode1640=p.u(4)==9?3u:2u; //18724C /1872D2
    out.course1652=out.numericRaceMode1640==3?p.u(28)/2:p.u(4); //062500..251A
    out.scene1656=p.u(28);out.direction1660=p.u(12);out.night1664=p.u(8);out.weather1668=p.u(32);
    out.condition1568=out.course1652*2+out.direction1660;
    out.ruleRow1564=out.course1652*3+(out.numericRaceMode1640==3?2:out.direction1660);
    out.playerGridSlot=out.numericRaceMode1640==2?1:0;
    out.rivalGridSlot=1-out.playerGridSlot;
    //06746E..747C overrides the timer selector with profile mode2, including
    // the special common race mode3. Actual191E80 and192040 provide its data.
    const auto seconds=originalTimeAttackInitialSeconds(out.condition1568,difficulty&15u);
    if(seconds>0)out.initialTime6000=std::uint32_t(seconds)*6000u+(out.weather1668==1?42000u:0u);
    out.extensionSeconds=originalTimeAttackBonusSeconds(out.condition1568);
    return out;
}
OriginalBuntaRaceSetup makeOriginalBuntaRaceSetup(const OriginalBattleProfile& p,std::uint32_t difficulty){
    if(p.u(4)>8||p.u(20)>=35||p.u(24)>=31)
        throw std::invalid_argument("Live Bunta requires an authored selected rival/course");
    OriginalBuntaRaceSetup out;
    static_cast<OriginalBuntaCommonRaceFields&>(out)=originalLegacyBuntaCommonRaceSetup(p,difficulty);
    out.numericRaceMode1640=0;out.playerGridSlot=0;out.rivalGridSlot=1;
    out.ordinaryRivalControl=std::bit_cast<std::int32_t>(p.u(20));
    out.enemyId0C9015E0=p.u(24);out.level0C9015D0=p.u(148);out.geometryCar0C9015F8=p.u(20);
    out.opponentProgress0C901644=p.byte(116+p.u(24));
    for(unsigned i=0;i<8;++i)out.progress0C901604[i]=p.u(1080+i*4);
    return out;
}
}
