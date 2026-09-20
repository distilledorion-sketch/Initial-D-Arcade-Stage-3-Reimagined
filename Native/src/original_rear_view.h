#pragma once
#include "math_types.h"
#include "original_matrix.h"
#include <bit>
namespace idas3 {
struct OriginalRearViewFrame {
    Vec3 eye{},target{},up{0,1,0};
    original::OriginalMatrix cameraWorld;
    // 063FD2 passes tile rectangle 5,1,14,2 to 0242C0 (32-pixel tiles).
    static constexpr float left=160,top=32,width=320,height=64;
    // 064044 stores phase2367; 1D09E0 truncates its half-phase to1183.
    static constexpr float verticalFieldOfView=2366.f*std::bit_cast<float>(0x40c90fdbu)/65536.f;
};
inline OriginalRearViewFrame originalRearViewFrame(original::OriginalMatrix carWorld){
    // ABackView::view05B520 loads the player's ACar+964 matrix and applies
    // the local(0,.8,0) translation. Its negative aspect selects rear+Z.
    original::translateOriginalMatrix(carWorld,{0,std::bit_cast<float>(0x3f4ccccdu),0});
    const auto& m=carWorld.elements;
    return {{m[12],m[13],m[14]},{m[12]+m[8],m[13]+m[9],m[14]+m[10]},
        {m[4],m[5],m[6]},carWorld};
}
}
