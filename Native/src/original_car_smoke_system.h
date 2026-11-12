#pragma once
#include "original_roadside_dust_system.h"

namespace idas3::original {
struct OriginalCarSmokeInput : OriginalRoadsideDustCar {
    // Actor+84..87, already computed by the original driving solver. These
    // bytes are in effect order, unlike actor+116..119's wheel contacts.
    std::array<std::uint8_t,4> intensity{};
};
struct OriginalCarSmokeScene {
    unsigned course=0,night=0,weather=0,profileCourse=0;
    unsigned numericRaceMode=2;
    bool playerRequested=true;
};

// Complete ARace::060860 admission and SmokeMgr lifetime ordering for both
// white tire smoke and brown roadside dust. The caller supplies the source
// actor outputs, scene and RNG; no host slip/speed threshold is substituted.
// The legacy dust-only system remains separate for IDR2 revision 2 playback.
class OriginalCarSmokeSystem {
public:
    static constexpr unsigned emitterCapacity=4,particleCapacity=60;
    OriginalCarSmokeSystem(){slots_.fill(-1);order_.reserve(emitterCapacity);}
    void reset(unsigned weather=0,bool alternate=false){
        *this=OriginalCarSmokeSystem{};
        parameters_=originalSmokeParameters(weather==1?1:0,alternate);
    }
    std::span<const unsigned> order()const{return order_;}
    std::size_t orderCapacity()const{return order_.capacity();}
    const OriginalRoadsideDustCloud& cloud(unsigned index)const{return clouds_.at(index);}
    const std::array<int,8>& slots()const{return slots_;}
    const std::array<unsigned,8>& kinds()const{return kinds_;}
    unsigned freeParticles()const{return freeParticles_;}
    std::size_t particleCount()const{std::size_t n=0;for(auto i:order_)n+=clouds_[i].emitter.particles.size();return n;}
    const OriginalSmokeParameters& parameters()const{return parameters_;}

    static float courseGain(unsigned course){
        // Original table at 0C2F4BF0, one scalar per original course.
        constexpr std::array<std::uint32_t,9> values{0x3f99999a,0x3fc00000,0x3f800000,
            0x3fb33333,0x3fa66666,0x3f99999a,0x3f800000,0x3f99999a,0x3f800000};
        return std::bit_cast<float>(values.at(course));
    }
    template<class RandomFloat> void advance(const OriginalCarSmokeScene& scene,
            const std::array<OriginalCarSmokeInput,2>& cars,RandomFloat&& random){
        if(scene.course>=9)throw std::out_of_range("Original car smoke course");
        const auto selected=originalRoadsideDustOwnerSelection(scene.numericRaceMode,scene.playerRequested);
        // Source clears opposite pointers, leaving manager kind bytes alone.
        for(unsigned slot=0;slot<8;++slot)if(slot/4!=selected.car)detach(slot,false);
        const auto& car=cars[selected.car];
        if(!car.available){for(unsigned slot=0;slot<8;++slot)detach(slot,false);}
        else {
            bool wetAllowed=true;
            for(auto surface:car.wheelSurfaces)if((surface&15)==6||(surface&15)==7)wetAllowed=false;
            const bool moving=car.owner.displacementMagnitude>.1f;
            // Rain processes effect slots 2 and 3 only (060A06..060A18).
            for(unsigned effect=scene.weather==1?2u:0u;effect<4;++effect){
                const unsigned slot=selected.slotBase+effect;
                const unsigned strength=scene.weather==1?unsigned(moving&&wetAllowed):car.intensity[effect];
                const bool dust=scene.course==7&&scene.night==0&&scene.weather==0&&
                    (car.wheelSurfaces[3-effect]&0x20)!=0;
                // Dust admission happens first. A still-attached white cloud
                // prevents allocation until the later white-stop branch.
                if(scene.course==7&&scene.night==0&&scene.weather==0){
                    if(dust){if(slots_[slot]<0)start(slot,2,0xff8b5439,selected,car);}
                    else if(slots_[slot]>=0&&kinds_[slot]==2)detach(slot,true);
                }
                if(strength>0){
                    if(slots_[slot]<0&&!dust)start(slot,1,0xffffffff,selected,car);
                    if(dust&&slots_[slot]>=0&&kinds_[slot]==1)detach(slot,true);
                }
                if(strength==0&&!dust&&slots_[slot]>=0)detach(slot,true);
                if(slots_[slot]<0)continue;
                float life=0;
                if(scene.profileCourse==8)life=moving?(scene.playerRequested?.4f:.6f):0.f;
                else if(scene.weather==1)life=moving?.4f:0.f;
                else if(dust)life=originalRoadsideDustLifetimeScale(effect,car.owner.displacementMagnitude);
                else life=std::min(float(car.intensity[effect])*courseGain(scene.course),255.f)/255.f;
                auto& cloud=clouds_[unsigned(slots_[slot])];
                attachOriginalSmokeEmitter(cloud.emitter,car.owner.positions[effect],life);
                cloud.footprint=car.owner.footprint;
            }
        }
        // Detached clouds retain the emitter budget while they drain. Phase
        // zero is removed before updating, so admission cannot reuse it early.
        for(auto it=order_.begin();it!=order_.end();){
            auto& cloud=clouds_[*it];
            if((cloud.emitter.flags&0xf0u)==0){it=order_.erase(it);continue;}
            advanceOriginalSmokeEmitter(cloud.emitter,parameters_,freeParticles_,random);++it;
        }
        for(auto index:order_)for(auto& particle:clouds_[index].emitter.particles)
            prepareOriginalSmokeParticleDisplay(particle);
    }
private:
    void detach(unsigned slot,bool clearKind){
        if(slots_[slot]>=0){stopOriginalSmokeEmitter(clouds_[unsigned(slots_[slot])].emitter);slots_[slot]=-1;}
        if(clearKind)kinds_[slot]=0;
    }
    void start(unsigned slot,unsigned kind,std::uint32_t color,
            const OriginalRoadsideDustOwnerSelection& selected,const OriginalCarSmokeInput& car){
        if(order_.size()==emitterCapacity)return;
        unsigned index=0;while(std::find(order_.begin(),order_.end(),index)!=order_.end())++index;
        auto& cloud=clouds_.at(index);cloud={};cloud.car=selected.car;cloud.effect=slot%4;
        cloud.color=color;cloud.gain=selected.displayGain;
        cloud.emitter.particles.reserve(particleCapacity);
        startOriginalCarSmokeEmitter(cloud.emitter,car.owner.positions[cloud.effect]);
        slots_[slot]=int(index);kinds_[slot]=kind;order_.push_back(index);
    }
    std::array<OriginalRoadsideDustCloud,emitterCapacity> clouds_{};
    std::vector<unsigned> order_;
    std::array<int,8> slots_{};
    std::array<unsigned,8> kinds_{};
    unsigned freeParticles_=particleCapacity;
    OriginalSmokeParameters parameters_=originalSmokeParameters(0);
};
}
