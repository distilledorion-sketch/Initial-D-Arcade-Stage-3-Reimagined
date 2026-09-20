#include "original_attract.h"
#include <array>
#include <bit>
#include <algorithm>
#include <stdexcept>

namespace idas3::original {
namespace {
constexpr std::array<std::uint32_t,18> withRanking{
    0,1,2,9,3,4,5,6,7,8,10,11,12,13,14,15,16,17};
constexpr std::array<std::uint32_t,16> withoutRanking{
    0,1,2,3,4,5,6,7,8,11,12,13,14,15,16,17};
}
std::span<const std::uint32_t> originalAttractChildren(std::uint32_t mode) {
    if(mode==1)return withRanking;
    return withoutRanking;
}
std::optional<std::uint32_t> originalAttractNextChild(std::uint32_t current,
    std::uint32_t mode) {
    const auto order=originalAttractChildren(mode);
    for(std::size_t i=0;i<order.size();++i)
        if(order[i]==current)return order[(i+1)%order.size()];
    return std::nullopt;
}
OriginalAttractCompletion originalAttractCompletion(std::uint32_t current,
    bool linkOverride,bool pendingReplay) {
    if(linkOverride)return {3};
    switch(current){
    case 5: return {pendingReplay?13:6,pendingReplay,pendingReplay,false};
    case 12:return {3,false,false,true};
    case 14:case 16:return {8};
    case 17:return {3};
    default:return {};
    }
}
bool originalAttractCanCheckStart(std::uint32_t current,std::uint32_t mode,
    std::uint8_t switches) {
    return std::bit_cast<std::int32_t>(current)>2 &&
        (mode!=1 || (switches&0x80)!=0);
}
void stepOriginalAttractTitle(OriginalAttractTitleState& s,bool ready,bool linked) {
    if(s.completed)return;
    if(s.child==8){
        s.fadeArgb=s.frame>870?(255u-unsigned(int(float(900-std::min(s.frame,900u))/30.f*255.f)))<<24:0;
        if(s.frame==900)s.completed=true;
        ++s.frame;return;
    }
    if(s.child!=6)throw std::invalid_argument("Invalid original title owner");
    s.fadeArgb=0;
    switch(s.phase){
    case 0:
        if(s.frame<=30)s.fadeArgb=(255u-unsigned(int(float(s.frame)/30.f*255.f)))<<24;
        if(s.frame==30){s.phase=1;s.frame=0;}break;
    case 1:if(ready){s.phase=2;s.frame=0;}break;
    case 2:if(linked){s.phase=4;s.frame=0;}break;
    case 4:
        s.fadeArgb=unsigned(s.frame<=30?int(float(s.frame)/30.f*255.f):255)<<24;
        if(s.frame==30)s.completed=true;break;
    default:throw std::logic_error("Invalid native title phase");
    }
    ++s.frame;
}
OriginalAttractExitEvents tickOriginalAttractExit(OriginalAttractExitState& s,
    bool acceptedStart) {
    OriginalAttractExitEvents e;
    if(s.pendingFrames348!=0){
        e.skipChildUpdate=true;
        --s.pendingFrames348;
        if(s.pendingFrames348==0){s.finishFlag28=true;e.finishRequested=true;}
    }else if(acceptedStart){
        s.pendingFrames348=3;
        e.clearBackgroundBlack=true;
    }
    ++s.frame84;
    return e;
}
std::optional<std::uint32_t> originalSingleEntryChild(std::uint8_t resume,
    std::uint32_t mode) {
    if(resume==1)return 6;
    if(resume<2 || resume>4)return 5;
    if(mode<=2)return mode+7;
    return std::nullopt;
}
}
