#include "unity_ui_capture.h"
#include "native_assets.h"
#include "original_mode_menu.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string_view>
using namespace idas3;
unsigned checks=0;
void check(bool ok,const char* what){++checks;if(!ok)throw std::runtime_error(what);}
UnityUiFrame frame(){UnityUiFrame f{sizeof(f)};check(Idas3UiGetFrame(&f)==1,"get frame");return f;}
int main(int argc,char** argv)try{
 check(argc==2||(argc==3&&std::string_view(argv[2])=="-idas3-scene-perf-baseline"),"usage unity_ui_capture_tests asset_root [-idas3-scene-perf-baseline]");
 check(!unityUiFrameReuseEnabled(),"disabled capture never enables command-only pixel ownership");
 NativeImage image{2,2,{0xffff0000,0xff00ff00,0xff0000ff,0xffffffff}};
 std::vector<unsigned> pixels(64*48,0),copy(128*96,0);
 OriginalSprite sprite;sprite.vertices={OriginalSpriteVertex{3,4,0,0,0,0xffffffff,0},{3,20,0,0,1,0xffffffff,0},{19,4,0,1,0,0xffffffff,0},{19,20,0,1,1,0xffffffff,0}};
 compositeOriginalSprite(pixels,64,48,image,sprite,{});
 check(std::count(pixels.begin(),pixels.end(),0)<int(pixels.size()),"disabled software painter remains functional");
 const auto software=pixels;
 Idas3UiEnable(1);Idas3UiBeginFrame(128,96);unityUiClear(pixels.data(),64,48);
 check(unityUiFrameReuseEnabled()==(argc==2),"exact baseline argument gates frame storage reuse");
 compositeOriginalSprite(pixels,64,48,image,sprite,{});
 check(pixels==software,"capture suppresses per-pixel work");
 unityUiCopy(copy.data(),pixels.data(),128,96,2,2);
 unityUiSubmit(copy.data(),128,96,false,false,false);
 auto f=frame();check(f.drawCount==2&&f.vertexCount==6&&f.textureCount==1&&!f.unresolvedSurfaces,"sprite capture counts");
 std::vector<UnityUiDraw> draws(f.drawCount);std::vector<UnityUiVertex> vertices(f.vertexCount);
 check(Idas3UiCopyDraws(draws.data(),int(draws.size()))==2,"copy draw records");
 check(Idas3UiCopyVertices(vertices.data(),int(vertices.size()))==6,"copy vertex records");
 for(unsigned repeat=0;repeat<300;++repeat){
  const auto previous=frame();Idas3UiBeginFrame(128,96);const auto empty=frame();
  check(empty.vertexCount==0&&empty.drawCount==0&&empty.unresolvedSurfaces==0&&empty.textureCount==1&&empty.revision==previous.revision+1,
      "new frame clears counts/revision without discarding source textures");
  unityUiSubmit(copy.data(),128,96,false,false,false);std::vector<UnityUiDraw> d(2);std::vector<UnityUiVertex> v(6);
  check(Idas3UiCopyDraws(d.data(),2)==2&&Idas3UiCopyVertices(v.data(),6)==6&&
      !std::memcmp(d.data(),draws.data(),2*sizeof(UnityUiDraw))&&!std::memcmp(v.data(),vertices.data(),6*sizeof(UnityUiVertex)),
      "reused frame changed captured draw or vertex bytes");
 }
 check(vertices[0].x==6&&vertices[0].y==8&&vertices[0].u==0&&vertices[0].v==0,"transformed first sprite corner");
 check(vertices[1].x==6&&vertices[1].y==40&&vertices[1].u==0&&vertices[1].v==1,"UV/corner binding preserved");
 check(draws[0].clipRight==128&&draws[0].clipBottom==96,"source clipping transformed with canvas");
 UnityUiTextureInfo ti{sizeof(ti)};check(Idas3UiGetTextureInfo(0,&ti)&&ti.width==2&&ti.height==2&&ti.bytes==16,"source texture dimensions");
 unsigned char rgba[16]{};check(Idas3UiCopyTextureRGBA(0,rgba,16)==16&&rgba[0]==255&&rgba[1]==0&&rgba[2]==0&&rgba[3]==255&&rgba[5]==255,"ARGB source to RGBA asset bytes");
 Idas3UiBeginFrame(128,96);unityUiSubmit(copy.data(),128,96,true,true,false);auto repeated=frame();
 check(repeated.vertexCount==f.vertexCount&&repeated.drawCount==f.drawCount,"cached surface survives BeginFrame");
 Idas3UiCopyDraws(draws.data(),int(draws.size()));check((draws[0].flags&6)==6,"ordered background/additive flags");
 unityUiClear(copy.data(),128,96);Idas3UiBeginFrame(128,96);unityUiSubmit(copy.data(),128,96,false,false,false);check(frame().drawCount==0,"reused allocation clear resets captured layer");
 unityUiSolid(copy.data(),128,96,0,0,128,96,0x80203040);unityUiLine(copy.data(),128,96,2,2,25,40,2,0xffffffff);
 unityUiCopyRegion(copy.data(),128,96,0,0,1,96,127,0,1,96);
 Idas3UiBeginFrame(128,96);unityUiSubmit(copy.data(),128,96,false,false,false);check(frame().drawCount>=4,"solid,line,edge extension primitives");
 unsigned unknown=0;Idas3UiBeginFrame(128,96);unityUiSubmit(&unknown,1,1,false,false,false);check(frame().unresolvedSurfaces==1,"unresolved surface is diagnosed");
 // Real mode owner animation through every authored mode and transition phase.
 original::OriginalModeMenu mode;mode.load(argv[1]);original::OriginalModeMenuState ms;
 for(unsigned choice=0;choice<3;++choice){original::selectOriginalGameMode(ms,original::OriginalGameMode(choice));
  for(float phase:{0.f,.25f,.5f,.75f,1.f}){
   original::setOriginalModeConfirmation(ms,phase);original::stepOriginalModeMenu(ms);
   Idas3UiBeginFrame(1280,720);const auto& surface=mode.paint(1280,720,ms,{-1,-1});unityUiSubmit(surface.data(),1280,720,false,false,false);
   const auto now=frame();check(now.drawCount>20&&now.vertexCount>60&&!now.unresolvedSurfaces,"actual original Mode commands captured");
   check(mode.preparedRasterBytes()==0,"capture bypasses mode software fragment cache");
   Idas3UiBeginFrame(1280,720);const auto& cached=mode.paint(1280,720,ms,{-1,-1});unityUiSubmit(cached.data(),1280,720,false,false,false);
   const auto same=frame();check(now.vertexCount==same.vertexCount&&now.drawCount==same.drawCount&&!same.unresolvedSurfaces,"settled mode cache preserves commands");
  }
 }
 Idas3UiEnable(0);pixels.assign(pixels.size(),0);compositeOriginalSprite(pixels,64,48,image,sprite,{});check(pixels==software,"opt-out restores exact software image");
 std::cout<<"PASS "<<checks<<" UI capture checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}
