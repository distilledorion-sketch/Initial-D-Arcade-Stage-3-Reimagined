#include "original_battle_profile.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <bit>
using namespace idas3::original;
using namespace idas3::reference;
void require(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("canonical image required");RefMemory m(argv[1]);
    constexpr unsigned base=0x0c31c99c,stack=0x0d100000,stop=0x00ff0000,owner=0x0d200000;
    std::uint64_t instructions=0,checks=0;
    auto put=[&](const OriginalBattleProfile& p){for(unsigned i=0;i<307;++i)m.write32(base+4*i,p.words[i]);};
    auto compare=[&](const OriginalBattleProfile& p,const char* label){for(unsigned i=0;i<307;++i){if(m.read32(base+4*i)!=p.words[i]){std::cerr<<label<<" offset="<<i*4<<" expected="<<m.read32(base+4*i)<<" actual="<<p.words[i]<<'\n';throw std::runtime_error(label);}++checks;}};
    RefCpu fresh(m);fresh.r[15]=stack;fresh.pr=stop;instructions+=fresh.run(0x0c134a60,stop,50000);
    const auto defaults=makeOriginalFreshBattleProfile();compare(defaults,"fresh entire profile");
    static_assert(sizeof(OriginalRivalRecord)==32);
    for(unsigned enemy=0;enemy<31;++enemy){
        const auto raw=std::bit_cast<std::array<unsigned,8>>(originalRival(enemy));
        for(unsigned i=0;i<8;++i){require(raw[i]==m.read32(0x0c31d8a8+enemy*32+i*4),"original roster literal");++checks;}
        for(unsigned mode=0;mode<4;++mode)for(unsigned progression:{0u,1u,15u,16u,17u,127u,128u,255u}){
            auto p=defaults;p.setu(0,mode);p.setu(4,7);p.setu(8,0);p.setu(12,1);p.setu(28,15);p.setu(32,1);
            p.setu(1180,0xa5316201);p.setByte(116+enemy,std::uint8_t(progression));put(p);
            RefCpu cpu(m);cpu.r[4]=enemy;cpu.r[15]=stack;cpu.pr=stop;cpu.callHooks[0x0c055d60]=[](auto&){};
            instructions+=cpu.run(0x0c133ca0,stop,2000);selectOriginalRival(p,enemy);compare(p,"rival profile selection");
        }
    }
    for(unsigned course=0;course<9;++course)for(unsigned selection=0;selection<6;++selection){
        RefCpu cpu(m);cpu.r[4]=course;cpu.r[5]=selection;cpu.r[15]=stack;cpu.pr=stop;
        instructions+=cpu.run(0x0c133b40,stop,1000);require(cpu.r[0]==originalLegendRivalId(course,selection),"rival ordinal mapping");++checks;
    }
    for(unsigned menu=0;menu<8;++menu)for(int level:{0,1,5,6,9,10,11,15,30}){
        auto p=defaults;p.setu(0,2);
        for(unsigned i=0;i<8;++i)p.setu(1080+i*4,unsigned(level));put(p);
        RefCpu mapping(m);mapping.r[4]=menu;mapping.r[5]=unsigned(level);mapping.r[15]=stack;mapping.pr=stop;
        instructions+=mapping.run(0x0c1347c0,stop,1000);require(mapping.r[0]==originalBuntaCourse(menu,level),"Bunta course map");++checks;
        m.zeroRegion(owner,0x1000);RefCpu cpu(m);cpu.r[13]=mapping.r[0];cpu.r[9]=owner+444;cpu.r[10]=1;cpu.r[14]=stack-200;cpu.r[15]=stack-200;cpu.pr=stop;
        cpu.callHooks[0x0c1140a0]=[](auto&){};cpu.callHooks[0x0c055d60]=[](auto&){};
        instructions+=cpu.run(0x0c184374,0x0c1845e6,2000);selectOriginalBuntaCourse(p,menu);compare(p,"Bunta complete selection profile");
    }
    bool rejected=false;try{(void)originalRival(31);}catch(const std::out_of_range&){rejected=true;}require(rejected,"solver profile31 must not index roster");
    std::cout<<"PASS original battle profiles: "<<checks<<" comparisons, "<<instructions<<" actual instructions. Only diagnostic/menu visual side effects hooked; complete roster/profile arithmetic executes.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
