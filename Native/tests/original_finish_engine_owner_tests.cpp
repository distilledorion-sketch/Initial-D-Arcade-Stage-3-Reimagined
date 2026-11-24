#include "sh4_scalar_reference.h"
#include <iostream>
#include <vector>
using namespace idas3::reference;
int main(int argc,char**argv)try{
 if(argc!=2)throw std::runtime_error("image required");RefMemory m(argv[1]);
 constexpr unsigned owner=0xd000000,go=0xd002000,finish=0xd002200,sentinel=0xd002400,aux=0xd002600,car=0xd004000,actor=0xd006000,stack=0xd030000,task=0xd007000,timerA=0xd008000,timerB=0xd008100;
 unsigned instructions=0;
 for(bool finishLine:{false,true}){
 m.clear();m.zeroRegion(0xca9b4ec,0x100);m.zeroRegion(owner,0x10000);m.zeroRegion(stack-0x1000,0x11000);
 m.write32(owner+12,0xc383c74);m.write32(owner+32,sentinel);m.write32(owner+36,go);m.write32(owner+68,go);
 m.write32(go+4,finish);m.write32(go+12,0xc3833bc);m.write32(go+16,2);m.write32(go+20,2);m.write32(go+76,owner);
 m.write32(finish+4,sentinel);m.write32(finish+12,0xc383314);m.write32(finish+16,3);m.write32(finish+80,owner);
 m.write32(owner+1600,aux);m.write32(aux,aux+0x100);m.write32(aux+4,aux+0x100);
 m.write32(owner+1640,2);m.write32(owner+1048,car);m.write32(car+2396,actor);
 m.write32(owner+1176,timerA);m.write32(owner+1180,timerB);m.write32(timerA+4,unsigned(-100));m.write32(timerA+8,finishLine?6000:0);m.write32(timerB+4,100);
 unsigned stopCommands=0;bool solverDisabled=false;
 for(unsigned frame=1;frame<=(finishLine?2u:3u);++frame){
  m.write32(actor+80,!finishLine&&frame>1?0xc000:0x8000);
  m.write32(0xc2f4bc8,0);m.write16(0xc92f0e2,65407);m.write16(0xc92f0e4,0);
  RefCpu c(m);c.r[15]=stack+0xf000;c.r[12]=owner;c.r[13]=0xc156700;
  for(auto a:{0xc156700u,0xc0654e0u,0xc024100u,0xc069c40u,0xc0683c0u,0xc067000u,0xc0680c0u,0xc067a80u,0xc068a60u,0xc055d60u,0xc141d80u,0xc16db80u,0xc16d500u,0xc05c4a0u})c.callHooks[a]=[](auto&){};
  c.callHooks[0xc221fc0]=[](auto&cpu){cpu.r[0]=task;};
  // Cabinet timer configuration: ordinary running countdown, not free play
  //or the timer-freeze service setting. The settings accessor is a boundary.
  c.callHooks[0xc04fcc0]=[](auto&cpu){cpu.r[0]=0xd008200;};
  c.callHooks[0xc16d1e0]=[](auto&cpu){cpu.r[0]=0;};c.callHooks[0xc160160]=[](auto&cpu){cpu.r[0]=0;};
  // The goal producer is a declared boundary, independently exercised by
  //original_race_rules. Execute the actual child dispatch/handoff around it.
  if(finishLine)c.callHooks[0xc067a80]=[&](auto&){m.write8(owner+0x624,1);};
  c.callHooks[0xc06a420]=[](auto&cpu){cpu.setFloat(0,0);};
  c.callHooks[0xc1ed6a0]=[&](auto&cpu){if(cpu.r[5]!=0xa6||cpu.r[6]!=0)throw std::runtime_error("not a source engine stop");++stopCommands;};
  c.callHooks[0xc159920]=[&](auto&){
   solverDisabled=m.read8(0xc31de55)!=0;
   const auto flags=m.read32(actor+80);
   if(!(flags&0x8000)||m.read32(0xc2f4bc8)!=0||m.read16(0xc92f0e2)!=65407||m.read16(0xc92f0e4)!=0)
    throw std::runtime_error("Original finish owner rewrote accepted pedals or gear engagement");
   if(bool(flags&0x2000)!=(finishLine?frame==2:true))throw std::runtime_error("Original automatic brake entered on wrong finish update");
  };c.callHooks[0xc063ce0]=[](auto&){};
  instructions+=c.run(0xc05fe6e,0xc05ffba,12000);
  std::cout<<"finish_line="<<finishLine<<" frame="<<frame<<" timeout="<<m.read32(owner+1608)<<" activechild="<<std::hex<<m.read32(owner+68)<<std::dec<<" engine_disabled_before_solver="<<solverDisabled<<" stop_commands="<<stopCommands<<'\n';
  if((frame==(finishLine?2u:3u))!=solverDisabled)throw std::runtime_error("source finish gate timing differs");
 }
 if(stopCommands!=4)throw std::runtime_error("source stop command count differs");
 }
 // The explicit challenger flag (+64C) has a different source owner. Its Init leaves
 // voices running; Main stops only after the prior actor has bit14 set.
 for(bool stopped:{false,true}){
  m.write8(owner+0x64c,1);m.write8(0xc31de55,0);m.write32(actor+80,stopped?0x4000:0);
  RefCpu init(m);init.r[4]=finish;init.r[15]=stack+0xf000;
  init.callHooks[0xc221fc0]=[](auto&cpu){cpu.r[0]=task;};
  instructions+=init.run(0xc05c280,0xc05c2c8,100);
  if(m.read8(0xc31de55))throw std::runtime_error("Challenger Init muted engine too early");
  RefCpu c(m);c.r[4]=finish;c.r[15]=stack+0xf000;
  c.callHooks[0xc221fc0]=[](auto&cpu){cpu.r[0]=task;};
  c.callHooks[0xc16d440]=[](auto&){};c.callHooks[0xc141d80]=[](auto&){};
  unsigned commands=0;c.callHooks[0xc1ed6a0]=[&](auto&cpu){if(cpu.r[5]!=0xa6||cpu.r[6]!=0)throw std::runtime_error("Challenger stop command differs");++commands;};
  instructions+=c.run(0xc05c4a0,0xc05c7f6,500);
  if((m.read8(0xc31de55)!=0)!=stopped||commands!=(stopped?4u:0u))throw std::runtime_error("Challenger Main source actorbit14 gate differs");
 }
 // Linked mode1 waits for both actors at the original timer boundary.
 // This executes the real linked predicate, without emulating linked transport.
 constexpr unsigned rivalCar=0xd009000,rivalActor=0xd00a000;
 m.write32(timerA+8,0); // Explicit linked expiry fixture after positive-time finish above.
 m.write32(owner+1640,1);m.write32(owner+1052,rivalCar);m.write32(rivalCar+2396,rivalActor);
 for(unsigned flags=0;flags<4;++flags){
  m.write32(owner+1608,0);m.write32(actor+80,(flags&1)?0x4000:0);m.write32(rivalActor+80,(flags&2)?0x4000:0);
  RefCpu c(m);c.r[4]=owner;c.r[15]=stack+0xf000;c.callHooks[0xc055d60]=[](auto&){};
  instructions+=c.run(0xc067d00,0xc067fdc,500);
  if(m.read32(owner+1608)!=(flags==3?1u:0u))throw std::runtime_error("Linked timeout did not wait for both stopped actors");
 }
 m.write8(owner+0x64c,0);m.write8(0xc31de55,0);
 RefCpu linkedInit(m);linkedInit.r[4]=finish;linkedInit.r[15]=stack+0xf000;
 linkedInit.callHooks[0xc221fc0]=[](auto&cpu){cpu.r[0]=task;};linkedInit.callHooks[0xc141d80]=[](auto&){};
 unsigned linkedStops=0;linkedInit.callHooks[0xc1ed6a0]=[&](auto&cpu){if(cpu.r[5]!=0xa6||cpu.r[6]!=0)throw std::runtime_error("Linked stop command differs");++linkedStops;};
 instructions+=linkedInit.run(0xc05c280,0xc05c2c8,500);
 if(m.read8(0xc31de55)!=1||linkedStops!=4)throw std::runtime_error("Linked mode omitted the common finish Init engine gate");
 std::cout<<"PASS actual race outer dispatch, actual067D00 expiry, actual05C280 finish prefix and061A80→1427E0→0C4200. "<<instructions<<" executed instructions; render/projection/outcome/task allocator/finish Main/stream output explicit boundaries.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}

