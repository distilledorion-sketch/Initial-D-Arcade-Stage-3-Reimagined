#include "original_vs_banner.h"
#include "sh4_scalar_reference.h"
#include <algorithm>
#include <iostream>
using namespace idas3;
using namespace idas3::reference;
int main(int argc,char**argv)try{
 if(argc<3)throw std::runtime_error("canonical image and native root required");
 RefMemory m(argv[1]);OriginalVsBanner banner;banner.load(argv[2]);
 const auto model=NativeModel::load(std::filesystem::path(argv[2])/"data/original_assets/hud/start2d/start2d.idasmesh");
 constexpr unsigned info=0xd000000,bank=0xd001000,stack=0xd010000,stop=0xf000000;
 constexpr int semanticChunks[]{113,109,114,107,115,111,108};
 unsigned cases=0,checks=0;std::size_t instructions=0;
 auto check=[&](bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);};
 for(unsigned course=0;course<9;++course)for(unsigned reverse=0;reverse<2;++reverse)
 for(unsigned night=0;night<2;++night)for(unsigned wet=0;wet<2;++wet){
  m.clear();m.zeroRegion(info,0x20000);m.zeroRegion(0xc98ad0c,12);m.zeroRegion(0xce00000,0x10000);
  m.write16(0xc98ad0e,32);m.write32(0xc98ad10,0xce00000);m.write32(0xc98ad14,0xce00000);
  m.write32(info+4,bank);m.write32(info+8,~0u);m.write32(stack,wet);
  RefCpu c(m);c.r[4]=info;c.r[5]=course;c.r[6]=reverse;c.r[7]=night;c.r[15]=stack;c.pr=stop;
  for(unsigned i=0;i<16;++i)c.xf[i]=std::bit_cast<unsigned>(i%5==0?1.f:0.f);
  c.callHooks[0xc145a00]=[&](auto& cpu){check(cpu.r[4]==bank&&cpu.r[5]==104,"projection boundary changed");};
  c.callHooks[0xc145ac0]=[](auto&){};
  std::vector<std::pair<unsigned,float>> draws;
  c.callHooks[0xc145ae0]=[&](auto& cpu){
   check(cpu.r[4]==bank,"Wrong condition bank");
   const auto& chunk=model.chunks.at(cpu.r[5]);float left=INFINITY;
   for(const auto& batch:chunk.batches)for(const auto& v:batch.vertices)
    left=std::min(left,(v.position.x+std::bit_cast<float>(cpu.xf[12]))*100.f);
   draws.emplace_back(cpu.r[5],left);
  };
  instructions+=c.run(0xc0da700,stop,20000);
  check(draws.size()==3&&m.read16(0xc98ad0c)==0,"Condition count/stack imbalance");
  OriginalVsBannerSetup setup;setup.course=course;setup.night=night;setup.wet=wet;
  setup.direction=unsigned(std::find(std::begin(semanticChunks),std::end(semanticChunks),int(draws[0].first))-std::begin(semanticChunks));
  setup.compactHeader=true;setup.drawBackdrop=false;setup.showVersus=false;
  // Imported names extend the source title anchor; condition spacing must
  // still come from the same executed original routine.
  for(const auto& custom:{std::string{},std::string{"HAKONE"},std::string{"SADAMINE"}}){
   setup.customCourseName=custom;banner.begin(setup);const auto boxes=banner.metadataPlacements();
   float anchor=0;
   for(unsigned i=0;i<draws.size();++i){
    const auto found=std::find_if(boxes.begin(),boxes.end(),[&](auto b){return b.chunk==int(draws[i].first);});
    check(found!=boxes.end(),"Original condition chunk missing, including SNOW");
    if(!i)anchor=found->left;
    const float spacingScale=.15f/(.15f-model.chunks.at(draws[i].first).batches.front().vertices.front().position.z);
    check(std::abs((found->left-anchor)-(draws[i].second-draws[0].second)*spacingScale)<.001f,"Condition spacing differs from original routine");
    check(found->left>=0&&found->left+found->width<=640&&found->top>=0&&found->top+found->height<=55,"Header leaves compact strip");
   }
   if(argc>3){
    const auto path=std::filesystem::path(argv[3]);std::filesystem::create_directories(path);
    std::vector<std::uint32_t> pixels(640*480,0xff34485b);banner.paint(pixels,640,480);
    const auto file=path/((custom.empty()?std::string{"original"}:custom)+"-"+std::to_string(course)+"-"+std::to_string(reverse)+"-"+std::to_string(night)+"-"+std::to_string(wet)+".bmp");
    std::ofstream f(file,std::ios::binary);auto u16=[&](unsigned v){for(int j=0;j<2;++j)f.put(char(v>>(8*j)));};auto u32=[&](unsigned v){for(int j=0;j<4;++j)f.put(char(v>>(8*j)));};
    u16(0x4d42);u32(54+640*480*4);u32(0);u32(54);u32(40);u32(640);u32(0u-480);u16(1);u16(32);u32(0);u32(640*480*4);u32(2835);u32(2835);u32(0);u32(0);for(auto p:pixels)u32(p);
    check(bool(f),"Header preview write failed");
   }
   ++cases;
  }
 }
 std::cout<<"PASS "<<cases<<" layouts, "<<checks<<" checks, "<<instructions<<" original instructions. Relative condition spacing/selection verified, including imported names. Absolute original projection has a separate oracle; imported title art is a host extension.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
