#include "original_actor_light_feedback.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <random>
using namespace idas3::original;
using namespace idas3::reference;
int main(int argc,char** argv)try{
    if(argc!=2)throw std::runtime_error("Canonical image required");
    RefMemory memory(argv[1]);
    constexpr unsigned car=0x0d000000,actor=0x0d010000,other=0x0d020000,done=0x0f000000;
    std::mt19937 rng(0x35220);std::size_t cases=0,checks=0,instructions=0;
    const auto check=[&](bool good,const char* what){++checks;if(!good)throw std::runtime_error(what);};
    const auto run=[&](unsigned flags,unsigned byte81){
        memory.clear();memory.zeroRegion(car,0x30000);
        std::array<unsigned,42> before;
        for(unsigned i=0;i<before.size();++i){before[i]=rng();memory.write32(actor+i*4,before[i]);}
        before[20]=flags;memory.write32(actor+0x50,flags);
        memory.write32(car+0x95c,actor);memory.write32(car+0x960,other);
        memory.write32(other+0x50,0x53b11604);memory.write8(car+81,std::uint8_t(byte81));
        RefCpu source(memory);source.r[4]=car;source.pr=done;
        instructions+=source.run(0x0c035220,done,100);
        const auto expected=originalActorLightFeedback(flags,byte81!=0);
        for(unsigned i=0;i<before.size();++i)
            check(memory.read32(actor+i*4)==(i==20?expected:before[i]),"Source035220 actor write set differs");
        check((expected&~0x10000u)==(flags&~0x10000u),"Helper altered unrelated actor flags");
        check(memory.read32(other+0x50)==0x53b11604,"Source wrote alternate/published actor instead of live+095C");
        check(memory.read8(car+81)==byte81,"Source feedback changed its lamp gate");
        ++cases;
    };
    for(unsigned value:{0u,1u,0x10000u,0x10001u,0xfffeffffu,0xffffffffu,0xdeadbeefu})
        for(unsigned byte=0;byte<256;++byte)run(value,byte);
    for(unsigned i=0;i<4096;++i)run(rng(),i&255);
    std::cout<<"PASS "<<cases<<" cases / "<<checks<<" checks / "<<instructions
             <<" original instructions; complete035220, no calls or memory hooks.\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
