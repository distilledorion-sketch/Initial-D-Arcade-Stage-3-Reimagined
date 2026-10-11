#define IDAS3_UNITY_EXPORT
#include "unity_bridge.h"
#include "unity_scene_capture.h"
#include <windows.h>
#include <DirectXMath.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs=std::filesystem;
namespace {
unsigned checks=0;
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
template<class T>T symbol(HMODULE library,const char* name){
    const auto found=GetProcAddress(library,name);
    if(!found)throw std::runtime_error(std::string("Missing export: ")+name);
    return reinterpret_cast<T>(found);
}
std::string utf8(const fs::path& path){const auto value=path.u8string();return {value.begin(),value.end()};}
struct Snapshot {
    unsigned views=0;
    std::array<Idas3SceneCamera,2> cameras{};
    std::array<float,184> constants{};
    Idas3UnityStatus status{};
};
void checkProjection(const Snapshot& shot){
    using namespace DirectX;
    for(unsigned view=0;view<shot.views;++view){
        const auto& c=shot.cameras[view];
        const auto eye=XMVectorSet(c.eye[0],c.eye[1],c.eye[2],1);
        const auto target=XMVectorSet(c.target[0],c.target[1],c.target[2],1);
        const auto up=XMVectorSet(c.up[0],c.up[1],c.up[2],0);
        const auto basis=(c.leftHanded&1)?XMMatrixLookAtLH(eye,target,up):XMMatrixLookAtRH(eye,target,up);
        const auto lens=(c.leftHanded&1)?XMMatrixPerspectiveFovLH(c.verticalFov,c.aspect,c.nearClip,c.farClip):
            XMMatrixPerspectiveFovRH(c.verticalFov,c.aspect,c.nearClip,c.farClip);
        XMFLOAT4X4 expected;XMStoreFloat4x4(&expected,basis*lens);
        const auto* values=&expected.m[0][0];
        for(unsigned i=0;i<16;++i)
            check(std::abs(values[i]-shot.constants[view*92+i])<=1.e-5f*std::max(1.f,std::abs(values[i])),
                "Exported camera and shader/culling projection disagree");
    }
}
void unchanged(const Snapshot& before,const Snapshot& after,const char* message){
    check(before.views==after.views&&before.cameras.size()==after.cameras.size()&&
        std::memcmp(before.cameras.data(),after.cameras.data(),sizeof(before.cameras))==0&&
        before.constants==after.constants,message);
}
}
int main(int argc,char** argv)try{
    if(argc!=4)throw std::invalid_argument("DLL, native asset root and NEW evidence directory required");
    const auto output=fs::absolute(argv[3]);check(!fs::exists(output),"Preserve existing evidence");fs::create_directories(output);
    const auto library=LoadLibraryW(fs::absolute(argv[1]).c_str());check(library!=nullptr,"Native plugin did not load");
#define API(name) const auto name=symbol<decltype(&::name)>(library,#name)
    API(Idas3SceneInitialize);API(Idas3SceneShutdown);API(Idas3SceneStep);API(Idas3UnityGetStatus);
    API(Idas3SceneSetCameraFov);API(Idas3SceneGetOptions);API(Idas3SceneApplyOptions);
    API(Idas3MultiplayerStart);API(Idas3MultiplayerSetGo);API(Idas3MultiplayerLeave);API(Idas3MultiplayerGetLocalSnapshot);
    API(Idas3ReplayStart);API(Idas3ReplayPose);API(Idas3ReplayFreeCamera);
#undef API
    const auto getFrame=symbol<int(__cdecl*)(Idas3SceneFrame*)>(library,"Idas3SceneGetFrame");
    check(Idas3SceneSetCameraFov(100)==0,"FOV accepted before initialization");
    const auto assets=utf8(fs::absolute(argv[2])),saves=utf8(output/"replay-viewer-session");
    check(Idas3SceneInitialize(assets.c_str(),saves.c_str(),1280,720)==1,"Scene initialization failed");
    Idas3UnityInput input{};input.size=sizeof(input);input.flags=1;
    const auto capture=[&]{
        check(Idas3SceneStep(&input)==1,"Scene repaint failed");
        Idas3SceneFrame frame{};frame.size=sizeof(frame);check(getFrame(&frame)==1,"Scene frame unavailable");
        Snapshot shot;shot.views=frame.viewCount;std::copy_n(frame.cameras,2,shot.cameras.begin());
        std::copy_n(frame.frameConstants,184,shot.constants.begin());
        shot.status.size=sizeof(shot.status);check(Idas3UnityGetStatus(&shot.status)==1,"Scene status unavailable");
        checkProjection(shot);return shot;
    };
    const auto title=capture();check(Idas3SceneSetCameraFov(200)==1,"Valid FOV rejected");
    unchanged(title,capture(),"FOV changed the title/menu projection");
    check(Idas3SceneSetCameraFov(100)==1,"Default FOV rejected");
    Idas3MultiplayerConfig race{sizeof(race),1,0,0,0,0,0,8,0,1};
    check(Idas3MultiplayerStart(&race)==1,"Isolated waiting race failed");
    Idas3MultiplayerSnapshot car{};car.size=sizeof(car);
    check(Idas3MultiplayerGetLocalSnapshot(&car)==1,"Race pose unavailable");
    std::ofstream report(output/"fov.csv");report<<"view,percent,vertical_degrees\n";
    for(unsigned view=0;view<3;++view){
        Idas3Options options{};options.size=sizeof(options);check(Idas3SceneGetOptions(&options)==1,"Camera options unavailable");
        options.cameraView=view;check(Idas3SceneApplyOptions(&options)==1,"Camera selection failed");
        const auto original=capture();check(original.views==2,"Race mirror missing from fixture");
        for(int percent:{50,100,150,200,100}){
            check(Idas3SceneSetCameraFov(percent)==1,"Valid FOV rejected");const auto adjusted=capture();
            const auto& before=original.cameras[0];const auto& after=adjusted.cameras[0];
            check(adjusted.status.simulationTicks==original.status.simulationTicks&&
                adjusted.status.speedMetresPerSecond==original.status.speedMetresPerSecond,"FOV changed simulation state");
            check(std::memcmp(before.eye,after.eye,9*sizeof(float))==0,"FOV moved the camera");
            check(std::memcmp(&original.cameras[1],&adjusted.cameras[1],sizeof(Idas3SceneCamera))==0&&
                std::equal(original.constants.begin()+92,original.constants.end(),adjusted.constants.begin()+92),"Main FOV changed the mirror");
            if(percent==100)unchanged(original,adjusted,"100% did not restore the exact authored projection");
            else check(percent<100?after.verticalFov<before.verticalFov:after.verticalFov>before.verticalFov,"FOV slider moved the wrong way");
            check(after.verticalFov>=DirectX::XMConvertToRadians(30.f)&&after.verticalFov<=DirectX::XMConvertToRadians(120.f),"FOV exceeded safe projection bounds");
            report<<view<<','<<percent<<','<<DirectX::XMConvertToDegrees(after.verticalFov)<<'\n';
        }
        for(int invalid:{-1,0,49,201,1000}){
            check(Idas3SceneSetCameraFov(invalid)==0,"Invalid FOV was accepted");unchanged(original,capture(),"Rejected FOV mutated the lens");
        }
    }
    check(Idas3MultiplayerSetGo(1)==1,"Showcase did not start");
    const auto showcase=capture();check(Idas3SceneSetCameraFov(200)==1,"Showcase setting rejected");
    unchanged(showcase,capture(),"FOV changed the authored showcase");
    check(Idas3MultiplayerLeave()==1,"Isolated race did not leave");
    const auto menu=capture();check(Idas3SceneSetCameraFov(50)==1,"Menu setting rejected");
    unchanged(menu,capture(),"FOV changed the returned lobby/menu");
    check(Idas3ReplayStart(0,0,0,0,0)==1,"Isolated replay failed");
    check(Idas3ReplayFreeCamera(car.actorPosition[0]+10,car.actorPosition[1]+5,car.actorPosition[2]+10,
        car.actorPosition[0],car.actorPosition[1],car.actorPosition[2])==1,"Replay free camera failed");
    for(int camera=0;camera<5;++camera){
        check(Idas3SceneSetCameraFov(100)==1,"Replay default FOV rejected");
        check(Idas3ReplayPose(0,car.actorPosition[0],car.actorPosition[1],car.actorPosition[2],car.yaw,0,1,0,camera,0,1280,720)==1,"Replay pose failed");
        const auto original=capture();check(Idas3SceneSetCameraFov(200)==1,"Replay FOV rejected");
        const auto wide=capture();check(wide.cameras[0].verticalFov>original.cameras[0].verticalFov,"Replay camera ignored the FOV setting");
        check(Idas3SceneSetCameraFov(100)==1,"Replay default FOV rejected");
        unchanged(original,capture(),"Replay default projection did not restore exactly");
    }
    check(Idas3SceneShutdown()==1,"Scene shutdown failed");FreeLibrary(library);
    std::ofstream(output/"PASS.txt")<<checks<<" checks: actual plugin projection/camera consistency, all driving/replay views, exact100%, unchanged mirror/menus/showcase/simulation, rejected invalid settings\n";
    std::cout<<"PASS "<<checks<<" native camera FOV projection checks\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
