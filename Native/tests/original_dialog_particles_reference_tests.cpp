#include "sh4_scalar_reference.h"
#include <iostream>
#include "original_dialog_particles.h"
using namespace idas3::original;
using namespace idas3::reference;
// Private source experiment, not linked into the game. The particle initializers
// and their RNG execute unhooked. Only final matrix/mesh dispatch is omitted in
// the render-owner experiment; the concrete generated records remain checked.
int main(int argc,char**argv)try{
 if(argc!=2)throw std::runtime_error("Canonical image required");
 RefMemory m(argv[1]);constexpr unsigned stack=0x0d00f000;
 constexpr unsigned owner=0x0d010000,element=0x0d011000,list=0x0d012000,model=0x0d013000,vt=0x0d014000;
 unsigned checks=0,cases=0;OriginalDialogParticles native;std::uint64_t instructions=0;
 auto eq=[&](unsigned a,unsigned b,const char* why){++checks;if(a!=b)throw std::runtime_error(std::string(why)+" actual="+hex(a)+" expected="+hex(b));};
 auto run=[&](RefCpu& c,unsigned a,unsigned z){try{instructions+=c.run(a,z,50000);}catch(const std::exception&e){throw std::runtime_error(hex(a)+": "+e.what());}};
 auto next=[](unsigned& seed){seed=seed*0x41c64e6du+12345u;return(seed>>16)&32767u;};
 auto randomFloat=[&](unsigned& seed){return float(next(seed))/32768.f;};
 auto word=[](float f){return std::bit_cast<unsigned>(f);};
 auto prepare=[&](unsigned seed){m.clear();m.zeroRegion(0x0d000000,0x20000);m.zeroRegion(0x0ca9b564,160);m.zeroRegion(0x0c31ac70,2400);m.write32(0x0c37c778,seed);};
 auto compareInitial=[&](unsigned& seed){
  for(unsigned i=0;i<10;++i){const unsigned p=0x0ca9b564+16*i;
   eq(m.read32(p),word(randomFloat(seed)-.5f),"Initial ten-particle X");
   eq(m.read32(p+4),word(randomFloat(seed)-.5f),"Initial ten-particle Y");
   eq(m.read32(p+8),0x391d4952,"Initial ten-particle Z");
   eq(m.read32(p+12),next(seed),"Initial ten-particle angle");
  }
  for(unsigned i=0;i<200;++i){const unsigned p=0x0c31ac70+12*i;
   eq(m.read32(p),word((randomFloat(seed)-.5f)*15.f),"Initial 200-particle X");
   eq(m.read32(p+4),word((randomFloat(seed)-.5f)*10.f),"Initial 200-particle Y");
   eq(m.read32(p+8),(i&1)?0x39ebedfa:0x3a03126f,"Initial 200-particle alternating Z");
  }
 };
 // This exact parent slice follows its virtual scene Init; it unconditionally
 // invokes FA540 then FB360, including in non-snow scenes. No hook at all.
 for(unsigned trial=0;trial<257;++trial){
  unsigned seed=trial==256?0xffffffffu:trial*0x9e3779b9u;prepare(seed);
  auto nativeSeed=seed;resetOriginalDialogParticles(native,nativeSeed);
  RefCpu c(m);c.r[8]=owner;c.r[15]=stack;run(c,0x0c0f8808,0x0c0f8814);
  compareInitial(seed);eq(m.read32(0x0c37c778),seed,"Normal dialog owner 430-draw completion");
  eq(nativeSeed,seed,"Translated initializer seed");
  for(unsigned i=0;i<10;++i){const auto& p=native.spectators[i];const unsigned a=0x0ca9b564+16*i;
   eq(word(p.x),m.read32(a),"Translated initial spectator X");eq(word(p.y),m.read32(a+4),"Translated initial spectator Y");eq(word(p.z),m.read32(a+8),"Translated initial spectator Z");eq(p.angle,m.read32(a+12),"Translated initial spectator angle");}
  for(unsigned i=0;i<200;++i){const auto& p=native.snow[i];const unsigned a=0x0c31ac70+12*i;
   eq(word(p.x),m.read32(a),"Translated initial snow X");eq(word(p.y),m.read32(a+4),"Translated initial snow Y");eq(word(p.z),m.read32(a+8),"Translated initial snow Z");}++cases;
 }
 // Bunta's distinct initializer has the same unconditional pair, after its
 // separate scene initialization. This does not assume a cheer draw for it.
 for(unsigned seedIn:{0u,1u,0xffffffffu,0x12345678u}){
  unsigned seed=seedIn;prepare(seed);RefCpu c(m);c.r[8]=owner;c.r[15]=stack;
  run(c,0x0c0f890c,0x0c0f8918);compareInitial(seed);
  eq(m.read32(0x0c37c778),seed,"Bunta dialog owner 430-draw completion");++cases;
 }
 // Actual parent draw gate reads the chosen element's +24 spectator flag.
 // Source FB240 initializes all10 visible records every eligible draw; its
 // RNG/record writes execute, but its subsequent matrix and final mesh calls
 // are explicit presentation boundaries. Source F4C60 executes unhooked.
 unsigned visibleFrames=0,snowFrames=0;
 for(unsigned course=0;course<=8;++course)for(unsigned flag:{0u,1u,2u,0xffffffffu}){
  unsigned seed=0x12345678+course*13+flag;prepare(seed);
  m.write32(0x0c31c9a0,course);m.write32(owner+424,list);m.write32(list+40,element);m.write32(element+24,flag);
  m.write32(owner+404,model);m.write32(model,vt);m.write16(vt+56,0);m.write32(vt+60,0x0f001000);
  native={};native.initialized=true;auto nativeSeed=seed;
  // Source snow branch is a boundary in this gate case; covered below.
  if(course!=8)prepareOriginalDialogParticles(native,course,flag,nativeSeed);
  RefCpu c(m);c.r[9]=owner;c.r[8]=owner+380;c.r[15]=stack;unsigned draws=0,snow=0;
  c.callHooks[0x0c0fb540]=[&](auto&){++snow;};
  for(unsigned address:{0x0c1fcc60u,0x0c1fd060u,0x0c1f68a0u,0x0c1f6950u,0x0c1fbd60u,0x0c1f65c0u,0x0c1f6610u})c.callHooks[address]=[](auto&){};
  c.callHooks[0x0f001000]=[&](auto&){++draws;};
  run(c,0x0c0fb0ee,0x0c0fb110);
  const bool visible=course!=8&&signed32(flag)>0;
  if(visible){
   for(unsigned i=0;i<10;++i){const unsigned p=0x0ca9b564+16*i;
    eq(m.read32(p),word((randomFloat(seed)-.5f)*15.f),"Visible ten-particle X");
    eq(m.read32(p+4),word((randomFloat(seed)-.5f)*10.f),"Visible ten-particle Y");
    eq(m.read32(p+8),0x391d4952,"Visible ten-particle Z");
    eq(m.read32(p+12),next(seed),"Visible ten-particle angle");
   }++visibleFrames;
  }
  if(course!=8){eq(nativeSeed,seed,"Translated gated draw seed");eq(native.showSpectators,visible,"Translated spectator gate");
   for(unsigned i=0;i<10;++i)if(visible){const auto& p=native.spectators[i];const unsigned a=0x0ca9b564+16*i;
    eq(word(p.x),m.read32(a),"Translated drawn spectator X");eq(word(p.y),m.read32(a+4),"Translated drawn spectator Y");eq(word(p.z),m.read32(a+8),"Translated drawn spectator Z");eq(p.angle,m.read32(a+12),"Translated drawn spectator angle");}}
  eq(draws,visible?10:0,"Parent visible spectator mesh count");eq(snow,course==8,"Separate snow draw branch");snowFrames+=snow;
  eq(m.read32(0x0c37c778),seed,"Parent conditional spectator RNG");++cases;
 }
 // Complete original snow painter advances every record only AFTER its
 // mesh submission. Capture those records at the final mesh sink and compare
 // both pre-update snapshot and persistent next-frame records over1000ticks.
 unsigned snowResets=0;
 for(unsigned seedIn:{1u,0x87654321u}){
  unsigned nativeSeed=seedIn;prepare(seedIn);resetOriginalDialogParticles(native,nativeSeed);
  RefCpu init(m);init.r[8]=owner;init.r[15]=stack;run(init,0x0c0f8808,0x0c0f8814);
  m.write32(owner+404,model);m.write32(model,vt);m.write16(vt+56,0);m.write32(vt+60,0x0f001000);
  for(unsigned tick=0;tick<500;++tick){
   prepareOriginalDialogParticles(native,8,0,nativeSeed);
   RefCpu c(m);c.r[4]=owner;c.r[15]=stack;c.pr=0x0f000000;unsigned drawn=0;
   c.callHooks[0x0c145900]=[](auto&){};
   c.callHooks[0x0c2223b8]=[](auto& q){q.fpul=unsigned(signed32(q.r[4])/signed32(q.r[5]));};
   c.callHooks[0x0f001000]=[&](auto& q){
    if(drawn>=200)throw std::runtime_error("Too many source snow draws");
    const auto& p=native.drawnSnow[drawn++];eq(word(p.x),m.read32(q.r[6]),"Snow draw before advance X");eq(word(p.y),m.read32(q.r[6]+4),"Snow draw before advance Y");eq(word(p.z),m.read32(q.r[6]+8),"Snow draw before advance Z");};
   run(c,0x0c0fb540,0x0f000000);eq(drawn,200,"All source snow records drawn");
   for(unsigned i=0;i<200;++i){const auto& p=native.snow[i];const unsigned a=0x0c31ac70+12*i;
    eq(word(p.x),m.read32(a),"Snow updated X");eq(word(p.y),m.read32(a+4),"Snow updated Y");eq(word(p.z),m.read32(a+8),"Snow retained Z");snowResets+=p.y==2.5f;}
   eq(nativeSeed,m.read32(0x0c37c778),"Snow conditional respawn shared RNG");++cases;
  }
 }
 if(!snowResets)throw std::runtime_error("No snow reset exercised");
 std::cout<<"Snow respawns="<<snowResets<<".\n";
 std::cout<<"PASS dialog RNG owners: "<<cases<<" cases, "<<checks<<" exact checks, "<<instructions<<" original instructions; 261 unconditional430-draw initializations; "<<visibleFrames<<" source30-draw spectator renders; "<<snowFrames<<" separately marked snow draw boundaries. No RNG/math hooks in initialization. Spectator rendering excludes seven matrix/mesh helpers and final mesh sink; snow initial gate is a marked boundary, then1000 full FB540 frames execute FB400 and RNG unhooked with size/finalmesh and quotient sinks.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
