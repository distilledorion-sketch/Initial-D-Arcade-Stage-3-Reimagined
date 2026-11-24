#include "sh4_scalar_reference.h"
#include "original_loading_selection.h"
#include <iostream>
using namespace idas3::reference;
using namespace idas3::original;
int main(int argc,char**argv)try{
 if(argc!=2)throw std::runtime_error("Canonical image required");RefMemory m(argv[1]);
 constexpr unsigned stack=0xd00f000,parent=0xd010000,tls=0xd011000,art=0xd012000,stop=0xf000000;
 std::uint64_t instructions=0;unsigned checks=0,cases=0;
 auto eq=[&](unsigned a,unsigned b,const char*why){++checks;if(a!=b)throw std::runtime_error(std::string(why)+" "+hex(a)+" != "+hex(b));};
 auto run=[&](RefCpu&c,unsigned a,unsigned z){try{instructions+=c.run(a,z,30000);}catch(const std::exception&e){throw std::runtime_error(hex(a)+" at "+hex(c.pc)+": "+e.what());}};
 // Test every possible fifteen-bit result, using the inverse LCG only to
 // construct an input seed. Original rand, count13 and mapping run;2223B8 exact positive integer division is an explicit math boundary.
 const unsigned inverse=0xeeb9eb65u;
 for(unsigned random=0;random<32768;++random){
  const unsigned seed=((random<<16)-12345u)*inverse;
  m.clear();m.zeroRegion(0xd000000,0x20000);m.write32(0xc37c778,seed);
  RefCpu c(m);c.r[15]=stack;c.pr=stop;c.callHooks[0xc2223b8]=[](auto&q){q.fpul=unsigned(signed32(q.r[4])/signed32(q.r[5]));};unsigned selected=99,calls=0;
  c.callHooks[0xc16cd80]=[&](auto&q){selected=q.r[4];++calls;q.r[0]=art;}; // asset loading boundary only
  run(c,0xc16cd20,stop);auto nativeSeed=seed;
  eq(selectOriginalLoadingArtwork(OriginalLoadingOwner::TimeAttack,1,0,nativeSeed),selected,"authored selection");
  eq(m.read32(0xc37c778),nativeSeed,"exact one source draw");eq(calls,1,"one asset load");
  eq(c.r[0],art,"asset handle propagated");++cases;
 }
 struct Owner {unsigned address,after;OriginalLoadingOwner kind;};
 // Complete owner entry through its load request. Device/audio/resource
 // setup is explicitly stubbed; RNG/13-count/mapping and modality branch run;2223B8 is the declared integer division boundary.
 for(const auto owner:{Owner{0xc07c0a0,0xc07c11c,OriginalLoadingOwner::TimeAttack},
  Owner{0xc08fe00,0xc08fe88,OriginalLoadingOwner::Visit}})
 for(unsigned mode:{0u,1u,2u})for(unsigned enemy:{0u,28u,29u,30u})for(unsigned seed:{0u,1u,3778067106u,484824946u,0xffffffffu}){
  m.clear();m.zeroRegion(0xd000000,0x20000);m.write32(0xc37c778,seed);m.write32(0xc31de08,0);
  m.write32(0xc31c99c,mode);m.write32(0xc31c99c+24,enemy);
  RefCpu c(m);c.r[4]=parent;c.r[15]=stack;c.pr=stop;c.callHooks[0xc2223b8]=[](auto&q){q.fpul=unsigned(signed32(q.r[4])/signed32(q.r[5]));};
  for(unsigned a:{0xc055d60u,0xc020460u,0xc059f80u,0xc1d0940u,0xc143140u})c.callHooks[a]=[](auto&){};
  c.callHooks[0xc221fc0]=[&](auto&q){q.r[0]=tls;};
  unsigned selected=99,calls=0;
  c.callHooks[0xc16cd80]=[&](auto&q){selected=q.r[4];++calls;q.r[0]=art;};
  run(c,owner.address,owner.after);auto nativeSeed=seed;
  eq(selectOriginalLoadingArtwork(owner.kind,mode,enemy,nativeSeed),selected,"parent selection");
  eq(m.read32(0xc37c778),nativeSeed,"parent shared stream");eq(calls,1,"parent requests one artwork");++cases;
 }
 std::cout<<"PASS "<<cases<<" loading chooser/parent cases, "<<checks<<" checks, "<<instructions<<" original instructions; resource loader and positive integer division stubbed, RNG and both installed loading owners execute; generic Visit is shared by Legend/Bunta.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
