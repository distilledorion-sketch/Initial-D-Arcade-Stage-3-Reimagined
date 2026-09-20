#include "original_ranking_attract.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <random>
using namespace idas3::original;using namespace idas3::reference;
constexpr unsigned obj=0x0d000000,stack=obj+0x80000,carAt=obj+0x3000,vt=obj+0x4000,records=obj+0x10000,stop=0x00ff0000;
void require(bool v,const char* text){if(!v)throw std::runtime_error(text);}
int main(int argc,char**argv){try{
 require(argc==2,"canonicalImage required");RefMemory m(argv[1]);std::mt19937 rng(251960);unsigned long long instructions=0,comparisons=0,hooks=0;
 for(unsigned trial=0;trial<2520;++trial){
  m.clear();m.zeroRegion(obj,0x100000);m.zeroRegion(0xc99aaec,8);m.zeroRegion(0xc98ad0c,12);m.zeroRegion(0xce00000,0x10000);
  m.write16(0xc98ad0e,32);m.write32(0xc98ad10,0xce00000);m.write32(0xc98ad14,0xce00000);
  auto p=initialOriginalRankingPage(trial%9);p.wet=trial%2;p.conditionIndex=(trial/2)%18;p.cooldown=(trial/4)%3;p.refreshCar=(trial/8)%2;
  OriginalRankingResourceState s;s.phase=trial%9;s.loadDelay=1+rng()%4;s.slideTicks=rng()%30;s.car=trial%35;s.packedAppearance=rng();s.yawUnits=rng();s.hasCar=(trial/3)%2;
  if(s.phase==3)++s.slideTicks;
  OriginalRankingCarSource source;source.car=(trial/9)%35;source.packedAppearance=rng();for(auto&v:source.name)v=std::uint8_t(rng()%221);if(trial%3==0)source.name[trial%5]=221;
  const unsigned ready=(trial/9)%3;auto expected=s;auto pageExpected=p;const auto e=stepOriginalRankingResources(expected,pageExpected,source,ready);
  for(auto [offset,value]:std::initializer_list<std::pair<unsigned,unsigned>>{{88,p.courseIndex},{96,p.conditionIndex},{104,p.wet},{116,p.cooldown},{120,s.hasCar?carAt:0},{128,s.car},{132,s.packedAppearance},{144,s.yawUnits},{148,records},{152,s.phase}})m.write32(obj+offset,value);
  m.write8(obj+124,p.refreshCar);m.write32(0xc99aaec,s.loadDelay);m.write32(0xc99aaf0,s.slideTicks);
  // Phase2 always has a valid resource in the source lifecycle. Keep its
  // original pointer intact while independently testing other null phases.
  if(s.phase==2){m.write32(obj+120,carAt);s.hasCar=true;expected.hasCar=true;}
  m.write32(carAt,vt);m.write32(carAt+32,vt+32);m.write32(vt+12,0x00ff0010);m.write32(vt+44,0x00ff0020);
  const unsigned course=originalRankingCourse(p),direction=p.conditionIndex&1,base=records+course*5920+direction*2960+p.wet*1480;
  for(unsigned i=0;i<5;++i)m.write8(base+748+4+i,source.name[i]);
  // The fifth character shares the low byte of the car-packed record word.
  m.write32(base+748+8,(source.car<<26)|source.name[4]);m.write32(base+1484,source.packedAppearance);
  RefCpu c(m);c.r[4]=obj;c.r[15]=stack;c.pr=stop;for(unsigned i=0;i<16;++i)c.xf[i]=std::bit_cast<unsigned>(i%5==0?1.f:0.f);
  unsigned created=0,destroyed=0,drawn=0;float slide=0;std::vector<OriginalRankingCarCommand> commands;
  c.callHooks[0x00ff0010]=[&](auto&cpu){++hooks;++destroyed;require(cpu.r[5]==3,"destructor argument");};
  c.callHooks[0x00ff0020]=[&](auto&cpu){++hooks;cpu.r[0]=ready;};
  c.callHooks[0xc036060]=[&](auto&cpu){++hooks;++created;require(cpu.r[4]==source.car&&cpu.r[5]==1,"ranking car allocation arguments");cpu.r[0]=carAt;};
  c.callHooks[0xc02f8e0]=[&](auto&cpu){++hooks;if(m.read32(obj+120)){++drawn;slide=std::bit_cast<float>(cpu.xf[12]);require(e.drawYawUnits==m.read32(obj+144),"source draw yaw");}};
  for(auto address:{0xc029000u,0xc0283c0u,0xc028400u,0xc028480u,0xc0284c0u,0xc028500u,0xc028540u,0xc028580u,0xc0285c0u,0xc0286c0u,0xc028660u,0xc0286a0u,0xc028720u,0xc028760u,0xc029040u,0xc029da0u})
   c.callHooks[address]=[&,address](auto&cpu){++hooks;require(cpu.r[4]==carAt,"appearance resource");commands.push_back({address,address==0xc029040?0:cpu.r[5]});};
  c.callHooks[0xc2223b8]=[&](auto&cpu){++hooks;cpu.fpul=unsigned(signed32(cpu.r[4])/signed32(cpu.r[5]));};
  instructions+=c.run(0xc02f1c0,0xc02f5ee,5000);
  for(auto [offset,value]:std::initializer_list<std::pair<unsigned,unsigned>>{{128,expected.car},{132,expected.packedAppearance},{152,expected.phase}}){++comparisons;if(m.read32(obj+offset)!=value)throw std::runtime_error("Resource state trial "+std::to_string(trial)+" offset "+std::to_string(offset));}
  require(m.read32(0xc99aaec)==expected.loadDelay,"source load delay");require(m.read32(0xc99aaf0)==expected.slideTicks,"source slide duration");require(bool(m.read8(obj+124))==pageExpected.refreshCar,"refresh consumption");
  require(created==unsigned(e.createCar)&&destroyed==unsigned(e.destroyCar),"resource lifetime events");require(drawn==unsigned(e.drawCar),"car draw event");
  //Identity FTRV canonicalizes the source negative-zero translation.
  require(slide==e.slideX,"source slide arithmetic");require(commands==e.carCommands,"source appearance command order/arguments");comparisons+=8;
  if(e.configureCar)for(unsigned i=0;i<5;++i){require(m.read8(carAt+1716+i)==e.plateDigits[i],"source ranking plate digits");++comparisons;}
 }
 // Transition timing: first resource is created immediately, then90-tick
 // shared cooldown,3 load-delay ticks, actual ready poll,30 slide draws.
 auto page=initialOriginalRankingPage(3);OriginalRankingResourceState s;OriginalRankingCarSource car;unsigned create=0,configure=0,draw=0;
 for(unsigned frame=0;frame<150;++frame){auto event=stepOriginalRankingResources(s,page,car);create+=event.createCar;configure+=event.configureCar;draw+=event.drawCar;stepOriginalRankingPage(page);}
 require(create==1&&configure==1&&draw==55&&s.phase==4,"initial resource timing");
 require(initialOriginalRankingPage(5).wet==1,"original Snow starts forced wet");
 std::cout<<"Ranking resources:2520 cases, "<<comparisons<<" comparisons, "<<instructions<<" original instructions, "<<hooks<<" explicit ACar/render/division boundary calls\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

