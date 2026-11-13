#pragma once
#include "original_rain.h"
#include <stdexcept>

namespace idas3::original {
struct OriginalRainCar {
    bool available=false;
    OriginalMatrix world=originalIdentityMatrix();
    // ACar+9A8 is the published raw actor XYZ, distinct from its visual matrix.
    std::array<float,3> actorPosition{};
    std::array<std::array<float,3>,4> effectOrigins{};
    std::array<unsigned char,4> wheelSurfaces{};
};
//17F7E0 owns weather admission and motion history. The optical rear-lamp pass
//17F500 is a distinct dependency and MUST be supplied by the caller, after
// particle publication and before the previous matrices are replaced.
class OriginalRainOwner {
public:
    OriginalRainFields fields;
    std::array<float,3> previousVisualPosition{},streakMotion{};
    std::array<std::array<float,3>,2> previousActorPosition{},actorMotion{};
    std::array<OriginalMatrix,2> previousWorld{};
    std::array<bool,2> glareAllowed{true,true},weatherAllowed{true,true};
    unsigned preserveStreakMotion=0;
    void reset(const std::array<OriginalRainCar,2>& cars,unsigned preserveMotion=0){
        *this={};
        preserveStreakMotion=preserveMotion;
        for(unsigned i=0;i<2;++i)if(cars[i].available)previousWorld[i]=cars[i].world;
    }
    template<class RandomFloat,class RearLampPass> void advance(const OriginalMatrix& view,
            const std::array<OriginalRainCar,2>& cars,RandomFloat&& random,RearLampPass&& rearLamps){
        if(!cars[0].available)throw std::invalid_argument("Original rain owner requires its primary car");
        const auto& primary=cars[0];
        for(unsigned axis=0;axis<3;++axis){
            if(!preserveStreakMotion)streakMotion[axis]=primary.world.elements[12+axis]-previousVisualPosition[axis];
            previousVisualPosition[axis]=primary.world.elements[12+axis];
        }
        glareAllowed={true,true};weatherAllowed={true,true};
        for(unsigned car=0;car<2;++car)if(cars[car].available)for(auto surface:cars[car].wheelSurfaces){
            // Both branches store into the SAME source stack flag:12C/130.
            // Rain/mist admission and17F500 receive that shared6-or7 gate.
            if((surface&15)==6||(surface&15)==7)glareAllowed[car]=weatherAllowed[car]=false;
        }
        fields.admitStreak(view,streakMotion,random);fields.advanceStreaks();
        for(unsigned i=0;i<3;++i)fields.admitSplash(primary.world,random);
        for(unsigned car=0;car<2;++car)if(cars[car].available){
            auto& delta=actorMotion[car];
            for(unsigned axis=0;axis<3;++axis){
                delta[axis]=cars[car].actorPosition[axis]-previousActorPosition[car][axis];
                previousActorPosition[car][axis]=cars[car].actorPosition[axis];
            }
            //1F6C90: FIPR into float, followed by FSQRT. Render dt and the
            // speedometer must not be used to decide whether wheels emit.
            double squared=double(delta[0])*double(delta[0]);
            squared+=double(delta[1])*double(delta[1]);squared+=double(delta[2])*double(delta[2]);squared+=0.0;
            if(std::sqrt(float(squared))>.08f&&weatherAllowed[car])
                fields.admitWheelMist(cars[car].world,delta,{cars[car].effectOrigins[3],cars[car].effectOrigins[2]},random);
        }
        fields.advanceSplashes(weatherAllowed[0]);fields.advanceWheelMist();
        rearLamps(glareAllowed,previousWorld,random);
        for(unsigned car=0;car<2;++car)if(cars[car].available)previousWorld[car]=cars[car].world;
    }
};
}
