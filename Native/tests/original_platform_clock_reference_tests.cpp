#include "sh4_scalar_reference.h"
#include "original_platform_clock.h"
#include <iostream>
using namespace idas3::reference;
int main(int argc,char**argv)try{
 if(argc!=2)throw std::runtime_error("Original image required");RefMemory m(argv[1]);
 constexpr unsigned stop=0xcffffff,stack=0xcfff000,owner=0xcfe0000,node=0xcfd002c;
 std::uint64_t count=0,hooks=0,checks=0;
 auto eq=[&](unsigned a,unsigned b,const char*s){++checks;if(a!=b)throw std::runtime_error(s);};
 auto run=[&](RefCpu&c,unsigned start,unsigned end){try{count+=c.run(start,end,300000);}catch(...){std::cerr<<"stage "<<std::hex<<start<<" PC "<<c.pc<<std::dec<<": ";throw;}};
 auto memory=[&]{m.clear();m.zeroRegion(0xcffc000,0x4000);m.zeroRegion(owner,128);m.zeroRegion(node-44,128);
  for(unsigned a:{0xc98b080u,0xc9900bcu,0xcb120e0u,0xc99a8c8u})m.zeroRegion(a,4);
  m.write32(0xcb11af4,0xa0200000);m.write32(0xcb120ac,1000);m.write32(0xa0200000,42);m.write32(0xa0200004,42);
  m.write32(0xa0710000,0);m.write32(0xa0710004,1008);m.write32(0xa05f7418,0);m.write32(0xa05f700c,0x4000);
 };
 for(unsigned counter:{0u,14u,15u,16u,0xfffffffeu,0xffffffffu})for(unsigned blocked:{0u,1u})for(unsigned dma:{0u,1u})for(unsigned ready:{0u,0x4000u}){
  memory();m.write32(0xcb120e0,counter);m.write32(0xc98b080,blocked);m.write32(0xa05f7418,dma);m.write32(0xa05f700c,ready);
  idas3::original::OriginalPlatformClock native;native.begin(42,1000);native.advanceReadyTicks(counter,1000);
  native.tick(1008,blocked!=0,dma!=0||ready==0);
  RefCpu c(m);c.r[15]=stack;c.pr=stop;run(c,0xc2050a0,stop);
  auto next=counter;bool update=false;if(!blocked){++next;if(!(next&15)){if(dma||!ready)next=15;else update=true;}}
  eq(m.read32(0xcb120e0),next,"Original clock scheduler counter/retry");eq(m.read32(0xa0200000),update?50:42,"Clock update cadence/gates");eq(m.read32(0xa0200004),42,"Scheduler never changes persisted-session base");
  eq(native.counter(),m.read32(0xcb120e0),"Native source counter/gates");eq(native.current(),m.read32(0xa0200000),"Native published value");eq(native.base(),m.read32(0xa0200004),"Native session base");
 }
 for(unsigned before:{0u,1u,15u,16u,0xfffffff0u,0xffffffffu})for(unsigned ticks:{0u,1u,15u,16u,17u,127u}){
  memory();m.write32(0xcb120e0,before);
  idas3::original::OriginalPlatformClock native;native.begin(42,1000);native.advanceReadyTicks(before,1000);
  native.advanceReadyTicks(ticks,1008);
  for(unsigned i=0;i<ticks;++i){RefCpu c(m);c.r[15]=stack;c.pr=stop;run(c,0xc2050a0,stop);}
  eq(native.counter(),m.read32(0xcb120e0),"Batched ready counter equals original iterations");eq(native.current(),m.read32(0xa0200000),"Batched publication equals original iterations");
 }
 const unsigned words[]{0,1,65535,65536,0x7fffffffu,0x80000000u,0xfffffffeu,0xffffffffu,0x12345678u};
 for(auto baseline:words)for(auto retained:words)for(auto rtc:words){
  memory();m.write32(0xcb120ac,baseline);m.write32(0xa0200004,retained);
  m.write32(0xa0710000,0xa5a50000u|(rtc>>16));m.write32(0xa0710004,0x5a5a0000u|(rtc&65535));
  RefCpu c(m);c.r[15]=stack;c.pr=stop;run(c,0xc203440,stop);
  idas3::original::OriginalPlatformClock native;native.begin(retained,baseline);native.advanceReadyTicks(16,rtc);
  eq(native.current(),m.read32(0xa0200000),"Native RTC/baseline/retained wrap arithmetic");
 }
 // Original parent order: maintenance precedes scene dispatch. Only unrelated
 // platform services and AMiddleSeq's remaining methods are explicit hooks.
 memory();m.write32(owner,owner);m.write32(owner+4,node);m.write32(owner+36,node);m.write32(node+4,owner);m.write32(node+12,0xc382ec4);m.write32(0xc99a8c8,1);
 RefCpu outer(m);outer.r[8]=owner;outer.r[15]=stack;
 for(unsigned call:{0xc1f8320u,0xc1f8ce0u,0xc1f8f80u,0xc1fbc60u,0xc056d80u})outer.callHooks[call]=[&](auto&){++hooks;};
 outer.callHooks[0xc056a20]=[&](auto&){++hooks;RefCpu seed(m);seed.r[15]=stack-1024;run(seed,0xc056a44,0xc056a54);};
 for(unsigned frame=1;frame<=16;++frame){run(outer,0xc021464,0xc021492);eq(m.read32(0xa0200000),frame==16?50:42,"Outer-loop clock publication");eq(m.read32(0xc37c778),42,"First scene seeds retained value before periodic update");}
 m.write32(node+20,0);m.write32(0xcb120e0,15);m.write32(0xa0710004,1016);run(outer,0xc021464,0xc021492);
 eq(m.read32(0xa0200000),58,"Boundary frame updates clock first");eq(m.read32(0xc37c778),58,"Reactivation reads newly published clock");
 // SRAM settings validation invokes original CRC and repair/clear routines.
 // Clock words are excluded from both checksummed records.
 for(unsigned a:{0u,1u})for(unsigned b:{0u,1u}){
  memory();m.zeroRegion(0xa0200000,32768);m.write32(0xa0200000,0x12345678);m.write32(0xa0200004,0x9abcdef0);
  for(unsigned i=0;i<240;++i){m.write8(0xa0200010+i,(i*3+7)&255);m.write8(0xa0200108+i,(i*5+9)&255);}
  auto checksum=[&](unsigned ptr){RefCpu c(m);c.r[4]=ptr;c.r[5]=240;c.r[15]=stack;c.pr=stop;run(c,0xc1feb40,stop);return c.r[0];};
  m.write32(0xa0200008,checksum(0xa0200010)^(a?0u:1u));m.write32(0xa0200100,checksum(0xa0200108)^(b?0u:1u));
  RefCpu init(m);init.r[15]=stack;init.pr=stop;run(init,0xc1ff940,stop);
  eq(init.r[0],a?(b?0:1):(b?2:3),"Original SRAM validity result");
  eq(m.read32(0xa0200000),a||b?0x12345678u:0u,"Settings repair preserves clock; both invalid clear it");
  eq(m.read32(0xa0200004),a||b?0x9abcdef0u:0u,"Settings repair preserves base; both invalid clear it");
 }
 std::cout<<"PASS clock48 scheduler cases,36 batch cases,729 RTC histories,17 outer frames,4 original SRAM CRC/repair branches; comparisons="<<checks<<" original_instructions="<<count-hooks<<" declared_hook_dispatches="<<hooks<<"\n";
 std::cout<<"2050A0/203440/1FEB00 complete;021464..021492/023480 with declared service and scene-method hooks;1FF940/1FEB40 and reset/repair complete. Pending-settings-write branch9900BC=1 excluded.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
