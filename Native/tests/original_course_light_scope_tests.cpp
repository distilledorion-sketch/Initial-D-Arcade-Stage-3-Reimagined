// Bounded submission-scope fixture derived from capture_course_roster.cpp.
// Geometry/PVR and matrix helpers are explicit boundaries;03AA40 executes.
#include "sh4_scalar_reference.h"
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <set>
using namespace idas3::reference;
namespace fs=std::filesystem;
int main(int argc,char**argv)try{
 if(argc!=4)throw std::runtime_error("canonical image, project root, output CSV required");
 RefMemory m(argv[1]);const fs::path root=argv[2];std::ofstream csv(argv[3]);if(!csv)throw std::runtime_error("CSV unavailable");
 csv<<"scene,weather,reverse,primary,static,path_samples,submissions,primary_draws,static_draws,backgrounds,crows,dynamic_boundaries,instructions\n";
 constexpr unsigned owner=0xd000000,stack=0xd020000,path=0xd040000,lightset=0xd050000,crow=0xd060000,flight=0xd070000,group=0xd080000,wing=0xd090000,stop=0xff0000;
 constexpr unsigned tables[]={0xc38de4c,0xc38df24,0xc38e1ac,0xc38e284,0xc38dffc,0xc38e0d4,0xc38dc9c,0xc38dd74,0xc38213c,0xc38213c,0xc38e50c,0xc38e5e4,0xc38e35c,0xc38e434,0xc38e6bc,0xc38e794,0xc38dd74,0xc38dd74};
 constexpr const char* names[]={"k_ez","s_nm","h_hd","k_df","s_vh","s_uh","n_sy","k_tu","k_df"};
 std::uint64_t instructions=0,checks=0,totalDraws=0,totalCrows=0,totalSubmissions=0;unsigned cases=0;
 const auto check=[&](bool good,const char*why){++checks;if(!good)throw std::runtime_error(why);};
 auto copy=[&](const fs::path&file,unsigned address){std::ifstream f(file,std::ios::binary);std::vector<unsigned char>b{std::istreambuf_iterator<char>(f),{}};if(b.empty()||b.size()>0x10000)throw std::runtime_error("Bounded fixture missing");for(unsigned i=0;i<b.size();++i)m.write8(address+i,b[i]);};
 for(unsigned scene=0;scene<18;++scene)for(unsigned wet=0;wet<2;++wet)for(unsigned reverse=0;reverse<2;++reverse){
  m.clear();m.zeroRegion(owner,0x100000);copy(root/"data/courses"/(std::string(names[scene/2])+"_path.bin"),path+0x1000);
  const unsigned count=m.read32(path+0x1000);check(count>2&&count<20000,"Path extent");
  const auto vt=tables[scene+((scene!=8&&!(scene&1)&&wet)?1:0)];m.write32(owner,vt);m.write32(owner+4,vt-40);m.write32(owner+8,vt-64);
  m.write32(owner+24,path);m.write32(path,path+0x100);m.write32(path+0x100+36,0xff0100);m.write32(owner+48,reverse);m.write32(owner+52,scene&1);m.write32(owner+56,wet);m.write32(owner+60,lightset);
  const auto boundary=m.read32(m.read32(0xc191cfc)+(scene/2)*4);m.write32(owner+36,boundary);std::vector<unsigned> limits;int sentinel=0;
  for(unsigned i=0;i<128;++i){const int n=signed32(m.read32(boundary+i*4));if(n<0){sentinel=n;break;}limits.push_back(unsigned(n));}
  check(!limits.empty()&&(sentinel==-1||sentinel==-2),"Sector table");
  if(scene>=4){for(unsigned i=0;i<((scene==12||scene==13)?6u:((scene==14||scene==15)?2u:4u));++i){const unsigned p=owner+0x8000+i*0x200;m.write32(owner+0x4e4+i*4,p);m.write32(p,owner+0x9000);}m.write32(owner+0x9000+36,0xff0104);}
  m.write32(owner+1280,crow);m.write32(crow+8,0xd0a0000);m.write32(crow+12,flight);m.write32(crow+16,group);m.write32(crow+36,wing);
  copy(root/"data/original_assets/crows/flight.bin",flight);copy(root/"data/original_assets/crows/group.bin",group);for(unsigned i=0;i<19;++i)m.write32(wing+i*4,i%30);
  RefCpu c(m);unsigned active=0,stage=0,depth=0,submissions=0,draws[2]{},backgrounds=0,crows=0,dynamics=0;std::uint64_t localInstructions=0;
  c.callHooks[0xff0100]=[&](auto&q){q.r[0]=count;};c.callHooks[0xff0104]=[&](auto&){check(active==lightset,"Dynamic child boundary lacks course lightset");++dynamics;};
  c.callHooks[0xc0538a0]=[&](auto&q){check(q.r[4]==lightset&&q.pr==0xc03aa4c,"Course must execute03AA40 loading owner+60");active=q.r[4];++submissions;};
  const auto draw=[&](auto&){check(active==lightset,"Geometry lacks course lightset");++draws[stage];};
  for(unsigned a:{0xc19cec0u,0xc19cf40u,0xc19d000u,0xc19d0c0u,0xc03d740u,0xc03d7e0u})c.callHooks[a]=draw;
  c.callHooks[0xc05a8e0]=[&](auto&q){if(q.r[4]==0xd0a0000){check(q.r[5]<30,"Crow wing index");q.r[0]=0xe000100;}else{check(q.r[5]==0,"Background chunk");q.r[0]=0xe000000;}};
  c.callHooks[0xc1d7120]=[&](auto&q){check(active==lightset,"Background/crow lacks course lightset");if(q.r[4]==0xe000100)++crows;else{check(q.r[4]==0xe000000,"Direct mesh identity");++backgrounds;}};
  c.callHooks[0xc2223b8]=[](auto&q){if(!q.r[5])throw std::runtime_error("Division by zero");q.fpul=unsigned(signed32(q.r[4])/signed32(q.r[5]));};
  c.callHooks[0xc1f6610]=[&](auto&){++depth;};c.callHooks[0xc1fcc60]=[&](auto&){++depth;};
  c.callHooks[0xc1f65c0]=[&](auto&q){const auto n=std::max(1u,q.r[4]);check(depth>=n,"Balanced matrix boundary");depth-=n;};
  c.callHooks[0xc1fbd60]=[&](auto&q){for(unsigned i=0;i<16;++i)m.writeFloat(q.r[4]+4*i,i%5==0?1.f:0.f);};
  for(unsigned a:{0xc1fc0a0u,0xc1fc5a0u,0xc1fd060u,0xc1f6ac0u,0xc1f67e0u,0xc1f68a0u,0xc1f6950u,0xc1fb360u,0xc1f6af0u,0xc1fbf80u,0xc1fc2a0u,0xc1f66a0u,0xc1f64a0u})c.callHooks[a]=[](auto&){};
  c.callHooks[0xc085220]=[&](auto&){check(active==lightset,"External dynamic boundary lacks course lightset");++dynamics;};
  const auto primary=m.read32(vt+52),statics=m.read32(vt+68);
  for(unsigned index=0;index<count;++index){
   // Keep resource bytes resident; scope is invariant under advancing the
   // source prop phases. Reset the CPU and per-draw publication state only.
   c.r.fill(0);c.fr.fill(0);c.xf.fill(0);c.t=false;c.fpscrSz=false;c.fpul=c.mach=c.macl=0;
   unsigned sector=unsigned(std::upper_bound(limits.begin(),limits.end(),index)-limits.begin());if(sentinel==-2&&sector==limits.size())sector=0;
   m.write32(owner+64,reverse?count-1-index:index);m.write32(owner+68,index);m.write32(owner+72,sector);m.write32(crow+32,index%900);active=0;depth=0;
   const auto oldSubmissions=submissions,oldBackgrounds=backgrounds,oldCrows=crows;
   for(stage=0;stage<2;++stage){const auto entry=stage?statics:primary;c.r[4]=owner;c.r[15]=stack+0xf000;c.pr=stop;
    try{localInstructions+=c.run(entry,stop,30000);}catch(const std::exception&e){throw std::runtime_error("scene"+std::to_string(scene)+" wet"+std::to_string(wet)+" reverse"+std::to_string(reverse)+" index"+std::to_string(index)+" entry"+hex(entry)+" pc"+hex(c.pc)+": "+e.what());}check(depth==0,"Matrix balance at draw exit");}
   check(submissions>oldSubmissions,"Missing source light submission");check(backgrounds==oldBackgrounds+1,"One background each primary/static pair");check(crows==oldCrows+(scene==0&&!wet?19:0),"Crow draw availability and count");
  }
  instructions+=localInstructions;totalDraws+=draws[0]+draws[1]+backgrounds;totalCrows+=crows;totalSubmissions+=submissions;++cases;
  csv<<scene<<','<<wet<<','<<reverse<<','<<hex(primary)<<','<<hex(statics)<<','<<count<<','<<submissions<<','<<draws[0]<<','<<draws[1]<<','<<backgrounds<<','<<crows<<','<<dynamics<<','<<localInstructions<<'\n';csv.flush();
 }
 // Separate car submission: execute the original034C20 prefix through the
 // first0538A0 call, with a deliberately different owner/lightset identity.
 m.clear();m.zeroRegion(owner,0x10000);m.zeroRegion(stack,0x10000);m.write32(owner+60,lightset);m.write32(owner+2540,lightset+0x1000);
 RefCpu car(m);car.r[4]=owner;car.r[15]=stack+0xf000;car.pr=stop;unsigned carCalls=0;
 car.callHooks[0xc221fc0]=[](auto&q){q.r[0]=0xd0b0000;};car.callHooks[0xc0538a0]=[&](auto&q){check(q.r[4]==lightset+0x1000&&q.pr==0xc034c44,"Car must use own+2540 lightset");++carCalls;};
 instructions+=car.run(0xc034c20,0xc034c44,100);check(carCalls==1,"One car lightset submission");
 // Exact parent supplies the player's previously published world matrix,
 // then updates lights, then updates course position. The matrix getter runs.
 for(unsigned scene=0;scene<18;++scene)for(unsigned sample=0;sample<16;++sample){
  constexpr unsigned race=0xd100000,player=0xd110000;const unsigned course=owner+sample*128;
  m.clear();m.zeroRegion(race,0x20000);m.zeroRegion(owner,0x10000);m.zeroRegion(stack,0x10000);
  m.write32(race+1036,course);m.write32(race+1048,player);m.write32(player+2468,0xc380b8c);m.write32(course,tables[scene]);
  m.write32(race+1408,sample%4);m.write32(race+1412+12*(sample%4),100+sample);m.write32(race+1460,200+sample);
  for(unsigned i=0;i<16;++i)m.write32(player+2404+i*4,0x3f000000+sample*100+i);
  RefCpu parent(m);parent.r[12]=race;parent.r[15]=stack+0xf000;unsigned order=0;
  const auto lightEntry=m.read32(tables[scene]+84),updateEntry=m.read32(tables[scene]+44);
  parent.callHooks[lightEntry]=[&](auto&q){check(order++==0&&q.r[4]==course&&m.read32(course+80)==player+2404,"Parent must bind player matrix before light update");};
  parent.callHooks[updateEntry]=[&](auto&q){check(order++==1&&q.r[4]==course&&q.r[5]==100+sample&&q.r[6]==200+sample,"Parent light/path update order and arguments");};
  instructions+=parent.run(0xc05fe1a,0xc05fe6a,300);check(order==2,"Both parent update callbacks");
  for(unsigned i=0;i<16;++i)check(m.read32(player+2404+i*4)==0x3f000000+sample*100+i,"Parent preserves published matrix bytes");
 }
 std::cout<<"PASS "<<cases<<" course/time/weather/direction cases, "<<checks<<" checks / "<<instructions<<" original instructions; "<<totalSubmissions<<" course lightset submissions, "<<totalDraws<<" geometry/background boundaries, "<<totalCrows<<" crow mesh draws; separate car+2540 prefix and288 parent player-matrix/update bindings verified. All authoring-path indices;03AA40 and042520 execute. Geometry/matrix/PVR/platform boundaries remain explicit. No devices, handling or user data.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
