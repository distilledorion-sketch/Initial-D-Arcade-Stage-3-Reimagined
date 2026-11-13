#pragma once
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace idas3::original {
// Scalar SmokeMgr particle primitive,13D780/13D800. Rendering, allocation,
// emitter scheduling and interpretation of the owner's flag bits are separate.
struct OriginalSmokeParticle {
    std::array<float,3> position{},velocity{}; // source particle+24/+36
    float size=0; // +48
    std::uint32_t remaining=0,flags=0; // +52/+56
};
struct OriginalSmokeParameters {
    std::array<float,9> values{}; // globalA9BADC copied by13EA58/13EAAA
    std::uint32_t alternateMode=0; // globalA9BFB0, tested for exactly1
};
inline OriginalSmokeParameters originalSmokeParameters(unsigned preset,bool alternateMode=false){
    // Exact three authored records at31DD30,31DD54,31DD78. The alternate
    // source option forces record2; ordinary mode selects record0 or record1.
    constexpr std::array<std::array<std::uint32_t,9>,3> bits{{
        {0x42a3ebebu,0x3db0b0f2u,0x3c60e0ebu,0x3f19999au,0x3fd55555u,0xbfcccccdu,0x42480000u,0x3bda3c21u,0x3cc08312u},
        {0x42a3ebebu,0x3dcccccdu,0x3bded289u,0x3f46a7f0u,0x3fa4f2ceu,0xbf208312u,0x42c80000u,0x3be56042u,0x3cc08312u},
        {0x42a1cccdu,0x3e23d70au,0x3cf5c28fu,0x40e00000u,0x3e124925u,0x40200000u,0x00000000u,0x3d79db23u,0x00000000u}
    }};
    if(preset>1&&!alternateMode)throw std::out_of_range("Original smoke preset");
    OriginalSmokeParameters out;out.alternateMode=alternateMode?1u:0u;
    for(unsigned i=0;i<9;++i)out.values[i]=std::bit_cast<float>(bits[alternateMode?2:preset][i]);
    return out;
}
inline void initializeOriginalSmokeParticle(OriginalSmokeParticle& p,
        std::array<float,3> position,std::array<float,3> velocity,float size,
        std::uint32_t remaining,std::uint32_t kind,std::uint32_t phase){
    p.position=position;p.velocity=velocity;p.size=size;p.remaining=remaining;
    p.flags=((p.flags&0xfffffff0u)|(kind&15u));
    p.flags=((p.flags&0xffffff0fu)|((phase&15u)<<4))&0xfffffeffu;
}
template<class RandomFloat> inline void advanceOriginalSmokeParticle(OriginalSmokeParticle& p,
        const OriginalSmokeParameters& parameters,float emitterScale,float emitterForce,RandomFloat&& random){
    if(((p.flags>>4)&15u)!=1)return;
    --p.remaining;
    if(std::bit_cast<std::int32_t>(p.remaining)<=0){p.flags&=0xffffff0fu;return;}
    const auto& k=parameters.values;
    //13D500 supplies the vertical force;13D540 consumes three random values
    // in X/Y/Z order. Keep the source's multiply/subtract/add order and FMAC.
    const std::array<float,3> vertical{0.f,k[5]*-k[3],0.f};
    std::array<float,3> noise{};
    for(auto& v:noise)v=(random()-.5f)*emitterForce;
    for(unsigned i=0;i<3;++i){
        const float force=(vertical[i]+noise[i])*k[7];
        const float drag=p.velocity[i]*k[8];
        p.velocity[i]=p.velocity[i]+(force-drag);
        p.position[i]=std::fma(p.velocity[i],k[7]*k[4],p.position[i]);
    }
    const float limit=parameters.alternateMode==1?1.f:2.f;
    if(limit>p.size){
        const float bias=parameters.alternateMode==1?std::bit_cast<float>(0x3b83126fu):std::bit_cast<float>(0x3c03126fu);
        p.size=std::fma(k[1],emitterScale,bias)+p.size;
    }else p.size=limit;
    p.flags|=0x100u;
}
}
