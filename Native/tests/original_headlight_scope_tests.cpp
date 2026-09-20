// Bounded original parent scheduling; independent of projection math tests.
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3::reference;
int main(int argc,char**argv)try{
 if(argc!=2)throw std::runtime_error("canonical image required");
 RefMemory m(argv[1]);std::size_t checks=0,instructions=0,cases=0;
 const auto check=[&](bool b,const char*s){++checks;if(!b)throw std::runtime_error(s);};
 constexpr unsigned race=0xd000000,player=0xd010000,rival=0xd020000,course=0xd030000;
 constexpr unsigned projection=0xd040000,stack=0xd050000,vt=0xd060000,view=0xd070000;
 constexpr unsigned bodyHook=0xf000000,coursePrimary=0xf000010,courseAlternate=0xf000020,done=0xf000100;
 check(m.read32(0xc38760c+28)==0xc0d84a0&&m.read32(0xc38760c+36)==0xc0d7ec0&&m.read32(0xc38760c+44)==0xc0d50a0,"Canonical headlight publish/draw/update vtable");
 check(m.read32(0xc383634+28)==0xc05b4c0,"Canonical rear-view scene callback");
 check(m.read32(0xc3811a4+12)==0xc034840&&std::int16_t(m.read16(0xc3811a4+8))==-2388,"ACar publication adjusted callback");
 for(unsigned mode=0;mode<4;++mode)for(unsigned body=0;body<2;++body)
 for(unsigned lights=0;lights<4;++lights)for(unsigned present=0;present<4;++present){
  m.clear();m.zeroRegion(race,0x80000);
  m.write32(race+1036,course);m.write32(race+1048,player);m.write32(race+1052,rival);
  m.write32(race+1640,mode);m.write8(race+1172,body);m.write32(course,vt+256);
  m.write32(vt+256+52,coursePrimary);m.write32(vt+256+60,courseAlternate);
  m.write16(vt+8,std::uint16_t(-76));m.write32(vt+12,bodyHook);
  m.write32(vt+128+20,0xc228a60); // Unchanged 64-byte matrix copy method.
  for(unsigned i=0;i<2;++i){const auto car=i?rival:player, p=projection+4096*i;
   m.write32(car+76,vt);m.write8(car+81,(lights>>i)&1);
   m.write32(car+2636,(present&(1u<<i))?p:0);m.write32(car+2468,vt+128);
   for(unsigned j=0;j<16;++j)m.write32(car+2404+j*4,0x3f000000+i*0x10000+j*0x100);
   m.write32(p,0xc38760c);m.write32(p+8,p+512);
  }
  RefCpu c(m);std::vector<unsigned> events;
  const auto active=[&](unsigned i){return bool((lights&present)&(1u<<i));};
  c.callHooks[bodyHook]=[&](auto&q){check(q.r[4]==player||q.r[4]==rival,"Body receives adjusted ACar");events.push_back(q.r[4]==player?10:20);};
  c.callHooks[coursePrimary]=[&](auto&q){check(q.r[4]==course,"Primary course owner");events.push_back(30);};
  c.callHooks[courseAlternate]=[&](auto&q){check(q.r[4]==course,"Alternate course owner");events.push_back(31);};
  c.callHooks[0xc1f6610]=[](auto&){};c.callHooks[0xc1f65c0]=[](auto&){};c.callHooks[0xc1fa9e0]=[](auto&){};
  c.callHooks[0xc1d7120]=[&](auto&q){check(q.r[4]==projection+512||q.r[4]==projection+4096+512,"Draw uses published mesh");events.push_back(q.r[4]==projection+512?11:21);};
  c.callHooks[0xc0d50a0]=[&](auto&q){const unsigned i=q.r[4]==projection?0:1, car=i?rival:player;
   check(q.r[4]==projection+4096*i,"Advance receives correct projection owner");
   for(unsigned j=0;j<16;++j)check(m.read32(q.r[5]+4*j)==m.read32(car+2404+4*j),"Advance receives current visual matrix unchanged");
   events.push_back(i?22:12);
  };
  c.callHooks[0xc0d84a0]=[&](auto&q){check(q.r[4]==projection||q.r[4]==projection+4096,"Publish receives correct projection owner");events.push_back(q.r[4]==projection?13:23);};
  // Gate inside the real ACar pose publication, before any draw callback.
  for(unsigned i=0;i<2;++i){c.r[13]=i?rival:player;c.r[15]=stack+0xff00;
   instructions+=c.run(0xc034b8e,0xc034bac,100);
  }
  std::vector<unsigned> expected;
  if(active(0))expected.push_back(13);if(active(1))expected.push_back(23);
  check(events==expected,"One publication per enabled existing projection");events.clear();
  c.r[15]=stack+0xff00;c.r[12]=race;c.r[11]=race+1020;c.r[13]=1;
  instructions+=c.run(0xc06a056,0xc06a0b4,3000);
  expected.clear();if(body)expected.push_back(10);
  if(active(0)){expected.push_back(11);expected.push_back(12);}
  if(mode<2){expected.push_back(20);if(active(1)){expected.push_back(21);expected.push_back(22);}}
  expected.push_back(30);check(events==expected,"Primary body -> published headlight -> working advance -> course order");events.clear();
  // The alternate scene callback itself; calling it is a caller-owned view gate.
  c.r[15]=stack+0xff00;c.r[4]=race;c.r[5]=view+52;
  instructions+=c.run(0xc06a2e0,0xc06a33e,3000);
  expected.clear();if(mode<2){expected.push_back(20);if(active(1)){expected.push_back(21);expected.push_back(22);}}
  expected.push_back(31);check(events==expected,"Rear pass draws/advances rival only and never republishes");events.clear();
  c.r[4]=race;c.r[15]=stack+0xff00;c.pr=done;
  instructions+=c.run(0xc06bb80,done,1000);
  expected.clear();if(body)expected.push_back(10);if(active(0)){expected.push_back(11);expected.push_back(12);}
  check(events==expected,"Standalone player wrapper retains the independent body gate");events.clear();
  c.r[4]=race;c.r[15]=stack+0xff00;c.pr=done;
  instructions+=c.run(0xc06bc00,done,1000);
  expected.clear();if(mode<2){expected.push_back(20);if(active(1)){expected.push_back(21);expected.push_back(22);}}
  check(events==expected,"Standalone rival wrapper retains the numeric-mode gate");events.clear();
  // Complete enclosing rear callback proves the race1704 scope and restoration.
  c.callHooks[0xc06a2e0]=[&](auto&q){check(q.r[4]==race&&q.r[5]==view+52,"Rear callback forwards race/camera");check(m.read8(race+1704)==1,"Rear context active during alternate scene");events.push_back(40);};
  m.write32(view+316,race);c.r[4]=view;c.r[15]=stack+0xff00;c.pr=done;
  instructions+=c.run(0xc05b4c0,done,200);
  check(events==std::vector<unsigned>{40}&&m.read8(race+1704)==0,"Rear scene called once and context restored");
  ++cases;
 }
 std::cout<<"PASS "<<cases<<" cases / "<<checks<<" checks / "<<instructions<<" original instructions. Actual parent primary/rear blocks,034E60,0D7EC0,matrix copy,mode/gate branches; body/course/PVR/working-update/publish leaves hooked. No global view-visibility policy or projection-math claim.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
