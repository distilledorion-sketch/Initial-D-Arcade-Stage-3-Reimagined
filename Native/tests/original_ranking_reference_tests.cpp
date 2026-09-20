#include "original_ranking_attract.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <random>
using namespace idas3::original;
using namespace idas3::reference;
constexpr unsigned obj=0x0d000000,stack=0x0d080000,stop=0x00ff0000;
void require(bool b,const char*m){if(!b)throw std::runtime_error(m);}
int main(int argc,char**argv){try{
 require(argc==3,"canonicalImage fscaTable required");RefMemory m(argv[1]);auto table=OriginalFscaTable::load(argv[2]);
 std::vector<unsigned> fsca(32768);std::ifstream f(argv[2],std::ios::binary);f.seekg(16);f.read(reinterpret_cast<char*>(fsca.data()),131072);require(bool(f),"FSCA");
 unsigned long long instructions=0,comparisons=0;
 auto reset=[&](){m.clear();m.zeroRegion(obj,0x100000);m.zeroRegion(0xc92ece0,0x100);m.zeroRegion(0xc98ad0c,12);m.zeroRegion(0xce00000,0x10000);m.write16(0xc98ad0e,32);m.write32(0xc98ad10,0xce00000);m.write32(0xc98ad14,0xce00000);};
 std::mt19937 rng(12011);
 for(unsigned trial=0;trial<1500;++trial){
  reset();auto s=initialOriginalRankingPage(trial%9);s.conditionIndex=trial%18;s.wet=trial%2;s.pageTicks=int(rng()%1001)-1;s.watchdogTicks=int(rng()%10)-2;s.cooldown=rng()%121;s.detailMode=trial%2;s.detailPage=trial%4;
  const OriginalRankingPageInput in{trial%7==0,trial%11==0};auto expected=s;auto event=stepOriginalRankingPage(expected,in);
  RefCpu c(m);c.r[13]=obj;c.r[14]=stack;c.r[15]=stack;c.r[10]=0xc2ef27c;m.write32(stack+56,stop);m.write32(obj+12,obj+0x1000);m.write32(obj+0x1000+44,0x00ff0010);
  for(auto[a,v]:std::initializer_list<std::pair<unsigned,unsigned>>{{88,s.courseIndex},{92,unsigned(s.pageTicks)},{96,s.conditionIndex},{104,s.wet},{108,s.detailMode},{112,s.detailPage},{116,s.cooldown},{160,unsigned(s.watchdogTicks)}})m.write32(obj+a,v);
  m.write32(obj+84,obj+0x2000);m.write32(0xc2ef2a0,s.persistedCourseIndex);m.write8(0xc92ed40,in.detailPressed?0x10:0);m.write8(0xc92ed00,in.nextConditionPressed?0x10:0);m.write32(obj+0x2000+4,0xabababab);
  bool completed=false;c.callHooks[0x00ff0010]=[&](auto&){completed=true;};for(unsigned a:{0xc1bd9a0u,0xc1bda20u,0xc1bdc00u})c.callHooks[a]=[](auto&){};
  instructions+=c.run(0xc02f5f4,stop,10000);
  for(auto[a,v]:std::initializer_list<std::pair<unsigned,unsigned>>{{88,expected.courseIndex},{92,unsigned(expected.pageTicks)},{96,expected.conditionIndex},{104,expected.wet},{108,expected.detailMode},{112,expected.detailPage},{116,expected.cooldown},{160,unsigned(expected.watchdogTicks)}}){++comparisons;if(m.read32(obj+a)!=v)throw std::runtime_error("Ranking state differs trial "+std::to_string(trial)+" offset "+std::to_string(a));}
  require(m.read32(0xc2ef2a0)==expected.persistedCourseIndex,"persisted course");require(completed==expected.completed,"finish request");require((m.read32(obj+0x2000+4)==0)==event.clearLeaderboard,"leaderboard clear");require(bool(m.read8(obj+124))==expected.refreshCar,"car refresh");comparisons+=4;
 }
 for(unsigned car=0;car<35;++car)for(unsigned yaw:{0u,0xffffe000u,12345u})for(float slide:{0.f,10.f,-5.f}){
  reset();RefCpu c(m);c.r[4]=obj;c.r[15]=stack;c.pr=stop;c.fscaHalfWave=fsca;
  auto base=originalIdentityMatrix();translateOriginalMatrix(base,{slide,0,0});for(unsigned i=0;i<16;++i)c.xf[i]=std::bit_cast<unsigned>(base.elements[i]);
  m.write32(obj+128,car);m.write32(obj+120,obj+0x3000);m.write32(obj+144,yaw);m.write32(obj+136,obj+0x4000);m.write32(obj+0x3000+76,obj+0x5000);m.write32(obj+0x5000+12,0x00ff0020);
  auto expected=originalRankingScene(car,yaw,slide,table);unsigned draws=0;
  auto compare=[&](const OriginalMatrix& mat){for(unsigned i=0;i<16;++i){++comparisons;if(c.xf[i]!=std::bit_cast<unsigned>(mat.elements[i]))throw std::runtime_error("Ranking matrix differs car "+std::to_string(car)+" draw "+std::to_string(draws)+" component "+std::to_string(i));}};
  c.callHooks[0xc053480]=[](auto&){};c.callHooks[0xc05a8e0]=[&](auto&cpu){require(cpu.r[5]==3,"shadow chunk");cpu.r[0]=obj+0x6000;};
  c.callHooks[0xc1d7120]=[&](auto&){require(draws==0,"shadow order");compare(expected.shadow);++draws;};
  c.callHooks[0x00ff0020]=[&](auto&){require(draws==1||draws==2,"car draw order");compare(draws==1?expected.car:expected.reflection);++draws;};c.callHooks[0xc1fb440]=[](auto&){};
  instructions+=c.run(0xc02f8e0,stop,20000);require(draws==3,"scene draws");require(m.read32(obj+144)==expected.nextYawUnits,"source yaw delta");require(m.read32(obj+0x3000+224)==2,"reflection material restore");
 }
 // Natural page cycling exits at the next course, rather than replaying a race.
 for(unsigned i=0;i<9;++i){auto s=initialOriginalRankingPage(i);unsigned frames=0;while(!s.completed&&frames<10810){stepOriginalRankingPage(s);++frames;}require(s.completed,"automatic ranking completion");std::cout<<"course index "<<i<<" completes at "<<frames<<" frames, next "<<s.persistedCourseIndex<<'\n';}
 std::cout<<"Ranking controller/scene: "<<comparisons<<" comparisons, "<<instructions<<" original instructions\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
