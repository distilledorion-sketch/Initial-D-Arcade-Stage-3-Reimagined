#include "original_hud.h"
#include "original_tuning.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace idas3;
void require(bool value,const char* what){if(!value)throw std::runtime_error(what);}
void bitmap(const std::filesystem::path& path,const std::vector<std::uint32_t>& pixels,unsigned w,unsigned h){
 std::ofstream f(path,std::ios::binary);auto u16=[&](unsigned v){for(int i=0;i<2;i++)f.put(char(v>>(8*i)));};auto u32=[&](unsigned v){for(int i=0;i<4;i++)f.put(char(v>>(8*i)));};
 u16(0x4d42);u32(54+w*h*4);u32(0);u32(54);u32(40);u32(w);u32(0u-h);u16(1);u16(32);u32(0);u32(w*h*4);u32(2835);u32(2835);u32(0);u32(0);for(auto p:pixels)u32(p);require(bool(f),"preview write failed");
}
int main(int argc,char**argv){try{
 if(argc<2)throw std::runtime_error("game-root [preview-directory]");
 const auto hud=OriginalRaceHud::load(argv[1]);const auto directory=argc>2?std::filesystem::path(argv[2]):std::filesystem::path{};
 if(!directory.empty())std::filesystem::create_directories(directory);
 unsigned cases=0;
 // Saved performance must select the source engine's authored dial,
 // including the AE86 racing-engine swap. Verify every car,
 // each package and all 76 supported performance levels against actual data.
 const auto physicsRoot=std::filesystem::path(argv[1])/"data/original_physics";
 const auto data=original::OriginalPhysicsData::load(physicsRoot/"tables.bin");
 const auto path=data.loadPath(physicsRoot,6);
 unsigned tuningCases=0,rpmCases=0;
 std::ofstream tuningReport;
 if(!directory.empty()){tuningReport.open(directory/"tachometer-car-tuning.csv");tuningReport<<"car,package,upgrade,tach_type,full_scale,source_working_base,source_tach_ceiling\n";}
 for(unsigned car=0;car<35;++car)for(unsigned package=0;package<4;++package)for(unsigned upgrade=0;upgrade<76;++upgrade){
  auto profile=original::makeOriginalFreshBattleProfile();profile.setu(16,car);profile.setByte(152,std::uint8_t(package));profile.setByte(164,std::uint8_t(upgrade));
  auto selection=original::makeOriginalFreshTimeAttackSelection(car,6,original::OriginalWeather::Dry);
  original::applyOriginalProfilePhysicsSelection(selection,profile);
  const auto parameters=data.parameters(selection,path);
  const auto type=originalTachTypeForCar(car,profile.byte(164),profile.byte(152));
  const float maximum=type==3?12000.f:float(type+8)*1000.f;
  // Source15EAxx caps tach overshoot to base+100 plus a sine of at most200.
  const float ceiling=parameters.transmission.workingBase+300.f;
  // At full throttle15E7F2..15E856 targets workingBase-500. The source
  // limiter can intentionally overshoot the face (car11 also does so stock).
  require(parameters.transmission.workingBase-500.f<=maximum,"Source full-throttle target exceeds selected dial");
  if(car==0)require(type==(upgrade>4&&package<=2?3u:0u),"AE86 swap dial does not follow saved package/performance");
  if(upgrade==0||upgrade==4||upgrade==5||upgrade==75)if(tuningReport)tuningReport<<car<<','<<package<<','<<upgrade<<','<<type<<','<<maximum<<','<<parameters.transmission.workingBase<<','<<ceiling<<'\n';
  for(float rpm:{0.f,800.f,maximum*.5f,parameters.transmission.workingBase-500.f,maximum}){
   require(std::bit_cast<std::uint32_t>(originalTachDisplayRpm(rpm,type))==std::bit_cast<std::uint32_t>(rpm),"Valid source RPM was changed for display");++rpmCases;
  }
  require(originalTachDisplayRpm(ceiling,type)==std::min(ceiling,maximum),"Source limiter overshoot escapes authored dial endpoint");++rpmCases;
  ++tuningCases;
 }
 for(unsigned type=0;type<4;++type){
  const float maximum=type==3?12000.f:float(type+8)*1000.f;
  for(float rpm:{-std::numeric_limits<float>::max(),-1.f,maximum+1.f,20000.f,std::numeric_limits<float>::max(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
   const float displayed=originalTachDisplayRpm(rpm,type);
   require(std::isfinite(displayed)&&displayed>=0&&displayed<=maximum,"Invalid RPM escapes host display bounds");
   OriginalHudState bounded;bounded.flags=0x1000;bounded.tachType=type;bounded.rpm=displayed;
   const auto list=hud.drawList(bounded);require(list.size()==2,"Guarded tach lost its source face or needle");
   for(const auto& d:list)for(float value:d.matrix.elements)require(std::isfinite(value),"Guarded tach matrix is non-finite");++rpmCases;
  }
 }
 bool rejected=false;try{(void)originalTachDisplayRpm(1,4);}catch(const std::out_of_range&){rejected=true;}require(rejected,"Invalid dial type accepted");
 std::cout<<"PASS "<<tuningCases<<" saved car/package/performance selections, "<<rpmCases<<" unchanged-valid or bounded-invalid RPM cases.\n";
 for(unsigned layout=0;layout<2;layout++)for(unsigned type=0;type<4;type++){
  OriginalHudState s;s.speedKmh=123.9f;s.rpm=6500;s.gear=4;s.automatic=layout!=0;s.tachType=type;s.elapsedTicks6000=1234567;s.alternateLayout=layout!=0;
  const auto commands=hud.drawList(s);require(commands.size()>=15,"essential HUD layers missing");
  std::vector<std::uint32_t> pixels(640*480,0xff34485b);hud.paint(pixels,640,480,s);
  const auto changed=std::count_if(pixels.begin(),pixels.end(),[](auto p){return p!=0xff34485b;});require(changed>2500&&changed<80000,"HUD footprint is empty or covers scene");
  if(!directory.empty())bitmap(directory/(std::string("hud-")+std::to_string(layout)+"-tach"+std::to_string(type)+".bmp"),pixels,640,480);++cases;
 }
 OriginalHudState s;s.flags=0;std::vector<std::uint32_t> empty(640*480);hud.paint(empty,640,480,s);require(std::all_of(empty.begin(),empty.end(),[](auto p){return p==0;}),"disabled HUD draws pixels");
 s.flags=0x4000;
 for(unsigned speed:{0u,9u,10u,99u,100u,999u}){s.speedKmh=float(speed);const auto list=hud.drawList(s);unsigned live=0,blank=0;for(const auto& d:list)if(d.kind==OriginalHudDraw::Kind::sprite){if(d.textureOverride!=0xffffffff)++live;else if(d.index==5||d.index==6)++blank;}require(live+blank==3,"digital speed columns incomplete");++cases;}
 s.flags=0x7004;s.tachType=1;s.speedKmh=140;s.rpm=7500;s.elapsedTicks6000=1234567;
 std::vector<std::uint32_t> wide(1280*720);hud.paint(wide,1280,720,s);
 for(int y=0;y<720;y++)for(int x=0;x<160;x++)require(wide[y*1280+x]==0&&wide[y*1280+1279-x]==0,"original4:3 HUD escaped letterbox");
 if(!directory.empty())bitmap(directory/"hud-wide-transparent.bmp",wide,1280,720);
 // TIME must remain on screen when the full original panel replaces the old
 // elapsed-only layout. Verify live/completed section rows and warning state.
 for(unsigned count=0;count<4;count++){
  s.timePanel=true;s.edgeAnchored=true;s.sectionCount=count;s.remainingTicks6000=count==3?30000:420000;
  s.sectionTimes6000={240000,510000,780000,0};s.finishTicks6000=count==3?1020000:0xffffffff;
  std::vector<std::uint32_t> panel(1280*720,0xff34485b);hud.paint(panel,1280,720,s);
  unsigned upper=0,rows=0;
  for(int y=0;y<330;y++)for(int x=0;x<240;x++)if(panel[y*1280+x]!=0xff34485b){if(y<125)++upper;else ++rows;}
  require(upper>4000,"TIME countdown was cropped above viewport");require(rows>4000,"section artwork or rows missing");
  if(!directory.empty())bitmap(directory/(std::string("hud-time-section-")+std::to_string(count)+".bmp"),panel,1280,720);++cases;
 }
 // Imported-course timers may exceed 99 seconds; the D3 default must stay capped.
 OriginalHudState extended;extended.flags=0;extended.timePanel=true;extended.remainingTicks6000=120*6000;
 auto digits=hud.drawList(extended);require(digits.size()==2&&digits[0].index==122&&digits[1].index==122,"Default D3 countdown no longer caps at 99");
 extended.extendedCountdown=true;
 for(unsigned seconds:{0u,5u,6u,9u,10u,99u,100u,120u,330u,999u}) {
  extended.remainingTicks6000=seconds*6000;digits=hud.drawList(extended);
  require(digits.size()==(seconds>=100?3u:seconds>=10?2u:1u),"Imported countdown loses digits");
  require(digits.back().index==(seconds<=5?124u:113u)+seconds%10,"Countdown units/warning glyph incorrect");
  if(seconds>=100)require(digits.front().index==113+seconds/100&&digits[1].index==113+(seconds/10)%10,"Imported countdown hundreds/tens incorrect");
  ++cases;
 }
 std::cout<<"PASS "<<cases<<" HUD appearance/digit cases, all4tach faces, both original layouts, disabled-layer and4:3 viewport checks. CPU rendering only.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
