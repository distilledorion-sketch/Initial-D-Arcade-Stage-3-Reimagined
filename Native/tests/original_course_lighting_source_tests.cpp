// Research-only bounded source validation; never linked into the game.
#include "sh4_scalar_reference.h"
#include <algorithm>
#include <iostream>
#include <random>
using namespace idas3::reference;
int main(int argc,char**argv){try{
 if(argc!=2)throw std::runtime_error("Supply canonical original image");
 RefMemory m(argv[1]);constexpr unsigned owner=0xd000000,stack=0xd100000,tls=0xd020000,query=0xd030000,lights=0xd040000,stop=0xff0000;
 std::array<std::array<float,3>,31> authored{};
 for(unsigned i=0;i<31;++i)for(unsigned a=0;a<3;++a)authored[i][a]=m.readFloat(0xc2a506c+i*12+a*4);
 std::mt19937 rng(0x1a5580);std::uint64_t instructions=0;unsigned cases=0,unsafeEnableCases=0;
 for(unsigned sample=0;sample<287;++sample)for(unsigned night:{0u,1u,2u}){
  auto ref=authored[sample%31];if(sample>=31)for(auto&v:ref)v+=float(int(rng()%20001)-10000)/50.f;
  m.clear();m.zeroRegion(owner,0x10000);m.zeroRegion(stack,0x10000);m.zeroRegion(tls,0x10000);m.zeroRegion(query,0x10000);m.zeroRegion(lights,0x10000);
  m.write32(owner+52,night);m.write32(owner+24,query);m.write32(owner+64,123);m.write32(query,query+128);m.write32(query+128+44,0xff0020);m.write32(tls+4,tls+256);m.write32(tls+256+4,tls+512);
  // Deliberately declared sentinel pointers beyond four physical slots allow
  // inspection of the source indexed write without dereferencing host memory.
  for(unsigned i=0;i<31;++i){m.write32(owner+176+i*4,lights+i*128);m.writeFloat(lights+i*128+44,-9999);}
  RefCpu c(m);c.r[4]=owner;c.r[15]=stack+0xf000;c.pr=stop;unsigned queries=0,baseUpdates=0;
  c.callHooks[0xc221fc0]=[&](auto&q){q.r[0]=tls;};c.callHooks[0xc19b060]=[&](auto&){++baseUpdates;};
  c.callHooks[0xff0020]=[&](auto&q){if(q.r[4]!=query||q.r[5]!=123)throw std::runtime_error("Reference position query contract");for(unsigned i=0;i<3;++i)m.writeFloat(q.r[6]+i*4,ref[i]);++queries;};
  instructions+=c.run(0xc1a5580,stop,20000);
  if(baseUpdates!=1||queries!=(night==1?1u:0u))throw std::runtime_error("Night/base update gate");
  if(night!=1){for(unsigned i=0;i<31;++i)if(m.read8(lights+i*128+24)||m.readFloat(lights+i*128+44)!=-9999)throw std::runtime_error("Non-night light mutation");++cases;continue;}
  std::array<std::pair<float,unsigned>,31> distances{};
  for(unsigned i=0;i<31;++i){std::array<float,3>d{};for(unsigned a=0;a<3;++a)d[a]=ref[a]-authored[i][a];
   // Same finite FIPR numerical boundary as the scalar oracle, independent sort.
   distances[i]={float((double(d[0])*d[0]+double(d[1])*d[1])+double(d[2])*d[2]),i};}
  std::stable_sort(distances.begin(),distances.end(),[](auto a,auto b){return a.first<b.first;});
  const unsigned frame=stack+0xf000-32-532;bool unsafe=false;
  for(unsigned i=0;i<4;++i){const auto [distance,index]=distances[i];
   if(m.read32(frame+416+i*4)!=index||m.read32(frame+400+i*4)!=std::bit_cast<unsigned>(distance))throw std::runtime_error("Original nearest four selection mismatch");
   for(unsigned a=0;a<3;++a)if(m.read32(lights+i*128+44+a*4)!=std::bit_cast<unsigned>(authored[index][a]))throw std::runtime_error("Source physical slot XYZ copy");
   if(!m.read8(lights+index*128+24))throw std::runtime_error("Source authored-index enable write changed");unsafe|=index>=4;
  }
  unsafeEnableCases+=unsafe;++cases;
 }
 std::cout<<"Course light source: "<<cases<<" cases, "<<instructions<<" bounded instructions, "<<unsafeEnableCases<<" night cases index enable beyond four physical slots. TLS/base update/position query are explicit hooks.\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
