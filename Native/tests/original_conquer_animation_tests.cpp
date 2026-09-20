#include "original_conquer_animation.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <vector>
using namespace idas3::original;using namespace idas3::reference;
int main(int argc,char**argv)try{
 if(argc!=2)throw std::runtime_error("Canonical image required");RefMemory m(argv[1]);std::uint64_t checks=0,instructions=0;
 constexpr unsigned owner=0x0cd00000,model=0x0cd01000,vt=0x0cd02000,stack=0x0cfff000,done=0x00ed1000;
 auto eq=[&](unsigned a,unsigned b,const char* why){++checks;if(a!=b)throw std::runtime_error(std::string(why)+": "+hex(a)+" != "+hex(b));};
 for(unsigned course=0;course<9;++course){
  m.clear();m.zeroRegion(owner,0x10000);m.zeroRegion(stack-0x4000,0x5000);m.write32(owner+4,model);m.write32(model,vt);
  m.write32(vt+44,0x00ed0000);m.write32(vt+60,0x00ed0004);m.write32(owner+8,30);m.write32(owner+12,8);m.write32(owner+88,course);
  m.writeFloat(owner+124,course==4?std::bit_cast<float>(0x3f933333u):0.f);
  for(unsigned i=0;i<8;++i)m.writeFloat(owner+92+i*4,i<4?-1.f:1.f);
  OriginalConquerAnimation animation;animation.begin(course);unsigned cueCount=0;
  for(unsigned frame=1;frame<=302;++frame){
   m.write32(owner+16,frame);RefCpu c(m);c.r[4]=owner;c.r[15]=stack;c.pr=done;
   std::vector<OriginalConquerDraw> draws;std::vector<unsigned> cues;
   c.callHooks[0x0c1458c0]=[](auto&){}; // render setup only
   c.callHooks[0x00ed0000]=[&](auto& x){draws.push_back({x.r[5],0});};
   c.callHooks[0x00ed0004]=[&](auto& x){draws.push_back({x.r[5],m.readFloat(x.r[6])});eq(m.read32(x.r[6]+4),0,"translation Y");eq(m.read32(x.r[6]+8),0,"translation Z");};
   c.callHooks[0x0c141f80]=[&](auto& x){cues.push_back(x.r[4]);eq(x.r[5],1,"sound enable");};
   instructions+=c.run(0x0c0c1dc0,done,10000);const auto& actual=animation.step();
   eq(actual.count,unsigned(draws.size()),"draw count");eq(actual.cueCount,unsigned(cues.size()),"cue count");
   for(unsigned i=0;i<actual.count;++i){eq(actual.draws[i].chunk,draws[i].chunk,"chunk");eq(std::bit_cast<unsigned>(actual.draws[i].x),std::bit_cast<unsigned>(draws[i].x),"offset bits");}
   for(unsigned i=0;i<actual.cueCount;++i){eq(actual.cues[i],cues[i],"sound cue");++cueCount;}
   for(unsigned i=0;i<8;++i){eq(animation.phases[i],m.read32(owner+56+i*4),"layer phase");eq(animation.ages[i],m.read32(owner+24+i*4),"layer age");}
  }
  eq(cueCount,2,"one sound per arriving title");
 }
 std::cout<<"PASS conquered animation: all 9 courses x 302 frames, "<<checks<<" comparisons, "<<instructions<<" original instructions; draw indices, exact float offsets, phases, sound timing. Rendering virtuals captured, not emulated.\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}

