#include "original_record_rules.h"
#include <stdexcept>
namespace idas3::original {
namespace {
void validate(const OriginalRecordPartition& p){if(p.scene>8||p.route>1||p.weather>1)throw std::out_of_range("Original record partition");}
}
std::uint32_t originalDefaultTimeAttackRecord6000(unsigned condition){
    constexpr std::array<std::uint32_t,9> seconds{210,240,240,240,210,210,240,240,240};
    if(condition>=18)throw std::out_of_range("Original default record condition");return seconds[condition/2]*6000;
}
OriginalRecordPartition originalRecordPartition(unsigned condition,bool wet){
    if(condition>=18)throw std::out_of_range("Original record condition");return {condition/2,condition&1,unsigned(wet)};
}
std::uint32_t OriginalRecordPartition::courseOffset(unsigned rank)const{validate(*this);if(rank>=10)throw std::out_of_range("Original ranking row");return 8+scene*5920+route*2960+weather*1480+740+rank*16;}
std::uint32_t OriginalRecordPartition::modelOffset(unsigned car)const{validate(*this);if(car>=35)throw std::out_of_range("Original ranking car");return 8+scene*5920+route*2960+weather*1480+900+car*16;}
std::uint32_t OriginalRecordPartition::personalIndex()const{validate(*this);return scene*2+route+weather*18;}
unsigned originalTimeAttackRank(const std::array<std::uint32_t,10>& times,std::uint32_t candidate){for(unsigned i=0;i<10;i++)if(times[i]>candidate)return i;return 10;}
bool originalModelRecordImproved(std::uint32_t existing,std::uint32_t candidate){return existing>candidate;}
bool originalPersonalRecordImproved(std::uint32_t existing,std::uint32_t candidate){return existing==0||existing>candidate;}
OriginalPersonalTimeAttackRecord originalPersonalTimeAttackRecord(const OriginalBattleProfile& profile,OriginalRecordPartition partition){
    const auto index=partition.personalIndex();
    OriginalPersonalTimeAttackRecord out{profile.u(176+4*index),profile.u(320+4*index)};
    for(unsigned i=0;i<3;++i)out.intermediate6000[i]=profile.u(500+4*(index*3+i));
    return out;
}
bool registerOriginalPersonalTimeAttackRecord(OriginalBattleProfile& profile,OriginalRecordPartition partition,
    std::uint32_t ticks6000,std::uint32_t night,std::span<const std::uint32_t> intermediate6000){
    const auto index=partition.personalIndex();
    if(intermediate6000.size()>3)throw std::invalid_argument("Original personal record has three intermediate slots");
    if(!(profile.u(1180)&3u)||!originalPersonalRecordImproved(profile.u(176+4*index),ticks6000))return false;
    profile.setu(320+4*index,night);profile.setu(176+4*index,ticks6000);
    for(unsigned i=0;i<intermediate6000.size();++i)profile.setu(500+4*(index*3+i),intermediate6000[i]);
    return true;
}
}
