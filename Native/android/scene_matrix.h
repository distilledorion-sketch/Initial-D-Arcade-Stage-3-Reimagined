#pragma once

#include <array>
#include <cmath>
#include <stdexcept>

#include "../src/math_types.h"

// Small row-major, row-vector matrices for the portable Unity scene capture
// path. The managed shader consumes the four constants as float4x4 rows and
// evaluates mul(float4(world, 1), viewProjection), matching DirectXMath's
// XMMatrixLookAt*/XMMatrixPerspectiveFov* convention used by Windows.
namespace idas3::portable {
using Matrix = std::array<float,16>;

inline Vec3 unit(Vec3 value) {
    const float length=std::sqrt(value.x*value.x+value.y*value.y+value.z*value.z);
    if(!(length>1.0e-12f)||!std::isfinite(length)) throw std::invalid_argument("Portable camera basis is degenerate");
    return {value.x/length,value.y/length,value.z/length};
}
inline float dot3(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec3 cross3(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}

inline Matrix multiply(const Matrix& a,const Matrix& b){
    Matrix result{};
    for(unsigned row=0;row<4;++row)for(unsigned column=0;column<4;++column)
        for(unsigned k=0;k<4;++k)result[row*4+column]+=a[row*4+k]*b[k*4+column];
    return result;
}

inline Matrix lookAt(Vec3 eye,Vec3 target,Vec3 up,bool leftHanded){
    const Vec3 z=unit(leftHanded?Vec3{target.x-eye.x,target.y-eye.y,target.z-eye.z}:
                       Vec3{eye.x-target.x,eye.y-target.y,eye.z-target.z});
    const Vec3 x=unit(cross3(up,z));
    const Vec3 y=cross3(z,x);
    return Matrix{
        x.x,y.x,z.x,0,
        x.y,y.y,z.y,0,
        x.z,y.z,z.z,0,
        -dot3(x,eye),-dot3(y,eye),-dot3(z,eye),1};
}

inline Matrix perspective(float verticalFov,float aspect,float nearClip,float farClip,bool leftHanded){
    if(!(verticalFov>0&&verticalFov<3.1415927f)||!(aspect>0)||!(nearClip>0)||!(farClip>nearClip))
        throw std::invalid_argument("Portable camera projection is invalid");
    const float y=1.0f/std::tan(verticalFov*.5f),x=y/aspect;
    if(leftHanded){
        return Matrix{x,0,0,0, 0,y,0,0, 0,0,farClip/(farClip-nearClip),1,
            0,0,-nearClip*farClip/(farClip-nearClip),0};
    }
    return Matrix{x,0,0,0, 0,y,0,0, 0,0,farClip/(nearClip-farClip),-1,
        0,0,nearClip*farClip/(nearClip-farClip),0};
}
}
