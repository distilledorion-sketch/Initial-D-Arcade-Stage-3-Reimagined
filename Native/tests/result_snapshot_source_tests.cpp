#include "original_time_attack_telemetry.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3::original;using namespace idas3::reference;
namespace {
unsigned checks=0;std::uint64_t instructions=0;
void check(bool v,const char* s){++checks;if(!v)throw std::runtime_error(s);}
constexpr unsigned drive=0xc900f00,stats=0xc91fb0c,stack=0xd080000,stop=0xff0000,
 wallXYZ=0xc92dfc4,wallMagnitude=0xc9032cc,wallIndex=0xc92dca0,ditchXYZ=0xc91155c,ditchIndex=0xc92de34;
unsigned bits(float x){return std::bit_cast<unsigned>(x);}
}
int main(int argc,char**argv)try{
 check(argc==2,"canonical image required");RefMemory m(argv[1]);
 for(bool timeout:{false,true})for(unsigned elapsed:{360u,500u}){
  m.clear();m.zeroRegion(drive,1280);m.zeroRegion(stats,128);m.zeroRegion(stack-4096,8192);
  for(unsigned p:{wallXYZ,wallMagnitude,wallIndex,ditchXYZ,ditchIndex})m.zeroRegion(p,1600);
  m.writeFloat(drive,-130);m.writeFloat(drive+4,17);m.writeFloat(drive+8,-285);
  m.writeFloat(drive+0x238,40);m.writeFloat(drive+0x3e4,145);m.write32(drive+0x3f0,400);
  m.writeFloat(drive+0x3e8,200);m.writeFloat(drive+0x3ec,20);m.writeFloat(drive+0x408,.01f);
  m.write32(drive+0x1a8,timeout);m.write32(0xc900e84,elapsed);m.write32(0xc31c9a0,1);m.write32(0xc92de30,1630);
  m.write32(stats+20,wallXYZ);m.write32(stats+24,wallMagnitude);m.write32(stats+28,wallIndex);
  m.write32(stats+36,ditchXYZ);m.write32(stats+40,ditchIndex);
  OriginalTimeAttackTelemetry collector;OriginalDriveState nativeDrive;OriginalTailState tail;OriginalContactCompletionState completion;
  auto load=[&]{for(unsigned i=0;i<320;++i)nativeDrive.setu(i*4,m.read32(drive+i*4));
   for(unsigned i=0;i<16;++i)tail.statistics0C91FB0C[i]=m.read32(stats+i*4);
   completion.positionCursor=completion.frameCursor=m.read32(drive+0x3f8);
   for(unsigned i=0;i<completion.frameCursor;++i){for(unsigned a=0;a<3;++a)completion.impactPositions[i][a]=m.read32(ditchXYZ+i*12+a*4);completion.impactFrames[i]=m.read32(ditchIndex+i*4);}
  };
  auto publish=[&](float throttle,float brake,float steering,float speed){
   m.writeFloat(drive+0x1b8,throttle);m.writeFloat(drive+0x1c4,brake);m.writeFloat(drive+0x1cc,steering);m.writeFloat(drive+0x238,speed);
   RefCpu c(m);c.r[15]=stack;c.pr=stop;instructions+=c.run(0xc15ece0,stop,1000);
  };
  auto latch=[&]{RefCpu c(m);c.r[5]=0x2000;c.r[7]=drive+0x17c;instructions+=c.run(0xc157c5a,0xc157c66,50);check(m.read32(drive+0x1a8)==1,"source actor brake latch");};
  auto append=[&]{
   RefCpu w(m);w.r[9]=drive;w.setFloat(12,.04f);instructions+=w.run(0xc1580b4,0xc158104,100);
   RefCpu d(m);d.r[9]=drive;instructions+=d.run(0xc157dd0,0xc157e10,100);
   const OriginalImpactRecord event{{-130,17,-285},1630,.04f};collector.record({&event,1});load();collector.recordDitches(completion);
  };
  publish(.75f,.1f,.1f,40);append();load();const auto goal=collector.snapshot(1,nativeDrive,tail,completion);
  const auto countAtGoal=m.read32(drive+0x3f0);const auto meanAtGoal=m.read32(stats+4),steerAtGoal=m.read32(stats+60);
  //Source Finish applies actorbit13, but157C64 latches it only AFTER the first publisher.
  publish(0,.8f,.9f,42);latch();append();
  if(!timeout&&elapsed>360){check(m.read32(drive+0x3f0)==countAtGoal+1,"first Finish sample omitted");check(m.read32(stats+4)!=meanAtGoal&&m.read32(stats+60)!=steerAtGoal,"first Finish averages/steering must change");}
  else check(m.read32(drive+0x3f0)==countAtGoal&&m.read32(stats+4)==meanAtGoal&&m.read32(stats+60)==steerAtGoal,"automatic brake/early gate violated");
  const auto settledCount=m.read32(drive+0x3f0),settledMean=m.read32(stats+4);
  //Actual source manual upshift-to-top-gear flag. Gear edge/admission is an explicit boundary.
  m.write32(0xc900e88,5);RefCpu shift(m);shift.r[6]=5;instructions+=shift.run(0xc15e656,0xc15e666,50);
  publish(.1f,.2f,-.8f,39);latch();append();
  check(m.read32(drive+0x3f0)==settledCount&&m.read32(stats+4)==settledMean,"post-latch means changed");
  publish(.2f,.3f,.8f,38);latch();append();load();
  const auto beforeDrive=nativeDrive.words;const auto beforeStats=tail.statistics0C91FB0C;
  const auto late=collector.snapshotForLecture(1,nativeDrive,tail,completion);
  check(nativeDrive.words==beforeDrive&&tail.statistics0C91FB0C==beforeStats,"capture mutates numerical state");
  check(late.publishedStatistics0C91FB0C==tail.statistics0C91FB0C,"late published record differs");
  check(late.wallCount==m.read32(stats+16)&&late.ditchCount==m.read32(stats+32),"late count differs from source publication");
  check(late.wallCount==3&&late.ditchCount==3&&nativeDrive.u(0x3f4)==4&&nativeDrive.u(0x3f8)==4,"final unpublished contact not separated");
  check(late.wallEvents.size()==3&&late.ditchEvents.size()==3,"unpublished histories reached HLecture");
  check(late.maxGearUsed==m.read32(stats+56)&&late.maxGearUsed==1&&goal.maxGearUsed==0,"late top-gear flag missed");
  check(bits(late.maxSpeedKph)==m.read32(stats)&&bits(late.acceleratorFraction)==m.read32(stats+4)&&bits(late.brakeFraction)==m.read32(stats+8)&&bits(late.maxSteeringDelta)==m.read32(stats+60),"late scalar source publication differs");
  RefCpu count(m);count.r[15]=stack;count.pr=stop;instructions+=count.run(0xc190f00,stop,1000);
  check(late.convertedEventCount==count.r[0]&&late.convertedEventCount==3,"source qualifying-event count differs");
  check(late.sectionWallMarkers[0].size()==3&&late.sectionDitchMarkers[0].size()==3,"late map markers differ");
  check(goal.wallCount==1&&goal.ditchCount==1,"goal snapshot changed retrospectively");
  if(!timeout&&elapsed>360)check(bits(goal.acceleratorFraction)!=bits(late.acceleratorFraction),"old snapshot does not expose regression");
  //After a further publisher the last contact becomes eligible, without a fabricated contact.
  publish(0,0,0,35);load();const auto next=collector.snapshotForLecture(1,nativeDrive,tail,completion);
  check(next.wallCount==4&&next.ditchCount==4&&next.convertedEventCount==4,"later actual publication failed admission");
 }
 // Defensive retained-history bounds. Original ditch reader trusts its count
 // beyond98; native deliberately retains only the written98 slots. This is
 // a safe bounded-history policy, not a claim about original overflow memory.
 OriginalTimeAttackTelemetry bounded;OriginalDriveState driveState;OriginalTailState published;OriginalContactCompletionState contacts;
 for(unsigned i=0;i<99;++i){const OriginalImpactRecord event{{-130,17,-285},1630,.04f};bounded.record({&event,1});}
 contacts.positionCursor=contacts.frameCursor=98;
 for(unsigned i=0;i<98;++i){contacts.impactPositions[i]={bits(-130),bits(17),bits(-285)};contacts.impactFrames[i]=1630;}
 bounded.recordDitches(contacts);
 for(const unsigned count:{0u,0xffffffffu,1u,98u,99u,1000u}){
  published.statistics0C91FB0C[4]=published.statistics0C91FB0C[8]=count;
  const auto value=bounded.snapshotForLecture(1,driveState,published,contacts);
  const auto allowed=std::bit_cast<std::int32_t>(count)>0?count:0u;
  check(value.wallCount==count&&value.ditchCount==count,"Signed bound rewrote published counters");
  check(value.wallEvents.size()==std::min(allowed,99u)&&value.ditchEvents.size()==std::min(allowed,98u),"Bounded snapshot expanded unavailable history");
  check(value.convertedEventCount==std::min(allowed,99u),"Bounded wall map admitted synthetic history");
 }
 std::cout<<"PASS "<<checks<<" late-result source checks, "<<instructions<<" original instructions. Actual15ECE0 publisher,157C5A latch, contact append blocks,15E656 gear flag,190F00 counter. Synthetic contact admission/state; no whole race or rendering claim.\n";
}catch(const std::exception&e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}
