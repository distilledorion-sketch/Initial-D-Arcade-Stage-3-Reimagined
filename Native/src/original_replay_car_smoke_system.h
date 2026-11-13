#pragma once
#include "original_car_smoke_system.h"

namespace idas3::original {
// AReplay::0E3900. Unlike ARace::060860 this updates both cars, only effects
// 2/3, uses unscaled actor intensity, and supplies .05 to CarSmokeDisp.
// sceneCode is replay owner+BC+48; weather is owner+BC+56. Keep those source
// selectors explicit: the live race's course/night admission is different.
class OriginalReplayCarSmokeSystem {
public:
    static constexpr unsigned emitterCapacity=4,particleCapacity=60;
    OriginalReplayCarSmokeSystem(){slots_.fill(-1);order_.reserve(emitterCapacity);}
    void reset(unsigned weather=0,bool alternate=false){
        *this=OriginalReplayCarSmokeSystem{};
        parameters_=originalSmokeParameters(weather==1?1:0,alternate);
    }
    std::span<const unsigned> order()const{return order_;}
    const OriginalRoadsideDustCloud& cloud(unsigned i)const{return clouds_.at(i);}
    const std::array<int,8>& slots()const{return slots_;}
    const std::array<unsigned,8>& kinds()const{return kinds_;}
    const OriginalSmokeParameters& parameters()const{return parameters_;}
    unsigned freeParticles()const{return freeParticles_;}
    std::size_t particleCount()const{std::size_t n=0;for(auto i:order_)n+=clouds_[i].emitter.particles.size();return n;}

    template<class RandomFloat> void advance(unsigned sceneCode,unsigned weather,
            unsigned carCount,const std::array<OriginalCarSmokeInput,2>& cars,RandomFloat&& random){
        // E3946 caps the source car count at two; slots of omitted cars are
        // left alone, not forcibly stopped as in the live race owner switch.
        for(unsigned carIndex=0;carIndex<std::min(carCount,2u);++carIndex){
            const auto& car=cars[carIndex];
            if(!car.available)throw std::invalid_argument("Missing replay smoke actor");
            const bool moving=car.owner.displacementMagnitude>.1f;
            for(unsigned effect=2;effect<4;++effect){
                const unsigned slot=carIndex*4+effect;
                const unsigned strength=weather==1?unsigned(moving):car.intensity[effect];
                const bool dust=sceneCode==14&&weather==0&&(car.wheelSurfaces[3-effect]&0x20)!=0;
                if(sceneCode==14&&weather==0){
                    if(dust){if(slots_[slot]<0)start(slot,2,0xff8b5439,car);}
                    else if(slots_[slot]>=0&&kinds_[slot]==2)detach(slot);
                }
                if(strength>0){
                    if(slots_[slot]<0&&!dust)start(slot,1,0xffffffff,car);
                    if(dust&&slots_[slot]>=0&&kinds_[slot]==1)detach(slot);
                }
                if(strength==0&&!dust&&slots_[slot]>=0)detach(slot);
                if(slots_[slot]<0)continue;
                const float life=sceneCode==17?(moving?.6f:0.f):
                    weather==1?(moving?.4f:0.f):dust?(moving?.5f:0.f):
                    float(car.intensity[effect])/255.f;
                auto& cloud=clouds_[unsigned(slots_[slot])];
                attachOriginalSmokeEmitter(cloud.emitter,car.owner.positions[effect],life);
                cloud.footprint=car.owner.footprint;
            }
        }
        // E3FB2 invokes the same SmokeMgr update only after both cars.
        for(auto it=order_.begin();it!=order_.end();){
            auto& cloud=clouds_[*it];
            if((cloud.emitter.flags&0xf0u)==0){it=order_.erase(it);continue;}
            advanceOriginalSmokeEmitter(cloud.emitter,parameters_,freeParticles_,random);++it;
        }
        for(auto i:order_)for(auto& p:clouds_[i].emitter.particles)prepareOriginalSmokeParticleDisplay(p);
    }
private:
    void detach(unsigned slot){
        stopOriginalSmokeEmitter(clouds_[unsigned(slots_[slot])].emitter);slots_[slot]=-1;kinds_[slot]=0;
    }
    void start(unsigned slot,unsigned kind,unsigned color,const OriginalCarSmokeInput& car){
        if(order_.size()==emitterCapacity)return;
        unsigned index=0;while(std::find(order_.begin(),order_.end(),index)!=order_.end())++index;
        auto& cloud=clouds_.at(index);cloud={};cloud.car=slot/4;cloud.effect=slot%4;
        cloud.color=color;cloud.gain=.05f;cloud.emitter.particles.reserve(particleCapacity);
        startOriginalCarSmokeEmitter(cloud.emitter,car.owner.positions[cloud.effect]);
        slots_[slot]=int(index);kinds_[slot]=kind;order_.push_back(index);
    }
    std::array<OriginalRoadsideDustCloud,emitterCapacity> clouds_{};
    std::array<int,8> slots_{};
    std::array<unsigned,8> kinds_{};
    std::vector<unsigned> order_;
    unsigned freeParticles_=particleCapacity;
    OriginalSmokeParameters parameters_=originalSmokeParameters(0);
};
}
