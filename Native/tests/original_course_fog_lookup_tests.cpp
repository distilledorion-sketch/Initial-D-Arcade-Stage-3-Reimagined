#include "original_course_fog.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace idas3::original;
namespace {
// Independent double-precision evaluation of the primary renderer's
// 128x2 bilinear texture equation. Row0 contains low bytes; row1 high bytes.
// Using double here prevents float log2 from rounding below-boundary input
// up to the next exponent; the native implementation uses raw IEEE fields.
double textureReference(const OriginalCourseFog& fog,float reciprocal){
    const int low=fog.packedDensity&255;
    const float density=static_cast<float>((fog.packedDensity>>8)/128.0*std::pow(2.0,low<128?low:low-256));
    const float scaled=density==0?0.f:density*reciprocal;
    const double z=std::clamp(scaled,1.f,255.9999f);
    const double e=std::floor(std::log2(z));
    const double m=z*16.0/std::pow(2.0,e)-16.0;
    const double tx=(std::floor(m)+e*16.0+.5)/128.0;
    const double ty=.75-(m-std::floor(m))/2.0;
    const int ix=static_cast<int>(std::floor(tx*128));
    const double y=ty*2-.5;
    const auto entry=fog.table.at(ix);
    return ((entry&255)*(1-y)+(entry>>8)*y)/255.0;
}
}
int main()try{
    std::size_t checks=0;double maximumError=0;
    auto check=[&](bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);};
    auto compare=[&](const OriginalCourseFog& fog,float reciprocal){
        const auto actual=originalFogTableCoefficient(fog,reciprocal);
        const auto expected=textureReference(fog,reciprocal);
        maximumError=std::max(maximumError,std::abs(actual-expected));
        check(std::abs(actual-expected)<0.0000002,"PVR texture equation mismatch");
        check(actual>=0&&actual<=1,"fog outside normalized range");
    };
    for(unsigned packed=0;packed<65536;++packed){
        const int low=packed&255;
        const float expected=float((packed>>8)/128.0*std::pow(2.0,low<128?low:low-256));
        check(originalFogDecodedDensity(std::uint16_t(packed))==expected,"PVR density decode");
    }
    for(unsigned course=0;course<9;++course)for(unsigned night=0;night<2;++night)for(unsigned wet=0;wet<2;++wet){
        const auto fog=originalCourseFog(course,night,wet);
        compare(fog,0);compare(fog,std::numeric_limits<float>::infinity());
        check(originalFogTableCoefficient(fog,0)==float(fog.table[0]>>8)/255.f,"far endpoint");
        check(originalFogTableCoefficient(fog,std::numeric_limits<float>::infinity())==float(fog.table[127]&255)/255.f,"near endpoint");
        float previous=1;
        for(unsigned n=0;n<=20000;++n){
            const float reciprocal=float(n)/20000.f;
            compare(fog,reciprocal);
            const auto value=originalFogTableCoefficient(fog,reciprocal);
            check(value<=previous+0.0000001f,"source authored fog monotonicity");previous=value;
        }
    }
    // Deliberately discontinuous bytes distinguish current/next interpolation
    // from interpolating between the high bytes of neighboring entries.
    OriginalCourseFog synthetic;synthetic.packedDensity=0x8000;
    for(unsigned i=0;i<128;++i)synthetic.table[i]=std::uint16_t((((i*37+11)&255)<<8)|((i*71+191)&255));
    for(unsigned e=0;e<8;++e)for(unsigned m=0;m<16;++m){
        const unsigned index=e*16+m;
        for(unsigned frac=0;frac<17;++frac){
            const float z=std::ldexp(1.f+(float(m)+float(frac)/17.f)/16.f,e);
            compare(synthetic,z);
        }
        const float boundary=std::ldexp(1.f+float(m)/16.f,e);
        compare(synthetic,std::nextafter(boundary,0.f));
        compare(synthetic,boundary);
        compare(synthetic,std::nextafter(boundary,std::numeric_limits<float>::infinity()));
        const float expected=float(synthetic.table[index]>>8)/255.f;
        check(originalFogTableCoefficient(synthetic,boundary)==expected,"entry start high byte");
    }
    for(float bad:{-1.f,-std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
        bool rejected=false;try{originalFogTableCoefficient(synthetic,bad);}catch(const std::invalid_argument&){rejected=true;}
        check(rejected,"invalid reciprocal depth rejected");
    }
    synthetic.packedDensity=0;
    compare(synthetic,0);compare(synthetic,std::numeric_limits<float>::infinity());
    std::cout<<"PASS original fog lookup: "<<checks<<" checks; 65536 density words,36 authored tables,128 discontinuous entries; max texture-equation error="<<maximumError<<".\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
