#include "original_hud.h"
#include "unity_ui_capture.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <new>
#include <stdexcept>

namespace { bool countAllocations=false;std::size_t allocations=0,allocatedBytes=0;
void* allocate(std::size_t size){if(countAllocations){++allocations;allocatedBytes+=size;}if(void* p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
std::uint64_t hashBytes(std::uint64_t h,const void* data,std::size_t size){const auto* bytes=static_cast<const unsigned char*>(data);for(std::size_t i=0;i<size;++i){h^=bytes[i];h*=1099511628211ull;}return h;}
}
void* operator new(std::size_t n){return allocate(n);}void* operator new[](std::size_t n){return allocate(n);}
void operator delete(void* p)noexcept{std::free(p);}void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
using namespace idas3;
int main(int argc,char** argv)try{
 if(argc!=3)throw std::runtime_error("hud_composition_perf_tests asset-root report-directory");
 const auto hud=OriginalRaceHud::load(argv[1]);const std::filesystem::path out=argv[2];std::filesystem::create_directories(out);
 std::ofstream parity(out/"parity.csv");parity<<"layout,tach,state,pixel_hash,draw_hash,vertex_hash,draws,vertices\n";
 unsigned cases=0;
 for(unsigned layout=0;layout<2;++layout)for(unsigned tach=0;tach<4;++tach)for(unsigned phase=0;phase<12;++phase){
  OriginalHudState s;s.tachType=tach;s.alternateLayout=layout!=0;s.edgeAnchored=true;s.timePanel=true;
  s.speedKmh=phase*24.125f;s.rpm=phase*1000.f;s.gear=phase%7;s.automatic=(phase&1)!=0;
  s.elapsedTicks6000=phase*123456;s.remainingTicks6000=phase*60000;s.sectionCount=phase%4;
  s.sectionTimes6000={123456,278990,444320,610032};s.timeExtended=phase==8;
  if(phase>=9)s.finishBanner=OriginalHudState::FinishBanner(phase-8);
  std::vector<std::uint32_t> pixels(1280*720,0xff526a80);
  Idas3UiEnable(0);hud.paint(pixels,1280,720,s);
  const auto pixelHash=hashBytes(1469598103934665603ull,pixels.data(),pixels.size()*sizeof(pixels[0]));
  Idas3UiEnable(1);Idas3UiBeginFrame(1280,720);unityUiClear(pixels.data(),1280,720);
  hud.paint(pixels,1280,720,s);unityUiSubmit(pixels.data(),1280,720,false,false,false);
  UnityUiFrame f{sizeof(f)};if(!Idas3UiGetFrame(&f)||f.unresolvedSurfaces)throw std::runtime_error("HUD command capture failed");
  std::vector<UnityUiDraw> draws(f.drawCount);std::vector<UnityUiVertex> vertices(f.vertexCount);
  Idas3UiCopyDraws(draws.data(),int(draws.size()));Idas3UiCopyVertices(vertices.data(),int(vertices.size()));
  parity<<layout<<','<<tach<<','<<phase<<','<<pixelHash<<','<<hashBytes(1469598103934665603ull,draws.data(),draws.size()*sizeof(UnityUiDraw))<<','<<hashBytes(1469598103934665603ull,vertices.data(),vertices.size()*sizeof(UnityUiVertex))<<','<<draws.size()<<','<<vertices.size()<<'\n';++cases;
 }
 // Warm the capture pools before measuring only the production HUD command path.
 OriginalHudState s;s.tachType=3;s.timePanel=true;s.edgeAnchored=true;s.speedKmh=151;s.rpm=8500;s.gear=4;s.sectionCount=3;
 s.sectionTimes6000={120000,240000,360000,0};s.elapsedTicks6000=450000;s.remainingTicks6000=320000;
 std::vector<std::uint32_t> pixels(1280*720);std::ofstream perf(out/"performance.csv");perf<<"trial,frames,milliseconds,allocations,allocated_bytes\n";
 for(unsigned trial=0;trial<6;++trial){
  constexpr unsigned frames=1000;allocations=allocatedBytes=0;
  const auto before=std::chrono::steady_clock::now();countAllocations=trial>0;
  for(unsigned i=0;i<frames;++i){Idas3UiBeginFrame(1280,720);unityUiClear(pixels.data(),1280,720);s.rpm=8000.f+float(i%1000);hud.paint(pixels,1280,720,s);unityUiSubmit(pixels.data(),1280,720,false,false,false);}
  countAllocations=false;const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-before).count();
  if(trial)perf<<trial<<','<<frames<<','<<ms<<','<<allocations<<','<<allocatedBytes<<'\n';
 }
 std::cout<<"PASS "<<cases<<" software-pixel/GPU-command snapshots and five 1000-frame allocation/timing trials\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
