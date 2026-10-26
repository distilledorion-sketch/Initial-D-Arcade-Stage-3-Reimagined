#include "unity_ui_capture.h"
#include "native_assets.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string_view>

namespace {
bool counting=false;
std::size_t allocations=0,allocatedBytes=0;
void* allocate(std::size_t n){if(counting){++allocations;allocatedBytes+=n;}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::uint64_t hash(std::uint64_t h,const void* data,std::size_t bytes){
 const auto* p=static_cast<const unsigned char*>(data);for(std::size_t i=0;i<bytes;++i){h^=p[i];h*=1099511628211ull;}return h;
}
}
void* operator new(std::size_t n){return allocate(n);}void* operator new[](std::size_t n){return allocate(n);}
void operator delete(void* p)noexcept{std::free(p);}void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
using namespace idas3;
UnityUiFrame frame(){UnityUiFrame f{sizeof(f)};require(Idas3UiGetFrame(&f)==1,"frame unavailable");return f;}
void triangle(unsigned* target,const NativeImage& image){
 require(unityUiTriangle(target,64,64,image,{1,2,0,0,0xffffffff,0},{3,4,1,0,0xffffffff,0},{5,6,0,1,0xffffffff,0},1,0,true,0),"capture disabled");
}
int main(int argc,char** argv)try{
 const bool bounded=argc==2&&std::string_view(argv[1])=="--expect-bounded";
 Idas3UiEnable(1);Idas3UiBeginFrame(128,128);
 NativeImage source{64,64,std::vector<unsigned>(64*64)},image{512,512,std::vector<unsigned>(512*512,0x7f123456)};
 std::vector<unsigned> output(128*128),other(128*128);
 for(unsigned visit=0;visit<32;++visit){
  unityUiClear(source.argb.data(),64,64);triangle(source.argb.data(),image);
  unityUiForgetTexture(image); // Actual source owners do this before replacing a bank.
 }
 const auto repeatedTextures=frame().textureCount;std::uint64_t retainedBytes=0;
 for(unsigned id=0;id<repeatedTextures;++id){UnityUiTextureInfo info{sizeof(info)};require(Idas3UiGetTextureInfo(id,&info)==1,"missing texture");retainedBytes+=info.bytes;}
 if(bounded)require(repeatedTextures==1&&retainedBytes==512u*512u*4u,"identical reloaded artwork accumulates exported textures");
 // Preserve old draw references when the source changes. Exact pixels and
 // dimensions both matter; equal texel counts do not imply equal artwork.
 image.argb[0]^=0x01000000;triangle(source.argb.data(),image);unityUiForgetTexture(image);
 image.width=1024;image.height=256;triangle(source.argb.data(),image);unityUiForgetTexture(image);
 require(frame().textureCount==repeatedTextures+2,"different pixels/dimensions were merged");
 std::vector<unsigned char> rgba(512*512*4);require(Idas3UiCopyTextureRGBA(0,rgba.data(),int(rgba.size()))==int(rgba.size()),"old export unavailable");
 require(rgba[0]==0x12&&rgba[1]==0x34&&rgba[2]==0x56&&rgba[3]==0x7f,"replaced source changed cached pixels");
 // Exercise copy, clipping, image-copy and same-surface aliases. The parity
 // hash normalizes texture IDs to dimensions/pixels, so deduplication may
 // change IDs but can never change rendered commands or artwork.
 Idas3UiEnable(1);Idas3UiBeginFrame(128,128);image={2,2,{0xff123456,0xffabcdef,0xffffffff,0}};
 unityUiClear(source.argb.data(),64,64);
 for(unsigned i=0;i<200;++i)triangle(source.argb.data(),image);
 auto compose=[&](){
  Idas3UiBeginFrame(128,128);
  unityUiCopy(output.data(),source.argb.data(),128,128,2,2);
  unityUiCopyRegion(output.data(),128,128,0,0,64,128,64,0,64,128);
  unityUiClear(other.data(),128,128);
  require(unityUiCopyImage(other.data(),128,128,source,1,2,64,64,.5f),"image surface unavailable");
  unityUiSubmit(output.data(),128,128,false,false,false);
  unityUiSubmit(other.data(),128,128,true,true,false);
 };
 for(unsigned i=0;i<4;++i)compose();
 auto f=frame();require(f.drawCount==600&&f.vertexCount==1800&&!f.unresolvedSurfaces,"copy/clip composition counts changed");
 std::vector<UnityUiDraw> draws(f.drawCount);std::vector<UnityUiVertex> vertices(f.vertexCount);
 Idas3UiCopyDraws(draws.data(),int(draws.size()));Idas3UiCopyVertices(vertices.data(),int(vertices.size()));
 std::uint64_t parity=hash(1469598103934665603ull,vertices.data(),vertices.size()*sizeof(vertices[0]));
 for(auto d:draws){UnityUiTextureInfo info{sizeof(info)};require(Idas3UiGetTextureInfo(d.texture,&info)==1,"draw texture missing");
  std::vector<unsigned char> pixels(info.bytes);Idas3UiCopyTextureRGBA(d.texture,pixels.data(),int(pixels.size()));
  d.texture=0;parity=hash(parity,&d,sizeof(d));parity=hash(parity,&info,sizeof(info));parity=hash(parity,pixels.data(),pixels.size());}
 allocations=allocatedBytes=0;counting=true;const auto began=std::chrono::steady_clock::now();
 for(unsigned i=0;i<2000;++i)compose();
 const auto milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();counting=false;
 const auto copyAllocations=allocations,copyBytes=allocatedBytes;
 if(bounded)require(copyAllocations==0,"warmed distinct surface copies still allocate temporary draw/vertex arrays");
 unityUiCopy(source.argb.data(),source.argb.data(),64,64,1,1,0,0,false);
 Idas3UiBeginFrame(64,64);unityUiSubmit(source.argb.data(),64,64,false,false,false);require(frame().drawCount==200,"self replacement lost source");
 unityUiCopy(source.argb.data(),source.argb.data(),64,64,1,1,2,3,true);
 Idas3UiBeginFrame(64,64);unityUiSubmit(source.argb.data(),64,64,false,false,false);require(frame().drawCount==400,"self append changed original extent");
 require(unityUiCopyImage(source.argb.data(),64,64,source,0,0,64,64,1),"self image-copy failed");
 Idas3UiBeginFrame(64,64);unityUiSubmit(source.argb.data(),64,64,false,false,false);require(frame().drawCount==800,"self image-copy changed original extent");
 std::cout<<"{\"passed\":true,\"boundedRequired\":"<<(bounded?"true":"false")<<",\"reloads\":32,\"exportedTextures\":"<<repeatedTextures
  <<",\"retainedPixelBytes\":"<<retainedBytes<<",\"copyFrames\":2000,\"copyAllocations\":"<<copyAllocations<<",\"copyAllocatedBytes\":"<<copyBytes
  <<",\"copyMilliseconds\":"<<milliseconds<<",\"visualCommandHash\":\""<<parity<<"\"}\n";
 Idas3UiEnable(0);return 0;
}catch(const std::exception& e){counting=false;std::cerr<<e.what()<<'\n';return 1;}
