#include "original_showroom_details.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <iomanip>
using namespace idas3;
using namespace idas3::reference;
constexpr unsigned object=0x0d000000,poses=0x0d001000,stack=0x0d080000,stop=0x00ff0000;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void setup(RefMemory& m,RefCpu& c,const std::vector<unsigned>& fsca){
 m.clear();m.zeroRegion(object,0x100000);m.zeroRegion(0x0c98ad0c,12);m.zeroRegion(0x0ce00000,0x10000);
 m.write16(0x0c98ad0e,32);m.write32(0x0c98ad10,0x0ce00000);m.write32(0x0c98ad14,0x0ce00000);
 c.r[15]=stack;c.pr=stop;c.fscaHalfWave=fsca;
 for(unsigned i=0;i<16;++i)c.xf[i]=std::bit_cast<unsigned>(i%5==0?1.f:0.f);
}
int main(int argc,char**argv){try{
 require(argc==4,"canonical-image fsca-table output-json required");
 RefMemory m(argv[1]);const auto table=original::OriginalFscaTable::load(argv[2]);
 std::vector<unsigned> fsca(32768);std::ifstream fs(argv[2],std::ios::binary);fs.seekg(16);fs.read(reinterpret_cast<char*>(fsca.data()),131072);require(bool(fs),"FSCA data");
 std::size_t instructions=0,comparisons=0;
 for(unsigned car=0;car<35;++car)for(float yaw:{0.f,.7f,2.4f}){
  RefCpu c(m);setup(m,c,fsca);m.write32(object+12,poses);m.write32(object+16,0x0d0f0000);m.writeFloat(poses+28,yaw);
  c.r[4]=object;c.r[5]=0;c.r[6]=poses+0x100;
  c.callHooks[0x0c133aa0]=[&](auto& cpu){cpu.r[0]=car;};
  auto expected=OriginalShowroomShadow::assembly(car,yaw,table);unsigned draw=0;
  c.callHooks[0x0c1d7120]=[&](auto&cpu){require(cpu.r[4]==0x0d0f0000,"Unexpected shadow model");require(draw<2,"Extra shadow draw");
   for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col){++comparisons;
    if(std::bit_cast<unsigned>(expected.instances[draw].transform[row*4+col])!=cpu.xf[col*4+row])throw std::runtime_error("Shadow matrix differs car "+std::to_string(car)+" draw "+std::to_string(draw)+" component "+std::to_string(row*4+col));}
   ++draw;};
  instructions+=c.run(0x0c10fa00,stop,10000);require(draw==2,"Missing original shadow draw");require(m.read16(0x0c98ad0c)==0,"Unbalanced source matrix stack");
 }
 std::ofstream out(argv[3]);require(bool(out),"Capture JSON output");out<<std::setprecision(9);
 // Original scene constructor arithmetic executes; allocations and independent
 // object constructors are explicit boundaries to inspect their exact inputs.
 RefCpu c(m);setup(m,c,fsca);const unsigned exceptions=object+0x2000,node=exceptions+0x100;
 m.write32(exceptions+4,node);m.write32(node,node);m.write32(node+4,node);
 c.callHooks[0x0c221fc0]=[&](auto&cpu){cpu.r[0]=exceptions;};
 c.callHooks[0x0c023e20]=[](auto&cpu){cpu.r[0]=cpu.r[4];};
 unsigned allocation=object+0x10000;
 c.callHooks[0x0c021960]=[&](auto&cpu){cpu.r[0]=allocation;allocation+=0x1000;};
 for(unsigned address:{0x0c0530a0u,0x0c0a7500u})c.callHooks[address]=[](auto&cpu){cpu.r[0]=cpu.r[4];};
 auto vector=[&](unsigned address){out<<'['<<m.readFloat(address)<<','<<m.readFloat(address+4)<<','<<m.readFloat(address+8)<<']';};
 out<<"{\"hooks\":[\"221FC0 exception context\",\"023E20 base constructor\",\"021960 allocator\",\"0530A0 chain constructor\",\"0A7500 camera constructor\",\"054C60 spotlight constructor inputs\",\"053FC0 parallel constructor inputs\",\"0535A0 chain insertion\"],";
 c.callHooks[0x0c054c60]=[&](auto&cpu){out<<"\"spotlight\":{\"position\":";vector(cpu.r[5]);out<<",\"direction\":";vector(cpu.r[6]);out<<",\"angle_units\":["<<cpu.r[7]<<','<<m.read32(cpu.r[15])<<"],\"color\":["<<cpu.getFloat(4)<<','<<cpu.getFloat(5)<<','<<cpu.getFloat(6)<<"],\"extra_float\":"<<cpu.getFloat(7)<<"},";cpu.r[0]=cpu.r[4];};
 c.callHooks[0x0c053fc0]=[&](auto&cpu){out<<"\"parallel\":{\"direction\":";vector(cpu.r[5]);out<<",\"color\":["<<cpu.getFloat(4)<<','<<cpu.getFloat(5)<<','<<cpu.getFloat(6)<<"],\"extra_float\":"<<cpu.getFloat(7)<<"},";cpu.r[0]=cpu.r[4];};
 unsigned links=0;c.callHooks[0x0c0535a0]=[&](auto&){++links;};
 c.r[4]=object;instructions+=c.run(0x0c112260,stop,30000);require(links==2,"Original light chain does not contain two lights");
 out<<"\"chain_ambient\":";vector(m.read32(object+0x140)+40);out<<",\"shadow_comparisons\":"<<comparisons<<",\"original_instructions\":"<<instructions<<"}\n";
 std::cout<<"PASS showroom shadows:35 cars x3 yaw poses x2 draw matrices; "<<comparisons<<" word comparisons. Light constructor inputs captured; "<<instructions<<" original instructions.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
