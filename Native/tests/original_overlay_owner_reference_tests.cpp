#include "sh4_scalar_reference.h"
#include "original_race_overlay_resources.h"
#include <iostream>
using namespace idas3::reference;
int main(int argc,char**argv)try{
 if(argc!=2)throw std::runtime_error("Canonical image required");RefMemory m(argv[1]);
 constexpr unsigned stack=0xd00f000,owner=0xd010000,tls=0xd011000,link=0xd012000,stop=0xf000000;
 unsigned checks=0,cases=0;std::uint64_t instructions=0;
 auto eq=[&](unsigned a,unsigned b,const char*why){++checks;if(a!=b)throw std::runtime_error(std::string(why)+" "+hex(a)+" != "+hex(b));};
 for(unsigned mode=0;mode<4;++mode)for(unsigned language=0;language<2;++language)for(unsigned option:{0u,1u})for(unsigned seed:{0u,1u,3778067106u,484824946u,0xffffffffu}){
  m.clear();m.zeroRegion(0xd000000,0x40000);m.write32(0xc37c778,seed);m.write32(0xc31c99c,mode);m.write8(0xc31ce40,std::uint8_t(language));m.write32(tls+4,link);
  RefCpu c(m);c.r[4]=owner;c.r[15]=stack;c.pr=stop;
  c.callHooks[0xc221fc0]=[&](auto&q){q.r[0]=tls;};
  c.callHooks[0xc16d1e0]=[&](auto&q){q.r[0]=option;}; // original option query boundary
  unsigned next=0xd020000,loads=0,formatted=0;
  c.callHooks[0xc021960]=[&](auto&q){q.r[0]=next;next+=0x100;}; // explicit asset object allocation
  c.callHooks[0xc145420]=[&](auto&q){++loads;q.r[0]=q.r[4];}; // model/texture IO boundary
  c.callHooks[0xc226980]=[&](auto&q){eq(q.r[6],2,"interrupt fixed artwork number");
   eq(q.r[5],formatted?0xc28b354:0xc28b328,"interrupt format literal");++formatted;};
  try{instructions+=c.run(0xc16b640,0xc16ba6e,30000);}catch(const std::exception&e){throw std::runtime_error("mode "+std::to_string(mode)+" at "+hex(c.pc)+": "+e.what());}
  auto expected=seed; const auto prepared=idas3::original::prepareOriginalRaceOverlayResources(mode,expected);
  eq(m.read32(0xc37c778),expected,"real resource owner RNG result");
  eq(formatted,mode==3?0:2,"interrupt path eligibility");
  eq(prepared.banner==idas3::original::OriginalRaceBannerArtwork::Versus,mode==3,"semantic banner choice");
  eq(prepared.modeLevelUp==idas3::original::OriginalModeLevelUpArtwork::Bunta,mode==2,"Bunta resource choice");
  eq(prepared.modeLevelUp==idas3::original::OriginalModeLevelUpArtwork::Versus,mode==3,"VS resource choice");
  eq(m.read32(owner+28),option?15:0,"source-only option delay branch (not inferred by native)");
  eq(m.read32(owner+24)!=0,prepared.driverLevelUp,"driver level-up resource presence");
  eq(m.read32(owner+20)!=0,prepared.modeLevelUp!=idas3::original::OriginalModeLevelUpArtwork::None,"mode level-up resource presence");
  eq(m.read32(owner+36),mode==3?m.read32(0xc16b7dc):language?(mode==1?0xbf51eb85u:0xbe99999au):mode==1?0xbffc28f6u:m.read32(0xc16b7dc),"source placement");
  eq(loads,mode==3?3:mode==2?3:mode==1?1:2,"declared original asset requests");++cases;
 }
 std::cout<<"PASS "<<cases<<" original race overlay resource entries, "<<checks<<" checks, "<<instructions<<" instructions; only allocator/asset IO/options/format sinks hooked; real rand and source mode branches execute.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
