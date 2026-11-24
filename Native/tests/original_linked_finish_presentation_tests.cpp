#include "original_linked_finish_presentation.h"
#include "original_driving_clock.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3::original;using namespace idas3::reference;
int main(int argc,char**argv)try{
 if(argc!=2)throw std::runtime_error("canonical image required");RefMemory m(argv[1]);
 constexpr unsigned owner=0xd000000,child=0xd001000,vt=0xd002000,stack=0xd003000,exit=0xff0010,stop=0xff0020;
 unsigned checks=0,instructions=0;
 const auto check=[&](bool ok,const char* text){++checks;if(!ok)throw std::runtime_error(text);};
 for(bool goal:{false,true})for(bool lost:{false,true})for(unsigned delay:{0u,7u}){
  m.clear();m.zeroRegion(owner,0x5000);m.write32(child+80,owner);m.write32(child+12,vt);
  m.write32(owner+0x668,1);m.write8(owner+0x624,goal);m.write32(owner+0x66c,lost);
  m.write16(vt+40,0);m.write32(vt+44,exit);m.write32(stack,child);
  unsigned advanced=0;bool blank=false;
  const auto cpu=[&](){RefCpu c(m);c.r[4]=child;c.r[8]=child+64;c.r[9]=child;c.r[10]=child+64;c.r[14]=stack;c.r[15]=stack+0xf00;c.pr=stop;
   c.callHooks[exit]=[&](auto&){++advanced;};c.callHooks[0xc141d80]=[](auto&){};
   c.callHooks[0xc211c80]=[&](auto& x){blank=x.r[4]!=0;};return c;};
  OriginalLinkedFinishPresentation state;state.begin(goal,lost);
  for(unsigned i=0;i<delay;++i){auto c=cpu();c.r[2]=owner;c.callHooks[0xc05a280]=[](auto& x){x.r[0]=0;};
   instructions+=unsigned(c.run(0xc05c800,0xc05c7fa,100));state.advance(false);
   check(m.read32(child+76)==state.counter&&!m.read8(child+88),"Unready linked result consumed finish time");}
  {auto c=cpu();instructions+=unsigned(c.run(0xc05d084,0xc05d08c,20));state.advance(true);}
  check(state.counter==0&&m.read32(child+76)==0&&m.read8(child+88)==1,"Readiness latch translation differs");
  unsigned ticks=1;
  while(state.phase==OriginalLinkedFinishPresentation::Phase::Hold){
   const auto oldCounter=m.read32(child+76),priorHud=m.read32(owner+0x630);
   auto c=cpu();m.write8(0xc92ed00,std::uint8_t(ticks*37));
   // Actual kind1 HUD-selection block, then actual timer/exit branch. The
   // two pose-copy virtual calls/067000 at pre120 are excluded boundaries.
   instructions+=unsigned(c.run(0xc05c5a0,0xc05c70a,100));
   if(oldCounter<120){instructions+=unsigned(c.run(0xc05c70a,0xc05c712,30));instructions+=unsigned(c.run(0xc05c7f4,0xc05c7fa,20));}
   else instructions+=unsigned(c.run(0xc05c70a,0xc05c7fa,100));
   state.advance(true);++ticks;
   check(state.selectedHudCode==m.read32(owner+0x630),"Linked HUD selection differs");
   check(state.visibleHudCode==priorHud,"Parent HUD-before-child ordering differs");
   check(state.counter==m.read32(child+76),"Linked finish counter differs");
   check(advanced==unsigned(state.phase==OriginalLinkedFinishPresentation::Phase::Exit),"Linked finish handoff differs");
  }
  check(ticks==603&&!blank&&!state.blank,"Readiness plus ready-phase duration differs");
  {auto c=cpu();instructions+=unsigned(c.run(0xc05d120,stop,100));}
  for(unsigned exitTick=1;exitTick<=6;++exitTick){
   auto c=cpu();instructions+=unsigned(c.run(0xc05d180,stop,100));
   if(advanced==2){auto end=cpu();instructions+=unsigned(end.run(0xc05d200,stop,100));}
   state.advance(true);++ticks;
   check(state.blank==blank&&state.exitCounter==m.read32(child+76),"Hardware blank exit lifecycle differs");
   check(state.done()==(advanced==2),"Exit completion differs");
  }
  check(ticks==609&&state.done()&&!state.blank,"Complete linked finish update count differs");
 }
 for(unsigned hz:{15u,30u,60u,120u,144u,240u}){
  idas3::OriginalDrivingClock clock;OriginalLinkedFinishPresentation state;state.begin(true,false);
  unsigned ticks=0,renders=0;
  while(!state.done()&&renders<hz*12){++renders;clock.advance(1.0/hz,[&]{if(!state.done()){state.advance(true);++ticks;}});}
  check(ticks==609&&state.done(),"Host render cadence changed source finish updates");
  const auto seconds=renders/double(hz),minimum=609*idas3::OriginalDrivingClock::step;
  check(seconds+1e-9>=minimum&&seconds<minimum+1.0/hz+1e-9,"Finish scheduler used rounded60Hz");
 }
 std::cout<<"PASS "<<checks<<" linked finish presentation checks, "<<instructions<<" original instructions. Exact hold/exit/HUD writes; readiness/outcome, parent HUD sampling, pose/camera, virtual transition/stream/display I/O are explicit boundaries.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
