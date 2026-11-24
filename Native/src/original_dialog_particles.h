#pragma once
#include <array>
#include <bit>
#include <cstdint>

namespace idas3::original {
// Native translation of original AS3 0FA540/0FB240/0FB360/0FB400/0FB540.
// Shared scene records A9B564 (ten 16-byte records) and31AC70 (200 12-byte
// records). Every normal0F8780/Bunta0F88C0 configuration reinitializes both.
// The host keeps the prepared draw snapshot separate from the next snow
// state because original0FB540 draws each particle before advancing it.
struct OriginalDialogParticles {
    struct Spectator {float x=0,y=0,z=0;std::uint32_t angle=0;};
    struct Snow {float x=0,y=0,z=0;};
    std::array<Spectator,10> spectators{};
    std::array<Snow,200> snow{},drawnSnow{};
    bool initialized=false,showSpectators=false,showSnow=false;
    std::uint32_t acceptedDraws=0;
};
namespace dialog_particles_detail {
inline float word(std::uint32_t value){return std::bit_cast<float>(value);}
inline std::uint32_t random(std::uint32_t& seed){seed=seed*0x41c64e6du+12345u;return(seed>>16)&32767u;}
inline float randomFloat(std::uint32_t& seed){return float(random(seed))/32768.f;}
}
inline void resetOriginalDialogParticles(OriginalDialogParticles& s,std::uint32_t& seed){
    using namespace dialog_particles_detail;
    s={};
    //0FA540, before0FB360 regardless of course or current picture.
    for(auto& p:s.spectators){p.x=randomFloat(seed)-.5f;p.y=randomFloat(seed)-.5f;p.z=word(0x391d4952);p.angle=random(seed);}
    for(unsigned i=0;i<s.snow.size();++i){auto& p=s.snow[i];
        p.x=(randomFloat(seed)-.5f)*15.f;p.y=(randomFloat(seed)-.5f)*10.f;
        p.z=word((i&1)?0x39ebedfa:0x3a03126f);
    }
    s.initialized=true;
}
inline void advanceOriginalDialogSnowParticle(OriginalDialogParticles::Snow& p,unsigned index,std::uint32_t& seed){
    using namespace dialog_particles_detail;
    //0FB400 constructs these two literal tables, with an explicit zero and
    //the repeated .027 word, then indexes them by original integer remainder.
    constexpr std::array<std::uint32_t,5> dx{0xbc1d4952,0xbbb78034,0,0x3bb78034,0x3c1d4952};
    constexpr std::array<std::uint32_t,6> dy{0x3c8b4396,0x3c03126f,0x3ba3d70a,0x3cdd2f1b,0x3c23d70a,0x3cdd2f1b};
    p.x+=word(dx[index%dx.size()]);p.y-=word(dy[index%dy.size()]);
    if(p.y<-4.5f){p.x=(randomFloat(seed)-.5f)*15.f;p.y=2.5f;}
}
// One accepted source Draw. No host repaint, paused frame, or invisible
// scene may invoke this. Caller determines that the scene actually draws;
// inside0FB0C0 the effect gate uses profile+4 and selected element+24.
inline void prepareOriginalDialogParticles(OriginalDialogParticles& s,unsigned profileCourse,
    std::uint32_t selectedElementSpectators,std::uint32_t& seed){
    if(!s.initialized)return;
    using namespace dialog_particles_detail;
    s.showSnow=profileCourse==8;
    s.showSpectators=!s.showSnow&&std::bit_cast<std::int32_t>(selectedElementSpectators)>0;
    if(s.showSnow){
        s.drawnSnow=s.snow;
        for(unsigned i=0;i<s.snow.size();++i)advanceOriginalDialogSnowParticle(s.snow[i],i,seed);
    }else if(s.showSpectators){
        for(auto& p:s.spectators){p.x=(randomFloat(seed)-.5f)*15.f;p.y=(randomFloat(seed)-.5f)*10.f;p.z=word(0x391d4952);p.angle=random(seed);}
    }
    ++s.acceptedDraws;
}
}
