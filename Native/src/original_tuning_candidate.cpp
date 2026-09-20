#include "original_tuning_candidate.h"
#include <array>
#include <stdexcept>

namespace idas3::original {
namespace {
constexpr std::array<unsigned,11> resetMethods{
    0x0c0283c0,0x0c028480,0x0c028400,0x0c0284c0,0x0c028500,0x0c028540,
    0x0c028580,0x0c0285c0,0x0c0286c0,0x0c028720,0x0c028760};
}
std::vector<OriginalTuningMutation::CarCall> originalTuningStockAppearanceCalls(bool rebuild){
    std::vector<OriginalTuningMutation::CarCall> out;out.reserve(12);
    for(auto method:resetMethods)out.push_back({method,0,0});
    if(rebuild)out.push_back({0x0c029040,0,0});
    return out;
}
OriginalTuningCandidateAppearance originalTuningCandidateAppearance(
        const OriginalTuningData& data,unsigned car,unsigned package,unsigned stepCount){
    const auto& packages=data.car(car).packages;const auto& selected=packages.at(package);
    if(stepCount==std::numeric_limits<unsigned>::max())stepCount=unsigned(selected.steps.size());
    if(stepCount>selected.steps.size())throw std::out_of_range("Original candidate step count");
    OriginalTuningCandidateAppearance out;out.carCalls=originalTuningStockAppearanceCalls(false);
    unsigned upgrades=0;
    const auto call=[&](unsigned address,unsigned value){out.carCalls.push_back({address,value,0});};
    for(unsigned i=0;i<stepCount;++i){
        const auto& record=selected.steps[i].words;const auto part=record[0];
        unsigned variant=package==packages.size()-1?1:package+1;
        if(part<8){
            if(part==1&&car==1&&package==0)variant=2;
            if(part==2&&car==15&&package==1)variant=0;
            if(part==6&&car==1&&package==2)variant=2;
            if(part==7&&((car==3&&package==3)||(car==4&&package==2)||(car==16&&package==1)))variant=1;
            call(resetMethods[part],variant);
        }else if(part==9)call(0x0c0286c0,++upgrades);
        else if(part==8){
            const bool special=car==4||car==18||car==26||car==30||
                (car==22&&package==1)||(car==25&&package!=0);
            if(special&&record[1]==4)call(0x0c028720,1);
            if(car==1&&package==0)call(0x0c028720,1);
        }
    }
    call(0x0c0286a0,0);call(0x0c029040,0);
    return out;
}
void applyOriginalTuningCandidateAppearance(OriginalCarAppearanceConfig& config,
                                          const OriginalTuningCandidateAppearance& candidate){
    for(const auto& c:candidate.carCalls)if(c.address!=0x0c029040)
        applyOriginalCarAppearanceCall(config,c.address,c.argument5,c.argument6);
}
}
