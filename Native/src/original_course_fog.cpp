#include "original_course_fog.h"
#include "original_course_fog_data.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace idas3::original {
std::uint16_t originalFogPackedDensity(float density){
    if(!std::isfinite(density)||density<=0)throw std::invalid_argument("Invalid original fog density");
    const auto bits=std::bit_cast<std::uint32_t>(density);
    int exponent=int(bits>>23)-127;
    unsigned mantissa=((bits>>16)|128u)&255u;
    if(exponent<0){mantissa=(-exponent>=32)?0u:mantissa>>(-exponent);exponent=0;}
    return std::uint16_t((mantissa<<8)|(unsigned(exponent)&255));
}
OriginalCourseFog originalCourseFog(unsigned course,bool night,bool wet){
    if(course>=9)throw std::out_of_range("Original fog course must be0..8");
    OriginalCourseFog fog;fog.sourceRow=course*4+unsigned(night)*2+unsigned(wet);
    fog.sourceAddress=0x0c33c864u+fog.sourceRow*368;
    const auto& row=originalCourseFogWords[fog.sourceRow];
    fog.density=std::bit_cast<float>(row[0]);fog.colorRgb=row[1];fog.maximum=std::bit_cast<float>(row[2]);
    fog.packedDensity=originalFogPackedDensity(fog.density);
    for(unsigned i=0;i<128;++i){
        // Keep the source divide and subtraction separate (strict float build).
        const float position=float(i)/127.0f;
        fog.samples[i]=std::min(1.0f-position,fog.maximum);
    }
    auto quantize=[](float value){return unsigned(std::int32_t(value*255.0f))&255u;};
    for(unsigned i=0;i<128;++i)fog.table[i]=std::uint16_t((quantize(fog.samples[i])<<8)|quantize(fog.samples[std::min(i+1,127u)]));
    return fog;
}
void applyHappoFogRegisters(OriginalCourseFog&fog,bool night,bool wet){
    fog.sourceRow=16+unsigned(night)*2+unsigned(wet);fog.sourceAddress=0x0c040d60;
    fog.density=night?(wet?1500.f:22500.f):310.f;
    fog.packedDensity=originalFogPackedDensity(fog.density);fog.colorRgb=night?0u:0x808080u;
}
OriginalCourseFog originalBootstrapFog(){
    OriginalCourseFog fog;fog.sourceRow=~0u;fog.sourceAddress=0x0c1ceb40;
    fog.density=100000.f;fog.maximum=1.f;fog.colorRgb=0xffffff;
    fog.packedDensity=originalFogPackedDensity(fog.density);
    for(unsigned i=0;i<128;++i)fog.samples[i]=1.f-float(i)/127.f;
    for(unsigned i=0;i<128;++i){
        const unsigned current=unsigned(fog.samples[i]*255.f);
        const unsigned next=i==127?0u:unsigned(fog.samples[i+1]*255.f);
        fog.table[i]=std::uint16_t((current<<8)|next);
    }
    return fog;
}
std::array<OriginalFogRegisterWrite,130> originalCourseFogWrites(const OriginalCourseFog& fog){
    std::array<OriginalFogRegisterWrite,130> writes{};writes[0]={0xb8,fog.packedDensity};
    for(unsigned i=0;i<128;++i){const unsigned index=127-i;writes[i+1]={0x200+4*index,fog.table[index]};}
    writes.back()={0xb0,fog.colorRgb};return writes;
}
float originalFogDecodedDensity(std::uint16_t packedDensity){
    const int low=packedDensity&255;
    const int exponent=low<128?low:low-256;
    return std::ldexp(float(packedDensity>>8)/128.f,exponent);
}
float originalFogTableCoefficient(const OriginalCourseFog& fog,float reciprocalDepth){
    if(std::isnan(reciprocalDepth)||reciprocalDepth<0)
        throw std::invalid_argument("Invalid original fog reciprocal depth");
    const float density=originalFogDecodedDensity(fog.packedDensity);
    const float z=std::clamp(density==0?0.f:density*reciprocalDepth,1.f,255.9999f);
    // The lookup uses the IEEE exponent and the first four mantissa bits.
    // Remaining mantissa bits interpolate the two bytes of that same entry.
    // This avoids log2 rounding a value just below a power of two upward.
    const auto bits=std::bit_cast<std::uint32_t>(z);
    const unsigned exponent=(bits>>23)-127;
    const unsigned mantissa=bits&0x7fffff;
    const unsigned index=(exponent<<4)|(mantissa>>19);
    const float fraction=float(mantissa&0x7ffff)/524288.f;
    const auto entry=fog.table[index];
    const float current=float(entry>>8),next=float(entry&255);
    return (current+(next-current)*fraction)/255.f;
}
}
