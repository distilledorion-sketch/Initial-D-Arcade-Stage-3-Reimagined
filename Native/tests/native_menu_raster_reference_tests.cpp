#include "native_menu_raster_reference.h"
#include <iostream>
#include <random>
using namespace idas3;
std::size_t frames{},pixels{};
void equal(const std::vector<std::uint32_t>& a,const std::vector<std::uint32_t>& b){++frames;pixels+=a.size();for(std::size_t i=0;i<a.size();++i)if(a[i]!=b[i])throw std::runtime_error("Raster pixel changed in case"+std::to_string(frames)+" at"+std::to_string(i));}
int main(int argc,char**argv)try{
 if(argc!=2)throw std::invalid_argument("Pass native project root");const std::filesystem::path root=argv[1];
 constexpr int w=480,h=270;std::vector<std::uint32_t>a(w*h),b(w*h);std::mt19937 rng(0x6001);
 for(const char* name:{"adv_newtitle","v3sS00common","v3sS04maker","v3sS00minicar","v3sS00emblem","v3sS05cars","v3sS06mission","v3sK01course","v3sT02route","v3sT03weather","v3sT04time"}){
  const auto folder=root/"data/original_assets/menus/v3"/name;auto model=NativeModel::load(folder/(std::string(name)+".idasmesh"));auto textures=NativeTextureBank::load(folder/"textures/textures.idastex");
  for(const auto& chunk:model.chunks)for(int mode=0;mode<3;++mode){for(auto& pixel:a)pixel=rng();b=a;SpritePlacement p;p.scale=mode==0?56.25f:mode==1?40.f:120.f;p.offsetX=mode==0?60.f:float(w/2);p.offsetY=mode==0?0.f:float(h/2);p.invertY=true;p.authoredHeight=0;p.defaultOriginalUiColors=mode!=2;p.opacity=mode==1?.45f:1.f;
   compositeOriginalMenuChunk(a,w,h,textures,chunk,p);menu_reference::compositeOriginalMenuChunk(b,w,h,textures,chunk,p);equal(a,b);
  }
 }
 // Exercise blend endpoint optimizations for all PVR factors, vertex/material
 // environment modes and opacity, including nonwhite/offset fallbacks.
 auto bank=NativeTextureBank::load(root/"data/original_assets/menus/v3/adv_newtitle/textures/textures.idastex");
 // This nonconst test-owned bank is replaced only to exercise every alpha.
 auto& fixture=const_cast<NativeImage&>(bank.at(0));fixture={16,16,{}};fixture.argb.resize(256);for(unsigned i=0;i<256;++i)fixture.argb[i]=(i<<24)|(rng()&0xffffffu);
 NativeModelChunk chunk;chunk.batches.resize(1);auto& batch=chunk.batches[0];batch.ich[0]=0x0a;batch.ich[6]=0x4a;batch.material[9]=0;batch.indices={0,1,2,2,1,3};batch.vertices.resize(4);
 for(int i=0;i<4;++i){auto& v=batch.vertices[i];v.position={float(i/2)*150.f,float(i%2)*130.f,0};v.u=float(i/2);v.v=float(i%2);v.color0=0xffffffffu;}
 for(unsigned src=0;src<8;++src)for(unsigned dest=0;dest<8;++dest)for(unsigned env=0;env<4;++env)for(float opacity:{0.f,.45f,1.f}){
  for(auto& pixel:a)pixel=rng();b=a;batch.ich[2]=(src<<29)|(dest<<26)|(env<<6)|(1u<<20)|(1u<<16)|(1u<<15);SpritePlacement p;p.scale=1;p.offsetX=73.25f;p.offsetY=51.75f;p.opacity=opacity;
  compositeOriginalMenuChunk(a,w,h,bank,chunk,p);menu_reference::compositeOriginalMenuChunk(b,w,h,bank,chunk,p);equal(a,b);
 }
 std::cout<<"PASS menu raster optimization: "<<frames<<" image comparisons, "<<pixels<<" byte-identical pixels against frozen prior compositor.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
