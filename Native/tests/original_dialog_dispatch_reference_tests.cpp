#include "sh4_scalar_reference.h"
#include "original_dialog_particles.h"
#include <iostream>
using namespace idas3::reference;
using namespace idas3::original;
int main(int argc,char**argv)try{
 if(argc!=2)throw std::runtime_error("Canonical image required");RefMemory m(argv[1]);
 constexpr unsigned stack=0xd00f000,scene=0xd010000,list=0xd011000,element=0xd012000,model=0xd013000,vt=0xd014000,parent=0xd015000,noop=0xf001000,stop=0xf000000;
 unsigned checks=0,cases=0;std::uint64_t instructions=0;
 auto eq=[&](unsigned a,unsigned b,const char*why){++checks;if(a!=b)throw std::runtime_error(std::string(why)+" "+hex(a)+" != "+hex(b));};
 auto run=[&](RefCpu&c,unsigned a,unsigned z){try{instructions+=c.run(a,z,200000);}catch(const std::exception&e){throw std::runtime_error(hex(a)+" at "+hex(c.pc)+": "+e.what());}};
 for(unsigned phase:{0u,2u,3u})for(unsigned timer:{0u,14u,20u,21u})for(unsigned flag:{0u,1u}){
  m.clear();m.zeroRegion(0xd000000,0x20000);m.zeroRegion(0xc400000,0x1000);m.zeroRegion(0xca9b564,160);m.zeroRegion(0xc31ac70,2400);
  m.write32(scene+12,0xc388564);m.write32(scene+40,0xc388e44);m.write32(scene+364,0xc38850c);m.write32(scene+344,scene+28);
  m.write32(scene+44,scene+44);m.write32(scene+48,scene+44); // empty embedded viewport child list
  m.write32(scene+424,list);m.write32(list+40,element);m.write32(element+24,flag);
  m.write32(scene+392,model);m.write32(scene+404,model);m.write32(model,vt);m.write32(vt+44,noop);m.write32(vt+60,noop);
  m.write32(scene+420,phase);m.write32(scene+408,timer);m.write32(0xc31c9a0,0);m.write32(0xc37c778,0x12345678);
  OriginalDialogParticles native;auto seed=0x12345678u;resetOriginalDialogParticles(native,seed);
  RefCpu init(m);init.r[8]=scene;init.r[15]=stack;run(init,0xc0f8808,0xc0f8814);
  RefCpu c(m);c.r[4]=scene;c.r[15]=stack;c.pr=stop;unsigned draws=0;
  // Final viewport/model/matrix/line presentation (including C5200 for kind7) is explicitly outside this
  // owner test. Main, real vtables,1141E0,024100,1121C0 and the complete
  // FB0C0 gate/FB240/RNG execute. No Draw or RNG entry is hooked.
  for(unsigned address:{0xc1458c0u,0xc112d40u,0xc112d60u,0xc1fc5a0u,0xc1fcc60u,0xc1fd060u,0xc1f68a0u,0xc1f6950u,0xc1fbd60u,0xc1f65c0u,0xc1f6610u,0xc1d0880u,0xc1cf0a0u,0xc1f83a0u,0xc1db900u,0xc1fb320u,0xc1f8440u,0xc0f4c40u,0xc0f6880u,0xc0c5200u})c.callHooks[address]=[](auto&){};
  c.callHooks[noop]=[&](auto&q){if(q.r[5]!=18&&q.r[5]!=19)++draws;};
  run(c,0xc0fad60,stop);prepareOriginalDialogParticles(native,0,flag,seed);
  eq(m.read32(0xc37c778),seed,"Main includes exact accepted Draw RNG");eq(draws,flag?10:0,"Main draw count");
  if(phase==3&&m.read32(scene+20)!=(timer>=20?8u:0u))std::cerr<<"phase="<<phase<<" timer="<<timer<<" after="<<m.read32(scene+408)<<" state="<<m.read32(scene+420)<<"\\n"; if(phase==3)eq(m.read32(scene+20),timer>=20?8:0,"Final closing tick still draws");
  for(unsigned i=0;i<10;++i)if(flag){const auto&p=native.spectators[i];unsigned a=0xca9b564+16*i;
   eq(std::bit_cast<unsigned>(p.x),m.read32(a),"Draw X");eq(std::bit_cast<unsigned>(p.y),m.read32(a+4),"Draw Y");eq(p.angle,m.read32(a+12),"Draw angle");}
  ++cases;
  if(phase==0&&timer==14&&flag==1){
   // Actual state-list dispatcher and iRivalWon Init/Main: allocation/asset
   // setup is a declared boundary, but F8780's two initializers, dispatch,
   // parent F0560, child Main and Draw are original instructions.
   constexpr unsigned dispatch=0xd016000,tls=0xd017000;
   m.zeroRegion(0xc31c99c,1600);m.zeroRegion(0xc92ed00,4);m.write32(0xc31de08,0);
   m.write32(parent+12,0xc3882dc);m.write32(parent+4,dispatch);m.write32(parent+20,0);
   m.write32(dispatch,dispatch);m.write32(dispatch+4,parent);m.write32(dispatch+36,parent);
   m.write32(parent+80,scene);m.write32(parent+112,0);m.write32(0xc37c778,0x12345678);draws=0;
   unsigned initializations=0;
   c.callHooks[0xc0ef620]=[&](auto&){m.write32(parent+80,scene);m.write32(parent+112,0);};
   c.callHooks[0xc0f8940]=[](auto&){}; // copied player-name presentation
   c.callHooks[0xc0f8b00]=[&](auto&){
    ++initializations;eq(draws,0,"No Draw before scene Init returns");
    RefCpu rand(m);rand.r[15]=stack-4096;rand.pr=stop;run(rand,0xc1f9e60,stop);
    m.write32(scene+420,0);m.write32(scene+408,15);m.write32(scene+412,0);m.write32(scene+428,0);
   };
   c.callHooks[0xc221fc0]=[](auto&q){q.r[0]=tls;};
   for(unsigned a:{0xc055d60u,0xc141ec0u,0xc0d4300u,0xc0d42c0u,0xc202b20u,0xc156c40u})c.callHooks[a]=[](auto&q){q.r[0]=0;};
   c.callHooks[0xc0d43a0]=[](auto&q){q.setFloat(0,0);};
   c.r[4]=dispatch;c.r[15]=stack;c.pr=stop;run(c,0xc023480,stop);
   auto expected=0x12345678u;dialog_particles_detail::random(expected);resetOriginalDialogParticles(native,expected);prepareOriginalDialogParticles(native,0,1,expected);
   eq(initializations,1,"Exactly one parent-entry initialization");eq(draws,10,"Exactly one Draw on first accepted parent tick");eq(m.read32(0xc37c778),expected,"Init then first Main shared RNG order");
   eq(m.read32(scene+408),14,"First entry includes exactly one child Main");eq(m.read32(parent+108),200,"Parent remains opening phase");
   c.r[4]=dispatch;c.r[15]=stack;c.pr=stop;run(c,0xc023480,stop);prepareOriginalDialogParticles(native,0,1,expected);
   eq(initializations,1,"Second parent tick does not reinitialize");eq(draws,20,"Second accepted tick draws once");eq(m.read32(0xc37c778),expected,"Second Main shared RNG order");
   // Ready's Draw precedes parent's CloseDialog state transition.
   m.write32(scene+420,2);m.write32(scene+408,0);unsigned phaseAtDraw=99;
   c.callHooks[noop]=[&](auto&q){if(q.r[5]!=18&&q.r[5]!=19){++draws;phaseAtDraw=m.read32(scene+420);}};
   c.r[4]=dispatch;c.r[15]=stack;c.pr=stop;run(c,0xc023480,stop);prepareOriginalDialogParticles(native,0,1,expected);
   eq(phaseAtDraw,2,"Ready Draw precedes parent close");eq(m.read32(scene+420),3,"Parent then changes child to closing");eq(m.read32(parent+108),201,"Parent close transition");
   eq(m.read32(0xc37c778),expected,"Transition still only one Draw");
   ++cases;
   for(unsigned parentVtable:{0xc38c86cu,0xc38c7c4u,0xc38c914u}){
    m.write32(parent+12,parentVtable);m.write32(parent+20,0);m.write32(parent+108,0);m.write32(scene+20,0);
    m.write32(0xc37c778,0x12345678);draws=0;initializations=0;
    c.callHooks[0xc059f80]=[](auto&){};
    c.callHooks[0xc1416a0]=[](auto&){};
    c.callHooks[0xc021960]=[](auto&q){q.r[0]=scene;}; // already allocated private scene
    c.callHooks[0xc0f8340]=[](auto&q){q.r[0]=scene;}; // actual scene vtables/list provided above
    c.callHooks[0xc0f9a00]=[&](auto&){++initializations;eq(draws,0,"Bunta Init before first Draw");
     m.write32(scene+420,0);m.write32(scene+408,15);m.write32(scene+412,0);m.write32(scene+428,0);};
    c.r[4]=dispatch;c.r[15]=stack;c.pr=stop;run(c,0xc023480,stop);
    expected=0x12345678;resetOriginalDialogParticles(native,expected);prepareOriginalDialogParticles(native,0,1,expected);
    eq(initializations,1,"Bunta own initializer called once");eq(draws,10,"Bunta first accepted tick draws once");eq(m.read32(scene+408),14,"Bunta first child Main epoch");
    eq(m.read32(0xc37c778),expected,"Bunta has430 init then Draw; no normal cheer draw");
    c.r[4]=dispatch;c.r[15]=stack;c.pr=stop;run(c,0xc023480,stop);prepareOriginalDialogParticles(native,0,1,expected);
    eq(initializations,1,"Bunta second tick does not reinitialize");eq(draws,20,"Bunta second tick draws once");eq(m.read32(0xc37c778),expected,"Bunta second Main RNG");
    ++cases;
   }
   // Actual normal pre-race Main051EE0: unlike the post-race return,
   // held skip is applied before child Main, Ready closes the same kind,
   // and the final closing draw requests the parent handoff immediately.
   for(unsigned childPhase:{2u,3u}){
    m.write32(parent+12,0xc3828c4);m.write32(parent+80,scene);m.write8(parent+28,0);
    m.write32(scene+20,0);m.write32(scene+420,childPhase);m.write32(scene+408,childPhase==3?20:0);
    m.write32(scene+508,0);m.write8(0xc92ed00,0x80);m.write32(0xc31de08,0);
    m.write32(scene+372,7); // F8780 stores kind at013C+56
    unsigned skipAtDraw=0,phaseAtDraw=99,parentReadyCalls=0;
    c.callHooks[noop]=[&](auto&q){if(q.r[5]!=18&&q.r[5]!=19){
     skipAtDraw=m.read32(scene+508);phaseAtDraw=m.read32(scene+420);++parentReadyCalls;}};
    const auto before=m.read32(0xc37c778);auto expectedPre=before;
    prepareOriginalDialogParticles(native,0,1,expectedPre);
    c.r[4]=parent;c.r[15]=stack;c.pr=stop;run(c,0xc051ee0,stop);
    eq(skipAtDraw,1,"Normal pre-race skip precedes child Draw");
    eq(parentReadyCalls,10,"Normal pre-race draws exactly once before handoff");
    eq(phaseAtDraw,childPhase,"Normal pre-race Draw precedes Ready close");
    eq(m.read32(scene+420),3,"Normal pre-race Ready closes instead of advancing kind");
    eq(m.read32(scene+372),7,"Normal pre-race retains configured kind");
    eq(m.read8(parent+28),childPhase==3?1:0,"Closed source child requests parent handoff now");
    eq(m.read32(0xc37c778),expectedPre,"Normal pre-race accepts one Draw RNG only");
    ++cases;
   }
  }
 }
 std::cout<<"PASS "<<cases<<" complete dialog Main→viewport→Draw cases, "<<checks<<" checks, "<<instructions<<" original instructions. Explicit presentation sinks only; final closed tick still advances Draw records.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}




