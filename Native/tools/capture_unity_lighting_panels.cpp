#include "renderer.h"
#include "original_car_lighting.h"
#include <fstream>
#include <iostream>
using namespace idas3;
int main(int argc,char**argv)try{
 if(argc!=2)throw std::runtime_error("Output directory required");
 const std::filesystem::path folder=argv[1];std::filesystem::create_directories(folder);
 const auto texture=folder/"panel.idastex",image=folder/"native-panel.bmp";
 {std::ofstream f(texture,std::ios::binary);f.write("IDAS3T1\0",8);const unsigned words[]{1,1,0,1,1,4,0xffc08040};f.write(reinterpret_cast<const char*>(words),sizeof(words));}
 Renderer native,captured;auto check=[](bool v,const std::string&s){if(!v)throw std::runtime_error(s);};
 check(native.initialize(nullptr,320,240,true),native.error);check(captured.initializeSceneCapture(320,240),captured.error);
 const auto bank=NativeTextureBank::load(texture);check(native.loadTextures(bank),native.error);check(captured.loadTextures(bank),captured.error);
 Mesh mesh;
 for(unsigned row=0;row<3;++row)for(unsigned col=0;col<4;++col){
  const bool textured=(col&1)!=0;Mesh tile;const float x=-5.6f+col*2.9f,y=-3.8f+row*2.7f;
  tile.quad({x,y,-10},{x+2.5f,y,-10},{x+2.5f,y+2.2f,-10},{x,y+2.2f,-10},{.7f,.5f,.3f,1});
  auto&r=tile.ranges[0];r.original=true;r.courseLighting=row==0;r.carLighting=row;
  r.pcw=(col<2?2:0)|(textured?12:0);r.gmp=col==3?512:0;r.tsp=textured?0x20880040:0x20900000;r.isp=0xc0000000;r.gloss=0x60;r.texture=textured?0:0xffffffff;
  for(unsigned i=0;i<tile.vertices.size();++i){auto&v=tile.vertices[i];v.normal=normalized(Vec3{float(i%3)-1,.5f,1});v.offsetColor={.2f,.1f,.05f,0};v.u=v.v=.5f;}
  mesh.append(tile);
 }
 std::ofstream out(folder/"panels.bin",std::ios::binary);auto write=[&](const void*p,std::size_t n){out.write(static_cast<const char*>(p),n);check(bool(out),"Panel fixture write failed");};
 const unsigned header[]{0x314e504c,36,320,240};write(header,sizeof(header));
 for(unsigned course=0;course<9;++course)for(unsigned condition=0;condition<4;++condition){
  const bool night=condition&2,wet=condition&1;auto base=original::originalCourseLighting(course,night,wet);
  auto player=original::originalCarLighting(),rival=original::originalCarLighting();
  original::OriginalCarLightingSetup setup{course,0,night,wet};player.ambient=original::originalCourseCarAmbient(setup);rival.ambient=player.ambient;
  player.gain=.7f;rival.gain=.4f;
  player.ownSpot.enabled=rival.ownSpot.enabled=night;player.ownSpot.position={-3,4,-8};rival.ownSpot.position={3,4,-8};
  auto sets=original::composeOriginalRaceLighting(base,player,rival,setup);
  for(auto* r:{&native,&captured}){
   r->vehicleLights=r->opponentLights=false;r->courseLampLighting=false;r->overrideClearColor=true;r->clearColor={.1f,.15f,.2f,1};
   r->courseLighting=&sets.course;r->playerLighting=&sets.player;r->rivalLighting=&sets.rival;
   check(r->draw(mesh,{0,0,0},{0,0,-1},night,wet),r->error);
  }
  check(native.saveBitmap(image.wstring()),native.error);
  std::ifstream f(image,std::ios::binary);BITMAPFILEHEADER h{};f.read(reinterpret_cast<char*>(&h),sizeof(h));f.seekg(h.bfOffBits);
  std::vector<unsigned> pixels(320*240);f.read(reinterpret_cast<char*>(pixels.data()),pixels.size()*4);check(bool(f),"Panel bitmap readback");
  const auto&frame=captured.sceneCapture()->frame();const unsigned fields[]{course,condition,frame.vertexCount,frame.rangeCount};write(fields,sizeof(fields));
  write(frame.frameConstants,184*4);write(frame.lightConstants,1248*4);write(frame.fogConstants,136*4);
  write(frame.vertices,frame.vertexCount*sizeof(Idas3SceneVertex));write(frame.ranges,frame.rangeCount*sizeof(Idas3SceneRange));write(pixels.data(),pixels.size()*4);
 }
 std::cout<<"PASS36 original course/day/night/wet lighting frames:432 panels across course/player/rival scopes, textured/untextured, flat/gouraud and unlit material paths. Native WARP pixels and production Unity capture records exported.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
