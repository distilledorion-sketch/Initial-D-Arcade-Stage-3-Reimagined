#include "original_showroom.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <stdexcept>
#include <bit>
using namespace idas3;
void require(bool v,const char* s){if(!v)throw std::runtime_error(s);}
int main(int argc,char** argv){try{
 if(argc!=2)throw std::runtime_error("Pass canonical image");
 reference::RefMemory m(argv[1]);
 constexpr unsigned obj=0x0c600000,poses=0x0c601000,stack=0x0c700000,stop=0x0c60ff00;
 m.zeroRegion(obj,0x10000);m.zeroRegion(stack-0x10000,0x20000);
 m.write32(obj+8,3);m.write32(obj+12,poses);
 OriginalShowroom native;
 std::uint64_t steps=0;
 for(unsigned tick=0;tick<600;++tick){
  native.advanceTicks();reference::RefCpu cpu(m);cpu.r[4]=obj;cpu.r[15]=stack;cpu.pr=stop;
  steps+=cpu.run(0x0c10f420,stop);
  for(unsigned i=0;i<3;++i)require(std::bit_cast<std::uint32_t>(native.yaw())==m.read32(poses+i*168+28),"Original yaw update mismatch");
 }
 for(unsigned car=0;car<35;++car){
  auto f=native.frame(car);require(std::bit_cast<unsigned>(f.carPosition.y)==m.read32(0x0c2f4ed8+4*car),"Rideheight table mismatch");
 }
 // Execute the original selected-car draw's literal/vector preparation block.
 reference::RefCpu camera(m);camera.r[14]=stack;camera.r[8]=0;
 bool called=false;
 camera.callHooks[0x0c1fc2a0]=[&](auto& c){
  const auto f=native.frame(0);const float values[]={f.eye.x,f.eye.y,f.eye.z,f.target.x,f.target.y,f.target.z};
  for(unsigned i=0;i<6;++i)require(m.read32((i<3?c.r[4]:c.r[5])+(i%3)*4)==std::bit_cast<unsigned>(values[i]),"Original camera literal mismatch");
  require(c.r[6]==0,"Original showroom camera roll mismatch");called=true;
 };
 steps+=camera.run(0x0c12eb6a,0x0c12eba6);
 require(called,"Camera block did not invoke source look-at");
 require(m.read16(0x0c12ec82)==0x2000,"Original showroom FOV phase mismatch");
 require(m.read32(0x0c266104)==0,"Original initial yaw is not zero");
 std::cout<<"Showroom:600 original yaw ticks,3 actors,35 ride heights,6 camera words passed; "<<steps<<" original instructions.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
