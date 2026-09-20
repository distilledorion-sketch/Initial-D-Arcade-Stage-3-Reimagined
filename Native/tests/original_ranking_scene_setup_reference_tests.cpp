#include "sh4_scalar_reference.h"
#include "original_ranking_scene_setup.h"
#include <iostream>
#include <iomanip>
using namespace idas3::reference;
int main(int argc,char**argv){try{
 if(argc!=3)throw std::runtime_error("canonical-image fsca-table required");
 RefMemory m(argv[1]);RefCpu c(m);constexpr unsigned obj=0x0d000000,ex=obj+0x2000,node=ex+0x100,stop=0x00ff0000;
 m.zeroRegion(obj,0x100000);m.zeroRegion(0xc92f000,0x10000);m.zeroRegion(0xc980000,0x20000);m.zeroRegion(0xcae9cb0,0x20);m.zeroRegion(0xce00000,0x10000);
 m.write16(0xc98ad0e,32);m.write32(0xc98ad10,0xce00000);m.write32(0xc98ad14,0xce00000);m.write32(ex+4,node);m.write32(node,node);m.write32(node+4,node);
 std::vector<unsigned> fsca(32768);std::ifstream fs(argv[2],std::ios::binary);fs.seekg(16);fs.read(reinterpret_cast<char*>(fsca.data()),131072);if(!fs)throw std::runtime_error("FSCA table");c.fscaHalfWave=fsca;
 c.r[15]=obj+0x80000;c.pr=stop;for(unsigned i=0;i<16;++i)c.xf[i]=std::bit_cast<unsigned>(i%5==0?1.f:0.f);
 c.callHooks[0xc221fc0]=[&](auto&cpu){cpu.r[0]=ex;};unsigned allocation=obj+0x10000;c.callHooks[0xc021960]=[&](auto&cpu){cpu.r[0]=allocation;allocation+=0x1000;};
 const auto invoke=[&](unsigned pc,unsigned r4){c.r[4]=r4;c.pr=stop;try{return c.run(pc,stop,100000);}catch(...){std::cerr<<"source failurePC "<<hex(c.pc)<<" r4 "<<hex(c.r[4])<<" r0 "<<hex(c.r[0])<<'\n';throw;}};
 std::size_t steps=invoke(0xc0530a0,obj);const auto chain=c.r[0];
 m.writeFloat(obj+0x4000,0);m.writeFloat(obj+0x4004,-1);m.writeFloat(obj+0x4008,-1);steps+=invoke(0xc1f6cf0,obj+0x4000);
 c.r[5]=obj+0x4000;for(unsigned i=4;i<=6;++i)c.setFloat(i,.7f);c.setFloat(7,0);steps+=invoke(0xc053fc0,obj+0x5000);const auto light=c.r[0];
 c.r[5]=light;steps+=invoke(0xc0535a0,chain);for(unsigned i=40;i<=48;i+=4)m.writeFloat(chain+i,.2f);
 steps+=invoke(0xc1cefe0,0);c.callHooks[0xc1cf0a0]=[](auto&){};steps+=invoke(0xc053480,chain);
 m.write32(0xc9801ec,1);m.write32(0xc980264,0x09000000);c.r[4]=0xc92f704;c.pr=stop;steps+=c.run(0xc1d3d00,0xc1d3e10,10000);
 for(unsigned i=0;i<8;++i)if(m.read32(c.r[7]+i*4)!=idas3::original::OriginalRankingSceneSetup::glmWords[i])throw std::runtime_error("Ranking GLM differs");
 for(unsigned i=0;i<3;++i){
  constexpr unsigned direction[]{0,0xbf3504f3,0xbf3504f3};
  if(m.read32(0xc92f110+i*4)!=direction[i])throw std::runtime_error("Ranking incoming light differs");
  if(m.read32(0xc92f2cc+i*4)!=0x3f333333)throw std::runtime_error("Ranking light RGB differs");
 }
 std::cout<<"LIGHT instructions "<<steps<<" glm ";for(unsigned i=0;i<8;++i)std::cout<<hex(m.read32(c.r[7]+i*4))<<' ';std::cout<<"\n";
 for(unsigned a:{0xc92f104u,0xc92f2c4u}){std::cout<<hex(a)<<' ';for(unsigned i=0;i<(a==0xc92f104?7u:17u);++i)std::cout<<hex(m.read32(a+i*4))<<' ';std::cout<<"\n";}
 steps=invoke(0xc1d0940,obj+0x6000);
 for(unsigned i=0;i<32;++i)if(m.read32(obj+0x6000+i*4)!=std::bit_cast<unsigned>(i%16%5==0?1.f:0.f))throw std::runtime_error("Ranking default camera matrices differ");
 constexpr unsigned camera[]{0,0x43f00000,0,0x44200000,0x3c23d70a,0x461c4000,8192,0,0x80000000,0x44200000,0x43f00000,0x3f800000,0x3f800000,0x3ed413cd,0x3faaaaab};
 for(unsigned i=0;i<15;++i)if(m.read32(obj+0x6000+128+i*4)!=camera[i])throw std::runtime_error("Ranking viewport/camera field differs");
 std::cout<<"CAMERA instructions "<<steps<<" words ";for(unsigned i=0;i<47;++i)std::cout<<hex(m.read32(obj+0x6000+i*4))<<' ';std::cout<<"\n";
 for(unsigned j=0;j<4;++j){const auto base=0xc98ad0c+j*12,save=0xce00000+j*0x1000;m.write32(base,0x00200000);m.write32(base+4,save);m.write32(base+8,save);for(unsigned i=0;i<16;++i)m.writeFloat(save+i*4,i%5==0?1.f:0.f);}
 c.callHooks[0xc1d9600]=[](auto&cpu){for(unsigned i=0;i<16;++i)if(cpu.xf[i]!=idas3::original::OriginalRankingSceneSetup::projectionWords[i])throw std::runtime_error("Ranking projection differs");std::cout<<"projectionXF ";for(auto x:cpu.xf)std::cout<<hex(x)<<' ';std::cout<<"\n";};steps+=invoke(0xc1d0880,obj+0x6000);
 for(unsigned i=0;i<16;++i)if(c.xf[i]!=std::bit_cast<unsigned>(i%5==0?1.f:0.f))throw std::runtime_error("Ranking enclosing matrix not identity");
 std::cout<<"PROJECTION instructions "<<steps<<" activeXF ";for(auto x:c.xf)std::cout<<hex(x)<<' ';std::cout<<"\n";
 for(unsigned a:{0xc989f80u,0xc989fc0u,0xc98a000u,0xc98a040u}){std::cout<<hex(a)<<' ';for(unsigned i=0;i<16;++i)std::cout<<hex(m.read32(a+i*4))<<' ';std::cout<<"\n";}
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}


