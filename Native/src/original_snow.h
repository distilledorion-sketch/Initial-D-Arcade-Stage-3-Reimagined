#pragma once
#include "original_matrix.h"
#include <algorithm>
#include <bit>
#include <cmath>

namespace idas3::original {
inline constexpr unsigned snowPcw=0x8a00072e,snowIsp=0x93800000,snowTspWithoutSize=0x941024c0;
inline constexpr unsigned snowDiffuse=0xff888888,snowMaterialFlags=0x6aa;
//1AD820/1ADFE0's snow owner uses the existing rain resource bank, but its
//320 records and camera-relative spawn/update rules are separate from rain.
struct OriginalSnowParticle {
    std::array<float,3> position{},velocity{},normal{};
    int life=100;
    unsigned active=0;
    template<class RandomFloat> void spawn(const OriginalMatrix& view,unsigned index,unsigned replay,
            std::array<std::array<float,3>,4>& quadOffsets,RandomFloat&& random){
        active=1;
        const float z=-random()*15.f;
        position[2]=z-2.f;
        const float x=random()*10.f;position[0]=x-5.f;
        position[1]=std::fma(position[2]+2.f,std::bit_cast<float>(0x3e888889u),4.f);
        const float limit=position[2]*-.8f;
        if(position[1]>limit)position[1]=limit;
        if(replay&&(index&1))position[2]=-position[2];
        const float lifetime=(position[1]+2.f)*20.f;
        life=std::min(int(lifetime)+5,80);normal={0,0,1};
        constexpr std::array<float,5> xSpeed{-.02f,-.01f,0,.01f,.02f};
        constexpr std::array<float,4> ySpeed{-.01f,0,0,.01f};
        constexpr std::array<float,3> zSpeed{-.01f,0,.01f};
        velocity={xSpeed[index%5],ySpeed[index%4],zSpeed[index%3]};
        auto inverse=view;invertOriginalMatrix(inverse);
        //The original has one shared billboard basis. Odd slots retain the
        //basis from the last even birth, including when the view has moved.
        if(!(index&1)){
            constexpr std::array<std::array<float,4>,4> corners{{
                {-.04f,.04f,0,0},{.04f,.04f,0,0},{-.04f,-.04f,0,0},{.04f,-.04f,0,0}}};
            for(unsigned i=0;i<4;++i){const auto point=transformOriginalVector(inverse,corners[i]);
                quadOffsets[i]={point[0],point[1],point[2]};}
        }
        position=transformOriginalPoint(inverse,position);
    }
    void advance(){
        --life;
        for(unsigned i=0;i<3;++i)position[i]+=velocity[i];
        position[1]-=.05f;
        if(life<0)active=0;
    }
};
class OriginalSnowFields {
public:
    std::array<OriginalSnowParticle,320> particles{};
    std::array<std::array<float,3>,4> quadOffsets{};
    std::array<bool,320> visible{};
    unsigned cursor=0;
    template<class RandomFloat> void admit(const OriginalMatrix& view,unsigned replay,RandomFloat&& random){
        for(unsigned i=0;i<particles.size();++i)if(particles[i].active!=1){cursor=i;break;}
        if(particles[cursor].active!=1)particles[cursor].spawn(view,cursor,replay,quadOffsets,random);
        if(++cursor>=particles.size())cursor=0;
    }
    template<class RandomFloat> void advance(const OriginalMatrix& view,unsigned replay,RandomFloat&& random){
        for(unsigned i=0;i<4;++i)admit(view,replay,random);
        //1ADFE0 tests activity before advancing: expiry is still drawn once.
        for(unsigned i=0;i<particles.size();++i){
            visible[i]=particles[i].active==1;
            if(visible[i])particles[i].advance();
        }
    }
    std::array<std::array<float,3>,4> quad(unsigned index)const{
        std::array<std::array<float,3>,4> result{};
        if(visible.at(index))for(unsigned i=0;i<4;++i)for(unsigned axis=0;axis<3;++axis)
            result[i][axis]=particles[index].position[axis]+quadOffsets[i][axis];
        return result;
    }
};
}
