#include "original_headlight_projection.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <random>
using namespace idas3::original;
using namespace idas3::reference;
int main(int argc,char**argv)try{
 if(argc!=3)throw std::runtime_error("canonical image and project root required");
 const std::filesystem::path root=argv[2],assets=root/"data/original_assets/headlight_projection";
 RefMemory m(argv[1]);constexpr unsigned obj=0xd000000,tls=obj+0x2000,node=tls+256,stack=obj+0x80000,mesh=obj+0x40000,bin=obj+0x50000,tbl=obj+0x60000,transform=obj+0x70000,road=obj+0x71000,stop=0xf000000;
 m.zeroRegion(obj,0x100000);m.zeroRegion(0xc98ad00,0x1000);m.zeroRegion(0xce00000,0x10000);
 m.write16(0xc98ad0e,32);m.write32(0xc98ad10,0xce00000);m.write32(0xc98ad14,0xce00000);
 m.write32(tls+4,node);m.write32(node,node);m.write32(node+4,node);
 auto copy=[&](const std::filesystem::path&p,unsigned addr){std::ifstream in(p,std::ios::binary);std::vector<unsigned char>b{std::istreambuf_iterator<char>(in),{}};if(b.empty())throw std::runtime_error("Missing source asset");for(unsigned i=0;i<b.size();++i)m.write8(addr+i,b[i]);return unsigned(b.size());};
 copy(assets/"source_chunk7.bin",mesh);const auto binsize=copy(assets/"k_light_test8.bin",bin);copy(assets/"k_light_test8.tbl",tbl);
 RefCpu c(m);c.r[15]=stack;c.pr=stop;c.r[4]=obj;c.r[5]=mesh;c.r[6]=3;
 std::vector<unsigned> fsca(32768);std::ifstream fs(root/"data/original_physics/fsca_table.bin",std::ios::binary);fs.seekg(16);fs.read(reinterpret_cast<char*>(fsca.data()),131072);if(!fs)throw std::runtime_error("FSCA table");c.fscaHalfWave=fsca;
 std::size_t checks=0,instructions=0;auto check=[&](bool b,const char*why){++checks;if(!b)throw std::runtime_error(why);};
 unsigned allocation=obj+0x10000;c.callHooks[0xc021960]=[&](auto&q){q.r[0]=allocation;allocation+=0x1000;};c.callHooks[0xc221fc0]=[&](auto&q){q.r[0]=tls;};
 // Source file handling stays at the byte-loading boundary. The unchanged
 // model scanner, both mapping loads, object constructors and TLS execute.
 c.callHooks[0xc04ce60]=[](auto&q){q.r[0]=q.r[4];};c.callHooks[0xc04cec0]=[](auto&){};c.callHooks[0xc04d340]=[](auto&q){q.r[0]=1;};
 unsigned loads=0;c.callHooks[0xc04e480]=[&](auto&q){std::string s;for(unsigned a=q.r[4];m.read8(a);++a)s+=char(m.read8(a));
  if(s=="/driveA/binary/k_light_test8.tbl"){check(loads==2,"Original mapping load order");q.r[0]=tbl;}
  else{check(s=="/driveA/binary/k_light_test8.bin"&&loads<2,"Original point file name/count");if(q.r[5])m.write32(q.r[5],binsize);q.r[0]=loads++?bin+0x1000:bin;if(loads==2)for(unsigned i=0;i<binsize;++i)m.write8(bin+0x1000+i,m.read8(bin+i));}};
 c.callHooks[0xc1fa9e0]=[](auto&){}; // Development performance-marker boundary.
 c.callHooks[0xc2223b8]=[&](auto&q){check(q.r[4]<9&&q.r[5]==3,"Source miss skip compiler division arguments");q.fpul=q.r[4]/3;};
 instructions+=c.run(0xc0d4c80,stop,100000);
 OriginalHeadlightProjection native;native.load(root);
 check(m.read32(obj)==0xc38760c&&m.read32(obj+8)==mesh,"Actual projected owner vtable/mesh");
 check(m.read32(obj+12)==12&&m.read32(obj+16)==74&&m.read32(obj+28)==9,"Actual12 strip vertices/9 projected points");
 for(unsigned i=0;i<9;++i)for(unsigned j=0;j<8;++j)check(m.read32(bin+i*32+j*4)==native.records()[i][j],"Constructor point records");
 const unsigned sourceQueries=m.read32(obj+132);
 for(unsigned i=0;i<9;++i)for(unsigned j=0;j<16;++j)check(m.read32(sourceQueries+i*64+j*4)==native.queries()[i].words[j],"Constructor source query defaults");
 std::mt19937 rng(0xd50a0);std::uniform_real_distribution<float> unit(-1.f,1.f);
 unsigned frames=0,noHit=0,flagged=0,totalQueries=0;
 for(unsigned frame=0;frame<384;++frame){
  OriginalCollisionQuery binding;for(auto&w:binding.words)w=rng();
  for(unsigned j=0;j<16;++j)m.write32(road+j*4,binding.words[j]);
  c.r[4]=obj;c.r[5]=road;c.r[15]=stack;c.pr=stop;instructions+=c.run(0xc0d5320,stop,10000);native.bindRoad(binding);
  // Explicit source publish phase precedes draw/advance; repeated drawing
  // consumes this cached model and never moves a point or performs a query.
  c.r[4]=obj;c.r[15]=stack;c.pr=stop;instructions+=c.run(0xc0d84a0,stop,10000);native.publish();
  const auto&verts=native.model().chunks[0].batches[0].vertices;
  for(unsigned i=0;i<12;++i){const auto&v=verts[i];const unsigned p=mesh+192+i*32;
   const std::array<unsigned,7> words{std::bit_cast<unsigned>(v.position.x),std::bit_cast<unsigned>(v.position.y),std::bit_cast<unsigned>(v.position.z),std::bit_cast<unsigned>(v.u),std::bit_cast<unsigned>(v.v),v.color0,v.color1};
   for(unsigned j=0;j<7;++j)check(m.read32(p+4+4*j)==words[j],"Actual0D84A0 projected mesh publication");}
  OriginalHeadlightProjection::Matrix matrix{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
  if(frame)for(unsigned i=0;i<16;++i)if(i%4!=3)matrix[i]=unit(rng)*(i<12?1.f:1500.f);
  for(unsigned i=0;i<16;++i)m.writeFloat(transform+i*4,matrix[i]);
  struct Answer{bool hit;unsigned flags;float distance;};std::array<Answer,9> answers{};
  for(unsigned i=0;i<9;++i)answers[i]={frame<8||rng()%7!=0,(frame+i)%3==0?0x8000u:0u,unit(rng)*12.f};
  std::vector<unsigned> queryOrder;std::vector<OriginalCollisionQuery> inputs;
  c.callHooks[0xc022ce0]=[&](auto&q){const unsigned i=(q.r[4]-sourceQueries)/64;check(i<9&&(q.r[4]-sourceQueries)%64==0,"Actual indexed road query");queryOrder.push_back(i);OriginalCollisionQuery in;for(unsigned j=0;j<16;++j)in.words[j]=m.read32(q.r[4]+4*j);inputs.push_back(in);
   const auto&a=answers[i];m.writeFloat(q.r[4]+24,a.distance);m.write32(q.r[4]+28,a.flags);q.r[0]=a.hit;++totalQueries;noHit+=!a.hit;flagged+=a.hit&&a.flags!=0;};
  c.r[4]=obj;c.r[5]=transform;c.r[15]=stack;c.pr=stop;instructions+=c.run(0xc0d50a0,stop,100000);
  unsigned ordinal=0;const auto count=native.advance(matrix,[&](OriginalCollisionQuery&q){check(ordinal<queryOrder.size(),"Native query count");check(q.words==inputs[ordinal].words,"Exact source projection input and complete query context");const auto&a=answers[queryOrder[ordinal++]];q.setf(24,a.distance);q.setu(28,a.flags);return a.hit;});
  check(count==queryOrder.size()&&ordinal==count,"Query count and source failure skips");
  for(unsigned i=0;i<9;++i){for(unsigned j=0;j<8;++j)check(m.read32(bin+i*32+j*4)==native.records()[i][j],"Exact projected XYZ/UV/colors across success/failure flags");
   for(unsigned j=0;j<16;++j)check(m.read32(sourceQueries+i*64+j*4)==native.queries()[i].words[j],"Complete source query post-state");}
  ++frames;
 }
 check(noHit>100&&flagged>100,"Meaningful contact failure/material coverage");
 // Original light activation is independent of popup motor completion. It
 // binds the road on an off->on edge; OFF clears both source light bytes.
 constexpr unsigned car=obj+0x30000;unsigned bindings=0;
 c.callHooks[0xc035160]=[&](auto&q){check(q.r[4]==car,"LightON road owner");++bindings;};
 for(unsigned previous:{0u,1u,2u,255u}){
  m.write8(car+81,previous);m.write8(car+2568,73);const auto before=bindings;
  c.r[4]=car;c.r[15]=stack;c.pr=stop;instructions+=c.run(0xc035120,stop,100);
  check(m.read8(car+81)==1&&m.read8(car+2568)==1,"Source LightON flags");check(bindings-before==(previous==0?1u:0u),"Source activation edge binds road once");
  c.r[4]=car;c.r[15]=stack;c.pr=stop;instructions+=c.run(0xc035200,stop,100);
  check(m.read8(car+81)==0&&m.read8(car+2568)==0,"Source LightOFF flags");
 }
 // Full035160 resolves the alternate actor, seeds BOTH position histories,
 // queries car+AA0, then binds the complete query even on a no-hit result.
 c.callHooks.erase(0xc035160);
 constexpr unsigned actor=car+0x4000,alternate=actor+0x100;
 for(unsigned alt:{0u,1u})for(unsigned present:{0u,1u})for(unsigned hit:{0u,1u}){
  m.write32(car+2396,actor);m.write32(car+2400,alt?alternate:0);m.write32(car+2636,present?obj:0);
  for(unsigned i=0;i<3;++i){m.writeFloat(actor+i*4,float(i+25));m.writeFloat(alternate+i*4,float(int(i)-15));}
  for(unsigned i=0;i<16;++i)m.write32(car+2720+i*4,rng());
  unsigned calls=0,diagnostics=0;
  c.callHooks[0xc022ce0]=[&](auto&q){check(++calls==1&&q.r[4]==car+2720,"LightON queries actual car+AA0");
   for(unsigned i=0;i<3;++i){check(m.read32(q.r[4]+32+i*4)==m.read32((alt?alternate:actor)+i*4),"LightON current point from chosen actor");check(m.read32(q.r[4]+44+i*4)==m.read32((alt?alternate:actor)+i*4),"LightON previous point from chosen actor");}
   m.writeFloat(q.r[4]+24,3.5f);m.write32(q.r[4]+28,0x12348000);q.r[0]=hit;};
  c.callHooks[0xc055d60]=[&](auto&q){check(q.r[4]==0xc23cc48,"Source LightON contact diagnostic");++diagnostics;};
  c.r[4]=car;c.r[15]=stack;c.pr=stop;instructions+=c.run(0xc035160,stop,10000);
  check(calls==1&&diagnostics==!hit,"Source LightON query/no-hit continues");
  if(present)for(unsigned i=0;i<9;++i)for(unsigned j=0;j<16;++j)check(m.read32(sourceQueries+i*64+j*4)==m.read32(car+2720+j*4),"LightON copies complete post-query including no-hit state");
 }
 // Full034E60 executes original228A60 matrix copy and0D7EC0 draw wrapper.
 // Only final1D7120 submission and already-proven0D50A0 are event hooks.
 for(unsigned enabled:{0u,1u,2u,255u})for(unsigned present:{0u,1u}){
  m.write8(car+81,enabled);m.write32(car+2636,present?obj:0);m.write32(car+2468,0xc380b8c);
  for(unsigned i=0;i<16;++i)m.writeFloat(car+2404+i*4,float(i+7));
  unsigned order=0;
  c.callHooks[0xc1d7120]=[&](auto&q){check(order++==0&&q.r[4]==mesh,"Original draws cached source mesh first");};
  c.callHooks[0xc0d50a0]=[&](auto&q){check(order++==1&&q.r[4]==obj,"Original advances after draw");for(unsigned i=0;i<16;++i)check(m.read32(q.r[5]+4*i)==m.read32(car+2404+4*i),"Exact ACar2404 pre-body matrix source");};
  c.r[4]=car;c.r[15]=stack;c.pr=stop;instructions+=c.run(0xc034e60,stop,1000);
  check(order==((enabled&&present)?2u:0u),"Headlight gate only enabled byte and object presence");
 }
 // Original initial setup calls car creation/initial pose BEFORE the night
 // LightON owner. Course light registration is a separate bounded leaf.
 constexpr unsigned race=obj+0x90000,course=race+0x1000,rival=car+0x8000,initFrame=race+0x2000;
 m.write32(initFrame+196,race);m.write32(race+1036,course);m.write32(race+1048,car);m.write32(race+1052,rival);
 m.write32(course,course+0x100);m.write16(course+0x110,0);m.write32(course+0x114,0xc03d340);
 m.write32(rival+2396,actor);m.write32(rival+2400,0);m.write32(rival+2636,obj);m.write32(car+2636,obj);
 std::vector<unsigned> initEvents;
 c.callHooks[0xc063ca0]=[&](auto&q){check(q.r[4]==race,"Original initial car setup owner");initEvents.push_back(1);};
 c.callHooks[0xc063d60]=[&](auto&q){check(q.r[4]==race,"Original night light plumbing owner");initEvents.push_back(2);};
 c.callHooks[0xc035160]=[&](auto&q){check(q.r[4]==car||q.r[4]==rival,"Original initial bind owner");initEvents.push_back(q.r[4]==car?3:4);};
 c.callHooks[0xc03d340]=[](auto&){};c.callHooks[0xc063be0]=[](auto&){};c.callHooks[0xc063460]=[](auto&){};c.callHooks[0xc063420]=[](auto&){};
 for(unsigned night:{0u,1u,2u,255u})for(unsigned mode:{0u,1u,2u,3u}){
  m.write32(race+1664,night);m.write32(race+1640,mode);m.write8(car+81,0);m.write8(rival+81,0);initEvents.clear();
  c.r[14]=initFrame;c.r[15]=stack;c.pr=stop;instructions+=c.run(0xc061c08,0xc061c26,10000);
  std::vector<unsigned> expected{1};if(night==1){expected.push_back(2);expected.push_back(3);if(mode!=2&&mode!=3)expected.push_back(4);}
  check(initEvents==expected,"Source setup/LightON order, exact night gate and rival mode gate");
  check(m.read8(car+81)==unsigned(night==1)&&m.read8(rival+81)==unsigned(night==1&&mode!=2&&mode!=3),"Source first light-request state");
 }
 std::cout<<"PASS "<<frames<<" original projected headlight frames, "<<checks<<" comparisons / "<<instructions<<" original instructions; "<<totalQueries<<" exact query inputs, "<<noHit<<" no-hit and "<<flagged<<" flagged results. Full0D4C80,0D5320,0D84A0,0D50A0,035120,035160,035200,034E60 and matrix helpers execute; file/TLS/allocation/debug/contact-result and asserted index/3 compiler-division boundaries. No device or user data.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
