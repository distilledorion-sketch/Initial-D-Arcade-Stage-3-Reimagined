#include "original_time_attack_stats.h"
#include "sh4_scalar_reference.h"
#include <algorithm>
#include <iostream>
#include <tuple>
using namespace idas3::original;using namespace idas3::reference;
namespace {
constexpr unsigned obj=0x0d000000,stack=0x0d080000,stop=0x00ff0000,stats=0x0c91fb0c;
void require(bool value,const std::string& why){if(!value)throw std::runtime_error(why);}
auto key(const OriginalTimeAttackStatDraw& d){return std::tuple(d.y,d.x,d.chunk);}
}
int main(int argc,char** argv){try{
 require(argc==2,"canonical image argument");RefMemory memory(argv[1]);unsigned fixtures=0;std::uint64_t instructions=0,comparisons=0;
 for(unsigned course=0;course<9;++course)for(unsigned scenario=0;scenario<9;++scenario){
  memory.clear();memory.zeroRegion(obj,0x100000);memory.zeroRegion(stats,128);RefCpu cpu(memory);
  OriginalTimeAttackTelemetrySnapshot telemetry;telemetry.valid=true;telemetry.maxSpeedKph=scenario==0?0:scenario==1?9.99f:scenario==2?99.99f:147.87f;
  telemetry.brakeFraction=scenario==3?std::numeric_limits<float>::quiet_NaN():scenario==4?std::numeric_limits<float>::infinity():scenario==5?-.01f:.031234f;
  telemetry.acceleratorFraction=scenario==6?1.01f:.8f;telemetry.convertedEventCount=scenario==6?128:scenario==7?99:4;telemetry.wallCount=45;
  telemetry.ditchCount=scenario==7?100:scenario==8?10:0;telemetry.startIntervalTicks6000=scenario==0?0:scenario==1?5:scenario==2?5999:4321;
  OriginalTimeAttackAnalysisInput in;in.course=course;in.finishTicks6000=scenario==0?0:987654;in.previousBestTicks6000=scenario<2?0:1000000;
  for(unsigned i=0;i<4;++i){in.currentSections6000[i]=(scenario==8&&i==3)?0:200000+i*100;in.previousSections6000[i]=scenario<2?0:200300-i*50;}
  memory.write32(0x0c31c99c+4,course);memory.write8(obj+0x1af98,course<2);memory.write8(obj+400,in.previousBestTicks6000!=0);
  memory.write32(stats+68,telemetry.startIntervalTicks6000);memory.write32(stats+72,in.finishTicks6000);memory.write32(stats+92,in.previousBestTicks6000);
  for(unsigned i=0;i<4;++i){memory.write32(stats+76+i*4,in.currentSections6000[i]);memory.write32(stats+96+i*4,in.previousSections6000[i]);}
  std::vector<OriginalTimeAttackStatDraw> source;
  // Narrow service boundaries: signed/unsigned division, power-of-ten libm,
  // and190820 model placement. No renderer/device/whole-program execution.
  cpu.callHooks[0x0c2223b8]=[](auto& c){c.fpul=unsigned(signed32(c.r[4])/signed32(c.r[5]));};
  cpu.callHooks[0x0c2223e0]=[](auto& c){c.fpul=c.r[4]/c.r[5];};
  cpu.callHooks[0x0c1f9da0]=[](auto& c){c.setFloat(0,std::pow(c.getFloat(4),c.getFloat(5)));};
  cpu.callHooks[0x0c190820]=[&](auto& c){const float x=memory.readFloat(c.r[15]),y=memory.readFloat(c.r[15]+4);
    source.push_back({c.r[5],x,y+24});memory.writeFloat(c.r[2],x);memory.writeFloat(c.r[2]+4,y);memory.writeFloat(c.r[2]+8,0);};
  auto run=[&](unsigned address){cpu.r[4]=obj;cpu.r[15]=stack;cpu.pr=stop;instructions+=cpu.run(address,stop,50000);};
  cpu.setFloat(4,telemetry.maxSpeedKph);run(0x0c18bbe0);
  cpu.setFloat(4,telemetry.brakeFraction*100.f);run(0x0c18bd80);
  cpu.setFloat(4,telemetry.acceleratorFraction*100.f);run(0x0c18bdc0);
  cpu.r[5]=telemetry.convertedEventCount;run(0x0c18bfe0);
  cpu.r[5]=telemetry.ditchCount;run(0x0c18c020);
  run(0x0c18c1e0);run(0x0c18c560);run(0x0c18c940);
  auto native=originalTimeAttackStatDraws(telemetry,in);
  std::sort(source.begin(),source.end(),[](auto& a,auto& b){return key(a)<key(b);});
  std::sort(native.begin(),native.end(),[](auto& a,auto& b){return key(a)<key(b);});
  require(source.size()==native.size(),"Glyph count course"+std::to_string(course)+" scenario"+std::to_string(scenario)+" source="+std::to_string(source.size())+" native="+std::to_string(native.size()));
  for(unsigned i=0;i<source.size();++i){const auto& a=source[i];const auto& b=native[i];
   require(key(a)==key(b),"Glyph mismatch course"+std::to_string(course)+" scenario"+std::to_string(scenario)+" index"+std::to_string(i)+" source="+std::to_string(a.chunk)+"@"+std::to_string(a.x)+","+std::to_string(a.y)+" native="+std::to_string(b.chunk)+"@"+std::to_string(b.x)+","+std::to_string(b.y));comparisons+=3;}
  ++fixtures;
 }
 // Invalid host maximum-speed values have no valid original digit-table
 // index. The native rendering boundary deliberately emits a dash instead
 // of an out-of-bounds asset lookup; do not claim a source UI for corrupt data.
 for(float bad:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),
     -std::numeric_limits<float>::infinity(),-123.f,2147483648.f}){
  OriginalTimeAttackTelemetrySnapshot telemetry;telemetry.valid=true;telemetry.maxSpeedKph=bad;
  for(const auto& draw:originalTimeAttackStatDraws(telemetry,{}))
   require(draw.chunk<47&&std::isfinite(draw.x)&&std::isfinite(draw.y),"Invalid host speed remains a bounded original-bank draw");
 }
 std::cout<<"PASS original Time Attack statistics: "<<fixtures<<" fixtures, "<<comparisons<<" glyph comparisons, "<<instructions<<" original instructions;5 invalid-host-speed guards\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
