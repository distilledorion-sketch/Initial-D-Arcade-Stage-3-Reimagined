#include "original_race_start.h"
#include "original_countdown_presentation.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <vector>
using namespace idas3::original;
using namespace idas3::reference;
namespace {
std::size_t checks{},instructions{};
void eq(std::uint32_t a,std::uint32_t b,const char* name){++checks;if(a!=b)throw std::runtime_error(std::string(name)+": "+hex(a)+" != "+hex(b));}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Usage: original_showcase_handoff_reference_tests canonical_image");
    RefMemory m(argv[1]);
    constexpr auto owner=0x0d000000u,aux=0x0d001000u,auxList=0x0d001100u,auxSentinel=0x0d001200u;
    constexpr auto pre=0x0d002000u,go=0x0d002100u,sentinel=0x0d002200u,vt=0x0d003000u;
    constexpr auto playerCar=0x0d004000u,rivalCar=0x0d005000u,player=0x0d006000u,rival=0x0d006400u;
    constexpr auto standby=0x0d008000u,camera=0x0d009000u,hud=0x0d00a000u,add=0x0f001200u;
    constexpr auto stack=0x0d030000u,noop=0x0f001000u,remove=0x0f001100u;
    for(auto mode:{0u,2u}){
        m.clear();m.zeroRegion(owner,0x20000);m.zeroRegion(stack-0x1000,0x11000);
        // Execute the actual ARace constructor gate write. Its allocation
        // is an explicit leaf; the gate is not seeded to the expected value.
        m.write32(0x0c2f4bc8,0);
        RefCpu constructor(m);constructor.r[15]=stack+0xf000;
        constructor.callHooks[0x0c021ee0]=[&](auto& cpu){eq(cpu.r[4],0x1a8,"constructor allocation boundary");cpu.r[0]=standby;};
        instructions+=constructor.run(0x0c05dbb6,0x0c05dbc2,32);
        eq(m.read32(0x0c2f4bc8),1,"constructor suppresses raw throttle before warmup");
        m.write8(0x0c92ece0,0);m.write32(0x0c31ce18+32,0);m.write32(0x0c31cdd8+52,0xffffffff);
        m.write32(owner+12,0x0c383c74); // actual ARace owner vtable
        m.write32(owner+32,sentinel);m.write32(owner+36,standby);m.write32(owner+68,standby);
        m.write32(standby+4,pre);m.write32(standby+12,0x0c38350c);m.write32(standby+16,1);m.write32(standby+0x1a4,owner);
        m.write32(owner+0x414,camera);m.write32(camera,vt+256);m.write32(vt+256+84,noop);
        m.write32(owner+0x560,hud);m.write32(hud,vt+384);m.write32(vt+384+20,noop);m.write32(vt+384+28,noop);
        m.write32(pre+4,go);m.write32(pre+12,0x0c383464);m.write32(pre+16,2);m.write32(pre+76,owner);
        m.write32(go+4,sentinel);m.write32(go+12,vt+128);m.write32(go+16,3);m.write32(go+76,owner);
        m.write32(vt+64+28,noop);m.write32(vt+64+36,noop);
        m.write32(vt+128+20,0x0c05bee0);m.write32(vt+128+28,0x0c05c040);
        m.write32(owner+1600,auxList);m.write32(auxList,auxSentinel);m.write32(auxList+4,auxSentinel);
        m.write32(auxList+32,vt+192);m.write32(vt+192+52,remove);m.write32(vt+192+28,add);
        m.write32(aux+4,auxSentinel);m.write32(aux+12,vt);m.write32(vt+20,0x0c05ae20);
        m.write32(aux+20,0);m.write32(aux+24,owner);
        m.write32(owner+1640,mode);m.write32(owner+1048,playerCar);m.write32(owner+1052,rivalCar);
        m.write32(playerCar+2396,player);m.write32(rivalCar+2396,rival);
        OriginalRaceStart native;native.reset(mode);unsigned creations=0,solvers=0,firstGo=0;
        for(unsigned frame=1;frame<=440;++frame){
            RefCpu c(m);c.r[15]=stack+0xf000;c.r[12]=owner;c.r[13]=0x0c156700;
            std::vector<unsigned> cues;std::vector<std::string> order;int digit=-1;unsigned service=0;
            c.callHooks[noop]=[](auto&){};
            for(auto address:{0x0c1d0940u,0x0c14b6c0u,0x0c16d440u,0x0c055d60u,0x0c222372u,0x0c1d0880u,0x0c069ec0u,0x0c17dfe0u,0x0c0a9da0u,0x0c021140u,0x0c064680u,0x0c0647a0u})c.callHooks[address]=[](auto&){};
            c.callHooks[0x0c021ee0]=[&](auto& cpu){eq(cpu.r[4],28,"countdown allocation size");cpu.r[0]=aux;};
            c.callHooks[add]=[&](auto& cpu){eq(cpu.r[5],aux,"insert actual countdown child");eq(m.read32(aux+20),240,"constructor240");eq(frame,242,"creation outer update");m.write32(aux+4,auxSentinel);m.write32(auxList+4,aux);++creations;};
            c.callHooks[remove]=[&](auto& cpu){eq(cpu.r[5],aux,"aux child removed");m.write32(auxList+4,auxSentinel);};
            c.callHooks[0x0c156700]=[](auto&){};
            c.callHooks[0x0c2223b8]=[](auto& cpu){cpu.fpul=std::uint32_t(std::int32_t(cpu.r[4])/std::int32_t(cpu.r[5]));};
            c.callHooks[0x0c068500]=[&](auto& cpu){digit=int(cpu.r[5]);order.push_back("countdown");};
            c.callHooks[0x0c1420c0]=[&](auto& cpu){eq(cpu.r[5],1,"cue argument");cues.push_back(cpu.r[4]);};
            c.callHooks[0x0c16d500]=[&](auto& cpu){eq(cpu.r[4],1,"source service");++service;};
            c.callHooks[0x0c0654e0]=[&](auto&){order.push_back("HUD");};
            for(auto a:{0x0c221fc0u,0x0c05a300u,0x0c05a600u})c.callHooks[a]=[](auto& cpu){cpu.r[0]=0x0d007000;};
            c.callHooks[0x0c16db80]=[](auto& cpu){eq(cpu.r[4],13,"GO service13");};
            c.callHooks[0x0c024100]=[](auto&){}; // scene rendering, explicit boundary
            for(auto a:{0x0c069c40u,0x0c0683c0u,0x0c067000u})c.callHooks[a]=[](auto&){};
            c.callHooks[0x0c16d1e0]=[](auto& cpu){cpu.r[0]=0;};
            for(auto [a,label]:std::vector<std::pair<unsigned,const char*>>{{0x0c0680c0,"path"},{0x0c067a80,"gates"},{0x0c068a60,"outcome"},{0x0c067d00,"timers"}})
                c.callHooks[a]=[&,label](auto&){order.push_back(label);};
            c.callHooks[0x0c06a420]=[](auto& cpu){cpu.setFloat(0,0.f);};
            c.callHooks[0x0c159920]=[&](auto&){order.push_back("solver");
                eq(m.read32(0x0c2f4bc8),frame<=20?1u:0u,"actual StandBy gate before solver; old counter20 releases on21");
                eq(bool(m.read32(player+80)&0x8000),frame>=422,"start flag before solver");++solvers;if(!firstGo&&(m.read32(player+80)&0x8000))firstGo=frame;};
            c.callHooks[0x0c063ce0]=[&](auto&){order.push_back("publish");};
            // The real outer span executes real auxiliary-list iteration,
            //0759C0 phase advance,075D60/023480 dispatch, GO Init/Main and
            //062DE0 numeric-mode selection. No hand-authored scheduler hook.
            instructions+=c.run(0x0c05fe6e,0x0c05ffba,12000);
            eq(solvers,frame,"exactly one solver per outer update");
            if(frame<=241){eq(creations,0,"no countdown before transition");eq(m.read32(standby+0x13c),frame,"real standby counter");}
            if(frame==241)eq(m.read32(owner+68),pre,"transition selects pre without initialization");
            if(frame==242){eq(creations,1,"countdown inserted after auxiliary pass");eq(m.read32(aux+20),240,"creation update does not decrement");eq(std::uint32_t(digit),0xffffffffu,"no countdown HUD on creation update");}
            if(frame>=243){const auto f=native.step();eq(f.countdownRemaining,m.read32(aux+20),"remaining after actual creation update");eq(f.gearEnabled,frame>=422,"native isolated countdown agrees after handoff");}
            eq(bool(m.read32(player+80)&0x8000),frame>=422,"first GO including source handoff");
        }
        eq(firstGo,422,"source first GO outer update");eq(creations,1,"one auxiliary allocation");
    }

    std::cout<<"PASS "<<checks<<" source StandBy-to-PreInit/countdown/GO checks, "<<instructions<<" original instructions. One solver per outer update; countdown creation update has no decrement.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
