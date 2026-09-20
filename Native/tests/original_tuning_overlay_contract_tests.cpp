#include "sh4_scalar_reference.h"
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace idas3::reference;
int main(int argc,char**argv)try{
 if(argc!=2)throw std::invalid_argument("canonical image required");
 RefMemory m(argv[1]);constexpr unsigned car=0x0d000000,stack=0x0d010000,stop=0x0f000000;
 m.zeroRegion(car,0x2000);m.zeroRegion(stack,0x2000);std::size_t instructions=0,checks=0;
 auto check=[&](bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);};
 // Actual constructor writes; no inferred zero-initialization for these fields.
 m.write32(stack+48,car);RefCpu ctor(m);ctor.r[0]=car;ctor.r[14]=stack;
 instructions+=ctor.run(0x0c026402,0x0c02642c,40);
 check(m.read8(car+212)==1&&m.read8(car+213)==1,"constructor current matrices");
 check(m.read32(car+216)==0&&m.read32(car+220)==0,"constructor layer parameters");
 check(m.read32(car+224)==3&&m.read32(car+228)==3,"constructor layer/effect mask");
 RefCpu candidate(m);candidate.r[9]=car;
 instructions+=candidate.run(0x0c12b78e,0x0c12b794,8);
 check(m.read32(car+224)==2,"candidate layer mask");
 // Parent11E260 owns the car array consumed by the three setup children.
 // Its exact selector3/rebuild/layer2 sequence precedes the child routes.
 constexpr unsigned parent=0x0d002000,cars=0x0d003000;
 m.zeroRegion(parent,0x1000);m.zeroRegion(cars,0x1000);
 for(unsigned index=0;index<4;++index){
  m.write32(stack+380,parent);m.write32(stack+384,index*4);m.write32(parent+8,cars);
  m.write32(cars+index*4,car);m.write32(car+224,3);std::vector<unsigned> calls;
  RefCpu setup(m);setup.r[14]=stack;setup.r[15]=stack+0x1000;
  setup.callHooks[0x0c029da0]=[&](auto& c){check(c.r[4]==car&&c.r[5]==3,"parent menu environment selector3");calls.push_back(0x029da0);};
  setup.callHooks[0x0c029040]=[&](auto& c){check(c.r[4]==car,"parent material rebuild owner");calls.push_back(0x029040);};
  instructions+=setup.run(0x0c11e4fc,0x0c11e54c,100);
  check(calls==std::vector<unsigned>{0x029da0,0x029040}&&m.read32(car+224)==2,"parent environment/rebuild/layer order");
 }
 // The secondary layer explicitly pushes current when constructor byte213=1.
 for(unsigned current=0;current<=1;++current){
  m.write8(car+213,std::uint8_t(current));unsigned matrixArg=~0u;
  RefCpu matrix(m);matrix.r[13]=car;matrix.r[15]=stack+0x1000;
  matrix.callHooks[0x0c1f6610]=[&](auto& c){matrixArg=c.r[4];};
  instructions+=matrix.run(0x0c027fac,0x0c027fc8,30);
  check(matrixArg==(current?0u:car+148),"secondary matrix selection");
 }
 m.write8(car+213,1);
 // The exact caller loads216->FR4 and220->FR5, then calls1D8380.
 RefCpu parameters(m);parameters.r[13]=car;parameters.r[15]=stack+0x1000;
 instructions+=parameters.run(0x0c027fc8,0x0c027fd6,100);
 check(m.read32(0x0c980234+28)==0&&m.read32(0x0c980234+32)==0,"source layer parameter globals");
 // State21 is the ELAN instance-matrix writer on lists0/2/4. Hook only the
 // downstream device submissions, not the list-mask iteration or arguments.
 std::vector<unsigned> lists;unsigned matrixWrites=0;
 RefCpu layer(m);layer.r[4]=21;layer.r[15]=stack+0x1000;layer.pr=stop;
 layer.callHooks[0x0c1d7e20]=[&](auto&){++matrixWrites;};
 layer.callHooks[0x0c205640]=[&](auto& c){lists.push_back(c.r[4]);check(c.r[5]==0x0cb0a564,"layer submitted instance buffer");};
 instructions+=layer.run(0x0c1d78a0,stop,300);
 check(matrixWrites==3&&lists==std::vector<unsigned>{0,2,4},"layer graphics-list routes");
 std::cout<<"PASS "<<checks<<" checks / "<<instructions<<" original instructions.\n"
  <<"Constructor: primaryUsesCurrent=1 secondaryUsesCurrent=1 parameters216/220=0/0; layers224=3 effects228=3.\n"
  <<"Candidate: layers224=2. Secondary matrix = existing current car/part matrix; stored matrix+148 unused.\n"
  <<"Parent11E260: all4car-array slots use029DA0(car,3),029040(car),layers224=2 in that order.\n"
  <<"Layer state:1D8380 writes globals980250/980254=0;1D78A0(21) submits1D7E20 on lists0,2,4.\n"
  <<"Boundary:1D7E20 ELAN packet math/raster semantics not exercised here. No full runtime or device used.\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
