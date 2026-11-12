#pragma once
#include "original_smoke_particle.h"
#include <algorithm>
#include <vector>

namespace idas3::original {
// SmokeEvt::update (13E240). This is the original tick controller, independent
// of host frame rate, car handling and renderer allocation. The caller owns the
// shared particle budget; renderer preparation is a separate source pass.
struct OriginalSmokeEmitter {
    std::array<float,3> spread{},position{},velocity{}; // +108/+124/+140
    std::array<float,3> previousPosition{}; // +164
    float scale=0,force=0,lifetimeScale=0; // +28/+32/+72
    std::uint32_t stopped=0,frame=0,flags=0; // +176/+180/+220
    std::vector<OriginalSmokeParticle> particles;
};

// Particle-relevant start stores in CarSmokeEvt::140660. ARace enters this
// call with the enclosing identity matrix and a world-space wheel position.
// Display gain belongs to each view; it must not overwrite scale or force.
inline void startOriginalCarSmokeEmitter(OriginalSmokeEmitter& e,const std::array<float,3>& position){
    e.flags=(e.flags&0xffffff0fu)|0x30u;
    e.stopped=0;e.frame=0;e.previousPosition=position;e.velocity={0,0,0};
    e.spread={0,0,1};e.position=position;
}

inline void stopOriginalSmokeEmitter(OriginalSmokeEmitter& e){e.stopped=1;} //13E940

// Particle-relevant outputs of13E160 and156340. The source retains its old
// basis for displacements <=1; its scale is the displacement magnitude, not
// the render-frame time or the car's speedometer reading.
inline void attachOriginalSmokeEmitter(OriginalSmokeEmitter& e,
        const std::array<float,3>& position,float lifetimeScale){
    for(unsigned i=0;i<3;++i)e.velocity[i]=position[i]-e.previousPosition[i];
    double squared=double(e.velocity[0])*double(e.velocity[0]);
    squared+=double(e.velocity[1])*double(e.velocity[1]);
    squared+=double(e.velocity[2])*double(e.velocity[2]);squared+=0.0;
    e.scale=std::sqrt(float(squared));
    if(e.scale>1.f){
        const auto& v=e.velocity;
        const float horizontal=std::fma(v[0],v[0],v[2]*v[2]);
        const float inverse=1.f/std::sqrt(horizontal+v[1]*v[1]);
        for(unsigned i=0;i<3;++i)e.spread[i]=v[i]*inverse;
    }
    e.position=e.previousPosition=position;e.lifetimeScale=lifetimeScale;
}

//13DA00 and13FCB4..13FCCC: the display update clears the live marker only
// after the particle phase becomes zero. Removal occurs on the next emitter
// update. Do not fold this into motion, or culling changes particle lifetimes.
inline bool prepareOriginalSmokeParticleDisplay(OriginalSmokeParticle& p){
    if((p.flags&0xf0u)==0){p.flags&=0xfffffeffu;return false;}
    return (p.flags&0x100u)!=0;
}

template<class RandomFloat> inline void advanceOriginalSmokeEmitter(OriginalSmokeEmitter& e,
        const OriginalSmokeParameters& parameters,std::uint32_t& freeParticles,RandomFloat&& random){
    for(auto& p:e.particles)advanceOriginalSmokeParticle(p,parameters,e.scale,e.force,random);
    const auto before=e.particles.size();
    std::erase_if(e.particles,[](const auto& p){return (p.flags&0x100u)==0;});
    freeParticles+=std::uint32_t(before-e.particles.size());
    const auto phase=(e.flags>>4)&15u;
    const auto setPhase=[&](unsigned value){e.flags=(e.flags&0xffffff0fu)|(value<<4);};
    switch(phase){
    case 0:return;
    case 2:if(e.particles.empty())setPhase(0);break;
    case 3:
        if(e.frame%5u==0||e.frame%5u==2)setPhase(1);
        if(e.stopped)setPhase(2);
        break;
    case 1:
        if(e.stopped){setPhase(2);break;}
        e.force=(1.f+random())*parameters.values[6];
        if(std::bit_cast<std::int32_t>(freeParticles)>0){
            --freeParticles;
            const bool alternate=parameters.alternateMode==1;
            auto position=e.position;
            if(!alternate){
                position[0]=std::fma(random()-.5f,.2f,position[0]);
                position[2]=std::fma(random()-.5f,.2f,position[2]);
            }
            std::array<float,3> velocity{};
            for(unsigned i=0;i<3;++i){
                const float spread=e.spread[i]*(random()-.5f);
                velocity[i]=std::fma(alternate?.1f:.5f,spread,e.velocity[i]*parameters.values[0]);
            }
            const float duration=e.lifetimeScale*(alternate?20.f:25.f);
            // SH-4 FTRC saturates, including the indefinite result for NaN.
            const auto lifetime=std::isnan(duration)||duration<=-2147483648.f?0x80000000u:
                duration>=2147483648.f?0x7fffffffu:std::uint32_t(std::int32_t(duration));
            OriginalSmokeParticle particle;
            initializeOriginalSmokeParticle(particle,position,velocity,alternate?.05f:.1f,lifetime,0,1);
            e.particles.push_back(particle);
        }
        setPhase(3);
        break;
    }
    ++e.frame;
}
}
