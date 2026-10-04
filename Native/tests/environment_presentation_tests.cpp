#include "environment_presentation.h"
#include <iostream>
#include <cstring>
#include <limits>
using namespace idas3;
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("Asset root and evidence directory required");
    const std::filesystem::path root=argv[1],out=argv[2];std::filesystem::create_directories(out);
    unsigned checks=0;auto check=[&](bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);};
    EnvironmentPresentation e;e.load(root);
    constexpr std::array<unsigned,8> lengths{1068,1400,3297,4088,4088,2950,3302,3730};
    for(unsigned course=0;course<8;++course)for(unsigned i=0;i<lengths[course];++i){
        check(e.sunVisible(course,false,false,false,float(i))==e.sunVisible(course,false,false,true,float(lengths[course]-1-i)),"Reverse sun occlusion is not mirrored by path index");
        check(!e.sunVisible(course,true,false,false,float(i)),"Sun appeared at night");
        check(!e.sunVisible(course,false,true,false,float(i)),"Sun appeared in rain");
    }
    check(!e.sunVisible(8,false,false,false,1),"Snow acquired dry-course sun");
    check(!e.sunVisible(0,false,false,false,-1)&&!e.sunVisible(0,false,false,false,1068),"Out-of-course sun index accepted");
    check(!e.sunVisible(0,false,false,false,std::numeric_limits<float>::quiet_NaN()),"Nonfinite sun progress accepted");
    std::array<DrivingEffects::Car,2> cars{};cars[0].visible=cars[0].grounded=true;cars[0].speed=20;
    cars[0].points={Vec3{-.6f,0,1},Vec3{.6f,0,1},Vec3{-.6f,0,-1},Vec3{.6f,0,-1}};
    std::array<std::array<unsigned,4>,2> materials{};
    for(unsigned material=0;material<16;++material){e.reset();materials[0].fill(material);e.advance(1./60,true,false,cars,materials);
        check(e.leafCount()==((material==1||material==2||material==11)?12u:0u),"Leaf admission disagrees with source road material");
    }
    materials[0].fill(1);e.reset();
    for(unsigned i=0;i<60;++i)e.advance(1./60,true,false,cars,materials);
    const auto at60=e.leaves();check(e.leafCount()==80,"Local leaf pool is not bounded to80");
    for(unsigned fps:{30u,120u,144u,240u}){e.reset();for(unsigned i=0;i<fps;++i)e.advance(1./fps,true,false,cars,materials);
        check(std::memcmp(at60.data(),e.leaves().data(),sizeof(at60))==0,"Leaf playback changed with frame rate");}
    e.advance(.1,true,true,cars,materials);check(std::memcmp(at60.data(),e.leaves().data(),sizeof(at60))==0,"Pause advanced leaves");
    Mesh leaves;e.appendLeaves(leaves,0);e.appendLeaves(leaves,0);
    check(std::memcmp(at60.data(),e.leaves().data(),sizeof(at60))==0,"Rendering advanced leaves");
    materials={};for(unsigned i=0;i<91;++i)e.advance(1./60,true,false,cars,materials);
    check(e.leafCount()==0,"Leaves never expired");
    materials[0].fill(1);e.advance(1./60,true,false,cars,materials);cars[0].position={100,0,0};materials={};e.advance(1./60,true,false,cars,materials);
    check(e.leafCount()==0,"Teleport retained old leaves");
    e.reset();cars[0].position={};cars[0].speed=0;materials[0].fill(1);e.advance(1./60,true,false,cars,materials);
    check(e.leafCount()==0,"Stopped tires emit leaves");
    cars[0].speed=20;cars[1]=cars[0];materials[1].fill(1);
    for(unsigned i=0;i<60;++i)e.advance(1./60,true,false,cars,materials);
    check(e.leafCount()==160,"Opponent leaf pool displaced local pool");
    e.advance(1./60,false,false,cars,materials);check(e.leafCount()==0,"Changing course retained leaves");
    // Exercise actual D3D materials and the portable Unity publication path.
    Renderer renderer;check(renderer.initialize(nullptr,960,720,true),renderer.error.c_str());
    renderer.nearClip=.05f;renderer.overrideClearColor=true;renderer.clearColor={.12f,.18f,.28f,1};
    renderer.vehicleLights=false;renderer.opponentLights=false;
    check(renderer.loadTextures(e.leafTextures),"Leaf upload failed");
    check(renderer.loadTextures(e.flareTextures,true),"Flare upload failed");
    const auto sun=e.sunDirection(0);const Vec3 eye{0,0,0},target=sun;
    Mesh empty,lit;check(e.appendSun(lit,0,false,false,false,0,eye,target,{0,1,0},.95f,.05f,4)==11,"Facing the sun did not render authored flare pieces");
    Mesh excluded;
    check(e.appendSun(excluded,0,false,false,false,0,eye,-sun,{0,1,0},.95f,.05f,4)==0,"Sun rendered behind camera");
    check(e.appendSun(excluded,0,true,false,false,0,eye,sun,{0,1,0},.95f,.05f,4)==0,"Night rendered sun geometry");
    for(const auto& r:lit.ranges)check(r.emissive&&r.viewMask==1&&(r.isp&(1u<<26)),"Lens material pollutes rear view/depth");
    check(renderer.draw(empty,eye,target,false,false),"Baseline draw failed");
    check(renderer.saveBitmap((out/"sun-off.bmp").wstring()),"Baseline capture failed");
    check(renderer.draw(lit,eye,target,false,false),"Sun draw failed");
    check(renderer.saveBitmap((out/"sun-on.bmp").wstring()),"Sun capture failed");
    for(unsigned frame=0;frame<10;++frame)e.advance(1./60,true,false,cars,materials);
    Mesh foliage;e.appendLeaves(foliage,0);check(!foliage.vertices.empty(),"Leaf meshes missing");
    check(renderer.draw(foliage,{0,.5f,-1.9f},{0,.12f,0},false,false),"Leaf draw failed");
    check(renderer.saveBitmap((out/"leaves.bmp").wstring()),"Leaf capture failed");
    Renderer portable;check(portable.initializeSceneCapture(1280,720),"Scene capture init failed");
    portable.loadTextures(e.leafTextures,false);portable.loadTextures(e.flareTextures,true);
    check(portable.draw(lit,eye,target,false,false),"Portable effect publication failed");
    const auto& frame=portable.sceneCapture()->frame();check(frame.rangeCount==lit.ranges.size()&&frame.vertexCount==lit.vertices.size()&&frame.textureCount==11,"Unity did not receive effect ranges/textures");
    std::ofstream(out/"PASS.txt")<<checks<<" checks: source path gates, direction, weather, bounded leaves, pause,30/60/120/144/240FPS,teleport,expiration,opponent,D3D11 and Unity scene publication.\n";
    std::cout<<"PASS "<<checks<<" environment checks\n";
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
