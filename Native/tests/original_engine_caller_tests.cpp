#include "original_engine_playback.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <vector>
using namespace idas3;
using namespace idas3::original;
using namespace idas3::reference;
namespace {
std::size_t checks=0,instructions=0;
void equal(std::uint32_t a,std::uint32_t b,const char* what){++checks;if(a!=b)throw std::runtime_error(std::string(what)+": "+hex(a)+" != "+hex(b));}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("canonical-image required");
    RefMemory memory(argv[1]);constexpr auto stack=0x0d020000u,stop=0x0f000000u,profile=0x0c31c99cu;
    std::size_t initializations=0,updates=0;
    // The actual ARace sound initializer chooses the one player engine.
    // Vary the visible rival, enemy, battle mode and rival pose independently;
    // they must not create a second owner or select a rival engine family.
    for(unsigned car=0;car<35;++car)for(unsigned level:{0u,3u,5u})for(unsigned opponent=0;opponent<35;++opponent){
        OriginalBattleProfile p=makeOriginalFreshBattleProfile();p.setu(16,car);p.setByte(164,std::uint8_t(level));
        p.setByte(152,std::uint8_t(opponent%4));p.setByte(162,std::uint8_t(opponent&1));
        p.setu(0,opponent%3);p.setu(20,opponent);p.setu(24,(opponent*7)%31);p.setu(148,opponent%16);
        p.setu(4,opponent%9);p.setu(8,opponent&1);p.setu(12,(opponent>>1)&1);p.setu(32,(opponent>>2)&1);
        memory.clear();memory.zeroRegion(stack,0x10000);
        for(unsigned i=0;i<p.words.size();++i)memory.write32(profile+i*4,p.words[i]);
        memory.write32(0x0c37c778,12345+opponent);
        for(unsigned i=0;i<42;++i)memory.writeFloat(0x0c8ff430+i*4,float(opponent*100+i));
        RefCpu cpu(memory);cpu.r[15]=stack+0xf000;cpu.pr=stop;
        const auto selected=selectOriginalEngineSound(p);std::vector<unsigned> calls;
        cpu.callHooks[0x0c1416a0]=[&](auto& c){calls.push_back(0x1416a0);equal(c.r[4],4,"race sound set");};
        cpu.callHooks[0x0c142580]=[&](auto& c){calls.push_back(0x142580);equal(c.r[4],selected.family,"player family");equal(c.r[5],unsigned(selected.level),"player sound tuning");};
        instructions+=cpu.run(0x0c067a00,stop,200);
        if(calls!=std::vector<unsigned>{0x1416a0,0x142580})throw std::runtime_error("Race sound initialization order/count changed");
        ++checks;equal(memory.read32(0x0c37c778),12345+opponent,"initializer RNG unchanged");
        for(unsigned i=0;i<p.words.size();++i)equal(memory.read32(profile+i*4),p.words[i],"initializer profile unchanged");
        ++initializations;
    }
    constexpr auto owner=0x0d010000u;
    for(unsigned opponent=0;opponent<35;++opponent)for(unsigned disabled:{0u,1u})for(unsigned overrideVolume:{0u,1u}){
        memory.clear();memory.zeroRegion(stack,0x10000);memory.zeroRegion(owner,128);
        memory.write32(0x0c31de44,owner);memory.write8(0x0c31de55,std::uint8_t(disabled));
        memory.write8(0x0c31de54,std::uint8_t(overrideVolume));memory.writeFloat(0x0c31de50,.375f);
        memory.write32(profile+20,opponent);memory.write32(profile+24,(opponent*7)%31);
        memory.writeFloat(0x0c8ff430,float(opponent*100));memory.write32(0x0c37c778,9981);
        const float rpm=1000.f+float(opponent)*100.f,throttle=float(opponent)/34.f;
        RefCpu cpu(memory);cpu.r[15]=stack+0xf000;cpu.pr=stop;cpu.r[4]=64;cpu.r[5]=3;
        cpu.setFloat(4,rpm);cpu.setFloat(5,.75f);cpu.setFloat(6,throttle);unsigned calls=0;
        cpu.callHooks[0x0c0c4360]=[&](auto& c){
            ++calls;equal(c.r[4],owner,"player controller pointer");equal(c.r[5],3,"player gear argument");
            equal(c.fr[4],std::bit_cast<unsigned>(rpm),"player RPM argument");equal(c.fr[5],std::bit_cast<unsigned>(throttle),"player throttle argument");
            equal(memory.read32(owner),64,"player channel");equal(memory.read32(owner+16),std::bit_cast<unsigned>(overrideVolume?.375f:.75f),"source owner volume override");
        };
        instructions+=cpu.run(0x0c142860,stop,200);equal(calls,disabled?0u:1u,"single gated player update");
        equal(memory.read32(0x0c37c778),9981,"wrapper RNG unchanged");++updates;
    }
    std::cout<<"PASS "<<initializations<<" original ARace initializations with independently varied rival/profile/pose inputs; "<<updates<<" original142860/142800 gated player updates; "<<checks<<" exact checks, "<<instructions<<" original instructions. Hooks: scene4 bank loading,142580 allocation,0C4360 controller boundary. This establishes the traced player call contract, not a universal proof that no other game path can emit opponent-related SFX.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
