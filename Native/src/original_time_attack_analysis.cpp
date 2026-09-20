#include "original_time_attack_analysis.h"
#include <bit>
#include <stdexcept>

namespace idas3::original {
namespace {
#include "original_time_attack_analysis_tables.inc"
std::int32_t s32(std::uint32_t v){return std::bit_cast<std::int32_t>(v);}
float f32(std::uint32_t v){return std::bit_cast<float>(v);}
void valid(unsigned course){if(course>=9)throw std::out_of_range("Original Time Attack analysis course");}
unsigned random(std::uint32_t& seed){seed=seed*0x41c64e6du+12345u;return (seed>>16)&0x7fffu;}
}
unsigned originalTimeAttackAnalysisKind(unsigned course,std::uint32_t status,std::uint32_t events){
    valid(course);return s32(status)>1?0u:(s32(events)>s32(thresholds[course][6])?1u:2u);
}
unsigned originalTimeAttackWorstSection(unsigned course,const std::array<std::uint32_t,4>& current,const std::array<std::uint32_t,4>& previous){
    valid(course);std::int32_t maximum=0;unsigned selected=0;
    for(unsigned i=0;i<sectionCounts[course];++i){const auto difference=s32(current[i]-previous[i]);
        if(difference>maximum){maximum=difference;selected=i;}}
    return selected;
}
std::uint32_t originalTimeAttackAdvicePredicateAddress(unsigned kind,unsigned index){
    if(kind==0&&index<predicates0.size())return predicates0[index];
    if(kind==1&&index<predicates1.size())return predicates1[index];
    if(kind==2&&index<predicates2.size())return predicates2[index];
    throw std::out_of_range("Original Time Attack advice predicate");
}
bool originalTimeAttackAdvicePredicate(const OriginalTimeAttackAnalysisInput& in,unsigned kind,unsigned index){
    valid(in.course);if(in.car>=sixSpeed.size())throw std::out_of_range("Original Time Attack analysis car");
    originalTimeAttackAdvicePredicateAddress(kind,index);const auto& limits=thresholds[in.course];
    if(kind==0){switch(index){
    case 0:return in.course!=4&&in.course!=5&&in.course!=6&&sixSpeed[in.car]==1&&in.manual&&in.maxGearUsed==0;
    case 1:return false; //18FDE0 is an authored dormant advice condition.
    case 2:return f32(limits[3])>in.maxSteeringDelta;
    case 3:return (100.f-(in.acceleratorFraction+in.brakeFraction)*100.f)>f32(limits[2]);
    case 4:return s32(in.wallCount)>s32(limits[4]);
    case 5:return f32(limits[0])>in.brakeFraction*100.f;
    case 6:return in.brakeFraction*100.f>f32(limits[1]);
    case 7:return true;
    }}
    if(kind==1){switch(index){
    case 0:return s32(in.wallCount)>=s32(limits[5]);
    case 1:return false; //18FFC0 is also dormant; fallback may choose its text.
    case 2:return in.previousBestTicks6000!=0;
    }}
    if(kind==2){switch(index){
    case 0:return in.previousBestTicks6000==0||s32(in.previousBestTicks6000)>s32(in.finishTicks6000);
    case 1:return .5f>float(s32(in.finishTicks6000-in.previousBestTicks6000))/6000.f;
    case 2:return in.previousBestTicks6000!=0;
    }}
    return false;
}
const OriginalTimeAttackAdviceText& originalTimeAttackAdviceText(unsigned kind,unsigned index){
    if(kind==0&&index<advice0.size())return advice0[index];
    if(kind==1&&index<advice1.size())return advice1[index];
    if(kind==2&&index<advice2.size())return advice2[index];
    throw std::out_of_range("Original Time Attack advice text");
}
OriginalTimeAttackAnalysis analyzeOriginalTimeAttack(const OriginalTimeAttackAnalysisInput& in,std::uint32_t& seed){
    OriginalTimeAttackAnalysis out;out.kind=originalTimeAttackAnalysisKind(in.course,in.resultStatus,in.convertedEventCount);
    out.countdownTicks=out.kind==2?1679:1279;out.worstSection=originalTimeAttackWorstSection(in.course,in.currentSections6000,in.previousSections6000);
    const unsigned count=out.kind==0?8:3;unsigned predicate=0;
    for(;predicate<count&&!originalTimeAttackAdvicePredicate(in,out.kind,predicate);++predicate){}
    out.adviceIndex=predicate;
    if(out.kind==1&&predicate==3)out.adviceIndex=s32(in.ditchCount)>0?1:0;
    if(out.kind==2){
        if(predicate<2){out.usedRandom=true;const auto draw=random(seed);out.adviceIndex=predicate==0?draw%2:draw%3+2;}
        else out.adviceIndex=5;
    }
    const auto& text=originalTimeAttackAdviceText(out.kind,out.adviceIndex);out.rowAddress=text.rowAddress;
    for(unsigned i=0;i<3;++i){out.lines[i]=text.text[i+1];out.lineAddresses[i]=text.addresses[i+1];}
    if((out.kind==1&&out.adviceIndex==2)||(out.kind==2&&out.adviceIndex==5)){
        if(in.course<2){out.lines[0]=circuitText[out.worstSection];out.lineAddresses[0]=circuitAddresses[out.worstSection];}
        else {out.lines[0]=mountainText[out.worstSection];out.lineAddresses[0]=mountainAddresses[out.worstSection];}
    }
    return out;
}
}
