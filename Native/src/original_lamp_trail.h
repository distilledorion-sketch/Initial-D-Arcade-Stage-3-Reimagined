#pragma once
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>

namespace idas3::original {
using OriginalLampTrailPoint=std::array<float,3>;
inline constexpr unsigned lampTrailPcw=0x8a00072e,lampTrailIsp=0x83800000,lampTrailTsp=0x941024e2;

struct OriginalLampTrailSwitches { bool front=false,rear=false; };
// The replay/attract effect manager constructs1A8AE0, not the driving owner.
//1A9AAC gates rear trails;1A9C64 explicitly disables its front trail calls.
inline OriginalLampTrailSwitches originalReplayLampTrailSwitches(
        unsigned night32,unsigned attract272,bool blindAttack){
    return {false,night32==1&&attract272==1&&!blindAttack};
}
// 181F0C..181FE8. These are owner fields, not guessed host day/night rules.
// ACar+80 is the published brake flag; actor+80 bit16 is blind attack.
inline OriginalLampTrailSwitches originalLampTrailSwitches(
        unsigned night32,unsigned brakeEffects28,unsigned attract272,
        bool blindAttack,bool braking){
    OriginalLampTrailSwitches out;
    if(night32==1){out.front=!blindAttack;out.rear=!blindAttack&&(attract272==1||braking);}
    else out.rear=brakeEffects28==1&&braking;
    return out;
}

struct OriginalLampTrailCut {
    unsigned frames276=0,blocked284=0;
    // 182206..18222C. A cut suppresses nine updates, including this one.
    void advance(unsigned sceneChanged){
        if(sceneChanged==1){blocked284=1;frames276=0;}
        if(blocked284==1&&++frames276>9){blocked284=0;frames276=0;}
    }
};

struct OriginalLampTrailVertex {
    OriginalLampTrailPoint position{};
    float u=0,v=0;
    std::uint32_t argb=0,stripControl=0;
};
struct OriginalLampTrailProjection {
    // Center, center minus0.1 localY, center plus/minus source halfWidth localX.
    std::array<OriginalLampTrailPoint,4> points{};
    float height1000=0;
};
// 1819B8..181A72. The trail is recorded on the camera's near+0.5 plane,
// not as a world-space ribbon left behind the car.
inline OriginalLampTrailProjection projectOriginalLampTrail(
        std::array<OriginalLampTrailPoint,4> cameraPoints,float nearClip){
    const float depth=-(nearClip+.5f);
    for(auto& p:cameraPoints){const float scale=depth/p[2];p[0]*=scale;p[1]*=scale;p[2]=depth;}
    float height=cameraPoints[0][1]-cameraPoints[1][1];if(height<0)height=-height;
    return {cameraPoints,height*1000.f};
}

struct OriginalLampTrailHistory {
    std::array<OriginalLampTrailPoint,9> points{};
    // 1817AA..1817C4, then181A54..181A64. Update once per source frame.
    void push(OriginalLampTrailPoint point){
        for(unsigned i=8;i>0;--i)points[i]=points[i-1];points[0]=point;
    }
    // 181E20..181E3E. Disabled/occluded lamps collapse, rather than retaining
    // an old streak which would bridge camera cuts and blind-attack changes.
    void collapse(){for(unsigned i=1;i<9;++i)points[i]=points[0];}
    // 181CD6..181D06. The original draws a ribbon only after horizontal screen
    // travel spans at least the lamp's projected width. Its glow is separate.
    bool spansLamp(const OriginalLampTrailProjection& p)const{
        float motion=points[8][0]-points[0][0];if(motion<0)motion=-motion;
        float width=p.points[3][0]-p.points[2][0];if(width<0)width=-width;
        return !(width>motion);
    }
    // 181ADC..181C16, then181600. Preserve the source float operation order,
    // nine paired samples, UV progression and quantized vertex alpha.
    std::array<OriginalLampTrailVertex,18> strip(float height1000,unsigned opacity)const{
        std::array<OriginalLampTrailVertex,18> out{};
        for(unsigned i=0;i<9;++i){
            const auto& a=points[i<8?i:i-1];const auto& b=points[i<8?i+1:i];
            float x=a[1]-b[1],y=b[0]-a[0];
            const float squared=std::fma(x,x,y*y);
            if(squared<0){x=0;y=1;}
            else{
                // 1F6CF0 FIPR/FSRRA: double accumulation, single rounding.
                const float length2=float(double(x)*x+double(y)*y+double(0.f)*0.f+double(0.f)*0.f);
                const float inverse=1.f/std::sqrt(length2);x*=inverse;y*=inverse;
            }
            const float width=.001f*height1000;x*=width;y*=width;
            const float fade=std::bit_cast<float>(0x3d2f0af1u)*float(i);
            const float opacityFloat=float(opacity)*(1.f-(fade>0?fade:0.f));
            const auto alpha=static_cast<unsigned>(opacityFloat);
            for(unsigned side=0;side<2;++side){const unsigned v=i*2+side;auto& vertex=out[v];
                vertex.position={side?points[i][0]-x:points[i][0]+x,
                    side?points[i][1]-y:points[i][1]+y,points[i][2]};
                vertex.u=.125f*float(i);vertex.v=side?0.f:1.f;
                vertex.argb=(alpha<<24)|0xffffffu;
                vertex.stripControl=v<2?0:v==2?0x60000000u:v==17?0xa0000000u:0x20000000u;
            }
        }
        return out;
    }
};
}
