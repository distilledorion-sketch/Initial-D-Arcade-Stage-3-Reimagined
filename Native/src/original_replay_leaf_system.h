#pragma once
#include "original_leaf_particle.h"
#include "original_roadside_dust_owner.h"
#include <span>
#include <algorithm>

namespace idas3::original {
struct OriginalReplayLeafFrame {
    std::array<std::array<float,3>,4> contacts{};
    std::array<std::uint8_t,4> surfaces{};
    std::array<float,3> wakeOrigin{},wakeDisplacement{};
};
// E4320 consumes the primary recorded actor, independently of the replay
// smoke owner's two-car loop. Its two matrix passes have different offsets.
inline OriginalReplayLeafFrame originalReplayLeafFrame(const OriginalMatrix& world,
        const std::array<std::array<float,3>,4>& origins,
        const std::array<std::uint8_t,4>& surfaces,
        const std::array<float,3>& displacement,const OriginalFscaTable& trig){
    OriginalReplayLeafFrame out;out.surfaces=surfaces;
    auto wheels=world;rotateOriginalMatrixPhase(wheels,1,0x8000,trig);
    for(unsigned i=0;i<4;++i)out.contacts[i]=transformOriginalPoint(wheels,origins[i]);
    auto wake=world;
    translateOriginalMatrix(wake,{0,std::bit_cast<float>(0xc00a3d71u),std::bit_cast<float>(0xbfbccccdu)});
    for(unsigned i=0;i<3;++i){
        out.wakeOrigin[i]=wake.elements[12+i];
        out.wakeDisplacement[i]=displacement[i]*(i==1?3.2f:1.2f);
    }
    return out;
}
inline bool originalReplayLeafSurface(unsigned surface){
    const unsigned kind=surface&15u;return kind==1||kind==2||kind==11;
}
class OriginalReplayLeafSystem {
public:
    // E2BD2 passes 300 to D44C0; its default of 80 is not used by replay.
    static constexpr unsigned capacity=300;
    OriginalReplayLeafSystem(){order_.reserve(capacity);}
    void reset(){order_.clear();occupied_.fill(false);admitted_=0;wake_={};cooldown_=0;}
    std::span<const unsigned> order()const{return order_;}
    const OriginalLeafParticle& particle(unsigned index)const{return particles_.at(index);}
    const OriginalLeafWake& wake()const{return wake_;}
    unsigned admitted()const{return admitted_;}
    unsigned cooldown()const{return cooldown_;}
    // D4B60 makes three sequential allocation attempts. A full pool consumes
    // no constructor RNG, and newly admitted leaves enter at the list front.
    template<class Random> void emit(const std::array<float,3>& position,
            const std::array<float,3>& velocity,Random&& random){
        for(unsigned model=4;model<=6;++model){
            if(order_.size()==capacity)continue;
            const unsigned slot=unsigned(std::find(occupied_.begin(),occupied_.end(),false)-occupied_.begin());
            auto origin=position;if(model==5)origin[0]+=.1f;else if(model==6)origin[0]-=.1f;
            initializeOriginalLeafParticle(particles_[slot],origin,velocity,model,random);
            occupied_[slot]=true;order_.insert(order_.begin(),slot);++admitted_;
        }
    }
    template<class Random> void advance(const OriginalReplayLeafFrame& frame,
            const OriginalFscaTable& trig,Random&& random){
        if(cooldown_)--cooldown_;
        else for(unsigned wheel=0;wheel<4;++wheel)if(originalReplayLeafSurface(frame.surfaces[wheel]))
            emit(frame.contacts[wheel],{0,0,0},random);
        wake_=originalLeafWake(frame.wakeOrigin,frame.wakeDisplacement);
        for(auto it=order_.begin();it!=order_.end();){
            if(!advanceOriginalLeafParticle(particles_[*it],wake_,trig,random)){occupied_[*it]=false;it=order_.erase(it);}
            else ++it;
        }
        // 020C60 retains the decayed value even though D4B00 overwrites it
        // on the following tick. Keep this observable owner field exact.
        wake_.strength*=std::bit_cast<float>(0x3f7ae148u);
    }
private:
    std::array<OriginalLeafParticle,capacity> particles_{};
    std::array<bool,capacity> occupied_{};
    std::vector<unsigned> order_;
    OriginalLeafWake wake_;
    unsigned admitted_=0,cooldown_=0;
};
} // namespace idas3::original
