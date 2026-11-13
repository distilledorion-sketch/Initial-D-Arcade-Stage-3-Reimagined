#include "original_smoke_particle.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3::original;
using namespace idas3::reference;
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Canonical image required");
    RefMemory memory(argv[1]);
    constexpr unsigned particle=0xc800000,parent=0xc810000,stack=0xc820000,stop=0xcfffffc;
    std::uint64_t checks=0,cases=0,instructions=0;unsigned seed=0x73a61681;
    const auto randomWord=[&](){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;};
    const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);};
    const auto equal=[&](unsigned a,unsigned b,const char* why){++checks;if(a!=b){std::cerr<<why<<" expected="<<std::hex<<a<<" actual="<<b<<std::dec<<'\n';throw std::runtime_error("Original smoke particle mismatch");}};
    const auto load=[&](const OriginalSmokeParticle& p){
        memory.clear();memory.zeroRegion(particle,60);memory.zeroRegion(parent,64);memory.zeroRegion(stack,4096);
        memory.write32(particle+20,parent);
        for(unsigned i=0;i<3;++i){memory.writeFloat(particle+24+i*4,p.position[i]);memory.writeFloat(particle+36+i*4,p.velocity[i]);}
        memory.writeFloat(particle+48,p.size);memory.write32(particle+52,p.remaining);memory.write32(particle+56,p.flags);
    };
    const auto compare=[&](const OriginalSmokeParticle& p){
        for(unsigned i=0;i<3;++i){equal(memory.read32(particle+24+i*4),std::bit_cast<unsigned>(p.position[i]),"position");equal(memory.read32(particle+36+i*4),std::bit_cast<unsigned>(p.velocity[i]),"velocity");}
        equal(memory.read32(particle+48),std::bit_cast<unsigned>(p.size),"size");equal(memory.read32(particle+52),p.remaining,"remaining");equal(memory.read32(particle+56),p.flags,"flags");
        equal(memory.read32(particle+20),parent,"parent pointer");
    };
    for(unsigned table=0;table<3;++table){
        const auto params=originalSmokeParameters(table==2?0:table,table==2);
        for(unsigned i=0;i<9;++i)equal(memory.read32(0xc31dd30+table*36+i*4),std::bit_cast<unsigned>(params.values[i]),"authored parameter table");
    }
    // Constructor masks, source argument layout and unrelated flag bits.
    for(unsigned kind=0;kind<32;++kind)for(unsigned phase=0;phase<32;++phase)for(unsigned trial=0;trial<4;++trial){
        OriginalSmokeParticle p;p.flags=randomWord();load(p);
        const std::array<float,3> pos{float(trial)*123.25f,-19.5f,98.25f},vel{-.125f,1.25f,4.5f};
        const float size=.2f;const unsigned life=randomWord();
        RefCpu cpu(memory);cpu.r[4]=particle;cpu.r[5]=std::bit_cast<unsigned>(pos[0]);cpu.r[6]=std::bit_cast<unsigned>(pos[1]);cpu.r[7]=std::bit_cast<unsigned>(pos[2]);cpu.setFloat(4,size);cpu.r[15]=stack+2048;cpu.pr=stop;
        for(unsigned i=0;i<3;++i)memory.writeFloat(cpu.r[15]+i*4,vel[i]);
        memory.write32(cpu.r[15]+12,life);memory.write32(cpu.r[15]+16,kind);memory.write32(cpu.r[15]+20,phase);memory.write32(cpu.r[15]+24,parent);
        instructions+=cpu.run(0xc13d780,stop,120);initializeOriginalSmokeParticle(p,pos,vel,size,life,kind,phase);compare(p);++cases;
    }
    // Execute the actual motion/size function and both force helpers. Only
    // the uniform random-value provider is an explicit dependency hook.
    for(unsigned table=0;table<3;++table)for(unsigned mode=0;mode<3;++mode)
    for(unsigned phase=0;phase<16;++phase)for(unsigned life:{0u,1u,2u,9u,0x80000000u,0xffffffffu})
    for(float size:{-.1f,0.f,.999f,1.f,1.999f,2.f,2.1f})for(unsigned trial=0;trial<4;++trial){
        OriginalSmokeParticle p;p.flags=(randomWord()&0xffffff0fu)|(phase<<4);p.remaining=life;p.size=size;
        for(unsigned i=0;i<3;++i){p.position[i]=float(int(randomWord()%80001)-40000)*.125f;p.velocity[i]=float(int(randomWord()%20001)-10000)/511.f;}
        const float scale=float(trial)*.13f,force=trial?float(trial)*21.5f:0.f;
        std::array<float,3> draws{0.f,.5f,1.f};if(trial&1)for(auto& d:draws)d=float(randomWord()%65537)/65536.f;
        auto params=originalSmokeParameters(table==2?0:table,table==2);params.alternateMode=mode;load(p);
        for(unsigned i=0;i<9;++i)memory.writeFloat(0xca9badc+i*4,params.values[i]);
        memory.write32(0xca9bfb0,mode);memory.writeFloat(parent+28,scale);memory.writeFloat(parent+32,force);
        unsigned sourceCalls=0,nativeCalls=0;
        RefCpu cpu(memory);cpu.r[4]=particle;cpu.r[15]=stack+2048;cpu.pr=stop;
        cpu.callHooks[0xc1f9ea0]=[&](RefCpu& c){check(sourceCalls<3,"Too many source random values");c.setFloat(0,draws[sourceCalls++]);};
        instructions+=cpu.run(0xc13d800,stop,500);
        advanceOriginalSmokeParticle(p,params,scale,force,[&]{check(nativeCalls<3,"Too many native random values");return draws[nativeCalls++];});
        compare(p);equal(sourceCalls,nativeCalls,"random consumption");++cases;
    }
    std::cout<<"PASS "<<cases<<" smoke particle cases, "<<checks<<" bitwise checks, "<<instructions<<" original instructions; constructor flag masks, all phases, signed lifetime boundaries, three source parameter tables, three alternate-mode values, scale limits/overshoot, motion and random consumption. RNG values are the only hook. Emission scheduling and rendering are not covered.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
