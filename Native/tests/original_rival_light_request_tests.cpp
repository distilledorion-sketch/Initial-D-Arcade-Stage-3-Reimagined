#include "original_rival_light_request.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <random>
using namespace idas3::original;
using namespace idas3::reference;
int main(int argc,char**argv)try{
 if(argc!=2)throw std::runtime_error("canonical image required");
 RefMemory m(argv[1]);std::size_t checks=0,instructions=0,cases=0;
 constexpr unsigned race=0xd000000,car=0xd010000,actor=0xd020000,alternate=0xd030000;
 constexpr unsigned hud=0xd040000,course=0xd050000,vt=0xd060000,stack=0xd070000,done=0xf000000;
 auto check=[&](bool b,const char*s){++checks;if(!b)throw std::runtime_error(std::string(s)+" case "+std::to_string(cases));};
 m.zeroRegion(race,0x80000);m.write32(race+1732,0xfeedbeef);
 RefCpu init(m);init.r[12]=race;instructions+=init.run(0xc063840,0xc063846,4);
 check(m.read32(race+1732)==OriginalRivalLightState{}.frames1732,"Actual rival constructor clears counter");
 check(m.read32(0xc063b3c)==0x41a00000&&m.read32(0xc063b40)==0xc099999a,"Canonical gap literal bits");
 // Constructor and pre-warmup producers execute original instructions over
 // poisoned destination fields. The authored history start is distinct from
 // accumulated progress; do not seed the latter from a native race rule.
 m.write32(stack+1392,race);m.write32(stack+1588,stack+0x400);
 m.write32(race+1404,0xfeedbeef);
 RefCpu ctor(m);ctor.r[14]=stack;ctor.r[3]=stack+0x800;
 instructions+=ctor.run(0xc05d6a8,0xc05d6e2,100);
 check(m.read32(race+1404)==0,"Actual ARace constructor leaves HUD pointer NULL");
 for(unsigned condition=0;condition<18;++condition){
  m.clear();m.zeroRegion(race,0x80000);
  for(unsigned off=0;off<168;off+=4)m.write32(hud+off,0xfeedbeef);
  m.write32(stack+48,hud);RefCpu h(m);h.r[14]=stack;
  instructions+=h.run(0xc0c7246,0xc0c72b0,100);
  check(m.read32(hud+100)==0,"Actual HUD constructor initializes retained advantage to positive zero");
  for(unsigned off=1408;off<=1476;off+=4)m.write32(race+off,0xfeedbeef);
  m.write32(race+1532+32,condition);
  RefCpu p(m);p.r[4]=race;p.r[15]=stack+0xff00;
  p.callHooks[0xc221fc0]=[&](auto&q){q.r[0]=stack+0x800;}; // TLS only.
  instructions+=p.run(0xc0671a0,0xc06726a,1000);
  for(unsigned off:{1412u,1424u,1436u,1448u})
   check(m.read32(race+off)==m.read32(0xc29e254+condition*4),"Actual191B00 authored history initializer");
  for(unsigned off:{1408u,1460u,1464u,1472u,1476u})
   check(m.read32(race+off)==0,"Actual progress initializer is zero independently of authored start");
 }
 auto run=[&](OriginalRivalLightState& state,OriginalRivalLightInputs in,bool oldOn,bool night,bool useAlternate,bool bootRead=false){
  m.clear();m.zeroRegion(race,0x80000);m.zeroRegion(0xc31c99c,1228);
  m.write32(race+1640,in.numericRaceMode);m.write32(race+1664,night);
  m.write32(race+1052,car);m.write32(race+1036,course);m.write32(race+1404,bootRead?0:hud);
  m.write32(race+1732,state.frames1732);m.write32(race+1460,std::bit_cast<unsigned>(in.playerProgress1460));
  m.writeFloat(bootRead?0x64:hud+100,in.signedAdvantage100);m.write32(0xc31c99c,in.profileMode);m.write32(0xc31c99c+24,in.enemy);
  m.write32(car+2388,0xc3811a4);m.write32(car+2392,vt);m.write16(vt+8,std::uint16_t(-2392));m.write32(vt+12,0xf000010);
  m.write32(car+2396,actor);m.write32(car+2400,useAlternate?alternate:0);
  m.write32(actor+80,useAlternate?(in.selectedActorFlags80^0x10000):in.selectedActorFlags80);
  m.write32(alternate+80,in.selectedActorFlags80);m.write8(car+81,oldOn);m.write8(car+2568,oldOn);
  m.write32(car+2636,car+4096);m.write32(car+4096,0xc38760c);
  const auto expected=advanceOriginalRivalLightRequest(state,in);
  unsigned bodyUpdates=0,publishes=0,binds=0,visibilityCalls=0;OriginalRivalLightRequest sourceRequest=OriginalRivalLightRequest::Hold;
  RefCpu c(m);c.r[4]=race;c.r[15]=stack+0xff00;c.pr=done;
  c.callHooks[0xc034840]=[&](auto&q){++bodyUpdates;check(q.r[4]==car,"Actual adjusted rival ACar callback");
   check(m.read8(car+81)==unsigned(oldOn),"Pose/publish occurs before light request");
   RefCpu p(m);p.r[13]=car;p.r[15]=stack+0x7f00;
   p.callHooks[0xc0d84a0]=[&](auto&){++publishes;};
   instructions+=p.run(0xc034b8e,0xc034bac,100);
  };
  c.callHooks[0xf000010]=[](auto&){}; // Unrelated car material color setter.
  c.callHooks[0xc035160]=[&](auto&q){++binds;check(q.r[4]==car,"Transition binds this rival");};
  // Record the real entry then execute its complete byte-level on/off setter.
  c.callHooks[0xc035120]=[&](auto&q){sourceRequest=OriginalRivalLightRequest::On;
   RefCpu setter(m);setter.r[4]=q.r[4];setter.r[15]=stack+0x6f00;setter.pr=done;
   setter.callHooks[0xc035160]=c.callHooks.at(0xc035160);
   instructions+=setter.run(0xc035120,done,100);
  };
  c.callHooks[0xc035200]=[&](auto&q){sourceRequest=OriginalRivalLightRequest::Off;
   RefCpu setter(m);setter.r[4]=q.r[4];setter.r[15]=stack+0x6f00;setter.pr=done;
   instructions+=setter.run(0xc035200,done,100);
  };
  c.callHooks[0xc035220]=[&](auto&){++visibilityCalls;}; // Separate actor visibility publication.
  instructions+=c.run(0xc0638c0,done,3000);
  check(sourceRequest==expected,"Source light request");
  check(m.read32(race+1732)==state.frames1732,"Counter including unsigned wrap/signed comparison");
  const bool finalOn=expected==OriginalRivalLightRequest::Hold?oldOn:expected==OriginalRivalLightRequest::On;
  check(m.read8(car+81)==unsigned(finalOn)&&m.read8(car+2568)==unsigned(finalOn),"Exact outer light bytes");
  const bool exists=in.numericRaceMode!=2&&in.numericRaceMode!=3;
  check(bodyUpdates==unsigned(exists)&&publishes==unsigned(exists&&oldOn),"Publication uses prior gate");
  check(binds==unsigned(expected==OriginalRivalLightRequest::On&&!oldOn),"On-to-on does not bind again");
  check(visibilityCalls==unsigned(exists&&in.numericRaceMode!=1),"Final visibility call scope");
  ++cases;return finalOn;
 };
 const std::array<unsigned,12> gapBits={0,0xbf800000,0xc0999999,0xc099999a,0xc099999b,0x419fffff,0x41a00000,0x41a00001,0x7f800000,0xff800000,0x7fc00000,0x80000000};
 for(unsigned bits:gapBits)for(unsigned count:{0u,239u,240u,241u,0x7fffffffu,0xffffffffu})
 for(int index:{-1,1439,1440,2249,2250}){
  OriginalRivalLightState state{count};OriginalRivalLightInputs in;in.enemy=29;in.signedAdvantage100=std::bit_cast<float>(bits);in.playerProgress1460=index;
  run(state,in,cases&1,(cases>>1)&1,(cases>>2)&1);
 }
 std::mt19937 rng(0x638c0);
 for(unsigned i=0;i<2048;++i){OriginalRivalLightState state{rng()};OriginalRivalLightInputs in;
  in.numericRaceMode=i%6;in.profileMode=(i/6)%3;in.enemy=28+(i/18)%3;in.selectedActorFlags80=rng();
  in.signedAdvantage100=std::bit_cast<float>(gapBits[i%gapBits.size()]);in.playerProgress1460=std::bit_cast<int>(rng());
  run(state,in,i&1,(i>>1)&1,(i>>2)&1);
 }
 // A continuous reachable special-rival sequence crosses the240->241 boundary,
 // restores when leaving the authored interval, then clears on a gap reset.
 OriginalRivalLightState sequence;OriginalRivalLightInputs in;in.enemy=29;in.signedAdvantage100=2.f;in.playerProgress1460=1440;bool on=true;
 for(unsigned tick=1;tick<=245;++tick){on=run(sequence,in,on,true,false);check(on==(tick<=240),"Natural dwell threshold exact frame241");}
 in.playerProgress1460=2250;on=run(sequence,in,on,true,false);check(on,"Outside authored progress interval restores on");
 in.playerProgress1460=1440;in.signedAdvantage100=20.f;on=run(sequence,in,on,true,false);check(on&&sequence.frames1732==0,"Gap upper bound resets dwell and restores on");
 // The initial tail precedes HUD allocation and reads address0x64. Its value
 // is an external boot-memory boundary, but any possible float requests On
 // from the freshly cleared counter. Subsequent60 warmups use actual0/0.
 for(unsigned bits:gapBits){OriginalRivalLightState warm;OriginalRivalLightInputs w;w.enemy=29;
  w.signedAdvantage100=std::bit_cast<float>(bits);bool enabled=run(warm,w,false,false,false);
  check(enabled&&warm.frames1732<=1,"Initial request is On for either boot-memory counter outcome");
  const unsigned firstCounter=warm.frames1732;w.signedAdvantage100=0;
  for(unsigned frame=1;frame<=60;++frame){enabled=run(warm,w,enabled,true,false);
   check(enabled&&warm.frames1732==firstCounter+frame,"Warmup retains zero HUD/progress and never resets counter at GO");}
 }
 // Supplied Naomi2 IC27 ROM word, independently checked by the boot-memory
 // tool. Execute the actual NULL+100 read, then the initialized HUD path.
 OriginalRivalLightState boot;OriginalRivalLightInputs bi;bi.enemy=29;
 bi.signedAdvantage100=std::bit_cast<float>(0xa05f7480u);
 bool bootOn=run(boot,bi,false,false,false,true);
 check(bootOn&&boot.frames1732==1,"Actual initial BIOS read enables rival and increments counter once");
 bi.signedAdvantage100=0;
 for(unsigned frame=1;frame<=60;++frame)bootOn=run(boot,bi,bootOn,true,false);
 check(bootOn&&boot.frames1732==61,"Source initial publication plus60warmups gives counter61");
 std::cout<<"PASS "<<cases<<" cases / "<<checks<<" checks / "<<instructions<<" original instructions. Full0638C0 owner,actual light setters/publication gate; body transform/material,road bind and actor visibility are explicit boundaries.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
