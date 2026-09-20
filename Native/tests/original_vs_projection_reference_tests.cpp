#include "sh4_scalar_reference.h"
#include "original_vs_banner.h"
#include <algorithm>
#include <iostream>
#include <iomanip>
using namespace idas3::reference;
using namespace idas3;
int main(int argc,char**argv)try{
 if(argc!=3)throw std::runtime_error("canonical image and native root required");
 RefMemory m(argv[1]);
 OriginalVsBanner banner;banner.load(argv[2]);
 const auto model=NativeModel::load(std::filesystem::path(argv[2])/"data/original_assets/hud/start2d/start2d.idasmesh");
 // Execute the default UI camera itself instead of assuming its projection.
 // ARaceStandBy creates this camera with 1D0940 at 05B69A, then copies only
 // the 64-byte world pose at05B8F2. The FOV/viewport fields remain default.
 {
  RefMemory cameraMemory(argv[1]);cameraMemory.zeroRegion(0xd000000,0x100000);cameraMemory.zeroRegion(0xc980000,0x20000);cameraMemory.zeroRegion(0xce00000,0x10000);
  RefCpu cameraCpu(cameraMemory);cameraCpu.r[4]=0xd006000;cameraCpu.r[15]=0xd080000;cameraCpu.pr=0xf000000;
  cameraCpu.run(0xc1d0940,0xf000000,100000);
  std::array<unsigned,47> before{};for(unsigned i=0;i<47;++i)before[i]=cameraMemory.read32(0xd006000+i*4);
  for(unsigned i=0;i<16;++i)cameraMemory.writeFloat(0xd007000+i*4,i%5==0?1.f:i==12?123.f:i==13?-7.f:i==14?42.f:0.f);
  cameraCpu.r[4]=0xd006000;cameraCpu.r[5]=0xd007000;cameraCpu.r[6]=7;cameraCpu.pr=0xf000000;cameraCpu.run(0xc222372,0xf000000,10000);
  for(unsigned i=0;i<47;++i)if(cameraMemory.read32(0xd006000+i*4)!=(i<16?cameraMemory.read32(0xd007000+i*4):before[i]))throw std::runtime_error("World pose copy touched camera projection or missed pose");
  for(unsigned j=0;j<4;++j){auto base=0xc98ad0c+j*12,save=0xce00000+j*0x1000;cameraMemory.write32(base,0x00200000);cameraMemory.write32(base+4,save);cameraMemory.write32(base+8,save);for(unsigned i=0;i<16;++i)cameraMemory.writeFloat(save+i*4,i%5==0?1.f:0.f);}
  cameraCpu.callHooks[0xc1d9600]=[](auto& cpu){constexpr unsigned expected[]{0x3fe7c3b5,0,0,0,0,0x401a8279,0,0,0,0,0xbf800010,0xbf800000,0,0,0xbca3d715,0};for(unsigned i=0;i<16;++i)if(cpu.xf[i]!=expected[i])throw std::runtime_error("Intro camera projection changed after world pose copy");std::cerr<<"PASS source intro camera projection unchanged after actual world pose copy.\n";};
  cameraCpu.r[4]=0xd006000;cameraCpu.pr=0xf000000;cameraCpu.run(0xc1d0880,0xf000000,100000);
 }
 constexpr unsigned info=0xd000000,bank=0xd001000,positions=0xd002000,models=0xd003000,stack=0xd010000,stop=0xf000000;
 std::size_t instructions=0;unsigned draws=0;
 // Check both carry inputs and every individual source bit for ROTCR, used
 // by the original double-precision division routine below.
 for(unsigned carry=0;carry<2;++carry)for(unsigned bit=0;bit<32;++bit){
  m.write16(0xd020000,0x4325);RefCpu rotate(m);rotate.r[3]=1u<<bit;rotate.t=carry!=0;
  rotate.run(0xd020000,0xd020002);
  if(rotate.r[3]!=((bit?1u<<(bit-1):0u)|(carry?0x80000000u:0u))||rotate.t!=(bit==0))throw std::runtime_error("ROTCR carry/bit test failed");
  m.write16(0xd020000,0x4324);rotate.r[3]=1u<<bit;rotate.t=carry!=0;rotate.run(0xd020000,0xd020002);
  if(rotate.r[3]!=((bit<31?1u<<(bit+1):0u)|carry)||rotate.t!=(bit==31))throw std::runtime_error("ROTCL carry/bit test failed");
 }
 for(unsigned course=0;course<9;++course)for(unsigned reverse=0;reverse<2;++reverse)for(unsigned night=0;night<2;++night)for(unsigned wet=0;wet<2;++wet){
  m.clear();m.zeroRegion(info,0x20000);m.zeroRegion(0xc98ad0c,12);m.zeroRegion(0xce00000,0x10000);
  m.write16(0xc98ad0e,32);m.write32(0xc98ad10,0xce00000);m.write32(0xc98ad14,0xce00000);
  m.write32(info+4,bank);m.write32(info+8,~0u);m.write32(bank,0xc38a8ec);m.write32(bank+56,positions);m.write32(bank+60,models);
  for(unsigned i=0;i<121;++i){m.write32(models+4*i,0xd004000+4*i);for(unsigned j=0;j<3;++j)m.write32(positions+12*i+4*j,m.read32(0xc145408+4*j));}
  RefCpu angle(m);angle.r[4]=4096;angle.r[15]=stack;angle.pr=stop;instructions+=angle.run(0xc1fa280,stop,100000);
  m.write32(bank+80,angle.fr[0]);
  constexpr unsigned semanticChunks[]{113,109,114,107,115,111,108};
  const auto directionChunk=m.read32(0xc2fca64+4*m.read32(0xc2fc994+course*8+reverse*4));
  OriginalVsBannerSetup setup;setup.course=course;setup.direction=unsigned(std::find(std::begin(semanticChunks),std::end(semanticChunks),directionChunk)-std::begin(semanticChunks));setup.night=night;setup.wet=wet;setup.compactHeader=true;setup.drawBackdrop=false;setup.showVersus=false;banner.begin(setup);
  const auto actual=banner.metadataPlacements();
  std::cout<<std::setprecision(9);
  RefCpu c(m);c.r[4]=info;c.r[5]=course;c.r[15]=stack;c.pr=stop;
  for(unsigned i=0;i<16;++i)c.xf[i]=std::bit_cast<unsigned>(i%5==0?1.f:0.f);
  c.xf[13]=std::bit_cast<unsigned>(.65f);
  // Only draw submission is intercepted. Execute the source model bank,
  // matrix stack and float/double arithmetic, including the live getter.
  c.callHooks[0xc1d7120]=[&](auto& cpu){
   const unsigned chunk=(cpu.r[4]-0xd004000)/4;
   const auto found=std::find_if(actual.begin(),actual.end(),[&](auto b){return b.chunk==int(chunk);});if(found==actual.end())throw std::runtime_error("Original header chunk missing");
   float left=INFINITY,top=INFINITY,right=-INFINITY,bottom=-INFINITY;
   for(const auto& batch:model.chunks.at(chunk).batches)for(const auto& v:batch.vertices){
    const float values[]{v.position.x,v.position.y,v.position.z,1.f};float p[4]{};
    for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)p[row]+=std::bit_cast<float>(cpu.xf[row+4*col])*values[col];
    const float sx=320.f+320.f*std::bit_cast<float>(0x3fe7c3b5u)*p[0]/-p[2],sy=240.f-240.f*std::bit_cast<float>(0x401a8279u)*p[1]/-p[2];
    left=std::min(left,sx);right=std::max(right,sx);top=std::min(top,sy);bottom=std::max(bottom,sy);
   }
   if(std::abs(found->left-left)>.001f||std::abs(found->top-top)>.001f||std::abs(found->width-(right-left))>.001f||std::abs(found->height-(bottom-top))>.001f){std::cerr<<"course="<<course<<" chunk="<<chunk<<" source "<<left<<','<<top<<','<<right-left<<','<<bottom-top<<" actual "<<found->left<<','<<found->top<<','<<found->width<<','<<found->height<<'\n';throw std::runtime_error("Projected layout differs from executed source");}
   ++draws;std::cout<<course<<','<<reverse<<','<<night<<','<<wet<<','<<chunk<<',';
   for(auto v:cpu.xf)std::cout<<std::bit_cast<float>(v)<<',';
   std::cout<<'\n';
  };
  instructions+=c.run(0xc0da440,stop,100000);
  c.r[4]=info;c.r[5]=course;c.r[6]=reverse;c.r[7]=night;c.r[15]=stack;c.pr=stop;m.write32(stack,wet);
  instructions+=c.run(0xc0da700,stop,100000);
  if(m.read16(0xc98ad0c)!=0)throw std::runtime_error("matrix stack imbalance");
 }
 std::cerr<<"PASS "<<draws<<" absolute title/condition layouts, "<<instructions<<" original instructions. Actual camera projection and model matrices executed; GPU submission remains a boundary.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
