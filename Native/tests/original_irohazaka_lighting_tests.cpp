// Isolated constructor/update proof. No device or original runtime startup.
#include "original_course_lighting.h"
#include "original_course_fog.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <random>
using namespace idas3::original;
using namespace idas3::reference;
int main(int argc,char**argv)try{
 if(argc!=2)throw std::runtime_error("canonical original image required");
 RefMemory m(argv[1]);constexpr unsigned owner=0xd000000,stack=0xd080000,tls=0xd002000,node=tls+256,query=0xd060000,sentinel=0xd070000,matrix=0xd050000,stop=0xf000000;
 unsigned checks=0,cases=0,omitted=0;std::size_t instructions=0;
 auto check=[&](bool good,const char*why){++checks;if(!good)throw std::runtime_error(why);};
 auto bits=[](float f){return std::bit_cast<unsigned>(f);};
 std::mt19937 rng(0x1a5580);std::uniform_real_distribution<float> unit(-1.f,1.f);
 std::array<OriginalLightVector,31> authored{};for(unsigned i=0;i<31;++i)for(unsigned j=0;j<3;++j)authored[i][j]=m.readFloat(0xc2a506c+i*12+j*4);
 for(unsigned row=20;row<24;++row){
  m.clear();m.zeroRegion(owner,0x100000);m.zeroRegion(0xc92f000,0x10000);m.zeroRegion(0xc98ad0c,16);m.zeroRegion(0xce00000,0x10000);
  m.write16(0xc98ad0e,32);m.write32(0xc98ad10,0xce00000);m.write32(0xc98ad14,0xce00000);
  m.write32(tls+4,node);m.write32(node,node);m.write32(node+4,node);
  RefCpu c(m);c.r[15]=stack;c.pr=stop;unsigned allocation=owner+0x10000;
  c.callHooks[0xc221fc0]=[&](auto&q){q.r[0]=tls;};
  c.callHooks[0xc021960]=[&](auto&q){q.r[0]=allocation;allocation+=0x1000;};
  c.callHooks[0xc21b460]=[](auto&){};c.callHooks[0xc1dbe80]=[](auto&){};
  m.write32(0xc92ea7c,5);m.write32(0xc92ea78,row/2%2);m.write32(0xc92ea74,row%2);
  auto run=[&](unsigned entry){c.r[4]=owner;c.r[15]=stack;c.pr=stop;instructions+=c.run(entry,stop,100000);};
  if(row>=22){
   // Full derived1A50A0 and common19BC80/19ACC0 constructors execute.
   // Resource hydration, geometry children, filename formatting and their
   // private transform stack are separate boundaries; none is a light ctor.
   c.callHooks[0xc19c020]=[](auto&){};c.callHooks[0xc03a380]=[](auto&){};
   c.callHooks[m.read32(0xc1a5314)]=[&](auto&q){q.r[0]=owner+0x90000;};
   for(auto a:{0xc226980u,0xc1fcc60u,0xc1f68a0u,0xc1f65c0u,0xc191a40u})c.callHooks[a]=[](auto&){};
   c.callHooks[0xc1fbd60]=[&](auto&q){for(unsigned i=0;i<16;++i)m.writeFloat(q.r[4]+i*4,i%5==0?1.f:0.f);};
   c.callHooks[0xc083fc0]=[](auto&q){q.r[0]=q.r[4];};
   try{run(0xc1a50a0);}catch(const std::exception&e){throw std::runtime_error("Iro derived constructor at"+hex(c.pc)+": "+e.what());}
  }else run(0xc19acc0);
  run(0xc19afa0);auto native=originalCourseLighting(5,row/2%2,row%2);
  check(m.read32(owner+64)==0,"Original course path index initializes zero");
  check(m.read32(m.read32(owner+60)+68)==native.count,"Actual registered light count");
  for(unsigned i=0;i<4;++i){const unsigned p=m.read32(owner+176+i*4);
   check(bool(p)==(row==22),"Iro four spot pointers allocated only for row22");
   if(p)check(m.read8(p+24)==1,"Actual spot constructor starts enabled");
  }
  m.write32(owner+52,row/2%2);m.write32(owner+56,row%2);m.write32(owner+80,matrix);
  m.write32(owner+24,query);m.write32(query,query+128);m.write32(query+128+44,0xff0020);
  c.callHooks[0xc1f6610]=[](auto&){};c.callHooks[0xc1f65c0]=[](auto&){};
  OriginalLightVector reference{};unsigned queries=0;
  c.callHooks[0xff0020]=[&](auto&q){check(q.r[4]==query&&q.r[5]==m.read32(owner+64),"Actual path query arguments");for(unsigned j=0;j<3;++j)m.writeFloat(q.r[6]+4*j,reference[j]);++queries;};
  // Source's erroneous authored-index write can touch pointers outside four
  // physical slots. Only these extra pointers are replaced by declared RAM
  // sentinels; the real four constructor-created pointers remain unchanged.
  if(row==22)for(unsigned i=4;i<31;++i)m.write32(owner+176+4*i,sentinel+128*i);
  for(unsigned sample=0;sample<287;++sample){
   reference=authored[sample%31];if(sample>=31)for(auto&v:reference)v+=unit(rng)*200.f;
   auto transform=originalLightIdentityMatrix;for(unsigned j=0;j<16;++j)if(j%4!=3)transform[j]=j<12?unit(rng):unit(rng)*320.f;
   for(unsigned j=0;j<16;++j)m.writeFloat(matrix+4*j,transform[j]);
   const auto oldQueries=queries;
   // Row23 has no registered or allocated spots. Stop before the source's
   // first invalid dereference; do not turn it into host memory operations.
   if(row==23){c.r[4]=owner;c.r[15]=stack;c.pr=stop;instructions+=c.run(0xc1a5580,0xc1a57ee,20000);c.r[15]=stack;
    check(m.read32(owner+176)==0,"Night/wet source first spot remains NULL");
   }else run(row&2?0xc1a5580:0xc19b060);
   const auto result=updateOriginalCourseLighting(native,transform,reference);
   check(result.coordinatesUpdated==(row==22),"Native registered-position update gate");
   check(queries-oldQueries==((row&2)?1u:0u),"Source path query gate");
   if(row==22){check(result.sourceEnableIndexOutsideFourSlots,"Unsafe source enable writes reported");++omitted;
    const unsigned frame=stack-32-532;
    for(unsigned i=0;i<4;++i){check(m.read32(frame+416+4*i)==result.authoredIndices[i],"Exact nearest four authored indices");
     const auto p=m.read32(owner+176+4*i);for(unsigned j=0;j<3;++j)check(m.read32(p+44+4*j)==bits(native.lights[i].position[j]),"Exact registered XYZ");
     check(m.read8(p+24)==1&&native.lights[i].enabled,"Original enabled1 persists; extra writes unnecessary for active lights");
    }
   }
   const auto set=m.read32(owner+60);for(unsigned i=0;i<native.count;++i){const auto p=m.read32(set+4+4*i);const auto&l=native.lights[i];check(m.read8(p+24)==l.enabled,"Registered enable parity");
    if(l.kind==OriginalCourseLightKind::RelativeParallel)for(unsigned j=0;j<3;++j)check(m.read32(p+44+4*j)==bits(l.incomingDirection[j]),"Full19B060 relative update parity");}
   ++cases;
  }
 }
 // Independent actual1CEB40 table generation and quantization, final register
 // write boundary only; verifies native bootstrap before Happo preserves it.
 m.clear();m.zeroRegion(owner,0x100000);RefCpu reset(m);reset.r[15]=stack;reset.pr=stop;
 std::vector<std::pair<unsigned,unsigned>> writes;reset.callHooks[0xc21b460]=[&](auto&q){writes.emplace_back(q.r[4],q.r[5]);};
 instructions+=reset.run(0xc1ceb40,stop,20000);const auto fog=originalBootstrapFog();check(writes.size()==130,"Original bootstrap write count");
 for(unsigned i=0;i<128;++i)check(writes[i]==std::pair(0x3fcu-4*i,unsigned(fog.table[127-i])),"Native bootstrap table equals original reset");
 check(writes[128]==std::pair(0xb8u,unsigned(fog.packedDensity))&&writes[129]==std::pair(0xb0u,fog.colorRgb),"Native bootstrap density/color");
 std::cout<<"PASS "<<cases<<" Iro source update cases, "<<checks<<" checks / "<<instructions<<" original instructions. "<<omitted<<" dry-night cases preserve enabled registered spots while omitting unsafe extra writes; wet-night has no allocated spots and stops before invalid source dereference. Native bootstrap matches full1CEB40.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
