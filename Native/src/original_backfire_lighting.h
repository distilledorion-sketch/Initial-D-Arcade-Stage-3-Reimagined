#pragma once
#include "original_course_lighting.h"

namespace idas3::original {
// 17CB00 publishes a point light separately from the flame mesh. The fixed
// light anchor (17CB9E..B2) is shared by all three tuned exhausts. The caller
// supplies the ACar visual WORLD matrix, without its body-only ride offset.
inline OriginalCourseLight originalBackfireLight(bool active,const OriginalLightMatrix& world){
    OriginalCourseLight light;
    light.kind=OriginalCourseLightKind::Point;light.enabled=active;
    light.incomingDirection={0,-1,0};
    light.color=active?OriginalLightVector{1.f,.8f,.4f}:OriginalLightVector{};
    light.distance0=.001f;light.distance1=7.45f;
    // Actual 054980 -> 2059A0 upper-floatword attenuation coefficients.
    light.coefficientWords={0x3f94be1fu,0x00003f80u};
    constexpr OriginalLightVector anchor{.788f,.227f,-3.156f};
    for(unsigned i=0;i<3;++i){
        double value=double(world[i])*anchor[0];
        value+=double(world[4+i])*anchor[1];
        value+=double(world[8+i])*anchor[2];
        value+=double(world[12+i]);light.position[i]=float(value);
    }
    return light;
}
}
