// Development-only actual Happo reset/packet capture; canonical image and FSCA table arguments. No device startup.
#include "sh4_scalar_reference.h"
#include <iostream>
#include <iomanip>
using namespace idas3::reference;
int main(int argc,char**argv)try{
 if(argc!=3)throw std::runtime_error("canonical image and FSCA table required");
 RefMemory m(argv[1]);constexpr unsigned object=0x0d000000,ex=object+0x2000,node=ex+0x100,stop=0x0f000000;
 for(unsigned row=0;row<4;++row){
 m.clear();m.zeroRegion(object,0x100000);m.zeroRegion(0x0c92f000,0x10000);m.zeroRegion(0x0c98ad0c,12);m.zeroRegion(0x0ce00000,0x10000);
 m.write16(0x0c98ad0e,32);m.write32(0x0c98ad10,0x0ce00000);m.write32(0x0c98ad14,0x0ce00000);
 m.write32(ex+4,node);m.write32(node,node);m.write32(node+4,node);
 RefCpu c(m);c.r[15]=object+0x80000;c.pr=stop;
 std::vector<unsigned> fsca(32768);std::ifstream fs(argv[2],std::ios::binary);fs.seekg(16);fs.read(reinterpret_cast<char*>(fsca.data()),131072);if(!fs)throw std::runtime_error("FSCA table");c.fscaHalfWave=fsca;
 c.callHooks[0x0c221fc0]=[&](auto& cpu){cpu.r[0]=ex;};
 unsigned allocation=object+0x10000;c.callHooks[0x0c021960]=[&](auto&cpu){cpu.r[0]=allocation;allocation+=0x1000;};
 c.callHooks[0x0c21b460]=[&](auto&q){std::cout<<" fog "<<row<<' '<<std::hex<<q.r[4]<<' '<<q.r[5]<<std::dec<<'\n';};
 c.callHooks[0x0c1dbe80]=[](auto&){};
 // Startup221E20 dispatches ctor041CE0 from table2EEEC0 before any course.
 // Run its unchanged bytes: raw image2EFF30 is not the initialized RGB.
 std::size_t instructions=c.run(0x0c041ce0,stop,1000);
 std::cout<<" initializer "<<row<<" 041ce0 words";
 for(unsigned i=0;i<4;++i)std::cout<<' '<<std::hex<<m.read32(0x0c2eff30+4*i);
 std::cout<<std::dec<<'\n';
 m.write32(0x0c92ea7c,row/4);m.write32(0x0c92ea78,row/2%2);m.write32(0x0c92ea74,row%2);
 c.r[4]=object;c.pr=stop;instructions+=c.run(0x0c03a0a0,stop,100000);m.write32(object,0x0c38213c);m.write32(object+52,row/2);m.write32(object+56,row%2);
 c.r[4]=object;c.pr=stop;instructions+=c.run(0x0c040d60,stop,100000);
 std::cout<<"row "<<row<<" instructions "<<instructions<<" matrix "<<std::hex<<m.read32(object+80)<<std::dec<<" ambient "<<m.readFloat(object+124)<<' '<<m.readFloat(object+128)<<' '<<m.readFloat(object+132)<<"\n";
 for(unsigned slot:{0u,4u,8u,12u,16u,20u,24u,28u,32u,36u,40u}){
   const auto p=m.read32(object+1028+slot);if(!p)continue;
   std::cout<<" slot "<<slot<<" ptr "<<std::hex<<p<<" words";
   for(unsigned i=0;i<23u;++i)std::cout<<' '<<m.read32(p+4*i);std::cout<<std::dec<<'\n';
 }
 std::cout<<" registered "<<m.read32(m.read32(object+60)+68)<<"\n"; for(unsigned i=0;i<m.read32(m.read32(object+60)+68);++i){const unsigned p=m.read32(m.read32(object+60)+4+4*i);unsigned ownerOffset=0;for(unsigned off=0;off<1100;off+=4)if(m.read32(object+off)==p)ownerOffset=off;std::cout<<" registeredObject "<<i<<" ownerOffset "<<ownerOffset<<" words";for(unsigned j=0;j<23;++j)std::cout<<' '<<std::hex<<m.read32(p+4*j);std::cout<<std::dec<<'\n';}
 c.pr=stop;instructions+=c.run(0x0c1cefe0,stop,100000);
 c.callHooks[0x0c1cf0a0]=[](auto&){};
 c.r[4]=m.read32(object+60);c.pr=stop;instructions+=c.run(0x0c0538a0,stop,100000);
 std::cout<<" highGLM";for(unsigned i=0;i<20;++i)std::cout<<' '<<std::hex<<m.read32(0x0c92f704+4*i);std::cout<<std::dec<<'\n';
 std::cout<<" materialized";for(unsigned i=0;i<9;++i)std::cout<<' '<<std::hex<<m.read32(0x0c92f104+28*i);std::cout<<std::dec<<'\n';
 for(unsigned i=0;i<6;++i){std::cout<<" light68 "<<i;for(unsigned j=0;j<17;++j)std::cout<<' '<<std::hex<<m.read32(0x0c92f2c4+68*i+4*j);std::cout<<std::dec<<'\n';}
 const unsigned count=m.read32(m.read32(object+60)+68);
 m.zeroRegion(0x0c99a16c,4);m.write32(0x0c99a16c,32);
 for(unsigned i=0;i<count;++i){
  const unsigned raw=0x0c92f104+28*i,hi=0x0c92f2c4+68*i;
  for(unsigned j=0;j<3;++j){m.write32(hi+20+4*j,m.read32(raw+4*j));m.write32(hi+32+4*j,m.read32(raw+12+4*j));}
  RefCpu d(m);d.fscaHalfWave=fsca;d.r[15]=object+0x70000;d.pr=stop;d.r[4]=hi;d.r[5]=object+0x60000;
  d.callHooks[0x0c2223e0]=[](auto&cpu){if(cpu.r[4]!=0xff000000||cpu.r[5]!=256)throw std::runtime_error("Unexpected source divider inputs");cpu.fpul=cpu.r[4]/cpu.r[5];};
  if(m.read8(hi+1)==4){instructions+=d.run(0x0c1d3860,0x0c206596,10000);std::cout<<" cosine "<<i<<' '<<std::hex<<d.fr[7]<<' '<<d.fr[6]<<std::dec<<'\n';instructions+=d.run(0x0c206596,stop,10000);}
  else instructions+=d.run(0x0c1d3860,stop,10000);
  std::cout<<" packed "<<i;for(unsigned j=0;j<8;++j)std::cout<<' '<<std::hex<<m.read32(object+0x60000+4*(7-j));std::cout<<std::dec<<'\n';
 }
 }
 return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}

