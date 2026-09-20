#pragma once
// Isolated RAM-only ACar light fixture. Geometry/resource leaves are boundaries;
// all ACar-owned light construction, ARRAY and ambient stores execute.
#include "sh4_scalar_reference.h"
#include <iostream>
namespace idas3::reference {
struct CarLightReference {
 RefMemory m;RefCpu c;
 static constexpr unsigned car=0x0d000000,course=0x0d004000,rival=0x0d008000;
 static constexpr unsigned ex=0x0d020000,node=ex+0x100,stack=0x0d080000,stop=0x0f000000;
 unsigned allocation=0x0d100000;std::size_t instructions=0;
 std::vector<unsigned> fsca;
 CarLightReference(const char*image,const char*trig):m(image),c(m),fsca(32768){
  std::ifstream fs(trig,std::ios::binary);fs.seekg(16);fs.read(reinterpret_cast<char*>(fsca.data()),131072);if(!fs)throw std::runtime_error("FSCA table");
  m.zeroRegion(car,0x200000);m.zeroRegion(0x0c92f000,0x10000);m.zeroRegion(0x0c98ad0c,12);m.zeroRegion(0x0ce00000,0x10000);
  m.write16(0x0c98ad0e,32);m.write32(0x0c98ad10,0x0ce00000);m.write32(0x0c98ad14,0x0ce00000);
  m.write32(ex+4,node);m.write32(node,node);m.write32(node+4,node);
  c.fscaHalfWave=fsca;c.r[15]=stack;c.pr=stop;
  c.callHooks[0x0c221fc0]=[&](auto&q){q.r[0]=ex;};
  c.callHooks[0x0c021960]=[&](auto&q){if(allocation+0x1000>car+0x200000)throw std::runtime_error("Light allocation bound");q.r[0]=allocation;allocation+=0x1000;};
  c.callHooks[0x0c026360]=[](auto&q){q.r[0]=q.r[4];}; // Base geometry constructor.
  c.callHooks[0x0c034da0]=[](auto&){};c.callHooks[0x0c034e00]=[](auto&){}; // Body/shadow geometry modifiers.
  c.callHooks[0x0c057920]=[](auto&q){q.r[0]=q.r[4];}; // Private lightobj bank resource.
  c.callHooks[0x0c034fa0]=[](auto&){}; // Separate projected-headlight mesh/query owner.
  c.callHooks[0x0c1dbe80]=[](auto&){};c.callHooks[0x0c21b460]=[](auto&){};
 }
 void run(unsigned entry,unsigned end=stop,unsigned limit=300000){c.pr=stop;try{instructions+=c.run(entry,end,limit);}catch(...){std::cerr<<"Car light reference entry="<<hex(entry)<<" pc="<<hex(c.pc)<<" pr="<<hex(c.pr)<<" r4="<<hex(c.r[4])<<" r14="<<hex(c.r[14])<<'\n';throw;}}
 void carConstructor(unsigned p,unsigned carId){c.r[4]=p;c.r[5]=carId;c.r[6]=0;c.r[7]=0;c.r[15]=stack;run(0x0c033a00);}
 void courseConstructor(unsigned courseId,bool night,bool wet){
  m.write32(0x0c92ea7c,courseId);m.write32(0x0c92ea78,night);m.write32(0x0c92ea74,wet);
  if(courseId==4){run(0x0c041ce0);c.r[4]=course;run(0x0c03a0a0);m.write32(course,0x0c38213c);m.write32(course+52,night);m.write32(course+56,wet);c.r[4]=course;run(0x0c040d60);}
  else{c.r[4]=course;run(0x0c19acc0);c.r[4]=course;run(0x0c19afa0);m.write32(course+52,night);m.write32(course+56,wet);}
 }
 void borrow(unsigned target,unsigned courseId,bool night,bool wet){
  // Vtables closed by the actual all-course roster constructor fixture.
  // Read the canonical adjustment/function pair; do not substitute a host
  // callback for virtual+112 or its ACar+2392 nested registrations.
  static constexpr unsigned vtables[]{0x0c38de4c,0x0c38df24,0x0c38e1ac,0x0c38e284,0x0c38dffc,0x0c38e0d4,0x0c38dc9c,0x0c38dd74,0x0c38213c,0x0c38213c,0x0c38e50c,0x0c38e5e4,0x0c38e35c,0x0c38e434,0x0c38e6bc,0x0c38e794,0x0c38dd74,0x0c38dd74};
  const unsigned table=vtables[courseId*2+unsigned(night||(wet&&courseId!=4))];m.write32(course,table);
  c.r[4]=course+std::int16_t(m.read16(table+112));c.r[5]=target+2392;run(m.read32(table+116));
 }
 std::vector<std::array<unsigned,8>> packets(unsigned set,const std::array<float,16>&matrix){
  run(0x0c1cefe0);c.callHooks[0x0c1cf0a0]=[](auto&){};c.r[4]=set;run(0x0c0538a0);c.callHooks.erase(0x0c1cf0a0);
  const unsigned count=std::popcount(m.read32(0x0c92f704+8)&65535u);std::vector<std::array<unsigned,8>> out(count);
  c.callHooks[0x0c1d0fc0]=[](auto&){};c.callHooks[0x0c1d1000]=[](auto&){};c.callHooks[0x0c1d3d00]=[](auto&){};
  c.callHooks[0x0c1d3a60]=[&](auto&q){unsigned index=m.read8(q.r[4]);if(index>=count)return;
   RefCpu d(m);d.r[15]=stack-0x10000;d.r[4]=q.r[4];d.r[5]=stack-0x20000;d.pr=stop;d.fscaHalfWave=fsca;
   d.callHooks[0x0c2223e0]=[](auto&q){if(q.r[4]!=0xff000000||q.r[5]!=256)throw std::runtime_error("Unexpected divider");q.fpul=q.r[4]/q.r[5];};
   m.write32(0x0c99a16c,32);instructions+=d.run(0x0c1d3860,stop,10000);
   for(unsigned j=0;j<8;++j){out[index][j]=m.read32(stack-0x20000+4*(7-j));if(j==1)out[index][j]|=index;}
  };
  for(unsigned j=0;j<16;++j)c.xf[j]=std::bit_cast<unsigned>(matrix[j]);run(0x0c1cf0a0);
  c.callHooks.erase(0x0c1d3d00);c.callHooks.erase(0x0c1d3a60);return out;
 }
 void dumpArray(unsigned p){const auto set=m.read32(p+2540);std::cout<<"array "<<std::hex<<set<<" count "<<std::dec<<m.read32(set+68)<<" ambient";for(unsigned j=0;j<3;++j)std::cout<<' '<<std::hex<<m.read32(set+72+4*j);std::cout<<" gain "<<m.read32(set+88)<<std::dec<<'\n';}
};
}
