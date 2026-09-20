#include "sh4_scalar_reference.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
using namespace idas3::reference;
int main(int argc,char**argv)try{
 if(argc!=3)throw std::runtime_error("canonical image, project root");
 RefMemory m(argv[1]);std::ifstream in(std::filesystem::path(argv[2])/"data/original_assets/headlight_projection/source_chunk7.bin",std::ios::binary);
 std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(in),{}};if(bytes.size()!=576)throw std::runtime_error("source chunk7 shape");
 constexpr unsigned car=0xd000000,obj=car+0x10000,mesh=car+0x20000,stack=car+0x3ff00,sq=0xe0010000,stop=0xf000000;
 constexpr unsigned prefs[]={0xc1dbff2,0xc1dc024,0xc1dc040,0xc1dc064,0xc1dc082,0xc1d80f4,0xc1d810c};
 unsigned checks=0,instructions=0,cases=0;auto check=[&](bool b,const char*s){++checks;if(!b)throw std::runtime_error(s);};
 for(unsigned globalPCW:{0u,0x80u,0x10000u,0x80000000u})for(unsigned globalTSP:{0u,0x800000u,0x200000u,0x6000u,0xfc000000u,0x3000000u}){
  m.clear();m.zeroRegion(car,0x40000);m.zeroRegion(0xc980000,0x10000);m.zeroRegion(sq,0x1000);
  for(unsigned pc:prefs){check((m.read16(pc)&0xf0ff)==0x0083,"actual queue prefetch");m.write16(pc,9);}
  for(unsigned i=0;i<bytes.size();++i)m.write8(mesh+i,bytes[i]);
  m.write8(car+81,1);m.write32(car+2636,obj);m.write32(car+2468,0xc380b8c);m.write32(obj,0xc38760c);m.write32(obj+8,mesh);
  for(unsigned i=0;i<16;++i)m.writeFloat(car+2404+i*4,i%5==0?1.f:0.f);
  m.write32(0xc980264,0x10010000);m.write32(0xc989ff0,globalPCW);m.write32(0xc989ff4,globalTSP);m.write32(0xc37f110,0);
  m.writeFloat(0xc980234+16,.1f);m.writeFloat(0xc980234+20,10000.f);m.writeFloat(0xc980234+24,1.f);
  RefCpu c(m);for(unsigned i=0;i<16;++i)c.xf[i]=std::bit_cast<unsigned>(i%5==0?1.f:0.f);
  c.r[4]=car;c.r[15]=stack;c.pr=stop;unsigned submits=0,advances=0;
  c.callHooks[0xc205640]=[&](auto&q){check(q.r[4]==2,"translucent original queue list2");check(m.read32(q.r[5]+4)==224,"source224byte instance/model packet");++submits;};
  c.callHooks[0xc0d50a0]=[&](auto&q){check(submits==1&&q.r[4]==obj,"projection advances after submission");++advances;};
  instructions+=c.run(0xc034e60,stop,10000);check(submits==1&&advances==1,"one draw/update");
  check(m.read32(sq+160)==(0x08000800|globalPCW),"model command carries globalPCW");
  check(m.read32(sq+164)==(globalPCW&0x80000000?0x10000000u:0x18000000u),"model cull sign");
  check(m.read32(sq+168)==globalTSP,"final modelTSP is989FF4 verbatim");
  check(m.read32(sq+176)==((mesh+96)&0x01fffff8|0x80000000),"submitted original GMP/ICH payload pointer");
  check(m.read32(sq+184)==480,"submitted exact GMP/ICH payload size");
  for(unsigned i=0;i<bytes.size();++i)check(m.read8(mesh+i)==bytes[i],"full source mesh/material unchanged by submission");
  check(m.read32(0xc989ff0)==globalPCW&&m.read32(0xc989ff4)==globalTSP,"headlight owner preserves global overrides");
  const unsigned tsp=m.read32(mesh+168)^m.read32(sq+168); // primary Elan setStateParams
  if(!globalTSP){check(tsp==0x4489a464,"effective default TSP");check((tsp>>24&3)==0,"no accumulation selects");check((tsp>>29&7)==2&&(tsp>>26&7)==1,"DST_COLOR/ONE");}
  ++cases;
 }
 unsigned restores=0;
 // Generic fade/reflection helpers can alter blend bits temporarily. Verify
 // their exact return behavior so a prior car helper is not assumed to leak
 // its temporary override into this separate headlight submission.
 for(unsigned helper:{0xc1d73a0u,0xc1d7640u})for(unsigned globalTSP:{0u,0x12345678u,0xfc000000u,0xffffffffu})for(unsigned opaque:{0u,1u})for(unsigned a=0;a<3;++a)for(unsigned b=0;b<3;++b){
  m.clear();m.zeroRegion(car,0x40000);m.zeroRegion(0xc980000,0x10000);
  for(unsigned i=0;i<bytes.size();++i)m.write8(mesh+i,bytes[i]);
  if(opaque){m.write32(mesh+64,32);m.write32(mesh+68,480);} // same bounded GMP/ICH payload in either list
  m.write32(0xc989ff4,globalTSP);m.write32(0xc985f64,0x12);
  RefCpu c(m);c.r[4]=mesh;c.r[5]=a;c.r[6]=b;c.r[15]=stack;c.pr=stop;
  c.callHooks[0xc1d8060]=[&](auto&q){check(q.r[5]==0||q.r[5]==2,"generic helper uses opaque/translucent submission only");};
  instructions+=c.run(helper,stop,10000);check(m.read32(0xc989ff4)==globalTSP,"source helper restores preexisting modelTSP on every tested branch");++restores;
 }
 std::cout<<"PASS "<<cases<<" actual headlight submissions + "<<restores<<" scoped override restoration cases, "<<checks<<" checks / "<<instructions<<" original instructions. Full034E60/228A60/0D7EC0/1D7120/1D8060/1DBFE0; exact576 mesh bytes unchanged.989FF4 forwarded verbatim as modelTSP; primary effective TSP=authored XOR override. Full1D73A0/1D7640 restore989FF4 after temporary changes. Test varies overrides; it does not prove every live scene's prior989FF4 value.7 verified SQ PREF transfers and final queue/advance leaves are explicit boundaries.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
