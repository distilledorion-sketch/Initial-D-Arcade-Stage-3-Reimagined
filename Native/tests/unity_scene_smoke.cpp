#define NOMINMAX
#define IDAS3_UNITY_EXPORT
#include "unity_bridge.h"
#include "unity_scene_capture.h"
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cmath>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
namespace fs=std::filesystem;
namespace {
unsigned checks=0;
void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
std::string utf8(const fs::path& p){auto s=p.u8string();return {s.begin(),s.end()};}
template<class T>T load(HMODULE lib,const char* name){auto p=GetProcAddress(lib,name);if(!p)throw std::runtime_error(std::string("Missing export ")+name);return reinterpret_cast<T>(p);}
struct Api {
 HMODULE library;
 decltype(&Idas3UnityGetStatus) status;
 decltype(&Idas3UnityCopyError) error;
 explicit Api(const fs::path& path){library=LoadLibraryW(path.c_str());check(library!=nullptr,"DLL failed to load");status=load<decltype(status)>(library,"Idas3UnityGetStatus");error=load<decltype(error)>(library,"Idas3UnityCopyError");}
 Idas3UnityStatus read(){Idas3UnityStatus s{};s.size=sizeof(s);check(status(&s)==1,"Status failed");if(s.state==2){char text[4096]{};error(text,sizeof(text));throw std::runtime_error(text);}return s;}
};
struct UiFrameDiagnostic{std::uint32_t size,width,height,drawCount,vertexCount,textureCount,unresolved,reserved;std::uint64_t revision;};
}
int main(int argc,char** argv)try{
 if(argc!=5)throw std::invalid_argument("unity_scene_smoke NEW-DLL REFERENCE-DLL asset-root NEW-output-dir");
 const auto output=fs::absolute(argv[4]);check(!fs::exists(output),"Output directory must be new");fs::create_directories(output);
 Api scene(fs::absolute(argv[1])),reference(fs::absolute(argv[2]));
 auto initialize=load<decltype(&Idas3SceneInitialize)>(scene.library,"Idas3SceneInitialize");
 auto step=load<decltype(&Idas3SceneStep)>(scene.library,"Idas3SceneStep");
 auto shutdown=load<decltype(&Idas3SceneShutdown)>(scene.library,"Idas3SceneShutdown");
 auto getFrame=load<int(__cdecl*)(Idas3SceneFrame*)>(scene.library,"Idas3SceneGetFrame");
 auto getUi=load<int(__cdecl*)(UiFrameDiagnostic*)>(scene.library,"Idas3UiGetFrame");
 auto checkUi=[&]{UiFrameDiagnostic u{};u.size=sizeof(u);check(getUi(&u)==1,"UI frame unavailable");check(u.unresolved==0,"UI surface missing from scene export");return u;};
 auto texture=load<decltype(&Idas3UnityGetTexture)>(scene.library,"Idas3UnityGetTexture");
 auto refInitialize=load<decltype(&Idas3UnityQueueInitialize)>(reference.library,"Idas3UnityQueueInitialize");
 auto refStep=load<decltype(&Idas3UnityQueueFrame)>(reference.library,"Idas3UnityQueueFrame");
 auto refShutdown=load<decltype(&Idas3UnityQueueShutdown)>(reference.library,"Idas3UnityQueueShutdown");
 auto callback=load<decltype(&Idas3UnityGetRenderEventFunc)>(reference.library,"Idas3UnityGetRenderEventFunc")();
 const auto assets=utf8(fs::absolute(argv[3])),saves=utf8(output/"scene-userdata"),priorSaves=utf8(output/"reference-userdata");
 // The new port initializes first, without any graphics device in this process.
 check(initialize(assets.c_str(),saves.c_str(),640,480)==1,"Scene init failed");
 check(scene.read().reserved==1&&texture()==nullptr,"Scene path created a native framebuffer");
 Idas3SceneFrame packet{};packet.size=sizeof(packet);check(getFrame(&packet)==1,"No scene packet");
 check(packet.version==1&&packet.width==640&&packet.height==480,"Scene packet ABI mismatch");
 // Only the frozen reference implementation receives a Direct3D device.
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level;
 check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context)),"Reference device failed");
 D3D11_TEXTURE2D_DESC desc{};desc.Width=desc.Height=16;desc.MipLevels=desc.ArraySize=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
 ComPtr<ID3D11Texture2D> anchor;check(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&anchor)),"Reference texture failed");
 int token=refInitialize(assets.c_str(),priorSaves.c_str(),anchor.Get(),640,480,0);check(token>0,"Reference init queue failed");callback(token);reference.read();
 std::ofstream trace(output/"handling-comparison.csv");trace<<"frame,ticks,new_speed,reference_speed,new_rpm,reference_rpm,meshes,vertices,views\n";
 Idas3UnityInput input{};input.size=sizeof(input);input.flags=1;input.deltaSeconds=1./60;
 auto set=[&](unsigned key){input.keys[key/32]|=1u<<(key%32);};
 float maximumSpeed=0;unsigned raceMeshes=0;bool mirrorSeen=false;
 for(unsigned frame=0;frame<480;++frame){
  for(auto& key:input.keys)key=0;
  if(frame==30)set(VK_F5);
  if(frame>=31&&frame<400)set('W');
  if(frame>=305&&frame<330)set('A');
  if(frame>=350&&frame<365)set('D');
  if(frame>=400&&frame<430)set('S');
  if(frame==420)set('C');
  if(frame==440||frame==450)set(VK_ESCAPE);
  if(step(&input)!=1){scene.read();throw std::runtime_error("Scene step failed");}
  token=refStep(&input);check(token>0,"Reference frame queue failed");callback(token);
  auto current=scene.read(),prior=reference.read();
  check(current.simulationTicks==prior.simulationTicks,"Fixed simulation tick divergence");
  check(current.flags==prior.flags&&current.racePhase==prior.racePhase,"Game state divergence");
  check(current.speedMetresPerSecond==prior.speedMetresPerSecond,"Handling speed changed");
  check(current.rpm==prior.rpm,"Handling engine RPM changed");
  check(texture()==nullptr,"Scene path acquired native framebuffer");
  packet.size=sizeof(packet);check(getFrame(&packet)==1,"Scene packet missing");
  auto ui=checkUi();if(!(current.flags&1))check(ui.drawCount>0,"Race HUD has no Unity draws");
  maximumSpeed=std::max(maximumSpeed,current.speedMetresPerSecond);
  if(!(current.flags&1)){
   raceMeshes=std::max(raceMeshes,packet.rangeCount);mirrorSeen|=packet.viewCount>1;
   check(packet.vertexCount>0&&packet.rangeCount>0&&packet.textureCount>0,"Race asset packet empty");
   for(unsigned v=0;v<packet.viewCount;++v){const auto& c=packet.cameras[v];check(std::isfinite(c.eye[0])&&std::isfinite(c.eye[1])&&std::isfinite(c.eye[2])&&c.verticalFov>0,"Invalid camera pose");}
   for(unsigned i=0;i<packet.rangeCount;++i){const auto& r=packet.ranges[i];check(r.first<=packet.vertexCount&&r.count<=packet.vertexCount-r.first,"Geometry range overflow");}
  }
  trace<<frame<<','<<current.simulationTicks<<','<<current.speedMetresPerSecond<<','<<prior.speedMetresPerSecond<<','<<current.rpm<<','<<prior.rpm<<','<<packet.rangeCount<<','<<packet.vertexCount<<','<<packet.viewCount<<'\n';
 }
 check(maximumSpeed>10&&raceMeshes>1,"Scene did not preserve driving content");
 // The original mirror is a Legend bumper feature, not a TimeAttack feature.
 auto both=[&](unsigned key=0){
  for(auto& word:input.keys)word=0;if(key)set(key);
  check(step(&input)==1,"Legend scene step failed");token=refStep(&input);check(token>0,"Legend reference queue failed");callback(token);
  const auto a=scene.read(),b=reference.read();
  check(a.flags==b.flags&&a.frontendStage==b.frontendStage&&a.simulationTicks==b.simulationTicks,"Legend owner divergence");
  check(a.speedMetresPerSecond==b.speedMetresPerSecond&&a.rpm==b.rpm,"Legend handling divergence");
  packet.size=sizeof(packet);check(getFrame(&packet)==1,"Legend scene packet missing");mirrorSeen|=packet.viewCount==2;
  checkUi();
  return a;
 };
 auto wait=[&](unsigned count){for(unsigned i=0;i<count;++i)both();};
 both(VK_ESCAPE);both();both(VK_BACK);both();check(scene.read().frontendStage==5,"Course return failed");
 both(VK_ESCAPE);wait(100);check(scene.read().frontendStage==4,"Mode return failed");
 both(VK_RETURN);wait(180);check(scene.read().frontendStage==5,"Legend mode selection failed");
 both(VK_RETURN);wait(45);check(scene.read().frontendStage==9,"Legend rival selection failed");
 both(VK_RETURN);wait(180);check(!(scene.read().flags&1),"Legend race failed to start");
 // Starting a new race preserves the user's last chosen camera (chase here).
 both('C');for(unsigned i=0;i<180;++i)both('W');
 check(mirrorSeen,"Legend bumper mirror was not exported");
 check(shutdown()==1,"Scene shutdown failed");callback(refShutdown());
 check(scene.read().state==0&&texture()==nullptr,"Scene resources remained active");
 check(initialize(assets.c_str(),saves.c_str(),640,480)==1,"Scene restart failed");
 check(scene.read().simulationTicks==0,"Restart retained simulation ticks");check(shutdown()==1,"Restart shutdown failed");
 std::ofstream report(output/"scene-smoke.txt");report<<"PASS "<<checks<<" checks.480 matching TimeAttack frames plus actual menu navigation into Legend against preserved DLL; exact speed/RPM and solver ticks, steering/braking/pause/camera, scene geometry/mirror, no native framebuffer, clean restart. Unity rendering is tested separately.\n";
 std::cout<<"PASS "<<checks<<" Unity scene/reference checks\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
