#pragma once
#include <array>
#include <cstdint>
#include "original_matrix.h"
#include <stdexcept>

namespace idas3::original {
//17CB32..17CCC2. This consumes the published ACar WORLD matrix, so the
//source's +0.3 mount height is retained (no host body offset/Euler rebuild).
inline OriginalMatrix originalBackfireDrawMatrix(OriginalMatrix world,unsigned muffler,
        float variation,const OriginalFscaTable& trig){
    if(muffler>=3)throw std::out_of_range("Original backfire exhaust mount");
    constexpr std::array<std::array<float,3>,3> mounts{{
        {.561f,.0062f,-2.1171f},{.5675f,-.0087f,-2.1164f},{.56f,-.005f,-2.104f}}};
    constexpr std::array<unsigned,3> phase{64285,63764,63305};
    auto point=mounts[muffler];point[1]+=.3f;point[2]+=.05f;
    const float depth=1.f+variation;
    if(depth<1.f)point[2]-=(1.f-depth)*.05f;
    translateOriginalMatrix(world,point);
    rotateOriginalMatrixPhase(world,1,std::uint16_t(phase[muffler]),trig);
    if(muffler==2){rotateOriginalMatrixPhase(world,0,0x059e,trig);scaleOriginalMatrix(world,{1.05f,1.05f,1});}
    scaleOriginalMatrix(world,{1,1,depth});
    return world;
}
//17C5E0/17CAC0/17CB00: replay effect state, independent of engine audio.
//Advance at the original frame boundary; rendering only reads the result.
struct OriginalBackfireFields {
    struct Car {
        bool active=false;
        unsigned frame=0;
        float variation=0;
        int presentedFrame=-1;
    };
    std::array<Car,2> cars{};
    unsigned updates=0;
    bool primarySoundRequest=false;

    void advance(const std::array<bool,2>& enabled,
            const std::array<std::uint32_t,2>& actorFlags,std::uint32_t& seed){
        const auto next=[&]{seed=seed*0x41c64e6du+0x3039u;return (seed>>16)&32767u;};
        ++updates;primarySoundRequest=false;
        //Both trigger decisions precede either car's presentation update.
        //The actor +5C bit1 gate is distinct from braking (bit0).
        for(unsigned i=0;i<2;++i)if(enabled[i]&&(actorFlags[i]&2u)){
            if(next()%5u==1u){
                auto& car=cars[i];
                if(!car.active){car.active=true;car.frame=0;car.variation=1.f;}
                if(i==0)primarySoundRequest=true;
            }
        }
        for(unsigned i=0;i<2;++i){
            auto& car=cars[i];car.presentedFrame=-1;
            if(!enabled[i])continue;
            //17CB00 consumes this draw even while the flame is inactive.
            const float random=float(next())*(1.f/32768.f);
            const float product=random*.95f;
            car.variation=product-.8f;
            if(car.active){
                car.presentedFrame=int(car.frame);
                if(++car.frame>1u)car.active=false;
            }
        }
    }
};
}
