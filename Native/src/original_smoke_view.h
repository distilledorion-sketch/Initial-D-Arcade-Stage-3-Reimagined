#pragma once
#include "original_matrix.h"
#include "original_rival_dialog.h"
#include <cmath>

namespace idas3::original {
struct OriginalSmokeView {
    OriginalMatrix worldToView;
    std::array<std::array<float,3>,4> planes{};
    float halfAngleTangent=0,viewportHeight=480;
    //13FC54..13FC66 multiplies these fields. Do not substitute the ordinary
    // projection cotangent; this is the source smoke sampling scale.
    float focalScale()const{return halfAngleTangent*viewportHeight;}
};
inline OriginalSmokeView originalSmokeView(const OriginalMatrix& worldToView,
        std::uint16_t fieldOfViewPhase,float viewportHeight=480.f,float aspect=4.f/3.f){
    OriginalSmokeView out;out.worldToView=worldToView;out.viewportHeight=viewportHeight;
    out.halfAngleTangent=originalDialogTangent(fieldOfViewPhase>>1);
    const auto normalized=[](std::array<float,3> v){
        double sum=double(v[0])*double(v[0]);sum+=double(v[1])*double(v[1]);sum+=double(v[2])*double(v[2]);sum+=0.0;
        const float inverse=1.f/std::sqrt(float(sum));for(auto& x:v)x*=inverse;return v;
    };
    //1D09E0's source aspect is4/3. A different aspect explicitly adapts the
    // host's wider view without changing simulation or the original4/3 path.
    out.planes[0]=normalized({0,-1,-out.halfAngleTangent});
    out.planes[1]=out.planes[0];out.planes[1][1]=-out.planes[1][1];
    out.planes[2]=normalized({1,0,-out.halfAngleTangent*aspect});
    out.planes[3]=out.planes[2];out.planes[3][0]=-out.planes[3][0];
    return out;
}

//1D0B60 returns -1 outside a side plane, otherwise -cameraZ. This helper
// uses ordered scalar FMAC, unlike the display's independent FTRV transform.
inline float originalSmokeFrustumDepth(const OriginalSmokeView& view,
        const std::array<float,3>& worldPosition,float radius){
    std::array<float,3> camera;
    const auto& m=view.worldToView.elements;
    for(unsigned row=0;row<3;++row){
        float value=m[row+4]*worldPosition[1];
        value=std::fma(worldPosition[0],m[row],value);
        value=std::fma(worldPosition[2],m[row+8],value);
        camera[row]=value+m[row+12];
    }
    for(const auto& plane:view.planes){
        float distance=plane[1]*camera[1];
        distance=std::fma(plane[0],camera[0],distance);
        distance=std::fma(camera[2],plane[2],distance);
        distance+=radius;
        if(distance<0)return -1.f;
    }
    return -camera[2];
}
}
