#include "original_driver_entry_route.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3::original;using namespace idas3::reference;
namespace {std::size_t checks{},instructions{};void eq(std::uint32_t a,std::uint32_t b,const char* n){++checks;if(a!=b)throw std::runtime_error(std::string(n)+":"+hex(a)+" != "+hex(b));}}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Usage: original_driver_entry_route_tests canonical_image");
    RefMemory m(argv[1]);constexpr auto owner=0x0d000000u,stack=0x0d003000u,pbase=0x0c31c99cu;
    OriginalCardAcceptanceRoutes routes{0x101,0x202,0x303,0x404};
    for(unsigned kind=0;kind<256;++kind)for(auto uses:{0u,1u,51u,0xffffffffu,0x7fffffffu})for(auto flags:{0u,1u,2u,3u,129u,0x400000u,0xffffffffu}){
        m.clear();m.zeroRegion(owner,0x4000);
        auto p=makeOriginalFreshBattleProfile();p.setByte(1192,std::uint8_t(kind));p.setu(1140,uses);p.setu(1180,flags);
        for(unsigned j=0;j<p.words.size();++j)m.write32(pbase+j*4,p.words[j]);
        for(auto [o,v]:{std::pair{76u,routes.ordinary76},{80u,routes.renewal80},{84u,routes.converted84},{88u,routes.integralColor88}})m.write32(owner+o,v);
        RefCpu c(m);c.r[9]=owner;c.r[15]=stack;c.callHooks[0x0c055d60]=[](auto&){}; // diagnostic logging only
        instructions+=c.run(0x0c0fec40,0x0c0fee1a,300);
        const auto event=acceptOriginalDriverCard(p,routes);eq(event,m.read32(owner+64),"card parent event");
        for(unsigned j=0;j<p.words.size();++j)eq(p.words[j],m.read32(pbase+j*4),"full accepted profile");
        eq(originalDriverNameImportsExisting(p),kind==2,"source imported name gate");
    }
    for(unsigned flags=0;flags<4096;++flags){
        m.clear();m.zeroRegion(owner,0x4000);m.write32(pbase+1180,flags);
        RefCpu c(m);instructions+=c.run(0x0c102cea,0x0c102cf2,40);
        auto p=makeOriginalFreshBattleProfile();p.setu(1180,flags);requestOriginalDriverSetup(p);
        eq(p.u(1180),m.read32(pbase+1180),"request setup mask");eq(originalDriverSetupRequested(p),true,"requested gate");
        m.write32(pbase+1180,flags);RefCpu f(m);f.r[9]=0x0c31ce18;f.r[11]=1;
        instructions+=f.run(0x0c080d06,0x0c080d20,40);
        p.setu(1180,flags);finishOriginalDriverSetupFlag(p);eq(p.u(1180),m.read32(pbase+1180),"finish mask");
    }
    std::cout<<"PASS driver entry routing: "<<checks<<" comparisons, "<<instructions<<" actual instructions; diagnostic hooks only.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
