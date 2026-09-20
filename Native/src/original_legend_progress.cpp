#include "original_legend_progress.h"
#include <array>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace idas3::original {
namespace {
void checkEnemy(std::uint32_t enemy){if(enemy>30)throw std::out_of_range("Original Legend rival must be0..30");}
// Exact original image2A0F5C,31 rows: participation,first win,second win,later.
constexpr std::array<std::array<std::uint32_t,4>,31> points{{
    {1000,2000,4000,1000},{1000,3000,5000,1250},{1000,4000,6000,1500},
    {1000,2000,4000,1000},{1000,3000,5000,1250},{1000,5000,7000,1750},
    {1000,3000,5000,1250},{1000,4000,6000,1500},{1000,6000,8000,2000},
    {1000,3000,5000,1250},{1000,4000,6000,1500},{1000,5000,7000,1750},
    {1000,6000,8000,2000},{1000,8000,10000,2500},{1000,4000,6000,1500},
    {1000,6000,8000,2000},{1000,7000,9000,2250},{1000,5000,7000,1750},
    {1000,5000,7000,1750},{1000,6000,8000,2000},{1000,7000,9000,2250},
    {1000,5000,7000,1750},{1000,6000,8000,2000},{1000,7000,9000,2250},
    {1000,4000,6000,1500},{1000,6000,8000,2000},{1000,7000,9000,2250},
    {1000,7000,9000,2250},{1000,8000,10000,2500},{1000,9000,11000,2750},
    {1000,10000,12000,3000}
}};
//2A114C and2A1170. Original192340/192360 map out-of-range course toMyogi.
constexpr std::array<float,9> factors{8.f,8.f,9.f,10.f,10.f,10.f,10.f,10.f,10.f};
constexpr std::array<std::uint32_t,9> limits{1000,1000,1500,1500,1500,1500,1500,1500,1500};
std::uint32_t originalTruncate(float value){
    if(std::isnan(value)||value<=-2147483648.f)return 0x80000000u;
    if(value>=2147483648.f)return 0x7fffffffu;
    return std::uint32_t(std::int32_t(value));
}
}
void recordOriginalLegendWin(OriginalBattleProfile& p,std::uint32_t enemy){
    checkEnemy(enemy);const auto old=p.byte(116+enemy);const auto high=old>>4;
    if(high<=14)p.setByte(116+enemy,std::uint8_t((old&15)|((high+1)<<4)));
    unsigned cleared=0;for(unsigned i=0;i<28;++i)if(p.byte(116+i)>>4)++cleared;
    if(cleared==28){p.setu(1180,p.u(1180)|0x08000000u);p.setu(108,4);}
    if(enemy==28){p.setu(1180,p.u(1180)|0x4000u);p.setu(108,5);}
    else if(enemy==29){p.setu(1180,p.u(1180)|0x8000u);p.setu(92,5);}
}
void recordOriginalLegendLoss(OriginalBattleProfile& p,std::uint32_t enemy){
    checkEnemy(enemy);const auto old=p.byte(116+enemy);const auto low=old&15;
    if(low<=14)p.setByte(116+enemy,std::uint8_t((old&240)|(low+1)));
}
OriginalLegendResult recordOriginalLegendResult(OriginalBattleProfile& p,
        std::uint8_t finishedByte1572,std::uint32_t outcome1644){
    if(p.u(0)!=0)return OriginalLegendResult::NotLegend;
    const auto enemy=p.u(24);
    if(finishedByte1572&&outcome1644==0){recordOriginalLegendWin(p,enemy);return OriginalLegendResult::Win;}
    recordOriginalLegendLoss(p,enemy);return OriginalLegendResult::Loss;
}
OriginalLegendPoints calculateOriginalLegendPoints(const OriginalBattleProfile& p,
        std::uint32_t resultStatus80,float advantage84){
    const auto enemy=p.u(24);checkEnemy(enemy);
    OriginalLegendPoints out;out.participation=points[enemy][0];
    if(resultStatus80==0){
        const auto wins=p.byte(116+enemy)>>4;
        out.win=points[enemy][wins==1?1:wins==2?2:3];
        if(advantage84>0.f){
            const auto course=p.u(4)>8?0:p.u(4);
            const float scaled=advantage84*factors[course];
            out.advantage=originalTruncate(scaled);
            if(std::bit_cast<std::int32_t>(out.advantage)>std::int32_t(limits[course]))out.advantage=limits[course];
        }
    }
    out.total=out.participation+out.win+out.advantage;
    out.balanceBeforeCap=p.u(72)+out.total;
    return out;
}
OriginalLegendPoints awardOriginalLegendPoints(OriginalBattleProfile& p,
        std::uint32_t resultStatus80,float advantage84){
    const auto out=calculateOriginalLegendPoints(p,resultStatus80,advantage84);
    p.setu(72,out.balanceBeforeCap>999999998u?999999999u:out.balanceBeforeCap);
    return out;
}
void refreshOriginalLegendCourseProgress(OriginalBattleProfile& p){
    constexpr std::array<unsigned,9> count{4,4,4,4,4,4,0,0,0};
    for(unsigned course=0;course<9;++course){
        unsigned cleared=0;
        for(unsigned choice=0;choice<count[course];++choice){
            const auto enemy=originalLegendRivalId(course,choice);
            if((p.byte(116+enemy)>>4)==0)break;
            ++cleared;
        }
        p.setu(80+course*4,cleared);
    }
}
std::uint32_t calculateOriginalDriverRank(const OriginalBattleProfile& p){
    const auto signedValue=[](std::uint32_t x){return std::bit_cast<std::int32_t>(x);};
    constexpr std::array<unsigned,9> first{0,3,6,9,18,14,21,24,17};
    constexpr std::array<unsigned,9> count{3,3,3,5,3,3,3,6,1};
    unsigned clearedCourses=0;
    //18A980 starts atcourse1, deliberately skippingcourse0.
    for(unsigned course=1;course<9;++course){
        bool cleared=true;for(unsigned i=0;i<count[course];++i)if(!(p.byte(116+first[course]+i)>>4))cleared=false;
        clearedCourses+=cleared;
    }
    std::uint32_t legend=clearedCourses;
    if(p.u(148)){
        legend=p.u(148)+7;
        if(clearedCourses>7&&p.byte(116+30))legend=p.u(148)+8;
        if(signedValue(legend)>10)legend=10;
    }
    std::uint32_t bunta=15;
    for(unsigned i=0;i<8;++i)if(signedValue(p.u(1080+i*4))<signedValue(bunta))bunta=p.u(1080+i*4);
    if(signedValue(bunta)>10)bunta=10;
    const auto versus=p.u(476)/3>10?10:p.u(476)/3;
    const auto total=legend+bunta+versus;
    return signedValue(total)>29?30:total;
}
void updateOriginalPostRaceRank(OriginalBattleProfile& p){
    const auto rank=calculateOriginalDriverRank(p);
    if(std::bit_cast<std::int32_t>(p.u(496))<std::bit_cast<std::int32_t>(rank)){
        p.setu(1180,p.u(1180)|0x00200000u);p.setu(496,rank);
    }
    if(!(p.u(1180)&3)){
        p.setu(480,0);p.setu(1180,p.u(1180)&~0x003c0000u);
    }
}
}
