#pragma once
#include "original_roadside_dust.h"
#include "original_roadside_dust_owner.h"
#include "original_smoke_emitter.h"
#include <span>

namespace idas3::original {
struct OriginalRoadsideDustCar {
    bool available=false;
    OriginalRoadsideDustOwner owner;
    std::array<std::uint8_t,4> wheelSurfaces{};
};
struct OriginalRoadsideDustCloud {
    OriginalSmokeEmitter emitter;
    OriginalMatrix footprint;
    unsigned car=0,effect=0;
    float gain=0;
    std::uint32_t color=0;
};

// Dust's presentation owner. The source manager budgets are4 emitters and60
// particles (13EA0E..13EA22); stopped emitters retain their budget until the
// source phase reaches0 and the following manager pass removes them.
// No clock or gameplay state is owned here: call once per accepted source
// tick, and supply the presentation RNG explicitly.
class OriginalRoadsideDustSystem {
public:
    static constexpr unsigned emitterCapacity=4,particleCapacity=60;
    OriginalRoadsideDustSystem(){slots_.fill(-1);order_.reserve(emitterCapacity);}
    void reset(){*this=OriginalRoadsideDustSystem{};}
    std::span<const unsigned> order()const{return order_;}
    const OriginalRoadsideDustCloud& cloud(unsigned index)const{return clouds_.at(index);}
    const std::array<std::array<bool,4>,2>& suppressedTireSmoke()const{return suppressed_;}
    unsigned freeParticles()const{return freeParticles_;}
    std::size_t particleCount()const{std::size_t n=0;for(auto index:order_)n+=clouds_[index].emitter.particles.size();return n;}

    template<class RandomFloat> void advance(unsigned course,unsigned night,unsigned weather,
            unsigned numericRaceMode,bool playerRequested,
            const std::array<OriginalRoadsideDustCar,2>& cars,RandomFloat&& random){
        suppressed_={};
        const auto selected=originalRoadsideDustOwnerSelection(numericRaceMode,playerRequested);
        for(unsigned slot=0;slot<8;++slot)if(slot/4!=selected.car)detach(slot);
        const auto& car=cars[selected.car];
        if(course!=7||night||weather||!car.available){
            for(unsigned slot=0;slot<8;++slot)detach(slot);
        }else for(unsigned effect=0;effect<4;++effect){
            const auto slot=selected.slotBase+effect;
            OriginalRoadsideDustInput input{course,night,weather,effect,car.wheelSurfaces};
            input.emitterPresent=slots_[slot]>=0;input.emitterKind=2;
            input.allocationAvailable=order_.size()<emitterCapacity;input.displayGain=selected.displayGain;
            const auto decision=originalRoadsideDustDecision(input);
            suppressed_[selected.car][decision.wheel]=decision.replacesTireSmoke;
            if(decision.action==OriginalRoadsideDustDecision::Action::Stop)detach(slot);
            else if(decision.action==OriginalRoadsideDustDecision::Action::Start){
                unsigned index=0;while(std::find(order_.begin(),order_.end(),index)!=order_.end())++index;
                auto& cloud=clouds_.at(index);cloud={};cloud.car=selected.car;cloud.effect=effect;
                cloud.gain=decision.displayGain;cloud.color=decision.color;
                cloud.emitter.particles.reserve(particleCapacity);
                startOriginalCarSmokeEmitter(cloud.emitter,car.owner.positions[effect]);
                slots_[slot]=int(index);order_.push_back(index);
            }
            if(slots_[slot]>=0){
                auto& cloud=clouds_[unsigned(slots_[slot])];
                attachOriginalSmokeEmitter(cloud.emitter,car.owner.positions[effect],
                    originalRoadsideDustLifetimeScale(effect,car.owner.displacementMagnitude));
                cloud.footprint=car.owner.footprint;
            }
        }
        //061068 invokes the manager after car admission/attachment.13EF00
        // tests phase BEFORE update, preserving source allocation/reuse order.
        for(auto it=order_.begin();it!=order_.end();){
            auto& cloud=clouds_[*it];
            if((cloud.emitter.flags&0xf0u)==0){it=order_.erase(it);continue;}
            advanceOriginalSmokeEmitter(cloud.emitter,parameters_,freeParticles_,random);++it;
        }
        // The original display's phase-zero cleanup precedes next tick's
        // removal regardless of visibility. Run that bookkeeping once here;
        // repainting at a different host FPS must not change particle life.
        for(auto index:order_)for(auto& particle:clouds_[index].emitter.particles)
            prepareOriginalSmokeParticleDisplay(particle);
    }
    const OriginalSmokeParameters& parameters()const{return parameters_;}
private:
    void detach(unsigned slot){if(slots_[slot]>=0){stopOriginalSmokeEmitter(clouds_[unsigned(slots_[slot])].emitter);slots_[slot]=-1;}}
    std::array<OriginalRoadsideDustCloud,emitterCapacity> clouds_{};
    std::vector<unsigned> order_;
    std::array<int,8> slots_{};
    std::array<std::array<bool,4>,2> suppressed_{};
    unsigned freeParticles_=particleCapacity;
    OriginalSmokeParameters parameters_=originalSmokeParameters(0); //0662B6: dry weather preset
};
}
