#include "roadside_dust_presentation.h"
#include "driving_effects.h"
#include <fstream>
#include <iostream>
#include <cstring>
using namespace idas3;
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("Native root and evidence directory required");
    const std::filesystem::path root=argv[1],out=argv[2];std::filesystem::create_directories(out);
    unsigned checks=0;const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);};
    const auto textures=NativeTextureBank::load(root/"data/original_assets/effects/smoke/textures.idastex");
    RoadsideDustPresentation dust;dust.load(root,textures);
    auto world=original::originalIdentityMatrix();
    const auto parts=original::OriginalCarParts::load(root/"data/original_models/toyota_ae86t/assembly_parts.bin");
    const auto origins=original::originalCarEffectOrigins(parts);
    std::array<original::OriginalRoadsideDustCar,2> cars;
    cars[0].available=true;cars[0].wheelSurfaces.fill(0x26);
    for(unsigned tick=0;tick<36;++tick){world.elements[14]+=.6f;
        cars[0].owner=original::originalRoadsideDustOwner(world,origins,{0,0,.6f});dust.advance(7,0,0,2,true,cars);}
    check(dust.system().order().size()==4&&dust.system().particleCount()>0,"Dust did not start from four source contacts");
    const Vec3 eye{0,1.6f,world.elements[14]-8},target{0,.6f,world.elements[14]};
    const float fov=1.2333658933f;
    Mesh dustMesh;check(dust.append(dustMesh,eye,target,{0,1,0},fov,4.f/3.f,0,1)>0,"Visible dust produced no original meshes");
    for(const auto& range:dustMesh.ranges){
        check(range.texture==3&&range.original&&range.viewMask==1,"Dust lost palette or main view binding");
        check((range.tsp>>29)==4&&((range.tsp>>26)&7)==6,"Dust lost original source/destination-alpha blending");
        check((range.gmp&512)!=0,"Original dust unexpectedly uses fallback car lights");
    }
    for(const auto& v:dustMesh.vertices){
        check(std::isfinite(v.position.x)&&std::isfinite(v.position.y)&&std::isfinite(v.position.z),"Nonfinite dust vertex");
        check(v.u==.9f&&v.v>=.008f&&v.v<=.99f,"Dust density stopped using palette UVs");
    }
    const auto before=dust;
    for(unsigned paint=0;paint<120;++paint){Mesh repeated;dust.append(repeated,eye,target,{0,1,0},fov,4.f/3.f,0,1);
        check(repeated.vertices.size()==dustMesh.vertices.size()&&std::memcmp(repeated.vertices.data(),dustMesh.vertices.data(),repeated.vertices.size()*sizeof(Vertex))==0,"Repaint changed dust state");}
    auto expected=before;dust.advance(7,0,0,2,true,cars);expected.advance(7,0,0,2,true,cars);
    Mesh actualNext,expectedNext;dust.append(actualNext,eye,target,{0,1,0},fov,4.f/3.f,0,1);expected.append(expectedNext,eye,target,{0,1,0},fov,4.f/3.f,0,1);
    check(actualNext.vertices.size()==expectedNext.vertices.size()&&std::memcmp(actualNext.vertices.data(),expectedNext.vertices.data(),actualNext.vertices.size()*sizeof(Vertex))==0,"Repainting consumed lifecycle/RNG state");
    Mesh behind;check(dust.append(behind,eye,eye-(target-eye),{0,1,0},fov,4.f/3.f,0,1)==0,"Dust behind camera rendered");
    Mesh wide;check(dust.append(wide,eye,target,{0,1,0},fov,32.f/9.f,0,1)>0,"Wide camera lost dust");
    // Ordinary tire smoke is suppressed only on wheels with admitted dust.
    DrivingEffects tire;std::array<DrivingEffects::Car,2> effectCars;
    effectCars[0].grounded=effectCars[0].visible=true;effectCars[0].speed=20;effectCars[0].slip=.5f;
    effectCars[0].smokeSuppressed=dust.system().suppressedTireSmoke()[0];
    tire.advance(.06,true,false,effectCars);check(tire.smokeCount()==0,"Generic smoke doubled the dusty wheels");
    effectCars[0].smokeSuppressed[2]=false;tire.advance(.06,true,false,effectCars);
    check(tire.smokeCount()==1,"Suppression hid a non-dusty wheel");
    constexpr unsigned w=960,h=720;Renderer renderer;
    check(renderer.initialize(nullptr,w,h,true),renderer.error.c_str());
    renderer.verticalFieldOfView=fov;renderer.nearClip=.05f;renderer.vehicleLights=false;renderer.opponentLights=false;
    renderer.overrideClearColor=true;renderer.clearColor={.12f,.18f,.28f,1};
    check(renderer.loadTextures(textures),"Smoke texture upload failed");
    const auto capture=[&](const Mesh& mesh,const char* name){
        check(renderer.draw(mesh,eye,target,false,false),renderer.error.c_str());
        const auto path=out/(std::string(name)+".bmp");check(renderer.saveBitmap(path.wstring()),"Dust readback failed");
        std::ifstream in(path,std::ios::binary);BITMAPFILEHEADER head{};in.read(reinterpret_cast<char*>(&head),sizeof head);in.seekg(head.bfOffBits);
        std::vector<unsigned> pixels(w*h);in.read(reinterpret_cast<char*>(pixels.data()),pixels.size()*4);check(bool(in),"Dust pixel read failed");return pixels;
    };
    Mesh empty;const auto baseline=capture(empty,"dust-off"),visible=capture(dustMesh,"dust-on");
    unsigned changed=0,brown=0;for(unsigned i=0;i<baseline.size();++i)if(baseline[i]!=visible[i]){
        // Source blend uses destination alpha: inspect the contributed color,
        // not whether faint brown dust completely overwhelms a blue backdrop.
        ++changed;const int r=int((visible[i]>>16)&255)-int((baseline[i]>>16)&255);
        const int g=int((visible[i]>>8)&255)-int((baseline[i]>>8)&255);
        const int b=int(visible[i]&255)-int(baseline[i]&255);brown+=r>g&&g>b;}
    check(changed>100&&brown>20,"Original mesh/palette did not draw visible brown dust");
    Mesh scene;scene.box({0,-.2f,world.elements[14]},{20,.2f,24},0,{.38f,.38f,.38f});
    scene.box({0,.6f,world.elements[14]},{1.7f,1.2f,4.1f},0,{.65f,.65f,.68f});scene.append(dustMesh);capture(scene,"dust-car-depth");
    Renderer portable;check(portable.initializeSceneCapture(w,h),"Portable scene initialization failed");
    check(portable.loadTextures(textures),"Portable texture capture failed");
    portable.verticalFieldOfView=fov;check(portable.draw(dustMesh,eye,target,false,false),"Portable dust publication failed");
    const auto& frame=portable.sceneCapture()->frame();
    check(frame.vertexCount==dustMesh.vertices.size()&&frame.rangeCount==dustMesh.ranges.size(),"Unity capture dropped dust geometry");
    for(unsigned condition=0;condition<3;++condition){auto gated=before;
        for(unsigned tick=0;tick<100;++tick)gated.advance(condition==0?6:7,condition==1,condition==2,2,true,cars);
        check(gated.system().order().empty()&&gated.system().freeParticles()==60,"Non-day/dry Tsuchisaka retained dust");}
    dust.reset();check(dust.system().order().empty()&&dust.system().freeParticles()==60&&dust.loaded(),"Reset lost assets or retained dust");
    std::ofstream(out/"PASS.txt")<<checks<<" checks; "<<changed<<" changed pixels, "<<brown<<" brown pixels. Authored mesh/palette, separate view projection, repaint independence, suppression, gating/drain, D3D11 and Unity scene publication. App integration and whole-race visual parity remain separate.\n";
    std::cout<<"PASS "<<checks<<" dust presentation checks; "<<changed<<" changed pixels, "<<brown<<" brown pixels.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
