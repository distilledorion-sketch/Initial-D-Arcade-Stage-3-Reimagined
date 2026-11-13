#include "original_leaf_particle.h"
#include "original_replay_leaf_system.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned particle=0xc900000,owner=particle+0x1000,wakeAddress=particle+0x2000,
    matrixStack=particle+0x10000,stack=particle+0x1f000,stop=0xcfffffc,forceAddress=0xcffff00;
unsigned cases=0;std::uint64_t checks=0,instructions=0;
void equal(unsigned source,unsigned host,const char* why){++checks;if(source!=host){
    std::cerr<<why<<" case="<<cases<<" source="<<std::hex<<source<<" host="<<host<<std::dec<<'\n';
    throw std::runtime_error("Original leaf mismatch");}}
void equal(float source,float host,const char* why){equal(std::bit_cast<unsigned>(source),std::bit_cast<unsigned>(host),why);}
struct Random{unsigned state=0,calls=0;float operator()(){++calls;state=state*1103515245u+12345u;return float((state>>16)&32767u)/32768.f;}};
}
#include "original_replay_leaf_checks.inl"
int main(int argc,char**argv)try{
    if(argc!=3)throw std::runtime_error("Canonical image and FSCA table required");
    RefMemory memory(argv[1]);const auto trig=OriginalFscaTable::load(argv[2]);
    std::ifstream f(argv[2],std::ios::binary);std::vector<unsigned>wave(32768);f.seekg(16);f.read(reinterpret_cast<char*>(wave.data()),131072);
    if(!f)throw std::runtime_error("FSCA table read failed");
    const auto setup=[&](unsigned seed){memory.clear();memory.zeroRegion(particle,0x20000);
        memory.write32(0xc37c778,seed);memory.write32(0xc98ad0c,0x00200000);
        memory.write32(0xc98ad10,matrixStack);memory.write32(0xc98ad14,matrixStack);
    };
    const auto transfer=[&](const OriginalLeafParticle& p,bool load){
        const std::array<const std::array<float,3>*,5> fields{&p.position,&p.velocity,&p.angles,&p.spin,&p.origin};
        for(unsigned field=0;field<fields.size();++field)for(unsigned i=0;i<3;++i){const auto address=particle+20+12*field+4*i;
            if(load)memory.writeFloat(address,(*fields[field])[i]);else equal(memory.readFloat(address),(*fields[field])[i],"Particle field");}
        if(load){memory.write32(particle+80,p.model);memory.write32(particle+84,p.remaining);}
        else{equal(memory.read32(particle+80),p.model,"Model identity");equal(memory.read32(particle+84),p.remaining,"Lifetime");}
    };
    for(unsigned seed:{0u,1u,0x37c778u,0xffffffffu})for(unsigned trial=0;trial<8;++trial){
        setup(seed);const std::array<float,3> pos{float(trial)*17.25f,-2.5f,float(trial)*-.125f},vel{.02f,.01f,-.3f};
        RefCpu c(memory);c.r[4]=owner;c.r[5]=0xc800000+trial*64;c.r[15]=stack;c.pr=stop;
        for(unsigned i=0;i<3;++i){c.setFloat(4+i,pos[i]);c.setFloat(7+i,vel[i]);}
        c.callHooks[0xc1d13e0]=[](RefCpu& cpu){cpu.r[0]=particle;};
        c.callHooks[0xc1d1480]=[](RefCpu& cpu){equal(cpu.r[5],particle,"Registered particle");};
        instructions+=c.run(0xc020940,stop,1000);
        OriginalLeafParticle p;Random random{seed};initializeOriginalLeafParticle(p,pos,vel,0xc800000+trial*64,random);transfer(p,false);
        equal(memory.read32(owner+8),1,"Owner admission count");equal(memory.read32(particle+16),0xc020ca0,"Source update callback");
        equal(memory.read32(0xc37c778),random.state,"Constructor RNG");equal(random.calls,5,"Constructor draws");++cases;
    }
    for(unsigned trial=0;trial<48;++trial){
        setup(1);const std::array<float,3> pos{float(trial)*.75f,-1.25f,17};
        const std::array<float,3> velocity=trial==0?std::array<float,3>{0,0,0}:
            std::array<float,3>{float(int(trial)-24)*.013f,float(trial%3)*-.17f,float(trial%7)*.19f};
        RefCpu c(memory);c.r[4]=owner;c.r[15]=stack;c.pr=stop;
        for(unsigned i=0;i<3;++i){c.setFloat(4+i,pos[i]);c.setFloat(7+i,velocity[i]);}
        instructions+=c.run(0xc020b80,stop,1000);
        const auto wake=originalLeafWake(pos,velocity);
        for(unsigned i=0;i<3;++i){equal(memory.readFloat(owner+12+4*i),wake.origin[i],"Wake origin");equal(memory.readFloat(owner+24+4*i),wake.direction[i],"Wake direction");}
        equal(memory.readFloat(owner+36),wake.strength,"Wake magnitude");++cases;
    }
    for(float axial:{-12.f,-.25f,-.002f,-.001f,0.f,.001f,.002f,.25f,12.f})
    for(float radial:{0.f,.0001f,.05f,.25f,1.f,9.25f,15.f})
    for(float strength:{0.f,.00001f,.01f,.1f,1.f,2.f}){
        setup(1);RefCpu c(memory);c.r[4]=owner;c.r[5]=owner+4;c.r[15]=stack;c.pr=stop;
        c.setFloat(4,axial);c.setFloat(5,radial);c.setFloat(6,strength);
        instructions+=c.run(0xc021080,stop,1000);
        const auto force=originalLeafWakeForce(axial,radial,strength);
        equal(memory.readFloat(owner),force[0],"Axial force");equal(memory.readFloat(owner+4),force[1],"Radial force");++cases;
    }
    // Execute the actual particle, RNG and matrices with both an observed
    // callback boundary and the complete original 021080 wake-force routine.
    for(bool realForce:{false,true})
    for(unsigned seed:{0u,1u,0xdeadbeefu})for(unsigned life:{0u,1u,2u,90u,0x80000000u,0xffffffffu})
    for(unsigned variant=0;variant<12;++variant){
        setup(seed);OriginalLeafParticle p;
        p.position={float(int(variant)-6)*.125f,float(variant&1)*.05f,float(int(variant%3)-1)*.25f};
        p.velocity={-.04f,(variant&2)?-.05f:.025f,.03f};p.origin={0,0,0};
        p.angles={float(variant)*.18f,-float(variant)*.15f,.71f};p.spin={.21f,0,.03f};
        p.model=variant&1?0xc800000:0;p.remaining=life;transfer(p,true);
        auto wake=originalLeafWake({.01f,0,-.03f},variant%3==0?std::array<float,3>{0,0,0}:
            variant%3==1?std::array<float,3>{.2f,-.1f,.7f}:std::array<float,3>{-.35f,.1f,-.5f});
        memory.write32(wakeAddress+4,realForce?0xc021080:forceAddress);
        for(unsigned i=0;i<3;++i){memory.writeFloat(wakeAddress+8+4*i,wake.origin[i]);memory.writeFloat(wakeAddress+20+4*i,wake.direction[i]);}
        memory.writeFloat(wakeAddress+32,wake.strength);
        unsigned sourceForces=0,hostForces=0,removed=0,drawn=0;std::array<float,3> arguments{};
        const std::array<float,2> forces{.01f*float(variant),-.003f*float(variant)};
        RefCpu c(memory);c.fscaHalfWave=wave;c.r[4]=owner;c.r[5]=particle;c.r[6]=wakeAddress;c.r[15]=stack;c.pr=stop;
        c.callHooks[forceAddress]=[&](RefCpu& cpu){++sourceForces;arguments={cpu.getFloat(4),cpu.getFloat(5),cpu.getFloat(6)};
            memory.writeFloat(cpu.r[4],forces[0]);memory.writeFloat(cpu.r[5],forces[1]);};
        c.callHooks[0xc1d1420]=[&](RefCpu& cpu){equal(cpu.r[4],particle,"Unlinked expired particle");++removed;cpu.r[0]=1;};
        c.callHooks[0xc1d7120]=[&](RefCpu& cpu){equal(cpu.r[4],p.model,"Submitted model");++drawn;};
        instructions+=c.run(0xc020ca0,stop,20000);
        Random random{seed};const bool alive=advanceOriginalLeafParticle(p,wake,trig,[&](float axial,float radial,float strength){
            if(realForce)return originalLeafWakeForce(axial,radial,strength);
            ++hostForces;equal(arguments[0],axial,"Axial wake input");equal(arguments[1],radial,"Radial wake input");equal(arguments[2],strength,"Wake strength");return forces;},random);
        transfer(p,false);equal(sourceForces,hostForces,"Wake callback count");equal(removed,unsigned(!alive),"Expiry");
        equal(drawn,unsigned(alive&&p.model!=0),"Visible model count");equal(memory.read32(0xc37c778),random.state,"Motion RNG");
        equal(random.calls,unsigned(alive),"Motion draw count");++cases;
    }
    checkOriginalReplayLeafCaller(memory,trig,wave,std::filesystem::path(argv[2]).parent_path().parent_path().parent_path());
    checkOriginalReplayLeafLifecycle(memory,trig,wave,std::filesystem::path(argv[2]).parent_path().parent_path().parent_path());
    std::cout<<"PASS "<<cases<<" leaf particle/caller cases / "<<checks<<" bitwise checks / "<<instructions<<" original instructions. Initialization, signed lifetime, wake, floor clamp, rotations, RNG, replay admission and full pool lifecycle. The lifecycle oracle hooks only exception TLS, profiling, model lookup and GPU submission; all particle pool operations execute. No gameplay integration.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
