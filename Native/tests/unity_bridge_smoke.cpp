#define NOMINMAX
#define IDAS3_UNITY_EXPORT
#include "unity_bridge.h"
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <stdexcept>
#include <cmath>
#include <cstring>
#include <cstdint>
using Microsoft::WRL::ComPtr;
namespace fs=std::filesystem;
namespace {
std::uint64_t checks=0;
void require(bool condition,const char* reason){++checks;if(!condition)throw std::runtime_error(reason);}
void hr(HRESULT result,const char* reason){if(FAILED(result))throw std::runtime_error(reason);}
template<class T>T symbol(HMODULE module,const char* name){auto value=GetProcAddress(module,name);if(!value)throw std::runtime_error(std::string("Missing export ")+name);return reinterpret_cast<T>(value);}
#define LOAD(name) auto name=symbol<decltype(&::name)>(library,#name)
std::string utf8(const fs::path& path){const auto s=path.u8string();return std::string(s.begin(),s.end());}
std::vector<std::uint32_t> pixels(ID3D11Texture2D* texture,ID3D11Device* device,ID3D11DeviceContext* context,const fs::path& bitmap={}){
 D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);require(desc.Format==DXGI_FORMAT_B8G8R8A8_UNORM,"Output is not BGRA8");
 desc.BindFlags=0;desc.MiscFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
 ComPtr<ID3D11Texture2D> staging;hr(device->CreateTexture2D(&desc,nullptr,&staging),"Readback texture creation failed");
 context->CopyResource(staging.Get(),texture);D3D11_MAPPED_SUBRESOURCE map{};hr(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&map),"Readback map failed");
 std::vector<std::uint32_t> result(std::size_t(desc.Width)*desc.Height);
 for(unsigned y=0;y<desc.Height;++y)std::memcpy(result.data()+std::size_t(y)*desc.Width,static_cast<const char*>(map.pData)+std::size_t(y)*map.RowPitch,desc.Width*4);
 context->Unmap(staging.Get(),0);
 if(!bitmap.empty()){
  BITMAPFILEHEADER header{};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);header.bfSize=header.bfOffBits+DWORD(result.size()*4);
  BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=LONG(desc.Width);info.biHeight=-LONG(desc.Height);info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;
  std::ofstream out(bitmap,std::ios::binary);out.write(reinterpret_cast<const char*>(&header),sizeof(header));out.write(reinterpret_cast<const char*>(&info),sizeof(info));out.write(reinterpret_cast<const char*>(result.data()),std::streamsize(result.size()*4));require(bool(out),"Capture write failed");
 }
 return result;
}
}
int main(int argc,char** argv)try{
 if(argc!=4)throw std::invalid_argument("unity_bridge_smoke DLL assetRoot NEW-output-directory");
 const auto output=fs::absolute(fs::path(argv[3]));require(!fs::exists(output),"Use a NEW isolated output directory");fs::create_directories(output);
 HMODULE library=LoadLibraryW(fs::absolute(fs::path(argv[1])).c_str());require(library!=nullptr,"Native DLL load failed");
 LOAD(Idas3UnityVersion);LOAD(Idas3UnityQueueInitialize);LOAD(Idas3UnityQueueFrame);LOAD(Idas3UnityQueueShutdown);LOAD(Idas3UnityGetRenderEventFunc);
 LOAD(Idas3UnityGetStatus);LOAD(Idas3UnityGetTexture);LOAD(Idas3UnityCopyError);LOAD(Idas3UnityWaitForEvent);
 require(Idas3UnityVersion()==1,"Bridge ABI version mismatch");
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level;
 hr(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context),"WARP device failed");
 D3D11_TEXTURE2D_DESC desc{};desc.Width=640;desc.Height=480;desc.MipLevels=desc.ArraySize=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
 ComPtr<ID3D11Texture2D> seed;hr(device->CreateTexture2D(&desc,nullptr,&seed),"Unity stand-in texture failed");
 const auto callback=Idas3UnityGetRenderEventFunc();const auto assets=utf8(fs::absolute(fs::path(argv[2]))),saves=utf8(output/"userdata");
 auto status=[&]{Idas3UnityStatus s{};s.size=sizeof(s);require(Idas3UnityGetStatus(&s)!=0,"Status read failed");if(s.state==2){char error[2048]{};Idas3UnityCopyError(error,sizeof(error));throw std::runtime_error(error);}return s;};
 auto issue=[&](int token){require(token>0,"Queue command failed");callback(token);require(Idas3UnityWaitForEvent(token,0)==1,"Render event did not complete");return status();};
 require(Idas3UnityQueueInitialize(assets.c_str(),utf8(fs::path(argv[2])/"userdata").c_str(),seed.Get(),640,480,0)==0,"Original userdata was not rejected");
 auto s=issue(Idas3UnityQueueInitialize(assets.c_str(),saves.c_str(),seed.Get(),640,480,0));require(s.state==1&&(s.flags&1),"App did not initialize in menu");
 require(Idas3UnityGetTexture()!=nullptr,"Missing native output texture");pixels(static_cast<ID3D11Texture2D*>(Idas3UnityGetTexture()),device.Get(),context.Get(),output/"initial.bmp");
 Idas3UnityInput input{};input.size=sizeof(input);input.flags=1;input.deltaSeconds=1./60;
 auto tick=[&]{return issue(Idas3UnityQueueFrame(&input));};
 auto key=[&](unsigned code,bool on){if(on)input.keys[code/32]|=1u<<(code%32);else input.keys[code/32]&=~(1u<<(code%32));};
 for(unsigned i=0;i<30;++i)s=tick();require(s.attractChild>=3,"Attract owner absent");
 pixels(static_cast<ID3D11Texture2D*>(Idas3UnityGetTexture()),device.Get(),context.Get(),output/"attract.bmp");
 input.flags=0;const auto frozen=s.renderedFrames;for(unsigned i=0;i<6;++i)s=tick();require(s.renderedFrames==frozen,"Unfocused attract advanced");
 input.flags=1;key(VK_F5,true);s=tick();key(VK_F5,false);require(!(s.flags&1)&&(s.flags&16),"TimeAttack did not launch original handling");
 key('W',true);for(unsigned i=0;i<240;++i)s=tick();require(s.simulationTicks>=240,"Fixed60Hz solver not running");require(std::isfinite(s.speedMetresPerSecond)&&s.speedMetresPerSecond>1,"Original vehicle did not accelerate");
 pixels(static_cast<ID3D11Texture2D*>(Idas3UnityGetTexture()),device.Get(),context.Get(),output/"akina-driving.bmp");
 input.flags=0;s=tick();require(s.flags&2,"Race focus loss did not pause");const auto paused=s.simulationTicks;
 for(unsigned i=0;i<6;++i)s=tick();require(s.simulationTicks==paused,"Unfocused race advanced");
 input.flags=1;s=tick();require((s.flags&2)&&s.simulationTicks==paused,"Refocus silently resumed race");
 const auto before=pixels(static_cast<ID3D11Texture2D*>(Idas3UnityGetTexture()),device.Get(),context.Get());
 const auto repeat=Idas3UnityQueueFrame(&input);s=issue(repeat);callback(repeat);require(status().renderedFrames==s.renderedFrames,"Duplicate event rendered again");
 const auto after=pixels(static_cast<ID3D11Texture2D*>(Idas3UnityGetTexture()),device.Get(),context.Get(),output/"paused.bmp");require(before==after,"Paused repaint changed pixels");
 key(VK_ESCAPE,true);s=tick();key(VK_ESCAPE,false);require(!(s.flags&2),"Escape did not unpause");
 const auto generation=s.textureGeneration;input.width=800;input.height=600;s=tick();require(s.width==800&&s.height==600&&s.textureGeneration>generation,"Target resize failed");input.width=input.height=0;
 s=issue(Idas3UnityQueueShutdown());require(s.state==0&&!Idas3UnityGetTexture(),"Shutdown left game/texture alive");require(fs::exists(output/"userdata/settings.txt"),"Settings not saved to isolated root");
 s=issue(Idas3UnityQueueInitialize(assets.c_str(),saves.c_str(),seed.Get(),640,480,0));require(s.state==1&&(s.flags&1)&&s.simulationTicks==0,"Editor restart state leaked");
 issue(Idas3UnityQueueShutdown());FreeLibrary(library);
 std::ofstream report(output/"bridge-smoke.txt");report<<"PASS "<<checks<<" C ABI checks: actual App attract, original TimeAttack, focus pause, duplicate events, paused pixels, shared-device resize, isolated settings and stop/restart. WARP only; no window/audio device. Unity Editor itself not exercised.\n";
 std::cout<<"PASS "<<checks<<" Unity native bridge checks\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
