#include "original_start_grid.h"
#include "original_math.h"
#include <bit>
#include <cmath>
#include <stdexcept>

namespace idas3::original {
namespace {
using Words3=std::array<std::uint32_t,3>;
#include "original_start_grid_data.inc"
std::array<float,3> floats(const Words3& words){
    return {std::bit_cast<float>(words[0]),std::bit_cast<float>(words[1]),std::bit_cast<float>(words[2])};
}
}
OriginalStartGridPose originalStartPose(std::uint32_t condition,std::uint32_t slot){
    if(condition>=18||slot>1)throw std::invalid_argument("Original start requires condition0..17 and slot0/1");
    OriginalStartGridPose out;
    out.gridSlot=slot;
    out.position=floats(positions[condition*2+slot]);
    // Original caller requests direction slot0 even when the player is slot1.
    out.authoredDirection=floats(directions[condition*2]);
    out.horizontalDirection=out.authoredDirection;
    out.horizontalDirection[1]=0.0f;
    // 1F6CF0 FIPR ordered double accumulation, oneF32 rounding, FSRRA,
    // then independent F32 multiplies. Matches the primary Flycast finite
    // FPU contract used by the actual-opcode oracle; not silicon certification.
    double sum=double(out.horizontalDirection[0])*double(out.horizontalDirection[0]);
    sum+=double(out.horizontalDirection[1])*double(out.horizontalDirection[1]);
    sum+=double(out.horizontalDirection[2])*double(out.horizontalDirection[2]);
    sum+=0.0;
    const float inverseLength=1.0f/std::sqrt(float(sum));
    for(auto& value:out.horizontalDirection)value*=inverseLength;
    const auto angle=originalAtan2Angle(out.horizontalDirection[0],out.horizontalDirection[2])&0xFFFFu;
    const float scaled=float(angle)*std::bit_cast<float>(0x40C90FDBu);
    out.angles[1]=std::fma(scaled,std::bit_cast<float>(0x37800000u),std::bit_cast<float>(0x40490FDBu));
    return out;
}
OriginalStartGridPose originalAkinaStartPose(std::uint32_t condition,std::uint32_t slot){
    if(condition<6||condition>7)throw std::invalid_argument("Original Akina start requires condition6/7");
    return originalStartPose(condition,slot);
}
std::uint32_t originalSoloStartGridSlot(std::uint32_t mode){
    if(mode==1||mode>3)throw std::invalid_argument("Original solo grid requires numeric race mode0,2,or3");
    return mode==2?1u:0u;
}
}
