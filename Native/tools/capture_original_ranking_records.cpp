#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3::reference;
int main(int argc,char**argv){try{
 if(argc!=3)throw std::runtime_error("canonicalImage output required");
 RefMemory m(argv[1]);constexpr unsigned obj=0x0d000000,stack=0x0d080000;
 m.zeroRegion(obj,0x100000);auto seed=m.read32(0xc37c778);
 RefCpu c(m);c.r[4]=obj;c.r[15]=stack;c.pr=0x00ff0000;
 // Byte-string comparison is an explicit libc boundary; all ranking defaults,
 // original character-table lookup, selection and PRNG execute original bytes.
 c.callHooks[0xc226b40]=[&](auto&cpu){int cmp=0;for(unsigned i=0;i<cpu.r[6];++i){auto a=m.read8(cpu.r[4]+i),b=m.read8(cpu.r[5]+i);if(a!=b){cmp=int(a)-int(b);break;}if(!a)break;}cpu.r[0]=unsigned(cmp);};
 auto instructions=c.run(0xc031860,0xc031d96,200000000);
 const unsigned size=8+9*5920;std::ofstream out(argv[2],std::ios::binary);if(!out)throw std::runtime_error("output");
 out.write("ID3RANK1",8);out.write(reinterpret_cast<const char*>(&size),4);out.write(reinterpret_cast<const char*>(&seed),4);
 for(unsigned i=0;i<size;++i){auto v=m.read8(obj+i);out.write(reinterpret_cast<const char*>(&v),1);}
 std::cout<<"Captured original ranking defaults seed="<<seed<<", bytes="<<size<<", instructions="<<instructions<<"; libc comparison hook only, stopped before cabinet checksum/write\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
