#include "original_time_attack_background.h"
#include <bit>
#include <cmath>
#include <stdexcept>

namespace idas3::original {
namespace {
constexpr float speed=std::bit_cast<float>(0x3c020821u); //112DEC
constexpr float spacing=std::bit_cast<float>(0x40228f5cu); //112F88
constexpr float originX=std::bit_cast<float>(0xc04cccccu);
constexpr float originY=std::bit_cast<float>(0x40199999u);
constexpr float originZ=std::bit_cast<float>(0xbe19999au);
}
std::uint32_t advanceOriginalTimeAttackBackground(std::uint32_t frame){
    if(frame>320)throw std::out_of_range("Original Time Attack background frame");
    ++frame;return float(frame)*speed>=spacing?0:frame;
}
std::array<OriginalTimeAttackBackgroundDraw,12> originalTimeAttackBackgroundDraws(std::uint32_t frame){
    if(frame>320)throw std::out_of_range("Original Time Attack background frame");
    std::array<OriginalTimeAttackBackgroundDraw,12> out{};unsigned i=0;
    const float phase=float(frame)*speed;
    //112EB6..112F32: columns-1..2, rows-1..1, one FMAC per coordinate.
    //145820 restores the source origin after drawing all twelve tiles.
    for(int row=-1;row<2;++row)for(int col=-1;col<3;++col){
        const auto x=std::fma(float(col),spacing,phase),y=std::fma(float(row),spacing,phase);
        out[i++].position={(originX+x)-originX,(originY-y)-originY,-100.f-originZ};
    }
    return out;
}
}
