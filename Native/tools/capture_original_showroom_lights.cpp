#include "sh4_scalar_reference.h"
#include <iostream>
#include <iomanip>
using namespace idas3::reference;
int main(int argc,char**argv){try{
 if(argc!=4)throw std::runtime_error("canonical-image fsca-table output-json required");
 RefMemory m(argv[1]);RefCpu c(m);constexpr unsigned obj=0x0d000000,ex=obj+0x2000,node=ex+0x100,stop=0x00ff0000;
 m.zeroRegion(obj,0x100000);m.zeroRegion(0x0c92f000,0x10000);m.zeroRegion(0x0c98ad0c,12);m.zeroRegion(0x0ce00000,0x10000);
 m.write16(0x0c98ad0e,32);m.write32(0x0c98ad10,0x0ce00000);m.write32(0x0c98ad14,0x0ce00000);
 m.write32(ex+4,node);m.write32(node,node);m.write32(node+4,node);
 std::vector<unsigned> fsca(32768);std::ifstream fs(argv[2],std::ios::binary);fs.seekg(16);fs.read(reinterpret_cast<char*>(fsca.data()),131072);if(!fs)throw std::runtime_error("FSCA table");c.fscaHalfWave=fsca;
 c.r[15]=obj+0x80000;c.pr=stop;for(unsigned i=0;i<16;++i)c.xf[i]=std::bit_cast<unsigned>(i%5==0?1.f:0.f);
 c.callHooks[0x0c221fc0]=[&](auto&cpu){cpu.r[0]=ex;};
 c.callHooks[0x0c023e20]=[](auto&cpu){cpu.r[0]=cpu.r[4];};
 c.callHooks[0x0c0a7500]=[](auto&cpu){cpu.r[0]=cpu.r[4];};
 unsigned allocation=obj+0x10000;c.callHooks[0x0c021960]=[&](auto&cpu){cpu.r[0]=allocation;allocation+=0x1000;};
 c.r[4]=obj;std::size_t steps=c.run(0x0c112260,stop,100000);
 // Reset the original low-level lighting state and inspect the constructor's
 // linked light chain before downstream renderer submission.
 c.pr=stop;steps+=c.run(0x0c1cefe0,stop,100000);
 bool submit=false;c.callHooks[0x0c1cf0a0]=[&](auto&){submit=true;};
 c.r[4]=m.read32(obj+0x140);c.pr=stop;steps+=c.run(0x0c053480,stop,100000);if(!submit)throw std::runtime_error("Missing original chain submission");
 // Capture the exact GLM command immediately before its first PREF. The
 // reference only writes private memory; no device or queue submission runs.
 m.write32(0x0c9801ec,1);m.write32(0x0c980264,0x09000000);
 c.r[4]=0x0c92f704;c.pr=stop;steps+=c.run(0x0c1d3d00,0x0c1d3e10,10000);
 std::array<unsigned,8> glm{};for(unsigned i=0;i<8;++i)glm[i]=m.read32(c.r[7]+4*i);
 if(glm[0]!=0x08000400||glm[1]!=0x000f00b0||glm[3]!=0xff262626||glm[4]!=0xff000000)throw std::runtime_error("Unexpected original effective GLM");
 std::ofstream out(argv[3]);out<<std::setprecision(9);
 out<<"{\"hook_boundary\":\"1CF0A0 downstream transform/submit only; original light constructors and chain update execute\",\"original_instructions\":"<<steps<<",\"highlevel_glm_words\":[";
 for(unsigned i=0;i<20;++i){if(i)out<<',';out<<m.read32(0x0c92f704+4*i);}out<<"],\"light_records\":[";
 for(unsigned light=0;light<2;++light){if(light)out<<',';out<<"{\"raw28\":[";for(unsigned i=0;i<7;++i){if(i)out<<',';out<<m.read32(0x0c92f104+28*light+4*i);}out<<"],\"raw68\":[";for(unsigned i=0;i<17;++i){if(i)out<<',';out<<m.read32(0x0c92f2c4+68*light+4*i);}out<<"]}";}out<<"],\"glm_command_words\":[";for(unsigned i=0;i<8;++i){if(i)out<<',';out<<glm[i];}out<<"]}\n";
 std::cout<<"PASS original showroom two-light chain capture, "<<steps<<" original instructions.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
