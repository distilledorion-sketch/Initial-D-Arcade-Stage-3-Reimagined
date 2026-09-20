#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3::reference;
int main(int argc,char**argv){try{
 if(argc!=2)throw std::runtime_error("Canonical image required");
 RefMemory mem(argv[1]);constexpr unsigned stack=0xd000000,record=0xd010000,context=0xd020000,stop=0xff0000;
 mem.zeroRegion(stack,0x10000);mem.zeroRegion(record,0x10000);mem.zeroRegion(context,0x10000);
 unsigned long long steps=0;
 for(unsigned scene=0;scene<18;++scene)for(unsigned wet=0;wet<2;++wet){
  RefCpu cpu(mem);cpu.r[14]=stack;cpu.r[7]=cpu.r[0]=scene;cpu.t=scene==8;
  mem.write32(stack+2504,scene&1);mem.write32(stack+2732,wet);
  steps+=cpu.run(0xc0427da,0xc0427fc,100);
  const unsigned expected=scene+((scene!=8&&!(scene&1)&&wet)?1:0);
  if(cpu.r[7]!=expected)throw std::runtime_error("Original weather callback selection mismatch");
 }
 mem.write32(0xc37f564,context);mem.write32(context+156,3);
 {RefCpu cpu(mem);cpu.r[4]=record;cpu.r[5]=512;cpu.r[6]=256;cpu.r[7]=0x707;cpu.r[15]=stack+0xf000;cpu.pr=stop;
  steps+=cpu.run(0xc2136a0,stop,10000);
  if(cpu.r[0]!=0||mem.read32(record+8)!=0x30000000||mem.read32(record+20)!=131072)throw std::runtime_error("Original PAL8 format mapping mismatch");
  cpu.r[4]=record;cpu.pr=stop;steps+=cpu.run(0xc1f7420,stop,1000);
  if(mem.read32(record+48)!=53||mem.read32(record+52)!=0x30000000)throw std::runtime_error("Original twiddled PAL8 TSP/TCW mismatch");
  std::cout<<"Original source0707 -> TCWformat="<<std::hex<<mem.read32(record+8)<<", flags="<<mem.read32(record+24)<<std::dec<<", bytes="<<mem.read32(record+20)<<'\n';}
 const unsigned table=mem.read32(0xc191aa8),paletteRam=mem.read32(0xc1f61cc);
 for(unsigned id=20;id<=24;++id){
  RefCpu cpu(mem);cpu.r[4]=id;cpu.r[5]=0;cpu.r[15]=stack+0xf000;cpu.pr=stop;
  // Hardware palette mode/cache binding boundaries; original palette copying executes unchanged.
  cpu.callHooks[0xc2123e0]=[](RefCpu&c){if(c.r[4]!=3)throw std::runtime_error("Not ARGB8888 palette");};
  cpu.callHooks[0xc1f77a0]=[](RefCpu&c){if(c.r[4]!=0)throw std::runtime_error("Unexpected course palette bank");};
  steps+=cpu.run(0xc191a40,0xc191a88,10000);
  const unsigned source=mem.read32(table+id*4);
  for(unsigned i=0;i<256;++i)if(mem.read32(paletteRam+i*4)!=mem.read32(source+i*4))throw std::runtime_error("Original palette upload differs");
  std::cout<<"palette"<<id<<" source="<<std::hex<<source<<std::dec<<": 256 original ARGB8888 words match\n";
 }
 std::cout<<"PASS: 36 original weather callback selections, source PAL8 format/size, five complete palettes; "<<steps<<" original instructions.\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
