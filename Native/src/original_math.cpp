#include "original_math.h"
#include <bit>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace idas3::original {
namespace {
constexpr float literal(std::uint32_t bits){return std::bit_cast<float>(bits);}
std::int32_t originalFtrc(float value){
    if(value<=-2147483648.f)return INT32_MIN;
    if(value>=2147483648.f)return INT32_MAX;
    return std::int32_t(value);
}
struct ReducedTangent {float value;std::int32_t quadrant;};
ReducedTangent tangent0C1FA120(float halfAngle){
    const float scaled=halfAngle/literal(0x3fc90fdb); // 0C1FA180
    const float biased=(halfAngle<0?-.5f:.5f)+scaled;
    const auto quadrant=originalFtrc(biased);
    const float q=float(quadrant);
    const float major=literal(0x3fc91000)*q;         // 0C1FA18C
    float reduced=halfAngle-major;
    reduced=std::fma(q,literal(0x3695777a),reduced); // actual FMAC at 0C1FA154
    const float squared=reduced*reduced;
    float fraction=0;
    for(int divisor=11;divisor>2;divisor-=2){
        const float denominator=float(divisor)-fraction;
        fraction=squared/denominator;
    }
    const float denominator=1.f-fraction;
    return {reduced/denominator,quadrant};
}
}
float originalSinF32(float angle){
    if(!std::isfinite(angle))throw std::invalid_argument("Original sine requires a finite angle");
    const auto tangent=tangent0C1FA120(angle*.5f); // original 0C1FA0E0
    const float twice=tangent.value+tangent.value;
    const float denominator=std::fma(tangent.value,tangent.value,1.f); // 0C1FA0FC
    const float result=twice/denominator;
    return (std::uint32_t(tangent.quadrant)&1)?-result:result;
}
float originalCosF32(float angle){
    if(!std::isfinite(angle))throw std::invalid_argument("Original cosine requires a finite angle");
    const float magnitude=std::bit_cast<float>(std::bit_cast<std::uint32_t>(angle)&0x7fffffffu);
    return originalSinF32(literal(0x3fc90fdb)-magnitude); // 0C1F9960
}
std::uint32_t originalAtanAngle(float value){
    if(!std::isfinite(value))throw std::invalid_argument("Original arctangent requires a finite input");
    float reduced=std::abs(value)>1.f?1.f/value:value;
    float fraction=0.f;
    // Preserve four separate multiplies/additions from 1F9722..1F9744.
    // Reassociating to i*i*x*x changes angle quantization near boundaries.
    for(int index=13;index>0;--index){
        const float i=float(index);
        float numerator=i*reduced;
        numerator*=i;
        numerator*=reduced;
        float denominator=fraction+1.f;
        denominator+=float(index+index);
        fraction=numerator/denominator;
    }
    const float denominator=fraction+1.f;
    reduced/=denominator;
    float quantized=reduced+literal(0x40c90fdb);
    quantized*=literal(0x47800000);
    quantized/=literal(0x40c90fdb);
    quantized+=.5f;
    const auto angle=std::uint32_t(originalFtrc(quantized))&0xffffu;
    if(value>1.f)return (0x4000u-angle)&0xffffu;
    if(-1.f>value)return (0xc000u-angle)&0xffffu;
    return angle;
}
std::uint32_t originalAsinAngle(float value){
    if(!std::isfinite(value))throw std::invalid_argument("Original arcsine requires a finite input");
    if(std::abs(value)>1.f)return value>0.f?0x4000u:0xffffc000u;
    const float squared=value*value;
    const float complement=1.f-squared;
    const float root=std::sqrt(complement);
    if(root==0.f)return value>0.f?0x4000u:0xc000u;
    return originalAtanAngle(value/root);
}
std::uint32_t originalAtan2Angle(float first,float second){
    if(!std::isfinite(first)||!std::isfinite(second))throw std::invalid_argument("Original two-input arctangent requires finite inputs");
    if(first>second){
        if(-first>second){
            if(second==0.f)return 0x8000u;
            return (originalAtanAngle(first/second)+0x8000u)&0xffffu;
        }
        if(first==0.f)return 0x4000u;
        return (0x4000u-originalAtanAngle(second/first))&0xffffu;
    }
    if(-first>second){
        if(first==0.f)return 0xc000u;
        return (0xc000u-originalAtanAngle(second/first))&0xffffu;
    }
    if(second==0.f)return 0;
    return originalAtanAngle(first/second);
}
}
