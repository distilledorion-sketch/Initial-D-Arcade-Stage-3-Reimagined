#pragma once
#include "original_showroom_lighting.h"
namespace idas3::original {
//1BD12A->1D0940 and Main's common1BDA36->1D0880. Source camera is
// identity, looking down negative Z; only clip-depth mapping changes for D3D.
struct OriginalRankingSceneSetup {
    static constexpr Vec3 eye{0,0,0},target{0,0,-1};
    static constexpr float verticalFieldOfView=pi/4,aspect=4.f/3.f;
    static constexpr float nearClip=.01f,farClip=10000.f;
    static constexpr std::array<std::uint32_t,16> projectionWords{
        0x3fe7c3b5,0,0,0,0,0x401a8279,0,0,0,0,0xbf800010,0xbf800000,0,0,0xbca3d715,0};
    //02ED3A/02EE20->053FC0,02EE56->0535A0,02F92E->053480.
    static constexpr std::array<std::uint32_t,8> glmWords{
        0x08000400,0x000f00b0,0x00010001,0xff333333,
        0xff000000,0x00010001,0xff333333,0xff000000};
    static OriginalShowroomLighting lighting(){
        OriginalShowroomLighting out;
        out.parameters={{0,std::bit_cast<float>(0x3f3504f3u),std::bit_cast<float>(0x3f3504f3u)},
            {.7f,.7f,.7f},51.f/255.f,2.f};
        return out;
    }
};
}
