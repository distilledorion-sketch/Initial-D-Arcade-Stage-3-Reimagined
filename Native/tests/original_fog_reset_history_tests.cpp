// Original global/reset and attract-parent fog boundaries, not a hardware boot.
#include "sh4_scalar_reference.h"
#include <iostream>
#include <map>
using namespace idas3::reference;
int main(int argc,char**argv)try{
 if(argc!=2)throw std::runtime_error("canonical image required");
 RefMemory m(argv[1]);std::size_t checks=0,instructions=0,cases=0;
 const auto check=[&](bool b,const char*s){++checks;if(!b)throw std::runtime_error(s);};
 constexpr unsigned stack=0xd000000,owner=0xd010000,stop=0xf000000;
 // Actual graphics reset leaves independent environment offsets at zero.
 const auto run=[&](unsigned entry,unsigned end,unsigned kind,unsigned child){
  m.clear();m.zeroRegion(stack,0x20000);m.write32(0xc34000c,0);
  RefCpu c(m);c.r[15]=stack+0xff00;c.r[14]=stack+0x4000;c.r[4]=owner;c.pr=stop;
  std::vector<std::pair<unsigned,unsigned>> writes;
  c.callHooks[0xc21b460]=[&](auto&q){writes.emplace_back(q.r[4],q.r[5]);};
  // These non-fog graphics/debug boundaries are explicit. The full1CEAA0,
  // 1CEB40, table generator, quantizer, density and color writers execute.
  for(auto a:{0xc1d01e0u,0xc1f76a0u,0xc1fb000u,0xc1f92a0u,0xc1d0800u,0xc1cefe0u,0xc1d0fa0u,
              0xc1cebc0u,0xc1cf300u,0xc1f6f80u,0xff1000u})c.callHooks[a]=[](auto&){};
  if(kind==3){m.write32(c.r[14]+336,owner);}
  if(kind==4){c.r[1]=owner;m.write32(owner+16,child);c.r[8]=0xff1000;}
  try{instructions+=c.run(entry,end,20000);}catch(const std::exception&e){throw std::runtime_error(hex(entry)+" at"+hex(c.pc)+": "+e.what());}
  const bool expected=kind!=4||child==11||child==12;
  check(writes.size()==(expected?130:0),"Fog reset source route gate");
  if(expected){
   for(unsigned order=0;order<128;++order){
    const unsigned i=127-order;const float sample=1.f-float(i)/127.f;
    const float next=i==127?0.f:1.f-float(i+1)/127.f;
    const unsigned packed=(unsigned(sample*255.f)<<8)|unsigned(next*255.f);
    check(writes[order]==std::pair(0x200+4*i,packed),"Uncapped linear fog table and descending writes");
   }
   check(writes[128]==std::pair(0xb8u,0xc310u),"Reset density100000 packedC310");
   check(writes[129]==std::pair(0xb0u,0x00ffffffu),"Reset fog white");
   if(kind!=4)check(m.read32(0xc980250)==0&&m.read32(0xc980254)==0,"Graphics reset clears instance env offsets through1D8380");
  }
  ++cases;
 };
 // Boot tail, common graphics reset, fresh selection Init, common result Init.
 run(0xc1cea3e,0xc1cea4c,0,0);
 run(0xc059f80,stop,1,0);
 run(0xc0522a0,0xc0522b0,2,0);
 run(0xc06fa2c,0xc06fa3c,3,0);
 // Actual attract switch predicate, not a guessed reset at every child.
 for(unsigned child=0;child<18;++child)run(0xc02e044,0xc02e05e,4,child);
 std::cout<<"PASS "<<cases<<" original fog reset/parent routes, "<<checks<<" checks / "<<instructions<<" original instructions. Boot/common/fresh-selection/result reset full-range table; attract reset occurs only children11/12. Actual table/density/color algorithms execute with final register and non-fog graphics boundaries only. No device/whole-runtime/userdata.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
