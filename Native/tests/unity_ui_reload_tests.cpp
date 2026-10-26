#include "original_rival_dialog_scene.h"
#include "unity_ui_capture.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
using namespace idas3;
using namespace idas3::original;
namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::uint64_t hash(std::uint64_t h,const void* data,std::size_t bytes){
 const auto* p=static_cast<const unsigned char*>(data);for(std::size_t i=0;i<bytes;++i){h^=p[i];h*=1099511628211ull;}return h;
}
}
int main(int argc,char** argv)try{
 require(argc==3||argc==4,"unity_ui_reload_tests asset_root csv_path [--expect-bounded]");
 const bool bounded=argc==4&&std::string_view(argv[3])=="--expect-bounded";
 std::ofstream output(argv[2]);require(bool(output),"report unavailable");
 output<<"visit,enemy,textures,retained_pixel_bytes,draws,vertices,visual_hash\n";
 Idas3UiEnable(1);OriginalRivalDialogScene scene;scene.load(argv[1]);
 auto profile=makeOriginalFreshBattleProfile();std::vector<unsigned> canvas(640*480);
 std::uint64_t firstCycleBytes=0;unsigned firstCycleCount=0;
 for(unsigned visit=0;visit<20;++visit){
  const unsigned enemy=visit%2?30:0;selectOriginalRival(profile,enemy);
  OriginalRivalDialogSetup setup;setup.enemy=enemy;scene.begin(profile,setup);
  // Exercise normal scene stepping and repainting, including retained layers.
  for(unsigned tick=0;tick<180;++tick){scene.step(profile);Idas3UiBeginFrame(640,480);unityUiClear(canvas.data(),640,480);scene.paint(canvas,640,480);unityUiSubmit(canvas.data(),640,480,false,false,false);}
  UnityUiFrame f{sizeof(f)};require(Idas3UiGetFrame(&f)&&f.drawCount>0&&!f.unresolvedSurfaces,"real dialogue produced no valid artwork");
  std::uint64_t bytes=0;std::unordered_map<unsigned,std::uint64_t> textureHashes;
  for(unsigned id=0;id<f.textureCount;++id){UnityUiTextureInfo info{sizeof(info)};require(Idas3UiGetTextureInfo(id,&info)==1,"texture unavailable");bytes+=info.bytes;}
  std::vector<UnityUiDraw> draws(f.drawCount);std::vector<UnityUiVertex> vertices(f.vertexCount);
  Idas3UiCopyDraws(draws.data(),int(draws.size()));Idas3UiCopyVertices(vertices.data(),int(vertices.size()));
  auto result=hash(1469598103934665603ull,vertices.data(),vertices.size()*sizeof(vertices[0]));
  for(auto draw:draws){
   if(!textureHashes.contains(draw.texture)){UnityUiTextureInfo info{sizeof(info)};Idas3UiGetTextureInfo(draw.texture,&info);
    std::vector<unsigned char> pixels(info.bytes);require(Idas3UiCopyTextureRGBA(draw.texture,pixels.data(),int(pixels.size()))==int(pixels.size()),"texture copy failed");
    auto h=hash(1469598103934665603ull,&info,sizeof(info));textureHashes[draw.texture]=hash(h,pixels.data(),pixels.size());}
   auto textureHash=textureHashes.at(draw.texture);draw.texture=0;result=hash(result,&draw,sizeof(draw));result=hash(result,&textureHash,sizeof(textureHash));
  }
  output<<visit<<','<<enemy<<','<<f.textureCount<<','<<bytes<<','<<f.drawCount<<','<<f.vertexCount<<','<<result<<'\n';output.flush();
  if(visit==1){firstCycleBytes=bytes;firstCycleCount=f.textureCount;}
  if(bounded&&visit>1)require(bytes==firstCycleBytes&&f.textureCount==firstCycleCount,"revisited rival artwork keeps growing the export pool");
 }
 Idas3UiEnable(0);std::cout<<"PASS 20 real rival-dialogue visits; "<<(bounded?"bounded":"baseline")<<" texture retention measured\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
