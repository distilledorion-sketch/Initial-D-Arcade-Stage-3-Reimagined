#include "../tests/sh4_scalar_reference.h"
#include <iostream>
using namespace idas3::reference;
constexpr unsigned object=0xcd00000;
unsigned checks=0;std::uint64_t instructions=0;
void eq(unsigned a,unsigned b){++checks;if(a!=b)throw std::runtime_error(hex(a)+" != "+hex(b));}
void run(RefCpu&cpu,unsigned start,unsigned stop){instructions+=cpu.run(start,stop,1000);}
int main(int argc,char**argv){try{
 if(argc!=2)throw std::runtime_error("image argument required");RefMemory m(argv[1]);
 for(unsigned course:{3u,4u,8u}){
  m.clear();m.zeroRegion(object,4096);m.zeroRegion(0xc31c99c,4096);
  m.write32(0xc31c9a0,course);m.write32(object+436,31);m.write32(object+460,30);
  m.write32(object+452,2);m.write32(object+468,course==3?3:course==4?2:1);
  RefCpu c(m);c.r[11]=object;run(c,0xc138740,0xc138808);
  eq(m.read32(object+452),3);eq(m.read32(object+440),0);eq(m.read32(object+444),1);
  unsigned fade=0,stop=0;
  for(unsigned tick=1;tick<=16;++tick){
   RefCpu phase(m);phase.r[11]=object;run(phase,0xc138840,tick==16?0xc138856:0xc138912);
   eq(m.read32(object+440),tick);
   if(tick==16){
    // Source coordinate/resource commit is outside this timer slice.
    // Execute the original phase increment after that successful commit.
    RefCpu complete(m);complete.r[11]=object;run(complete,0xc1388a0,0xc138912);
   }
   m.write32(0xc31de08,object+2048);
   RefCpu wrapper(m);wrapper.r[8]=object;wrapper.r[2]=object+444;
   wrapper.callHooks[0xc1431e0]=[&](auto&){++fade;eq(tick,1);};
   wrapper.callHooks[0xc143140]=[&](auto&){++stop;eq(tick,16);};
   run(wrapper,0xc135642,0xc13568e);
   eq(fade,1);eq(stop,tick==16?1:0);
  }
 }
 // Legend confirmation has a separate accepted tick, then121/8/5 updates.
 m.clear();m.zeroRegion(object,4096);m.write32(object+476,2);
 unsigned total=1;
 for(unsigned tick=1;tick<=121;++tick){RefCpu c(m);c.r[10]=object;run(c,0xc132220,0xc1322bc);++total;eq(m.read32(object+476),tick==121?3:2);}
 for(unsigned tick=1;tick<=8;++tick){RefCpu c(m);c.r[10]=object;run(c,0xc132240,0xc1322bc);++total;eq(m.read32(object+476),tick==8?4:3);}
 for(unsigned tick=1;tick<=5;++tick){RefCpu c(m);c.r[10]=object;run(c,0xc1322a0,0xc1322bc);++total;eq(m.read32(object+64),tick==5?1:0);}
 eq(total,135);
 std::cout<<"PASS selection exit instruction slices: "<<checks<<" comparisons, "<<instructions<<" instructions; TA31+16, Legend135; only manager output hooks and explicit successful resource-commit boundary\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
