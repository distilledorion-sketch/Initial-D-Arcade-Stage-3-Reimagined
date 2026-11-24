#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3::reference;
int main(int argc,char** argv)try{
 if(argc!=2)throw std::runtime_error("canonical image required");RefMemory m(argv[1]);
 constexpr unsigned owner=0xd000000,child=0xd001000,vtable=0xd002000,exit=0x00ff0010,stack=0xd003000,stop=0x00ff0020;
 const auto minimum=m.read32(0xc2f4bcc),maximum=m.read32(0xc2f4bd0);
 if(maximum<minimum)throw std::runtime_error("Unexpected original finish threshold order");
 unsigned cases=0,instructions=0;
 // Actual post-minimum branch; camera setup before the threshold, linked
 // outcome preparation, and virtual transition/stream output are boundaries.
 for(unsigned kind:{0u,1u,2u,3u})for(unsigned frame:{minimum,maximum,maximum+1})for(unsigned digital=0;digital<256;++digital){
  m.clear();m.zeroRegion(owner,0x4000);m.write32(child+80,owner);m.write32(child+76,frame);
  m.write32(owner+0x668,kind);m.write8(0xc92ed00,std::uint8_t(digital));
  m.write32(child+12,vtable);m.write16(vtable+40,0);m.write32(vtable+44,exit);
  unsigned advanced=0,closed=0;RefCpu c(m);c.r[9]=child;c.r[10]=child+64;
  c.callHooks[exit]=[&](auto& cpu){if(cpu.r[4]!=child)throw std::runtime_error("Wrong finish owner");++advanced;};
  c.callHooks[0xc141d80]=[&](auto&){++closed;};
  // Starting at708 executes the real minimum comparison. The selected
  // fixture counters are >=minimum so no camera hooks replace that branch.
  instructions+=unsigned(c.run(0xc05c708,0xc05c7fa,100));
  const bool expected=frame>maximum||((digital&128)!=0&&kind!=1);
  if(advanced!=unsigned(expected)||closed!=advanced||m.read32(child+76)!=frame+1)
   throw std::runtime_error("Original linked/offline finish skip or timer mismatch");
  ++cases;
 }
 // Execute the actual readiness latch write after the explicitly bounded
 // original outcome/link preparation. That call does not increment counter.
 m.clear();m.zeroRegion(owner,0x5000);m.write32(stack,child);m.write32(child+76,0);
 RefCpu ready(m);ready.r[14]=stack;instructions+=unsigned(ready.run(0xc05d084,0xc05d08c,20));
 if(m.read8(child+88)!=1||m.read32(child+76)!=0)throw std::runtime_error("Readiness consumed a timed hold tick");
 // Actual next child Init/Main/End: external display-register I/O is a hook,
 // not an assumed alpha fade. It blanks once, advances six calls, unblanks.
 m.write32(child+80,owner);m.write32(owner+0x668,1);m.write32(child+12,vtable);
 m.write16(vtable+40,0);m.write32(vtable+44,exit);
 unsigned blanks=0,unblanks=0,advanced=0;
 const auto makeCpu=[&](){RefCpu c(m);c.r[4]=child;c.r[15]=stack+0xf00;c.pr=stop;
  c.callHooks[0xc211c80]=[&](auto& cpu){if(cpu.r[4]==1)++blanks;else if(cpu.r[4]==0)++unblanks;else throw std::runtime_error("Invalid display blank mode");};
  c.callHooks[exit]=[&](auto&){++advanced;};return c;};
 auto init=makeCpu();instructions+=unsigned(init.run(0xc05d120,stop,100));
 if(blanks!=1||unblanks||m.read32(child+76)!=0)throw std::runtime_error("Linked exit Init did not blank/reset once");
 for(unsigned tick=1;tick<=6;++tick){auto c=makeCpu();instructions+=unsigned(c.run(0xc05d180,stop,100));
  if(advanced!=(tick==6?1u:0u)||m.read32(child+76)!=tick+1)throw std::runtime_error("Linked exit differs from six source updates");++cases;}
 auto end=makeCpu();instructions+=unsigned(end.run(0xc05d200,stop,100));
 if(blanks!=1||unblanks!=1||advanced!=1)throw std::runtime_error("Linked exit display lifecycle differs");
 std::cout<<"PASS "<<cases<<" original finish cases, "<<instructions<<" instructions; min="<<minimum<<" max="<<maximum
          <<". All256 accepted digital-byte values ignored in linked mode1 until frame>max; offline bit7 accepts only after min. Readiness consumes no hold counter; exit Init/Main/End blank for six source updates. Virtual advance/stream/display I/O and preceding pose/outcome are boundaries.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
