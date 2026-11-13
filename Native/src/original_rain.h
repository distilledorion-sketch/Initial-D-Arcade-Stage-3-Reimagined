#pragma once
#include "original_matrix.h"
#include <cmath>

namespace idas3::original {
inline constexpr unsigned rainPcw=0x8a00072e,rainIsp=0x83800000,rainTspWithoutSize=0x941024c0;
// Original rain-owner records (streaks, ground splashes and wheel mist). The caller
// advances once per source tick and supplies the current render view and the
// shared effect RNG. Rendering never changes records or consumes random values.
struct OriginalRainStreak {
    std::array<float,3> position{},direction{},normal{};
    std::array<std::array<float,3>,4> offsets{};
    int life=100;
    unsigned active=0;
    void setMotion(const std::array<float,3>& motion){direction={motion[0],0,motion[2]};}
    template<class RandomFloat> void spawn(const OriginalMatrix& view,RandomFloat&& random){
        //17FFC0: two random draws, camera-relative birth, inverse-view quad.
        life=25;active=1;
        const float x=random()*10.f;
        position={x-5.f,5,0};
        const float z=-random()*15.f;
        position[2]=z-2.f;
        normal={0,0,1};direction[0]=-direction[0];direction[2]=-direction[2];
        auto inverse=view;invertOriginalMatrix(inverse);
        constexpr std::array<std::array<float,4>,4> authored{{
            {-.02f,.7f,0,0},{.02f,.7f,0,0},{-.02f,-.7f,0,0},{.02f,-.7f,0,0}}};
        for(unsigned i=0;i<4;++i){
            const auto transformed=transformOriginalVector(inverse,authored[i]);
            offsets[i]={transformed[0],transformed[1],transformed[2]};
            for(unsigned axis:{0u,2u}){
                if(i<2){const float shift=direction[axis]*.5f;offsets[i][axis]-=shift;}
                else offsets[i][axis]=std::fma(direction[axis],2.8f,offsets[i][axis]);
            }
        }
        position=transformOriginalPoint(inverse,position);
    }
    void advance(){--life;position[1]-=.8f;if(life<0)active=0;}
};
struct OriginalRainSplash {
    std::array<float,3> position{};
    int life=100;
    unsigned active=0;
    template<class RandomFloat> void spawn(const OriginalMatrix& carWorld,RandomFloat&& random){
        //1801E0: births follow the car matrix, not the camera.
        life=3;active=1;
        const float x=random()*10.f;
        position={x-5.f,.1f,0};
        const float z=random()*15.f;
        position[2]=z-2.f;
        position=transformOriginalPoint(carWorld,position);
    }
    void advance(){--life;if(life<0)active=0;}
};
struct OriginalRainWheelMist {
    std::array<float,3> position{};
    float velocityX=0,drag=0,velocityZ=0;
    int life=100;
    unsigned active=0;
    template<class RandomFloat> void spawn(unsigned index,const std::array<float,3>& wheel,
            const std::array<float,3>& motion,const OriginalMatrix& carWorld,RandomFloat&& random){
        //180440/1802E0. This is the rain owner's72-record pool, separate from
        // the smoke manager's wet tyre smoke.
        life=5;active=1;velocityX=motion[0];velocityZ=motion[2];
        const float randomX=random()*.15f-.075f,randomZ=random()*.15f-.075f;
        position={wheel[0],0,wheel[2]-.7f};
        if(index==2||index==3){position[0]+=.01f;position[2]+=std::fma(velocityZ,.55f,-.05f);}
        else if(index==4||index==5){position[0]-=.01f;const float shift=velocityZ*.55f;position[2]+=-.05f-shift;}
        position[0]+=randomX;position[2]-=randomZ;
        position=transformOriginalPoint(carWorld,position);
        drag=std::fma(random(),.2f,.15f);
    }
    void advance(){
        const float dampingX=velocityX*drag,dampingZ=velocityZ*drag;
        velocityX-=dampingX;velocityZ-=dampingZ;
        position[0]+=velocityX;position[2]+=velocityZ;position[1]+=.02f;
        --life;if(life<0)active=0;
    }
};
inline OriginalMatrix originalRainBillboardMatrix(const OriginalMatrix& view,const std::array<float,3>& position){
    auto result=view;translateOriginalMatrix(result,position);
    for(unsigned column=0;column<3;++column)for(unsigned row=0;row<3;++row)
        result.elements[column*4+row]=row==column?1.f:0.f;
    return result;
}
inline OriginalMatrix originalRainSplashViewMatrix(const OriginalMatrix& view,
        const std::array<float,3>& position,const OriginalFscaTable& table){
    //17EF60: translate in the current view,1F6AF0 replaces only the upper3x3,
    // then rotateY(0x8000) and scale(.1). Retain the source matrix's fourth row.
    auto result=originalRainBillboardMatrix(view,position);
    rotateOriginalMatrixPhase(result,1,0x8000,table);scaleOriginalMatrix(result,{.1f,.1f,.1f});
    return result;
}
inline OriginalMatrix originalRainWheelMistViewMatrix(const OriginalMatrix& view,const OriginalRainWheelMist& particle){
    auto result=originalRainBillboardMatrix(view,particle.position);
    const float lifeScale=float(particle.life)*.03f;
    const float scale=.4f>lifeScale?.25f+lifeScale:.65f;
    scaleOriginalMatrix(result,{scale,scale,scale});return result;
}
struct OriginalRainVertex {
    std::array<float,3> position{};
    float u=0,v=0;
    unsigned argb=0,stripControl=0;
};
class OriginalRainFields {
public:
    std::array<OriginalRainStreak,25> streaks{};
    std::array<OriginalRainSplash,9> splashes{};
    std::array<OriginalRainWheelMist,72> wheelMist{},drawnWheelMist{};
    unsigned streakCursor=0,splashCursor=0,wheelMistCursor=0;
    // A record expiring during this tick is still published once by17F7E0 /
    //17EF60. Keep that snapshot separate from the post-tick admission flags.
    std::array<bool,25> visibleStreaks{};
    std::array<bool,9> visibleSplashes{};
    void reset(){*this={};}
    template<class RandomFloat> void admitStreak(const OriginalMatrix& view,
            const std::array<float,3>& motion,RandomFloat&& random){
        for(unsigned i=0;i<streaks.size();++i)if(streaks[i].active!=1){streakCursor=i;break;}
        auto& streak=streaks[streakCursor];
        if(streak.active!=1){streak.setMotion(motion);streak.spawn(view,random);}
        if(++streakCursor==streaks.size())streakCursor=0;
    }
    template<class RandomFloat> void admitSplash(const OriginalMatrix& carWorld,RandomFloat&& random){
        for(unsigned i=0;i<splashes.size();++i)if(splashes[i].active!=1){splashCursor=i;break;}
        auto& splash=splashes[splashCursor];
        if(splash.active!=1)splash.spawn(carWorld,random);
        if(++splashCursor==splashes.size())splashCursor=0;
    }
    template<class RandomFloat> void admitWheelMist(const OriginalMatrix& carWorld,
            const std::array<float,3>& motion,const std::array<std::array<float,3>,2>& wheels,RandomFloat&& random){
        for(unsigned index=0;index<6;++index){
            for(unsigned i=0;i<wheelMist.size();++i)if(wheelMist[i].active!=1){wheelMistCursor=i;break;}
            auto& particle=wheelMist[wheelMistCursor];
            if(particle.active!=1)particle.spawn(index,wheels[index<3?0:1],motion,carWorld,random);
            if(++wheelMistCursor==wheelMist.size())wheelMistCursor=0;
        }
    }
    void advanceStreaks(){
        for(unsigned i=0;i<streaks.size();++i){visibleStreaks[i]=streaks[i].active==1;if(visibleStreaks[i])streaks[i].advance();}
    }
    void advanceSplashes(bool drawEnabled){
        for(unsigned i=0;i<splashes.size();++i){visibleSplashes[i]=drawEnabled&&splashes[i].active==1;if(visibleSplashes[i])splashes[i].advance();}
    }
    void advanceWheelMist(){
        //17F120 publishes each active record before advancing it.
        drawnWheelMist=wheelMist;
        for(auto& particle:wheelMist)if(particle.active==1)particle.advance();
    }
    std::array<OriginalRainVertex,4> streakQuad(unsigned index,unsigned night)const{
        std::array<OriginalRainVertex,4> quad{};
        for(unsigned vertex=0;vertex<4;++vertex){
            auto& output=quad[vertex];
            if(visibleStreaks.at(index))for(unsigned axis=0;axis<3;++axis)
                output.position[axis]=streaks[index].position[axis]+streaks[index].offsets[vertex][axis];
            output.u=vertex%2?1.f:0.f;output.v=vertex<2?1.f:0.f;
            output.argb=night==1?0x50ffffffu:0x40ffffffu;
            output.stripControl=vertex==3?0xa0000000u:vertex==2?0x60000000u:0;
        }
        return quad;
    }
};
}
