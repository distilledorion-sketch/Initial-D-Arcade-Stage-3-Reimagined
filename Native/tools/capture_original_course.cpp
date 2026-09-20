#include "sh4_scalar_reference.h"
#include <iostream>
#include <set>
using namespace idas3::reference;
// Development-only original Akina render-selection capture. Device/matrix and
// separate dynamic crowd/tree objects are explicit excluded dependencies.
int main(int argc,char**argv){try{
 if(argc!=3)throw std::runtime_error("image output.json required");RefMemory mem(argv[1]);
 constexpr unsigned obj=0xd000000,modelBase=0xd010000,treeBase=0xd020000,stack=0xd030000,stop=0xff0000;
 mem.zeroRegion(obj,0x2000);mem.zeroRegion(modelBase,0x10000);mem.zeroRegion(treeBase,0x10000);mem.zeroRegion(stack,0x10000);
 mem.write32(obj,0xc38dc9c);mem.write32(obj+4,0xc38dc74);mem.write32(obj+8,0xc38dc5c);
 const unsigned counts[]={20,25,90,1};
 for(unsigned i=0;i<4;++i){mem.write32(obj+0x14c+i*4,modelBase+i*0x100);mem.write32(modelBase+i*0x100+36,counts[i]);}
 constexpr unsigned treeHook=0xd02ff00;
 for(unsigned i=0;i<4;++i){unsigned p=treeBase+i*0x100;mem.write32(obj+0x4e4+i*4,p);mem.write32(p,treeBase+0x1000);}
 mem.write32(treeBase+0x1000+36,treeHook);
 RefCpu cpu(mem);std::vector<unsigned>draws;unsigned matrixDepth=0,external=0;
 cpu.callHooks[0xc05a8e0]=[&](RefCpu&c){unsigned bank=(c.r[4]-modelBase)/0x100;if(bank>=4||c.r[5]>=counts[bank])throw std::runtime_error("Original course selected invalid bank index");unsigned offset=bank==0?0:bank==1?20:bank==2?45:1000;c.r[0]=0xe000000+offset+c.r[5];};
 cpu.callHooks[0xc1d7120]=[&](RefCpu&c){if(c.r[4]<0xe000000)throw std::runtime_error("Invalid captured geometry pointer");unsigned id=c.r[4]-0xe000000;if(id<135)draws.push_back(id);else ++external;};
 cpu.callHooks[0xc2223b8]=[](RefCpu&c){if(!c.r[5])throw std::runtime_error("division by zero");c.fpul=std::uint32_t(signed32(c.r[4])/signed32(c.r[5]));};
 cpu.callHooks[0xc1f6610]=[&](RefCpu&){++matrixDepth;};cpu.callHooks[0xc1f65c0]=[&](RefCpu&c){unsigned n=std::max(1u,c.r[4]);if(matrixDepth<n)throw std::runtime_error("Matrix underflow");matrixDepth-=n;};
 cpu.callHooks[0xc1fbd60]=[&](RefCpu&c){for(unsigned i=0;i<16;++i)mem.writeFloat(c.r[4]+4*i,(i%5==0)?1.f:0.f);};
 for(unsigned a:{0xc03a320u,0xc03aa40u,0xc1fb360u,0xc1fc0a0u,0xc1fc5a0u,treeHook})cpu.callHooks[a]=[](RefCpu&){};
 std::ofstream out(argv[2]);out<<"{\"sectors\":[";unsigned long long allInstructions=0;
 for(unsigned sector=0;sector<30;++sector){
  if(sector)out<<",";out<<"{\"sector\":"<<sector;
  for(auto [entry,name]:std::vector<std::pair<unsigned,const char*>>{{0xc19d860,"primary"},{0xc19db20,"alternate"},{0xc19dc80,"static"}}){
   draws.clear();external=0;matrixDepth=0;mem.write32(obj+72,sector);mem.write32(obj+48,0);cpu.r[4]=obj;cpu.r[15]=stack+0xf000;cpu.pr=stop;auto n=cpu.run(entry,stop,20000);allInstructions+=n;if(matrixDepth)throw std::runtime_error("Unbalanced original course matrix stack");
   out<<",\""<<name<<"\":{\"entry\":"<<entry<<",\"instructions\":"<<n<<",\"excluded_external_draws\":"<<external<<",\"chunks\":[";for(unsigned i=0;i<draws.size();++i){if(i)out<<",";out<<draws[i];}out<<"]}";
  }
  out<<"}";
 }
 out<<"],\"total_original_instructions\":"<<allInstructions<<"}\n";std::cout<<"Captured30 original Akina sectors, "<<allInstructions<<" instructions\n";
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
