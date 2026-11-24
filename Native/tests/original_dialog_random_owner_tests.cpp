#include "original_legend_visit.h"
#include "original_bunta_visit.h"
#include "sh4_scalar_reference.h"
#include <bit>
#include <iostream>
using namespace idas3::original;
using namespace idas3::reference;
namespace {
unsigned checks=0,cases=0;std::uint64_t instructions=0;
void require(bool ok,const char*why){++checks;if(!ok)throw std::runtime_error(why);}
struct Oracle {
 RefMemory m; unsigned seed=0x12345678u;
 explicit Oracle(const char* image):m(image){m.zeroRegion(0xd000000,0x10000);m.zeroRegion(0xca9b564,160);m.zeroRegion(0xc31ac70,2400);m.write32(0xc37c778,seed);}
 void run(unsigned begin,unsigned end){RefCpu c(m);c.r[15]=0xd00f000;c.pr=0xf000000;instructions+=c.run(begin,end,200000);seed=m.read32(0xc37c778);}
 void init(bool bunta){if(!bunta)run(0xc1f9e60,0xf000000);run(0xc0fa540,0xf000000);run(0xc0fb360,0xf000000);}
 void draw(const OriginalRivalDialogState& state,unsigned course){
  constexpr unsigned owner=0xd001000,model=0xd002000,vt=0xd003000;
  m.write32(owner+404,model);m.write32(model,vt);m.write32(vt+60,0xf001000);
  RefCpu c(m);c.r[4]=owner;c.r[15]=0xd00f000;c.pr=0xf000000;
  c.callHooks[0xf001000]=[](auto&){};
  if(course==8){
   c.callHooks[0xc145900]=[](auto&){};
   c.callHooks[0xc2223b8]=[](auto&q){q.fpul=unsigned(signed32(q.r[4])/signed32(q.r[5]));};
   instructions+=c.run(0xc0fb540,0xf000000,100000);
  } else if(std::bit_cast<std::int32_t>(state.spectators)>0) {
   for(unsigned a:{0xc1fcc60u,0xc1fd060u,0xc1f68a0u,0xc1f6950u,0xc1f65c0u,0xc1f6610u,0xc1fbd60u})c.callHooks[a]=[](auto&){};
   instructions+=c.run(0xc0fb240,0xf000000,20000);
  }
  seed=m.read32(0xc37c778);
 }
 void compare(const OriginalRivalDialogState& s,unsigned actual){
  require(actual==seed,"owner shared random diverged from original records");
  for(unsigned i=0;i<10;++i){auto&p=s.particles.spectators[i];auto a=0xca9b564+i*16;
   require(std::bit_cast<unsigned>(p.x)==m.read32(a),"spectator X");require(std::bit_cast<unsigned>(p.y)==m.read32(a+4),"spectator Y");require(std::bit_cast<unsigned>(p.z)==m.read32(a+8),"spectator Z");require(p.angle==m.read32(a+12),"spectator angle");}
  for(unsigned i=0;i<200;++i){auto&p=s.particles.snow[i];auto a=0xc31ac70+i*12;
   require(std::bit_cast<unsigned>(p.x)==m.read32(a),"snow X");require(std::bit_cast<unsigned>(p.y)==m.read32(a+4),"snow Y");require(std::bit_cast<unsigned>(p.z)==m.read32(a+8),"snow Z");}
 }
};
}
int main(int argc,char**argv)try{
 if(argc!=3)throw std::runtime_error("canonical image and native root required");
 auto p=makeOriginalFreshBattleProfile();p.setu(1180,1);p.setu(72,100000);
 std::vector<unsigned> pixels(640*480);
 for(unsigned course:{0u,3u,8u})for(bool bunta:{false,true}){
  p.setu(4,course);OriginalRivalDialogScene scene;scene.load(argv[2]);
  OriginalRivalDialogSetup setup;setup.enemy=bunta?30:0;setup.buntaChallenge=bunta;setup.buntaCourse=course;
  Oracle ref(argv[1]);auto seed=ref.seed;scene.beginWithSharedRandom(p,setup,seed);ref.init(bunta);ref.compare(scene.state(),seed);
  require(scene.state().particles.acceptedDraws==0,"begin must not fabricate standalone Draw");
  for(unsigned tick=0;tick<35;++tick){scene.stepWithSharedRandom(p,seed);ref.draw(scene.state(),course);ref.compare(scene.state(),seed);}
  const auto frozen=seed;const auto draws=scene.state().particles.acceptedDraws;
  scene.paint(pixels,640,480);scene.paint(pixels,640,480);
  require(seed==frozen&&scene.state().particles.acceptedDraws==draws,"repaints must not draw random");
  closeOriginalRivalDialog(scene.state());
  for(unsigned tick=0;tick<21;++tick){scene.stepWithSharedRandom(p,seed);ref.draw(scene.state(),course);ref.compare(scene.state(),seed);}
  require(scene.closed(),"closing completed");require(scene.state().particles.acceptedDraws==56,"closed final tick drew exactly once");
  scene.beginWithSharedRandom(p,setup,seed);ref.init(bunta);ref.compare(scene.state(),seed);require(scene.state().particles.acceptedDraws==0,"reconfigure resets prepared state");++cases;
 }
 // Wrapper checks exercise their accepted steps and each fresh configuration.
 // On snow, every accepted scene step must move the same retained records.
 for(bool before:{false,true}){
  p.setu(0,2);p.setu(4,8);p.setu(1112,4);OriginalBuntaVisit visit;visit.load(argv[2]);Oracle ref(argv[1]);auto seed=ref.seed;
  visit.begin(p,{before,1,0},seed);ref.init(true);ref.compare(visit.dialogueState(),seed);
  unsigned n=0;while(!visit.finished()&&n++<70){visit.advance(p,{true},seed);ref.draw(visit.dialogueState(),8);ref.compare(visit.dialogueState(),seed);}
  require(visit.finished(),"Bunta skip source lifetime");const auto frozen=seed;visit.advance(p,{true},seed);require(seed==frozen,"finished Bunta never steps again");++cases;
 }
 p=makeOriginalFreshBattleProfile();p.setu(4,8);p.setu(24,28);p.setu(1180,1);p.setu(72,100000);
 OriginalLegendVisit visit;visit.load(argv[2]);Oracle ref(argv[1]);auto seed=ref.seed;OriginalLegendVisit::Setup setup;setup.resultStatus=1;
 visit.begin(p,setup,seed);ref.init(false);ref.compare(visit.dialogueState(),seed);
 for(unsigned tick=0;tick<20;++tick){visit.advance(p,{},seed);ref.draw(visit.dialogueState(),8);ref.compare(visit.dialogueState(),seed);}
 ++cases;std::cout<<"PASS "<<cases<<" private scene/wrapper cases, "<<checks<<" checks, "<<instructions<<" original record instructions; init, accepted steps, reconfigure, final close and repaints.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
