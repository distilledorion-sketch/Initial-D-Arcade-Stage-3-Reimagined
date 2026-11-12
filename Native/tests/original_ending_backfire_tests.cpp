#include "original_car_lighting_reference.h"
#include "original_backfire.h"
#include <random>
using namespace idas3::reference;
using namespace idas3::original;
namespace {
constexpr unsigned owner=0xd030000,refs=owner+0x100;
constexpr std::array<unsigned,2> children{owner+0x200,owner+0x300};
constexpr std::array<unsigned,2> lights{owner+0x400,owner+0x500};
constexpr std::array<unsigned,2> actors{owner+0x4000,owner+0x8000};
constexpr std::array<unsigned,2> samples{owner+0xc000,owner+0xd000};
constexpr unsigned matrices=owner+0xe000,vtable=owner+0xf000;
unsigned checks=0,cases=0;
void equal(unsigned expected,unsigned actual,const char* why){
    ++checks;if(expected!=actual){std::cerr<<why<<" case="<<cases<<" expected="<<expected<<" actual="<<actual<<'\n';
        throw std::runtime_error("Original ending backfire mismatch");}
}
}
int main(int argc,char**argv)try{
    if(argc!=3)throw std::runtime_error("Canonical image and FSCA table required");
    CarLightReference r(argv[1],argv[2]);
    const auto trig=OriginalFscaTable::load(argv[2]);
    std::array<OriginalMatrix,2> world{originalIdentityMatrix(),originalIdentityMatrix()};
    const std::array<float,16> identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    for(unsigned i=0;i<2;++i){
        r.c.r[4]=lights[i];r.c.r[5]=refs+32;r.c.setFloat(4,.3f);
        r.c.setFloat(5,0);r.c.setFloat(6,0);r.c.setFloat(7,0);r.run(0xc054660);
        r.m.write32(owner+24+4*i,children[i]);r.m.write32(children[i]+40,lights[i]);
        r.m.write32(owner+40+4*i,refs+4*i);r.m.write32(refs+4*i,actors[i]);
        r.m.write32(actors[i]+0x93c+32,samples[i]);
        r.m.write32(actors[i]+0x964+64,vtable);
        for(unsigned n=0;n<16;++n)r.m.writeFloat(matrices+64*i+4*n,identity[n]);
    }
    r.m.write16(vtable+24,0);r.m.write32(vtable+28,0xcffff00);
    //Explicit boundaries: render state, light-list preparation, model lookup,
    //GPU submission, sound dispatch and positive integer division. RNG and
    //all effect state run unhooked; division preserves the source r0 value.
    r.c.callHooks[0xc1fa9e0]=[](auto&){};
    r.c.callHooks[0xc053480]=[](auto&){};
    r.c.callHooks[0xc2223b8]=[&](auto& c){
        equal(5,c.r[5],"Source probability divisor");
        if(c.r[4]>32767)throw std::runtime_error("Source random outside 15-bit range");
        c.fpul=c.r[4]/c.r[5];
    };
    std::array<int,2> presented{-1,-1};unsigned currentSlot=0,sounds=0;
    r.c.callHooks[0xc05a8e0]=[&](auto& c){
        equal(children[currentSlot],c.r[4],"Source model identity");
        presented[currentSlot]=int(c.r[5]);c.r[0]=0;
    };
    r.c.callHooks[0xc1d7120]=[&](auto& c){
        const auto expected=originalBackfireDrawMatrix(world[currentSlot],
            r.m.read32(children[currentSlot]+12),r.m.readFloat(children[currentSlot]+16),trig);
        for(unsigned i=0;i<16;++i)equal(std::bit_cast<unsigned>(expected.elements[i]),c.xf[i],"Submitted flame transform");
    };
    r.c.callHooks[0xc141fc0]=[&](auto& c){equal(7,c.r[4],"Source sound command");equal(1,c.r[5],"Source sound argument");++sounds;};
    //Use the real child's +0 model pointer to identify submissions while its
    //full update executes. The matrix getter establishes draw order.
    r.c.callHooks[0xcffff00]=[&](auto& c){
        currentSlot=c.r[4]==actors[0]+0x964?0:1;
        equal(actors[currentSlot]+0x964,c.r[4],"Actor matrix receiver");c.r[0]=matrices+64*currentSlot;
    };
    std::mt19937 random(0x17c5e0);unsigned flashes=0,heldRequests=0;
    for(unsigned enabledMask=0;enabledMask<4;++enabledMask){
        OriginalBackfireFields host;std::uint32_t seed=0xa1b2c3d4;
        const std::array<bool,2> enabled{bool(enabledMask&1),bool(enabledMask&2)};
        r.m.write32(0xc37c778,seed);r.m.write32(owner+36,0);
        for(unsigned i=0;i<2;++i){
            r.m.write8(owner+32+i,enabled[i]);r.m.write8(children[i]+4,0);
            r.m.write32(children[i],children[i]);r.m.write32(children[i]+8,0);r.m.writeFloat(children[i]+16,0);
        }
        for(unsigned tick=0;tick<1200;++tick){
            //Exercise primary/fallback actor selection and unrelated flag
            //bits as well as long held/released trigger sequences.
            std::array<std::uint32_t,2> flags;
            for(unsigned i=0;i<2;++i){
                flags[i]=tick<256?tick:(tick<512?2u:tick<768?0u:random());
                r.m.write32(samples[i]+92,flags[i]);
                const bool overrideSample=(tick+i)%3==0;
                r.m.write32(actors[i]+0x93c+36,overrideSample?samples[i]:0);
                r.m.write32(actors[i]+0x93c+32,overrideSample?0:samples[i]);
                r.m.write32(children[i]+12,(tick+i)%3);
                //Translate and rotate the actual supplied world transform;
                //the draw must not reconstruct a rounded host car pose.
                world[i]=originalIdentityMatrix();
                translateOriginalMatrix(world[i],{float(tick)*.123f,float(i)*3.7f,float(tick)*-.043f});
                rotateOriginalMatrixPhase(world[i],1,std::uint16_t(tick*51+i*16000),trig);
                rotateOriginalMatrixPhase(world[i],0,std::uint16_t(tick*3),trig);
                for(unsigned n=0;n<16;++n)r.m.writeFloat(matrices+64*i+4*n,world[i].elements[n]);
            }
            presented={-1,-1};sounds=0;
            const bool wasActive=host.cars[0].active;
            host.advance(enabled,flags,seed);
            for(unsigned n=0;n<16;++n)r.c.xf[n]=std::bit_cast<unsigned>(identity[n]);
            r.c.r[4]=owner;r.c.r[15]=CarLightReference::stack;r.run(0xc17c5e0);
            equal(seed,r.m.read32(0xc37c778),"Shared RNG after both cars");
            equal(host.updates,r.m.read32(owner+36),"Owner frame counter");
            equal(host.primarySoundRequest,sounds,"Primary-only sound request");
            heldRequests+=wasActive&&host.primarySoundRequest;
            for(unsigned i=0;i<2;++i){
                equal(host.cars[i].active,r.m.read8(children[i]+4),"Two-frame active lifetime");
                equal(host.cars[i].frame,r.m.read32(children[i]+8),"Animation cursor");
                equal(std::bit_cast<unsigned>(host.cars[i].variation),r.m.read32(children[i]+16),"Random flame variation");
                equal(unsigned(host.cars[i].presentedFrame),unsigned(presented[i]),"Submitted original model frame");
                flashes+=presented[i]>=0;
            }
            ++cases;
        }
    }
    if(!flashes||!heldRequests)throw std::runtime_error("Insufficient backfire coverage");
    std::cout<<"PASS "<<cases<<" source-owner frames / "<<checks<<" checks / "<<r.instructions
        <<" original instructions / "<<flashes<<" flame submissions / "<<heldRequests<<" already-active requests. "
        <<"Full17C5E0 trigger,17CAC0 admission and17CB00 placement/light/variation/lifetime plus original RNG execute. "
        <<"Rendering, light-list preparation, matrix getters, model lookup, sound dispatch and integer division are explicit boundaries.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
