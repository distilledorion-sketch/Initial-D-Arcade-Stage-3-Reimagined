#include "sh4_scalar_reference.h"
#include <iostream>
#include <vector>
using namespace idas3::reference;
namespace {
std::size_t checks=0,instructions=0;
void eq(std::uint32_t a,std::uint32_t b,const char* label){++checks;if(a!=b)throw std::runtime_error(std::string(label)+": "+hex(a)+" != "+hex(b));}
}
int main(int argc,char**argv)try{
    if(argc!=2)throw std::invalid_argument("canonical image required");
    RefMemory m(argv[1]);constexpr auto stack=0x0d030000u,stop=0x0f000000u,owner=0x0d000000u,child=0x0d001000u,vt=0x0d002000u;
    m.zeroRegion(stack,0x10000);m.zeroRegion(owner,0x3000);m.write32(child+20,240);m.write32(child+24,owner);m.write32(owner+12,vt);
    m.write16(vt+56,0);m.write32(vt+60,0x0f001000);std::vector<unsigned> beeps,advance,services;
    for(unsigned frame=1;frame<=240;++frame){
        RefCpu c(m);c.r[15]=stack+0xf000;c.r[4]=child;c.pr=stop;unsigned digitCalls=0;
        c.callHooks[0x0c2223b8]=[](auto& cpu){cpu.fpul=std::uint32_t(std::int32_t(cpu.r[4])/std::int32_t(cpu.r[5]));};
        c.callHooks[0x0c068500]=[&](auto& cpu){eq(cpu.r[4],owner,"countdown HUD owner");eq(cpu.r[5],(240-frame)/60,"countdown digit");++digitCalls;};
        c.callHooks[0x0c1420c0]=[&](auto& cpu){eq(cpu.r[4],2,"countdown cue");eq(cpu.r[5],1,"cue argument");beeps.push_back(frame);};
        c.callHooks[0x0c16d500]=[&](auto& cpu){eq(cpu.r[4],1,"countdown service");services.push_back(frame);};
        c.callHooks[0x0f001000]=[&](auto& cpu){eq(cpu.r[4],owner,"advance owner");eq(cpu.r[5],0xffffffff,"advance argument");advance.push_back(frame);};
        instructions+=c.run(0x0c05ae20,stop,1000);
        eq(m.read32(child+20),240-frame,"countdown remaining");eq(m.read8(child+16),frame==240,"countdown completion");eq(digitCalls,frame<240,"countdown HUD count");
    }
    if(beeps!=std::vector<unsigned>{1,61,121,181}||advance!=std::vector<unsigned>{180}||services!=std::vector<unsigned>{170})throw std::runtime_error("Original countdown event epochs differ");checks+=3;
    // GO constructor, including the real numeric-mode predicate06BCE0.
    constexpr auto playerCar=0x0d004000u,rivalCar=0x0d005000u,player=0x0d006000u,rival=0x0d006400u;
    for(unsigned mode:{0u,2u,3u})for(unsigned initial:{0u,0x2000u,0x4000u,0xc123u}){
        m.clear();m.zeroRegion(stack,0x10000);m.zeroRegion(owner,0x7000);m.write32(child+76,owner);m.write32(owner+1640,mode);
        m.write32(owner+1048,playerCar);m.write32(owner+1052,rivalCar);m.write32(playerCar+2396,player);m.write32(rivalCar+2396,rival);
        m.write32(player+80,initial);m.write32(rival+80,initial);RefCpu c(m);c.r[15]=stack+0xf000;c.r[4]=child;c.pr=stop;unsigned cues=0;
        for(auto address:{0x0c221fc0u,0x0c05a300u,0x0c05a600u})c.callHooks[address]=[](auto& cpu){cpu.r[0]=0x0d007000;};
        c.callHooks[0x0c1420c0]=[&](auto& cpu){eq(cpu.r[4],3,"GO cue");eq(cpu.r[5],1,"GO cue argument");eq(m.read32(player+80),initial|0x8000,"player enabled before GO cue");eq(m.read32(rival+80),mode==0?(initial|0x8000):initial,"rival GO gate");++cues;};
        c.callHooks[0x0c16db80]=[](auto& cpu){eq(cpu.r[4],13,"GO service");};
        instructions+=c.run(0x0c05bee0,stop,1000);eq(cues,1,"single GO cue");eq(m.read32(child+80),0,"GO elapsed reset");eq(m.read32(child+88),0,"GO local reset");
    }
    // The original solver dispatcher receives a measured battle gap only in
    // mode0; solo modes2/3 receive exact +0. This is a caller-boundary test.
    for(unsigned mode:{0u,2u,3u,4u,5u})for(float gap:{-123.5f,0.f,48.25f}){
        m.clear();m.zeroRegion(stack,0x10000);m.zeroRegion(owner,0x2000);m.write32(owner+1640,mode);RefCpu c(m);c.r[15]=stack+0xf000;c.r[4]=owner;c.pr=stop;unsigned calls=0,metrics=0;
        c.callHooks[0x0c06a420]=[&](auto& cpu){eq(cpu.r[4],owner,"gap owner");cpu.setFloat(0,gap);++metrics;};
        c.callHooks[0x0c159920]=[&](auto& cpu){eq(cpu.fr[4],std::bit_cast<unsigned>(mode==0?gap:0.f),"solver progress correction");++calls;};
        instructions+=c.run(0x0c062de0,stop,1000);eq(calls,mode==0||mode==2||mode==3,"solver dispatch count");eq(metrics,mode==0,"measured gap count");
    }
    for(unsigned flags:{0u,0x2000u,0x4000u,0x8000u,0xffffffffu})for(unsigned gear=0;gear<7;++gear){
        m.clear();m.write32(0x0c900954,player);m.write32(player+80,flags);m.write32(0x0c900e88,gear);RefCpu c(m);
        instructions+=c.run(0x0c15e6cc,0x0c15e6e2,30);eq(m.read32(0x0c900e88),flags&0x8000?gear:0,"pre-GO neutral gear gate");
    }
    std::cout<<"PASS "<<checks<<" exact countdown/GO/solver-caller/neutral-gate checks, "<<instructions<<" original instructions. Explicit hooks: integer division, HUD, cues, owner transition, platform services, measured-gap provider and solver body. Outer owner, input polling and warmup scheduling remain separate integration boundaries.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
