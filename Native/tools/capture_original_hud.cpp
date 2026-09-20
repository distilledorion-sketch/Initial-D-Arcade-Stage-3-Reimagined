#include "sh4_scalar_reference.h"
#include "original_hud.h"
#include <iomanip>
#include <iostream>
// Development-only actual-byte HUD draw capture. No graphics device or guest runtime.
// Asset lookup and draw submission are explicit boundaries. Matrix/trig, number
// conversion, digit suppression and all inspected selector code execute original
// bytes except the explicitly declared PR1 division arithmetic boundaries.
using namespace idas3::reference;
constexpr unsigned hud=0x0d000000,bank=0x0d001000,sprite=0x0d002000,digits=0x0d003000;
constexpr unsigned records=0x0d004000,state=0x0d005000,frame=0x0d006000,stack=0x0d010000,stop=0x00ff0000;
void setup(RefMemory& m,RefCpu& c,const std::vector<unsigned>& fsca,unsigned mode){
 m.clear();m.zeroRegion(hud,0x7000);m.zeroRegion(stack,0x10000);m.zeroRegion(0x0c98ad0c,12);m.zeroRegion(0x0ce00000,0x10000);
 m.write16(0x0c98ad0e,32);m.write32(0x0c98ad10,0x0ce00000);m.write32(0x0c98ad14,0x0ce00000);
 m.write32(hud+104,bank);m.write32(hud+100,sprite);m.write32(hud+96,mode);m.writeFloat(hud+92,mode?.7f:1.f);
 m.write32(bank,0x0c38aa3c);m.write32(sprite,0x0c38a9dc);m.write32(sprite+20,records);
 m.write32(hud+28,digits);m.write32(digits,0x0c387a7c);m.write32(digits+4,records+80);m.write32(digits+12,records+80);m.write32(digits+24,3);
 // Texture IDs are original sprite bank indices; opaque sentinel payload makes
 // copying/digit selection observable without inventing renderer field meanings.
 for(unsigned i=0;i<15;i++)m.write32(records+i*80,i);
 c.r[15]=stack+0xf000;c.pr=stop;c.fscaHalfWave=fsca;
 for(unsigned i=0;i<16;i++)c.xf[i]=std::bit_cast<unsigned>(i%5==0?1.f:0.f);
}
int main(int argc,char**argv){try{
 if(argc!=5)throw std::runtime_error("canonical-image fsca-table capture-json game-root required");
 const auto native=idas3::OriginalRaceHud::load(argv[4]);
 RefMemory m(argv[1]);std::vector<unsigned> fsca(32768);std::ifstream fs(argv[2],std::ios::binary);fs.seekg(16);fs.read(reinterpret_cast<char*>(fsca.data()),131072);if(!fs||fsca[0]!=0||fsca[16384]!=0x3f800000)throw std::runtime_error("FSCA table missing or wrong header");
 std::ofstream out(argv[3]);if(!out)throw std::runtime_error("capture output");out<<std::setprecision(9);
 out<<"{\"schema\":\"idas3-original-hud-capture-v1\",\"hooks\":[\"235B00 unused asset bounds output\",\"145A00 bank projection: comparisons use model coordinates\",\"145AE0 polygon draw\",\"144480 sprite draw\",\"1E7E80 digit sprite submission\",\"1FA9E0 sprite submission state\",\"2223E0 bounded unsigned division: arithmetic boundary pending PR1 oracle support\",\"2223B8 bounded signed division: arithmetic boundary pending PR1 oracle support\",\"144600 alternate layout sprite placement capture\"],\"cases\":[";
 bool firstCase=true;std::size_t instructions=0,cases=0,comparisons=0;unsigned polygonDraws=0,spriteDraws=0;
 std::vector<idas3::OriginalHudDraw> expected;std::size_t next=0;
 auto checkPolygon=[&](RefCpu& cpu){if(next>=expected.size())throw std::runtime_error("extra original polygon");const auto& e=expected[next++];if(e.kind!=idas3::OriginalHudDraw::Kind::polygon||e.index!=cpu.r[5])throw std::runtime_error("native polygon selection mismatch draw "+std::to_string(next)+" expected "+std::to_string(e.index)+" original "+std::to_string(cpu.r[5]));++comparisons;for(unsigned i=0;i<16;i++){++comparisons;if(std::bit_cast<unsigned>(e.matrix.elements[i])!=cpu.xf[i])throw std::runtime_error("native HUD matrix word differs at "+std::to_string(i)+" draw "+std::to_string(next)+" chunk "+std::to_string(e.index));}};
 auto checkSprite=[&](unsigned index,bool digit){if(next>=expected.size())throw std::runtime_error("extra original sprite");const auto& e=expected[next++];++comparisons;if(e.kind!=idas3::OriginalHudDraw::Kind::sprite||(digit?e.textureOverride:e.index)!=index)throw std::runtime_error("native sprite selection mismatch");};
 auto checkComplete=[&](){if(next!=expected.size())throw std::runtime_error("missing native draw");};
 auto begin=[&](const char*kind,unsigned mode,unsigned value){if(!firstCase)out<<',';firstCase=false;out<<"{\"kind\":\""<<kind<<"\",\"mode\":"<<mode<<",\"value\":"<<value<<",\"draws\":[";++cases;};
 for(unsigned mode=0;mode<2;mode++)for(unsigned type=0;type<4;type++)for(unsigned rpm:{0u,1000u,4500u,8000u,12000u}){
  RefCpu c(m);setup(m,c,fsca,mode);begin("tach",mode,rpm);out.flush();bool first=true;unsigned draws=0;
  idas3::OriginalHudState ns;ns.alternateLayout=mode!=0;ns.rpm=float(rpm);ns.tachType=type;ns.flags=0x1000;expected=native.drawList(ns);next=0;
  c.callHooks[0x0c235b00]=[](auto&){};
  c.callHooks[0x0c145ae0]=[&](auto&cpu){checkPolygon(cpu);if(!first)out<<',';first=false;++draws;++polygonDraws;out<<"{\"chunk\":"<<cpu.r[5]<<",\"matrix\":[";for(unsigned r=0;r<4;r++)for(unsigned col=0;col<4;col++){if(r||col)out<<',';out<<std::bit_cast<float>(cpu.xf[col*4+r]);}out<<"]}";};
  c.r[4]=hud;c.r[5]=type;c.r[6]=0;c.setFloat(4,float(rpm));instructions+=c.run(0x0c0c9da0,stop,200000);
  checkComplete();if(draws!=2||m.read16(0x0c98ad0c))throw std::runtime_error("tach draw count or matrix balance");out<<"],\"tach_type\":"<<type<<"}";
 }
 for(unsigned mode=0;mode<2;mode++)for(unsigned value:{0u,1u,9u,10u,99u,100u,123u,199u,255u}){
  RefCpu c(m);setup(m,c,fsca,mode);begin("instruments",mode,value);bool first=true;
  idas3::OriginalHudState ns;ns.alternateLayout=mode!=0;ns.rpm=4500;ns.tachType=1;ns.gear=4;ns.speedKmh=float(value);ns.flags=0x7000;expected=native.drawList(ns);next=0;
  auto separator=[&](){if(!first)out<<',';first=false;};
  c.callHooks[0x0c235b00]=[](auto&){};
  c.callHooks[0x0c145ae0]=[&](auto&cpu){checkPolygon(cpu);separator();++polygonDraws;out<<"{\"chunk\":"<<cpu.r[5]<<",\"matrix\":[";for(unsigned r=0;r<4;r++)for(unsigned col=0;col<4;col++){if(r||col)out<<',';out<<std::bit_cast<float>(cpu.xf[col*4+r]);}out<<"]}";};
  c.callHooks[0x0c144480]=[&](auto&cpu){checkSprite(cpu.r[5],false);separator();++spriteDraws;out<<"{\"sprite\":"<<cpu.r[5]<<"}";};
  c.callHooks[0x0c1fa9e0]=[](auto&){};
  c.callHooks[0x0c1e7e80]=[&](auto&cpu){checkSprite(m.read32(cpu.r[4]),true);separator();++spriteDraws;out<<"{\"digit_texture\":"<<m.read32(cpu.r[4])<<"}";};
  m.write32(frame,hud);m.write32(frame+4,state);m.write32(frame+8,state+64);m.write32(state+104,0x7000);m.write32(state+12,4);m.write32(state+16,1);m.write32(state+20,1);m.writeFloat(state+84,float(value));m.writeFloat(state+88,4500);
  c.r[14]=frame;c.r[13]=hud+64;c.r[1]=hud;instructions+=c.run(0x0c0c9a8c,0x0c0c9c32,200000);
  checkComplete();if(m.read16(0x0c98ad0c))throw std::runtime_error("instrument matrix balance");out<<"]}";
 }
 for(unsigned mode=0;mode<2;mode++)for(unsigned time:{0u,1u,59u,60u,5999u,6000u,59999u,60000u,123456u,3599999u,3600000u,21600000u,0xffffffffu}){
  RefCpu c(m);setup(m,c,fsca,mode);begin("total_time",mode,time);bool first=true;
  idas3::OriginalHudState ns;ns.alternateLayout=mode!=0;ns.elapsedTicks6000=time;ns.flags=4;expected=native.drawList(ns);if(!mode)expected.erase(expected.begin());next=0;
  c.callHooks[0x0c2223e0]=[](auto&cpu){if(!cpu.r[5])throw std::runtime_error("timer zero divisor");cpu.fpul=cpu.r[4]/cpu.r[5];};
  c.callHooks[0x0c2223b8]=[](auto&cpu){if(!cpu.r[5])throw std::runtime_error("glyph zero divisor");cpu.fpul=unsigned(signed32(cpu.r[4])/signed32(cpu.r[5]));};
  c.callHooks[0x0c145ae0]=[&](auto&cpu){checkPolygon(cpu);if(!first)out<<',';first=false;++polygonDraws;out<<"{\"chunk\":"<<cpu.r[5]<<",\"matrix\":[";for(unsigned r=0;r<4;r++)for(unsigned col=0;col<4;col++){if(r||col)out<<',';out<<std::bit_cast<float>(cpu.xf[col*4+r]);}out<<"]}";};
  m.write32(frame,hud);m.write32(frame+4,state);m.write32(frame+8,state+64);m.write32(state+28,time);m.write32(state+104,4);
  c.r[14]=frame;c.r[13]=hud+64;instructions+=c.run(0x0c0c971c,0x0c0c98a2,200000);checkComplete();if(m.read16(0x0c98ad0c))throw std::runtime_error("timer matrix balance");out<<"]}";
 }
 for(unsigned capacity=2;capacity<=4;capacity++)for(unsigned count=0;count<=capacity;count++)for(int remaining:{-100,0,29999,30000,35999,36000,59999,60000,594000,700000}){
  RefCpu c(m);setup(m,c,fsca,0);begin("time_section_panel",count,unsigned(remaining));bool first=true;
  c.callHooks[0x0c145a00]=[](auto&){}; // Bank projection selection; compare in model units.
  idas3::OriginalHudState ns;ns.timePanel=true;ns.flags=4;ns.remainingTicks6000=remaining;ns.elapsedTicks6000=1234567;ns.sectionCapacity=capacity;ns.sectionCount=count;
  for(unsigned i=0;i<count;i++)ns.sectionTimes6000[i]=123456*(i+1);
  if(count==capacity-1&&remaining==0)ns.finishTicks6000=987654;
  expected=native.drawList(ns);next=0;
  c.callHooks[0x0c2223e0]=[](auto&cpu){if(!cpu.r[5])throw std::runtime_error("timer zero divisor");cpu.fpul=cpu.r[4]/cpu.r[5];};
  c.callHooks[0x0c2223b8]=[](auto&cpu){if(!cpu.r[5])throw std::runtime_error("glyph zero divisor");cpu.fpul=unsigned(signed32(cpu.r[4])/signed32(cpu.r[5]));};
  c.callHooks[0x0c145ae0]=[&](auto&cpu){checkPolygon(cpu);if(!first)out<<',';first=false;++polygonDraws;out<<"{\"chunk\":"<<cpu.r[5]<<",\"matrix\":[";for(unsigned r=0;r<4;r++)for(unsigned col=0;col<4;col++){if(r||col)out<<',';out<<std::bit_cast<float>(cpu.xf[col*4+r]);}out<<"]}";};
  const unsigned flags=0x414|(((1u<<capacity)-1)<<5);m.write32(state+104,flags);m.write32(state+4,unsigned(std::max(remaining,0)));m.write32(state+28,ns.elapsedTicks6000);
  unsigned previous=0;
  for(unsigned row=0;row<4;row++){
   unsigned duration=0xffffffff;
   if(row<count){duration=ns.sectionTimes6000[row]-previous;previous=ns.sectionTimes6000[row];}
   else if(row==count&&row<capacity)duration=(ns.finishTicks6000!=0xffffffff?ns.finishTicks6000:ns.elapsedTicks6000)-previous;
   m.write32(state+32+row*4,duration);
  }
  c.r[4]=hud;c.r[5]=state;instructions+=c.run(0x0c0c8d80,stop,200000);
  m.write32(frame,hud);m.write32(frame+4,state);m.write32(frame+8,state+64);c.r[14]=frame;c.r[13]=hud+64;
  instructions+=c.run(0x0c0c971c,0x0c0c98a2,200000);
  instructions+=c.run(0x0c0c98a2,0x0c0c9a80,200000);
  checkComplete();if(m.read16(0x0c98ad0c))throw std::runtime_error("time/section matrix balance");out<<"],\"section_capacity\":"<<capacity<<"}";
 }
 {
  RefCpu c(m);setup(m,c,fsca,1);begin("alternate_sprite_placement",1,0);bool first=true;unsigned count=0;
  m.write32(frame+0x2fc,hud+64);c.r[14]=frame;
  c.callHooks[0x0c144600]=[&](auto&cpu){if(!first)out<<',';first=false;++count;out<<"{\"sprite\":"<<cpu.r[5]<<",\"position\":["<<cpu.getFloat(4)<<','<<cpu.getFloat(5)<<','<<cpu.getFloat(6)<<"]}";};
  instructions+=c.run(0x0c0c7d88,0x0c0c7f44,200000);if(count!=15)throw std::runtime_error("alternate sprite count");out<<"]}";
 }
 {
  begin("car_tach_mapping",0,0);bool first=true;
  for(unsigned car=0;car<35;car++)for(unsigned tuning:{0u,4u,5u,127u,128u,255u})for(unsigned selection:{0u,2u,3u,255u}){
   RefCpu c(m);setup(m,c,fsca,0);m.write32(frame+0x140,hud);m.write32(hud+16,car);m.write32(frame+0xf0,0x0d007000);m.write32(0x0d00757c,state);m.write8(0x0c31ca40,std::uint8_t(tuning));m.write8(0x0c31ca34,std::uint8_t(selection));c.r[14]=frame;
   instructions+=c.run(0x0c065032,0x0c0650d8,10000);const auto actual=m.read32(state+20);++comparisons;
   if(actual!=idas3::originalTachTypeForCar(car,std::uint8_t(tuning),std::uint8_t(selection)))throw std::runtime_error("original car tach mapping mismatch");
   if(!first)out<<',';first=false;out<<"{\"car\":"<<car<<",\"tuning_byte8\":"<<tuning<<",\"selection_byte0\":"<<selection<<",\"tach_type\":"<<actual<<"}";
  }out<<"]}";
 }
 out<<"],\"original_instructions\":"<<instructions<<",\"case_count\":"<<cases<<",\"bit_exact_native_comparisons\":"<<comparisons<<",\"polygon_draws\":"<<polygonDraws<<",\"sprite_draws\":"<<spriteDraws<<"}\n";
 std::cout<<"PASS "<<cases<<" actual-byte HUD cases; "<<instructions<<" instructions; "<<comparisons<<" exact native comparisons; "<<polygonDraws<<" polygon draws and "<<spriteDraws<<" sprite submissions. All matrix stacks balanced; declared graphics and arithmetic boundaries.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
