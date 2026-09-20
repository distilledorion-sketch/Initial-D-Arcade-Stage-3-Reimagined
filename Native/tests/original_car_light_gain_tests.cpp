#include "original_car_light_gain.h"
#include "original_car_lighting_reference.h"
#include <iostream>
#include <map>
#include <algorithm>
using namespace idas3::reference;
using namespace idas3::original;
static unsigned bits(float v){return std::bit_cast<unsigned>(v);}
int main(int argc,char**argv)try{
 if(argc!=4)throw std::runtime_error("canonical image, project root, FSCA table required");
 const std::filesystem::path root=argv[2];std::uint64_t checks=0,instructions=0;unsigned constructors=0,lookups=0,parents=0,producers=0;
 auto check=[&](bool yes,const char*why){++checks;if(!yes)throw std::runtime_error(why);};
 // Full derived constructors. Only common geometry/resource hydration,
 // private geometry children and their filename/matrix operations are hooked.
 // Base03A0A0/19ACC0 and every derived owner's resource pointer store execute.
 constexpr unsigned entries[]={0xc19ea40,0xc19f4c0,0xc1a18c0,0xc1a2160,0xc19fde0,0xc1a0a80,0xc19d1a0,0xc19ddc0,0xc040ae0,0xc040ae0,0xc1a4480,0xc1a50a0,0xc1a2b80,0xc1a36e0,0xc1a6180,0xc1a6e40,0xc19ddc0,0xc19ddc0};
 constexpr const char* names[]{"k_ez","s_nm","h_hd","k_df","s_vh","s_uh","n_sy","k_tu","k_df"};
 for(unsigned course=0;course<9;++course)for(unsigned night=0;night<2;++night)for(unsigned wet=0;wet<2;++wet)for(unsigned reverse=0;reverse<2;++reverse){
  CarLightReference r(argv[1],argv[3]);r.m.write32(0xc92ea7c,course);r.m.write32(0xc92ea78,night);r.m.write32(0xc92ea74,wet);
  r.c.callHooks[0xc19c020]=[](auto&){};r.c.callHooks[0xc03a380]=[](auto&){};
  std::map<unsigned,std::string> resources;
  r.c.callHooks[0xc04e480]=[&](auto&q){std::string name;for(unsigned i=0;i<128&&r.m.read8(q.r[4]+i);++i)name+=char(r.m.read8(q.r[4]+i));q.r[0]=r.allocation;r.allocation+=0x1000;resources[q.r[0]]=name;};
  for(auto a:{0xc226980u,0xc1fcc60u,0xc1f68a0u,0xc1f65c0u,0xc191a40u})r.c.callHooks[a]=[](auto&){};
  r.c.callHooks[0xc1fbd60]=[&](auto&q){for(unsigned i=0;i<16;++i)r.m.writeFloat(q.r[4]+i*4,i%5==0?1.f:0.f);};
  r.c.callHooks[0xc083fc0]=[](auto&q){q.r[0]=q.r[4];};
  // Execute042700's weather remap and original switch table through branch.
  const unsigned scene=course*2+night,selected=scene+((scene!=8&&!night&&wet)?1:0);
  const unsigned branch=0xc042806+r.m.read16(0xc042844+2*selected);
  r.c.r[14]=r.stack-0x2000;r.c.r[0]=2496;r.m.write32(r.c.r[14]+2496,scene);r.m.write32(r.c.r[14]+2504,night);r.m.write32(r.c.r[14]+2732,wet);
  r.run(0xc0427d2,branch,1000);check(r.c.r[7]==selected*2,"Original constructor table selector");
  r.c.r[15]=r.stack;r.c.r[4]=r.course;r.c.r[5]=r.course+0x800;r.c.r[6]=r.course+0x1000;r.c.r[7]=r.course+0x1800;
  r.run(entries[selected],r.stop,1000000);
  auto gain=OriginalCarLightGain::load(root,course,night,wet,reverse);const auto table=r.m.read32(r.course+44);
  check(bool(table)==gain.hasTable(),"Full source derived constructor table availability");
  if(table)check(resources[table]=="/driveA/path/"+std::string(names[course])+"_sdw.bin","Exact source resource filename");
  instructions+=r.instructions;++constructors;
 }
 constexpr unsigned object=0xd000000,table=object+0x10000,path=object+0x30000,coordinate=path+0x1000,tls=path+0x2000,node=tls+0x100,stack=object+0x6ff00,stop=0xf000000;
 RefMemory m(argv[1]);m.zeroRegion(object,0x80000);m.write32(tls+4,node);m.write32(node,node);m.write32(node+4,node);m.write32(object+24,path);m.write32(path,0xc386194);m.write32(path+12,path+128);
 RefCpu c(m);c.callHooks[0xc221fc0]=[&](auto&q){q.r[0]=tls;};
 for(unsigned course=0;course<9;++course)for(bool reverse:{false,true}){
  auto gain=OriginalCarLightGain::load(root,course,false,false,reverse);m.write32(object+48,reverse);m.write32(object+44,gain.hasTable()?table:0);
  m.write32(path+128,unsigned(gain.values().size()));for(unsigned i=0;i<gain.values().size();++i)m.writeFloat(table+4*i,gain.values()[i]);
  const auto period=gain.hasTable()?unsigned(gain.values().size()-1):1u;
  // Every authored cell, exact boundaries, midpoint, and extrapolation clamps.
  for(unsigned index=0;index<period;++index)for(float fraction:{-.5f,0.f,.37f,1.f,1.5f}){
   m.write32(coordinate,index);m.writeFloat(coordinate+4,fraction);m.write32(coordinate+8,0x12345678);
   c.r[4]=object;c.r[5]=coordinate;c.r[15]=stack;c.pr=stop;instructions+=c.run(0xc03d100,stop,1000);
   check(c.fr[0]==bits(gain.evaluate(index,fraction)),"Exact03D100 stream interpolation/reverse/clamp");
   check(m.read32(node+4)==node,"Source gain TLS restored");++lookups;
  }
 }
 // Whole daytime parent including full two setters, original03D100, and
 // original035500. Late0693C0 graphics scheduling is the only parent hook.
 constexpr unsigned race=object+0x40000,car=race+0x1000,rival=race+0x3000,carArray=race+0x6000,rivalArray=race+0x7000;
 m.write32(race+1036,object);m.write32(race+1048,car);m.write32(race+1052,rival);m.write32(object+44,table);m.write32(object+48,0);m.writeFloat(table,.2f);m.writeFloat(table+4,.8f);
 for(unsigned p:{car,rival}){m.write32(p,0xc38120c);m.write32(p+2540,p==car?carArray:rivalArray);m.write32(p+2524,0);m.writeFloat(p+2528,p==car?.25f:.75f);}
 for(unsigned mode=0;mode<4;++mode)for(unsigned night=0;night<2;++night){m.write32(race+1640,mode);m.write32(race+1664,night);
  for(unsigned set:{carArray,rivalArray}){m.writeFloat(set+88,.913f);for(unsigned j=0;j<3;++j)m.writeFloat(set+72+4*j,.321f);}
  unsigned lateCalls=0;c.callHooks[0xc0693c0]=[&](auto&q){check(q.r[4]==race,"Final parent receives race");++lateCalls;};
  c.r[4]=race;c.r[15]=stack;c.pr=stop;instructions+=c.run(0xc069020,stop,5000);
  check(lateCalls==1,"Parent final callback");
  for(unsigned p:{car,rival}){const bool changed=!night&&(p==car||mode<2);const float fraction=p==car?.25f:.75f;
   const float expected=changed?std::fma(.2f,1.f-fraction,.8f*fraction):.913f;const auto set=p==car?carArray:rivalArray;
   check(m.read32(set+88)==bits(expected),"Source day/mode gain gate and035500 setter");
   for(unsigned j=0;j<3;++j)check(m.read32(set+72+4*j)==bits(.321f),"Source gain excludes ambient");
  }++parents;
 }
 // Actual ACar producer copies CURRENT actor XYZ and supplies retained
 // coordinate to path096200. The already-independent path test proves math;
 // here it is explicitly hooked to test caller and failure retention.
 constexpr unsigned actor=race+0x8000,frame=race+0x9000;
 m.write32(car+2520,path);m.write32(frame+172,tls);
 for(unsigned sample=0;sample<32;++sample){
  for(unsigned j=0;j<3;++j)m.writeFloat(actor+4*j,float(sample*13+j)*.37f);m.write32(car+2524,100+sample);m.writeFloat(car+2528,.137f);
  unsigned calls=0;c.callHooks[0xc096200]=[&](auto&q){++calls;check(q.r[4]==path&&q.r[5]==frame+132&&q.r[6]==car+2524,"ACar exact projection object/input/output");
   for(unsigned j=0;j<3;++j)check(m.read32(q.r[5]+4*j)==m.read32(actor+4*j),"Unshifted current actor XYZ");
   check(m.read32(q.r[6])==100+sample&&m.read32(q.r[6]+4)==bits(.137f),"Retained previous ACar coordinate input");if(sample&1){m.write32(q.r[6],500+sample);m.writeFloat(q.r[6]+4,.731f);}q.r[0]=sample&1;
  };
  c.r[12]=actor;c.r[13]=car;c.r[14]=frame;c.r[9]=132;c.r[8]=0xc229080;c.r[15]=stack;c.pr=stop;
  instructions+=c.run(0xc034a86,0xc034ac2,2000);check(calls==1,"One path projection per ACar publication");
  check(m.read32(car+2524)==((sample&1)?500+sample:100+sample),"No caller reset on projection failure");++producers;
 }
 std::cout<<"PASS "<<constructors<<" actual derived constructor/042700 selections, "<<lookups<<" full03D100 lookups, "<<parents<<" complete069020 parent gates, "<<producers<<" actual ACar projection adapters; "<<checks<<" checks / "<<instructions<<" original instructions. Geometry/resource hydration and final scene scheduling are explicit boundaries; no source math hooks in gain.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
