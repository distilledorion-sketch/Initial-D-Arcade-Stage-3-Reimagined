#pragma once
#include "original_matrix.h"
#include <bit>
#include <cmath>
#include <limits>

namespace idas3::original {
// Numerical fields of the original 88-byte pool record. Allocation/list links
// and the model pointer belong to the owner, not to this value type.
struct OriginalLeafParticle {
    std::array<float,3> position{},velocity{},angles{},spin{},origin{};
    std::uint32_t model=0,remaining=0;
};
struct OriginalLeafWake {
    std::array<float,3> origin{},direction{};
    float strength=0;
};
inline OriginalLeafWake originalLeafWake(const std::array<float,3>& origin,
        const std::array<float,3>& displacement){
    // 020B80: ordered FMAC, including 1F9BC0's finite zero-length result.
    float squared=displacement[1]*displacement[1];
    squared=std::fma(displacement[0],displacement[0],squared);
    squared=std::fma(displacement[2],displacement[2],squared);
    const float inverse=squared==0?std::numeric_limits<float>::max():1.f/std::sqrt(squared);
    OriginalLeafWake wake;wake.origin=origin;wake.strength=squared*inverse;
    for(unsigned i=0;i<3;++i)wake.direction[i]=displacement[i]*inverse;
    return wake;
}
inline std::array<float,2> originalLeafWakeForce(float axial,float radial,float strength){
    // 021080: finite wake radius, soft normalization and separate products.
    // The outputs are the axial/radial coefficients consumed by 020D40.
    const float radius=strength*9.25f;
    float a=radius>radial?radius-radial:0.f,b=.5f*axial;
    const float squared=std::fma(a,a,b*b);
    const float magnitude=std::bit_cast<float>(0x3ddd67c9u)*a;
    if(squared>std::bit_cast<float>(0x358637bdu)){
        const float inverse=1.f/std::sqrt(squared);a*=inverse;b*=inverse;
    }else{a=0;b=1;}
    a*=magnitude;b*=magnitude;a*=.1f;b*=.1f;
    return {a,b};
}
template<class Random> void initializeOriginalLeafParticle(OriginalLeafParticle& p,
        const std::array<float,3>& position,const std::array<float,3>& velocity,
        unsigned model,Random&& random){
    // 020940: called only after successful pool allocation. Exactly five
    // draws, including the two spin components. No invented launch velocity.
    p.position=p.origin=position;p.velocity=velocity;p.model=model;p.remaining=90;
    for(auto& angle:p.angles)angle=random()*std::bit_cast<float>(0x40c90fdbu);
    p.spin[0]=random()*std::bit_cast<float>(0x3ea0d97cu);p.spin[1]=0;
    p.spin[2]=random()*std::bit_cast<float>(0x3d80adfdu);
}
template<class Force,class Random> void moveOriginalLeafParticle(OriginalLeafParticle& p,
        const OriginalLeafWake& wake,const OriginalFscaTable& trig,Force&& force,Random&& random){
    // 020D40: resolve the car wake into axial and radial components. The
    // caller supplies the source wake's scalar-force callback (+4).
    std::array<float,3> radial;
    for(unsigned i=0;i<3;++i)radial[i]=p.position[i]-wake.origin[i];
    float axial=radial[1]*wake.direction[1];
    axial=std::fma(radial[0],wake.direction[0],axial);
    axial=std::fma(radial[2],wake.direction[2],axial);
    for(unsigned i=0;i<3;++i)radial[i]-=axial*wake.direction[i];
    float squared=radial[1]*radial[1];squared=std::fma(radial[0],radial[0],squared);
    squared=std::fma(radial[2],radial[2],squared);
    // 1F9BC0 returns FLT_MAX for exactly zero, not infinity.
    const float inverse=squared==0?std::numeric_limits<float>::max():1.f/std::sqrt(squared);
    for(auto& component:radial)component*=inverse;
    const auto forces=force(axial,squared*inverse,wake.strength);
    for(unsigned i=0;i<3;++i){
        const float outward=radial[i]*forces[1];
        const float delta=axial<0?wake.direction[i]*forces[0]-outward:
            std::fma(wake.direction[i],forces[0],outward);
        p.velocity[i]+=delta;p.angles[i]+=p.spin[i];
        p.velocity[i]*=std::bit_cast<float>(0x3f733333u);
        p.position[i]+=p.velocity[i];
    }
    if(p.origin[1]>p.position[1]){
        p.position[1]=p.origin[1];if(!(p.velocity[1]>0))p.velocity[1]=0;
        p.angles[0]=p.angles[2]=0;
    }
    auto rotation=originalIdentityMatrix();
    rotateOriginalMatrixY(rotation,p.angles[1],trig);
    rotateOriginalMatrixX(rotation,p.angles[0],trig);
    rotateOriginalMatrixZ(rotation,p.angles[2],trig);
    const float flutter=random()*std::bit_cast<float>(0x3c23d70au);
    p.velocity[0]=std::fma(flutter,rotation.elements[4],p.velocity[0]);
    p.velocity[1]-=std::bit_cast<float>(0x3b326750u);
    p.velocity[2]=std::fma(flutter,rotation.elements[6],p.velocity[2]);
}
template<class Force,class Random> bool advanceOriginalLeafParticle(OriginalLeafParticle& p,
        const OriginalLeafWake& wake,const OriginalFscaTable& trig,Force&& force,Random&& random){
    // 020CA0 decrements before testing the signed result; frame zero still
    // moves/draws, removal happens at -1. Expired records consume no RNG.
    --p.remaining;if(std::bit_cast<std::int32_t>(p.remaining)<0)return false;
    moveOriginalLeafParticle(p,wake,trig,force,random);return true;
}
template<class Random> bool advanceOriginalLeafParticle(OriginalLeafParticle& p,
        const OriginalLeafWake& wake,const OriginalFscaTable& trig,Random&& random){
    return advanceOriginalLeafParticle(p,wake,trig,originalLeafWakeForce,random);
}
} // namespace idas3::original
