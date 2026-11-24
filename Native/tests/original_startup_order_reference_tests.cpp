#include "sh4_scalar_reference.h"
#include <algorithm>
#include <iostream>
using namespace idas3::reference;
int main(int argc,char**argv)try {
 if(argc!=2)throw std::runtime_error("Canonical image required");
 RefMemory m(argv[1]);unsigned cases=0,checks=0;std::uint64_t instructions=0;
 constexpr unsigned stack=0xd00f000,owner=0xd010000,tls=0xd011000,link=0xd012000,list=0xd013000,body=0xd014000,stop=0xf000000;
 auto eq=[&](unsigned a,unsigned b,const char*why){++checks;if(a!=b)throw std::runtime_error(std::string(why)+" "+hex(a)+" != "+hex(b));};
 for(unsigned mode=0;mode<4;++mode)for(unsigned kind=0;kind<4;++kind){
  m.clear();m.zeroRegion(0xd000000,0x50000);m.write32(tls+4,link);m.write32(list,body);
  m.write32(body+12,1);m.write32(body+16,0xffffffffu);m.write32(body+0x100,body+0x200);
  m.write32(owner+0x668,kind);m.write32(0xc31c99c,mode);m.write32(0xc31c9a0,0);m.write32(0xc31c9a8,0);m.write32(0xc31c9b8,0);
  RefCpu c(m);c.r[4]=owner;c.r[15]=stack;c.pr=stop;std::vector<unsigned> sequence;
  c.callHooks[0xc221fc0]=[&](auto&q){q.r[0]=tls;};
  c.callHooks[0xc055d60]=[](auto&){}; // diagnostic print only
  c.callHooks[0xc142100]=[](auto&){}; // audio scene reset boundary
  // These callbacks have separate original/numerical proofs. Here they are
  // explicit boundaries so the original construction parent orders them.
  for(unsigned a:{0xc062480u,0xc0625e0u,0xc063ca0u,0xc063d20u,0xc063f80u,0xc068c80u,0xc064820u,0xc0661a0u,0xc066880u,0xc0671a0u,0xc061520u,0xc062de0u,0xc063ce0u})
   c.callHooks[a]=[&,a](auto&){sequence.push_back(a);};
  c.callHooks[0xc05a300]=[&](auto&q){q.r[0]=list;}; // declared single clear TA-record
  c.callHooks[0xc161b20]=[&](auto&q){q.r[0]=body+0x100;};
  c.callHooks[0xc161be0]=[&](auto&q){q.r[0]=body+0x200;};
  c.callHooks[0xc1641e0]=[](auto&q){q.r[0]=0;};
  unsigned allocation=0xd020000;
  c.callHooks[0xc021960]=[&](auto&q){q.r[0]=allocation;allocation+=0x100;};
  for(unsigned a:{0xc14c4c0u,0xc14d760u,0xc14b880u})c.callHooks[a]=[](auto&q){q.r[0]=q.r[4];}; // event-model constructors
  try{instructions+=c.run(0xc061b00,0xc06211a,30000);}catch(const std::exception&e){throw std::runtime_error("mode "+std::to_string(mode)+" kind "+std::to_string(kind)+" at "+hex(c.pc)+": "+e.what());}
  auto index=[&](unsigned address){auto it=std::find(sequence.begin(),sequence.end(),address);if(it==sequence.end())throw std::runtime_error("Missing owner "+hex(address));return unsigned(it-sequence.begin());};
  eq(index(0xc062480),0,"Course constructor is first owner");
  eq(index(0xc0625e0),1,"Player construction follows course");
  eq(index(0xc064820)>index(0xc0625e0),1,"HUD follows player construction");
  eq(index(0xc062de0)>index(0xc064820),1,"Warmup follows HUD");
  eq(unsigned(std::count(sequence.begin(),sequence.end(),0xc062de0u)),60,"Exactly60 solver warmups");
  eq(unsigned(std::count(sequence.begin(),sequence.end(),0xc063ce0u)),60,"Exactly60 pose warmups");
  auto warmup=index(0xc062de0);eq(unsigned(sequence.size()),warmup+120,"No other owner interleaves warmup");
  for(unsigned i=0;i<60;++i){eq(sequence[warmup+2*i],0xc062de0,"Solver warmup order");eq(sequence[warmup+2*i+1],0xc063ce0,"Pose warmup order");}
  ++cases;
 }
 std::cout<<"PASS "<<cases<<" construction parent cases, "<<checks<<" checks, "<<instructions<<" original instructions. Explicit child/resource/TA-data boundaries; source061B00 orders course/player/HUD before its60 solver+pose warmups.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
