#pragma once
#include "original_car_smoke_system.h"

namespace idas3 {
// Numerical presentation state: copy with the race checkpoint, advance once
// per simulated tick, and only observe it while rendering. Live races advance
// with the driving owner's shared LCG after pose publication and before the
// outer tire-sound update (05FDC0/066840, then056ECC/1415E0). The retained seed
// also supports replay's recorded boundary without mutating a live session.
class CarSmokeState {
public:
    void reset(unsigned weather=0,bool alternate=false){
        system_.reset(weather,alternate);random_=0x37c778u;frames_=0;
    }
    void advance(const original::OriginalCarSmokeScene& scene,
            const std::array<original::OriginalCarSmokeInput,2>& cars){
        system_.advance(scene,cars,[&](){
            random_=random_*0x41c64e6du+12345u;
            return float((random_>>16)&32767u)/32768.f;
        });
        ++frames_;
    }
    void advanceShared(const original::OriginalCarSmokeScene& scene,
            const std::array<original::OriginalCarSmokeInput,2>& cars,unsigned& sharedRandom){
        random_=sharedRandom;advance(scene,cars);sharedRandom=random_;
    }
    // Replay stores the seed before each accepted tick. This also preserves
    // future source-owner RNG interleaving without depending on repaint rate.
    void advanceRecorded(const original::OriginalCarSmokeScene& scene,
            const std::array<original::OriginalCarSmokeInput,2>& cars,unsigned randomBefore){
        random_=randomBefore;advance(scene,cars);
    }
    const original::OriginalCarSmokeSystem& system()const{return system_;}
    unsigned randomState()const{return random_;}
    std::uint64_t frames()const{return frames_;}
    std::size_t dynamicBytes()const{
        std::size_t bytes=system_.orderCapacity()*sizeof(unsigned);
        for(unsigned i=0;i<original::OriginalCarSmokeSystem::emitterCapacity;++i)
            bytes+=system_.cloud(i).emitter.particles.capacity()*sizeof(original::OriginalSmokeParticle);
        return bytes;
    }
    // Identity of every mutable field, independent of padding and allocation.
    // Live online synchronization includes it because particle lifetime now
    // determines future consumption of the driving owner's shared random seed.
    std::uint64_t digest()const{
        std::uint64_t hash=14695981039346656037ull;
        const auto add=[&](std::uint64_t value){for(unsigned i=0;i<8;++i){hash^=(value>>(8*i))&255;hash*=1099511628211ull;}};
        const auto f=[&](float value){add(std::bit_cast<std::uint32_t>(value));};
        add(random_);add(frames_);add(system_.freeParticles());
        for(float value:system_.parameters().values)f(value);add(system_.parameters().alternateMode);
        add(system_.order().size());for(auto index:system_.order())add(index);
        for(auto slot:system_.slots())add(std::uint32_t(slot));for(auto kind:system_.kinds())add(kind);
        for(unsigned i=0;i<original::OriginalCarSmokeSystem::emitterCapacity;++i){
            const auto& cloud=system_.cloud(i);const auto& e=cloud.emitter;
            add(cloud.car);add(cloud.effect);add(cloud.color);f(cloud.gain);
            for(float value:cloud.footprint.elements)f(value);
            for(const auto* values:{&e.spread,&e.position,&e.velocity,&e.previousPosition})for(float value:*values)f(value);
            f(e.scale);f(e.force);f(e.lifetimeScale);add(e.stopped);add(e.frame);add(e.flags);
            add(e.particles.size());for(const auto& p:e.particles){
                for(float value:p.position)f(value);for(float value:p.velocity)f(value);
                f(p.size);add(p.remaining);add(p.flags);
            }
        }
        return hash;
    }
private:
    original::OriginalCarSmokeSystem system_;
    unsigned random_=0x37c778u;
    std::uint64_t frames_=0;
};

// The same ACar matrix and authored wheel origins used in the source display
// owner. Never use render interpolation or network correction offsets here:
// they depend on the observing peer and cannot belong to rollback state.
inline original::OriginalCarSmokeInput makeCarSmokeInput(
        const std::array<std::uint32_t,42>& actor,
        const std::array<std::array<float,3>,4>& origins,const original::OriginalFscaTable& fsca){
    const auto value=[&](unsigned offset){return std::bit_cast<float>(actor.at(offset/4));};
    auto matrix=original::originalActorMatrix({value(0),value(4)-std::bit_cast<float>(0x3ca3d70au),value(8)},
        {value(24),value(28),value(32)},fsca);
    original::rotateOriginalMatrixPhase(matrix,1,0x8000,fsca);
    original::OriginalCarSmokeInput out;out.available=true;
    out.owner=original::originalRoadsideDustOwner(matrix,origins,{value(12),value(16),value(20)});
    for(unsigned i=0;i<4;++i){out.wheelSurfaces[i]=std::uint8_t(actor[29]>>(8*i));out.intensity[i]=std::uint8_t(actor[21]>>(8*i));}
    return out;
}
}
