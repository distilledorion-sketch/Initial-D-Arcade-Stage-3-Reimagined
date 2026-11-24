#include "sh4_scalar_reference.h"
#include "original_offline_finish_presentation.h"
#include <iostream>
using namespace idas3::reference;
using namespace idas3::original;
namespace {
unsigned checks=0;std::uint64_t instructions=0;
void check(bool ok,const char* label){++checks;if(!ok)throw std::runtime_error(label);}
constexpr unsigned owner=0xd000000,child=0xd002000,car=0xd004000,rival=0xd006000,
 actor=0xd009000,course=0xd00a000,geometry=0xd00a100,vtable=0xd00a200,
 childVtable=0xd00a300,tls=0xd00b000,stack=0xd030000,stop=0xff0000,complete=0xff0010,project=0xff0020;
void seed(RefMemory&m,unsigned kind,bool goal,bool lost,unsigned flags,bool card){
 m.clear();m.zeroRegion(owner,0x50000);m.write32(child+80,owner);m.write32(child+12,childVtable);
 m.write32(childVtable+44,complete);m.write32(owner+0x668,kind);m.write8(owner+0x624,goal);m.write32(owner+0x66c,lost);
 m.write32(owner+0x418,car);m.write32(owner+0x41c,rival);m.write32(car+0x95c,actor);
 m.write32(owner+0x40c,course);m.write32(course+24,geometry);m.write32(geometry,vtable);m.write32(vtable+20,project);
 m.write32(tls+4,tls+0x100);m.write32(tls+0x104,tls+0x200);m.write32(0xc31ce38,card?3:0);
 m.write8(child+88,1);unsigned count=0;
 for(auto f:{0x40000000u,0x20000000u,0x10000000u})if(flags&f)m.write32(child+96+4*count++,f);
 m.write32(child+92,count);m.write8(child+89,count!=0);
}
}
int main(int argc,char**argv)try{
 check(argc==2,"canonical image required");RefMemory m(argv[1]);
 check(m.read32(0xc2f4bcc)==120&&m.read32(0xc2f4bd0)==600,"verified source thresholds changed");
 unsigned cases=0;
 for(unsigned kind:{0u,2u})for(bool goal:{false,true})for(bool lost:{false,true})
 for(unsigned records:{0u,0x40000000u,0x20000000u,0x10000000u,0x70000000u})for(bool card:{false,true})
 for(unsigned press:{119u,120u,600u,~0u}){
  const auto flags=kind==2&&goal?records:0u;seed(m,kind,goal,lost,flags,card);
  OriginalOfflineFinishPresentation native;native.begin(kind,goal,lost,flags,card);
  //Actual readiness-latch suffix after declared outcome/record qualification.
  m.write32(stack,child);m.write8(child+88,0);m.write32(child+76,0);
  RefCpu ready(m);ready.r[14]=stack;instructions+=ready.run(0xc05d084,0xc05d08c,20);
  native.advance(false);check(m.read8(child+88)==1&&m.read32(child+76)==0&&native.counter==0,"readiness consumed Hold time");
  unsigned transitions=0,streams=0;bool blank=false;
  auto cpu=[&]{RefCpu c(m);c.r[4]=child;c.r[15]=stack;c.pr=stop;
   c.callHooks[0xc221fc0]=[](auto&x){x.r[0]=tls;};
   c.callHooks[0xc024100]=[](auto&){};c.callHooks[project]=[](auto&){};c.callHooks[0xc067000]=[](auto&){};
   c.callHooks[0xc2223b8]=[](auto&x){x.fpul=x.r[4]/x.r[5];};
   c.callHooks[complete]=[&](auto&){++transitions;};c.callHooks[0xc141d80]=[&](auto&){++streams;};
   c.callHooks[0xc211c80]=[&](auto&x){blank=x.r[4]!=0;};return c;};
  for(unsigned n=0;n<=601;++n){
   const auto previousHud=m.read32(owner+0x630),previousRecord=m.read32(owner+0x634);
   m.write8(0xc92ed00,n==press?128:0);auto c=cpu();instructions+=c.run(0xc05c4a0,stop,2500);
   const bool requested=native.advance(n==press);
   check(native.visibleHudCode==previousHud&&native.visibleRecord==previousRecord,"pre-child HUD publication lag");
   check(native.counter==m.read32(child+76),"Hold counter differs");
   check(native.selectedHudCode==m.read32(owner+0x630),"selected Finish/Win/Lose/TimeUp/Record differs");
   check(native.selectedRecord==m.read32(owner+0x634)&&native.recordCount==m.read32(child+92),"record LIFO120tick selection differs");
   check(transitions==unsigned(requested)&&streams==transitions,"Start/min/max or audio stop decision differs");
   if(requested)break;
  }
  check(native.phase==OriginalOfflineFinishPresentation::Phase::Exit,"source Finish did not enter Exit");
  check(native.counter==(press==120?121u:press==600?601u:602u),"pre-minimum edge was retained");
  m.write32(child+76,0);auto init=cpu();instructions+=init.run(0xc05d120,stop,200);
  check(blank,"Exit Init did not hardware blank");
  for(unsigned i=1;i<=6;++i){auto c=cpu();instructions+=c.run(0xc05d180,stop,200);native.advance(false);
   if(i==6){auto end=cpu();instructions+=end.run(0xc05d200,stop,200);}
   check(native.exitCounter==m.read32(child+76)&&native.blank==blank,"Exit six Main/five retained blank boundary");
   check(native.done()==(i==6),"Exit completion differs");
  }
  ++cases;
 }
 //The accepted edge bank runs before Finish, not only after its minimum.
 for(unsigned preheld:{0u,1u}){
  OriginalCabinetStartEdge edge;edge.accept(preheld!=0);
  OriginalOfflineFinishPresentation n;n.begin(2,true,false,0,false);n.advance(edge.accept(true));
  for(unsigned i=0;i<125;++i)n.advance(edge.accept(true));
  check(n.phase==OriginalOfflineFinishPresentation::Phase::Hold,"held Start became delayed edge");
  n.advance(edge.accept(false));n.advance(edge.accept(true));
  check(n.phase==OriginalOfflineFinishPresentation::Phase::Exit,"fresh accepted Start did not exit after minimum");
 }
 std::cout<<"PASS "<<cases<<" ordinary source Finish cases; "<<checks<<" checks; "<<instructions
 <<" SH4 instructions. Actual Hold selection/records/min/max/accepted-bit7 and Exit Init/Main/End; qualification, view/projection leaves and stream/display I/O declared boundaries.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
