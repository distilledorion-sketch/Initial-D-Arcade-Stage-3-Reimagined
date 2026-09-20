#include "renderer.h"
#include <d3dcompiler.h>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace idas3;
template<class T> using Com=Microsoft::WRL::ComPtr<T>;
namespace {
unsigned checks=0;
void check(bool value,const char* text){++checks;if(!value)throw std::runtime_error(text);}
void okay(HRESULT value,const char* text){check(SUCCEEDED(value),text);}
struct HostState {
    Com<ID3D11RenderTargetView> target;Com<ID3D11DepthStencilView> depth;
    Com<ID3D11RasterizerState> raster;Com<ID3D11BlendState> blend;
    Com<ID3D11DepthStencilState> depthState;Com<ID3D11Buffer> vertex,index;
    Com<ID3D11VertexShader> vs;Com<ID3D11PixelShader> ps;Com<ID3D11ComputeShader> cs;
    Com<ID3D11ShaderResourceView> image;Com<ID3D11SamplerState> sampler;
    std::array<Com<ID3D11Buffer>,6> constants;
    D3D11_VIEWPORT viewport{};D3D11_RECT scissor{};std::array<float,4> factors{};
    UINT stride{},offset{},indexOffset{},sampleMask{},stencil{};
    DXGI_FORMAT indexFormat{};D3D11_PRIMITIVE_TOPOLOGY topology{};
    explicit HostState(ID3D11DeviceContext* c){
        c->OMGetRenderTargets(1,&target,&depth);c->RSGetState(&raster);
        c->OMGetBlendState(&blend,factors.data(),&sampleMask);c->OMGetDepthStencilState(&depthState,&stencil);
        c->IAGetVertexBuffers(0,1,&vertex,&stride,&offset);c->IAGetIndexBuffer(&index,&indexFormat,&indexOffset);
        c->IAGetPrimitiveTopology(&topology);c->VSGetShader(&vs,nullptr,nullptr);c->PSGetShader(&ps,nullptr,nullptr);c->CSGetShader(&cs,nullptr,nullptr);
        c->PSGetShaderResources(0,1,&image);c->PSGetSamplers(0,1,&sampler);
        c->VSGetConstantBuffers(0,1,&constants[0]);c->PSGetConstantBuffers(0,1,&constants[1]);
        c->GSGetConstantBuffers(0,1,&constants[2]);c->HSGetConstantBuffers(0,1,&constants[3]);
        c->DSGetConstantBuffers(0,1,&constants[4]);c->CSGetConstantBuffers(0,1,&constants[5]);
        UINT n=1;c->RSGetViewports(&n,&viewport);n=1;c->RSGetScissorRects(&n,&scissor);
    }
    void matches(ID3D11DeviceContext* c)const{
        const HostState s(c);
        check(target==s.target&&depth==s.depth,"Unity output targets changed");
        check(raster==s.raster&&blend==s.blend&&depthState==s.depthState,"Unity render states changed");
        check(vertex==s.vertex&&index==s.index&&stride==s.stride&&offset==s.offset&&indexOffset==s.indexOffset&&indexFormat==s.indexFormat&&topology==s.topology,"Unity input assembler changed");
        check(vs==s.vs&&ps==s.ps&&cs==s.cs,"Unity shader bindings changed");
        check(image==s.image&&sampler==s.sampler,"Unity texture/sampler bindings changed");
        for(unsigned i=0;i<6;++i)check(constants[i]==s.constants[i],"Unity stage constants changed");
        check(factors==s.factors&&sampleMask==s.sampleMask&&stencil==s.stencil,"Unity blend/stencil values changed");
        check(std::memcmp(&viewport,&s.viewport,sizeof(viewport))==0&&std::memcmp(&scissor,&s.scissor,sizeof(scissor))==0,"Unity viewport/scissor changed");
    }
};
void seedHost(ID3D11Device* d,ID3D11DeviceContext* c){
    D3D11_TEXTURE2D_DESC td{};td.Width=td.Height=32;td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;
    td.Format=DXGI_FORMAT_B8G8R8A8_UNORM;td.BindFlags=D3D11_BIND_RENDER_TARGET;
    Com<ID3D11Texture2D> tex;Com<ID3D11RenderTargetView> rtv;okay(d->CreateTexture2D(&td,nullptr,&tex),"Host render texture");okay(d->CreateRenderTargetView(tex.Get(),nullptr,&rtv),"Host target view");
    td.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;td.BindFlags=D3D11_BIND_DEPTH_STENCIL;
    Com<ID3D11Texture2D> z;Com<ID3D11DepthStencilView> depth;okay(d->CreateTexture2D(&td,nullptr,&z),"Host depth");okay(d->CreateDepthStencilView(z.Get(),nullptr,&depth),"Host depth view");
    c->OMSetRenderTargets(1,rtv.GetAddressOf(),depth.Get());
    D3D11_RASTERIZER_DESC rd{};rd.FillMode=D3D11_FILL_WIREFRAME;rd.CullMode=D3D11_CULL_FRONT;rd.ScissorEnable=TRUE;
    Com<ID3D11RasterizerState> raster;okay(d->CreateRasterizerState(&rd,&raster),"Host raster");c->RSSetState(raster.Get());
    D3D11_BLEND_DESC bd{};bd.RenderTarget[0].RenderTargetWriteMask=3;Com<ID3D11BlendState> blend;
    okay(d->CreateBlendState(&bd,&blend),"Host blend");const float factors[]{.1f,.2f,.3f,.4f};c->OMSetBlendState(blend.Get(),factors,0x12345678);
    D3D11_DEPTH_STENCIL_DESC dd{};dd.DepthEnable=TRUE;dd.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;dd.DepthFunc=D3D11_COMPARISON_GREATER;
    Com<ID3D11DepthStencilState> ds;okay(d->CreateDepthStencilState(&dd,&ds),"Host depth state");c->OMSetDepthStencilState(ds.Get(),17);
    D3D11_BUFFER_DESC b{};b.ByteWidth=256;b.BindFlags=D3D11_BIND_VERTEX_BUFFER|D3D11_BIND_INDEX_BUFFER;
    Com<ID3D11Buffer> geometry;okay(d->CreateBuffer(&b,nullptr,&geometry),"Host geometry");UINT stride=12,offset=16;
    c->IASetVertexBuffers(0,1,geometry.GetAddressOf(),&stride,&offset);c->IASetIndexBuffer(geometry.Get(),DXGI_FORMAT_R16_UINT,4);c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
    b.BindFlags=D3D11_BIND_CONSTANT_BUFFER;Com<ID3D11Buffer> cb;okay(d->CreateBuffer(&b,nullptr,&cb),"Host constants");
    c->VSSetConstantBuffers(0,1,cb.GetAddressOf());c->PSSetConstantBuffers(0,1,cb.GetAddressOf());c->GSSetConstantBuffers(0,1,cb.GetAddressOf());
    c->HSSetConstantBuffers(0,1,cb.GetAddressOf());c->DSSetConstantBuffers(0,1,cb.GetAddressOf());c->CSSetConstantBuffers(0,1,cb.GetAddressOf());
    const char* shader="float4 vs(uint n:SV_VertexID):SV_POSITION{return float4(n,0,0,1);}\nfloat4 ps():SV_TARGET{return float4(.3,.6,.9,1);}\n[numthreads(1,1,1)]void cs(){}";
    auto compile=[&](const char* entry,const char* profile){Com<ID3DBlob> code;okay(D3DCompile(shader,std::strlen(shader),nullptr,nullptr,nullptr,entry,profile,0,0,&code,nullptr),"Host shader compile");return code;};
    auto vb=compile("vs","vs_5_0"),pb=compile("ps","ps_5_0"),cbCode=compile("cs","cs_5_0");
    Com<ID3D11VertexShader> vs;Com<ID3D11PixelShader> ps;Com<ID3D11ComputeShader> cs;
    okay(d->CreateVertexShader(vb->GetBufferPointer(),vb->GetBufferSize(),nullptr,&vs),"Host VS");
    okay(d->CreatePixelShader(pb->GetBufferPointer(),pb->GetBufferSize(),nullptr,&ps),"Host PS");
    okay(d->CreateComputeShader(cbCode->GetBufferPointer(),cbCode->GetBufferSize(),nullptr,&cs),"Host CS");
    c->VSSetShader(vs.Get(),nullptr,0);c->PSSetShader(ps.Get(),nullptr,0);c->CSSetShader(cs.Get(),nullptr,0);
    td.Format=DXGI_FORMAT_B8G8R8A8_UNORM;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    Com<ID3D11Texture2D> image;Com<ID3D11ShaderResourceView> srv;okay(d->CreateTexture2D(&td,nullptr,&image),"Host image");okay(d->CreateShaderResourceView(image.Get(),nullptr,&srv),"Host image view");c->PSSetShaderResources(0,1,srv.GetAddressOf());
    D3D11_SAMPLER_DESC sd{};sd.Filter=D3D11_FILTER_MIN_MAG_MIP_POINT;sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;
    Com<ID3D11SamplerState> sampler;okay(d->CreateSamplerState(&sd,&sampler),"Host sampler");c->PSSetSamplers(0,1,sampler.GetAddressOf());
    D3D11_VIEWPORT viewport{3,5,24,20,.125f,.875f};c->RSSetViewports(1,&viewport);D3D11_RECT scissor{7,8,21,23};c->RSSetScissorRects(1,&scissor);
}
std::vector<std::uint32_t> bitmap(Renderer& renderer,const std::filesystem::path& file){
    check(renderer.saveBitmap(file.wstring()),renderer.error.c_str());std::ifstream in(file,std::ios::binary);BITMAPFILEHEADER h{};in.read(reinterpret_cast<char*>(&h),sizeof(h));in.seekg(h.bfOffBits);
    std::vector<std::uint32_t> result(std::size_t(renderer.width)*renderer.height);in.read(reinterpret_cast<char*>(result.data()),result.size()*4);check(bool(in),"Bitmap read");return result;
}
NativeTextureBank texture(const std::filesystem::path& file){
    std::ofstream out(file,std::ios::binary);out.write("IDAS3T1\0",8);auto u=[&](std::uint32_t value){out.write(reinterpret_cast<char*>(&value),4);};
    u(1);u(1);u(0);u(4);u(4);u(64);
    for(unsigned i=0;i<16;++i)u(i&1?0xffc08040:0xff2060a0);out.close();return NativeTextureBank::load(file);
}
Mesh panel(){
    Mesh m;m.quad({-12,-10,-14},{12,-10,-14},{12,10,-14},{-12,10,-14},{.25f,.4f,.6f,.8f});
    for(auto& v:m.vertices){v.u=(v.position.x+12)/24;v.v=(v.position.y+10)/20;v.offsetColor={.02f,.04f,.08f,0};}
    auto& r=m.ranges[0];r.original=true;r.pcw=2;r.tsp=0x20900000;r.isp=0xc0000000;return m;
}
}
int main()try{
    const auto dir=std::filesystem::temp_directory_path()/("idas3-unity-renderer-"+std::to_string(GetCurrentProcessId()));std::filesystem::create_directories(dir);
    Com<ID3D11Device> device;Com<ID3D11DeviceContext> immediate;D3D_FEATURE_LEVEL level=D3D_FEATURE_LEVEL_11_0;
    okay(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,&level,1,D3D11_SDK_VERSION,&device,nullptr,&immediate),"Host WARP device");
    seedHost(device.Get(),immediate.Get());const HostState before(immediate.Get());
    Renderer shared,standalone;check(!shared.initializeSharedDevice(nullptr,320,240),"Null host device accepted");before.matches(immediate.Get());
    check(shared.initializeSharedDevice(device.Get(),320,240),shared.error.c_str());before.matches(immediate.Get());
    check(standalone.initialize(nullptr,320,240,true),standalone.error.c_str());
    check(shared.sharedTexture()&&shared.sharedTextureGeneration()==1&&!standalone.sharedTexture(),"External texture contract");
    D3D11_TEXTURE2D_DESC desc{};shared.sharedTexture()->GetDesc(&desc);Com<ID3D11Device> owner;shared.sharedTexture()->GetDevice(&owner);
    check(owner==device&&desc.Format==DXGI_FORMAT_B8G8R8A8_UNORM&&desc.Width==320&&desc.Height==240&&desc.MipLevels==1&&(desc.BindFlags&D3D11_BIND_SHADER_RESOURCE),"External texture descriptor/device");
    Com<ID3D11ShaderResourceView> externalView;okay(device->CreateShaderResourceView(shared.sharedTexture(),nullptr,&externalView),"Host external texture view");
    const auto bank=texture(dir/"synthetic.idastex");check(shared.loadTextures(bank),shared.error.c_str());before.matches(immediate.Get());check(standalone.loadTextures(bank),standalone.error.c_str());
    auto course=original::originalCourseLighting(3,true,false),player=course,rival=course;player.ambient={.6f,.2f,.1f};rival.ambient={.1f,.7f,.3f};
    auto fog=original::originalCourseFog(3,true,false);OriginalShowroomLighting showroom;OriginalRearViewFrame rear;rear.eye={0,0,1};rear.target={0,0,-1};
    std::vector<std::uint32_t> overlay(320*240),glow(640*480,0x000a0307),foreground(640*480);
    for(unsigned y=0;y<240;++y)for(unsigned x=0;x<320;++x)overlay[y*320+x]=y<20?0x904030c0:0;
    for(unsigned y=200;y<240;++y)for(unsigned x=80;x<560;++x)foreground[y*640+x]=0x80604020;
    const std::array<OverlayPass,2> passes{{{foreground.data(),false,true},{glow.data(),true,true}}};
    std::vector<std::uint32_t> previous;
    for(unsigned scene=0;scene<12;++scene){
        auto mesh=panel();auto& range=mesh.ranges[0];
        if(scene&1){range.texture=0;range.pcw|=8;range.tsp|=3u<<6;}
        if(scene==2||scene==3)range.courseLighting=true;
        if(scene==4)range.carLighting=1;if(scene==5)range.carLighting=2;
        if(scene==6){range.emissive=true;range.gmp=512;range.tsp=0x44000000;} // Projected DST_COLOR+ONE.
        if(scene==7){range.gmp|=1u<<11;range.originalLightDirection=std::array{.2f,-.7f,.4f};range.pcw&=~2u;}
        if(scene==8){range.billboard=true;range.emissive=true;for(auto& v:mesh.vertices){v.normal={0,0,-14};v.position.z=0;}}
        if(scene==9)range.viewMask=1;if(scene==10)range.viewMask=2;
        const auto* show=scene==7?&showroom:nullptr;const auto* mirror=scene>=8?&rear:nullptr;
        for(auto* r:{&shared,&standalone}){
            r->courseLighting=&course;r->playerLighting=&player;r->rivalLighting=&rival;r->courseFog=&fog;
            r->fitOriginalViewport=true;r->screenFadeArgb=scene==11?0x70301020:0;
            r->vehicleLights=true;r->opponentLights=true;r->courseLampPositions={{0,3,-10}};
            check(r->draw(mesh,{0,0,0},{0,0,-1},scene!=0,scene==0,overlay.data(),scene==11,show,mirror,passes),r->error.c_str());
        }
        before.matches(immediate.Get());const auto a=bitmap(shared,dir/"shared.bmp"),b=bitmap(standalone,dir/"standalone.bmp");before.matches(immediate.Get());
        check(a==b,"Shared/deferred framebuffer differs from native immediate framebuffer");
        if(scene==0)check(a[120*320+160]!=a[5*320],"Synthetic frame has no distinct overlay/scene pixels");
        if(scene==3)check(a!=previous,"Textured case did not change pixels");previous=a;
    }
    // Simulate Unity keeping the native output bound as an SRV between events.
    // Runtime state restore must preserve it even when our list binds it as RTV.
    immediate->PSSetShaderResources(0,1,externalView.GetAddressOf());const HostState sampling(immediate.Get());
    shared.measureGpuFrame=true;check(shared.draw(panel(),{0,0,0},{0,0,-1},true,false),shared.error.c_str());sampling.matches(immediate.Get());
    double milliseconds=0;check(shared.readGpuMilliseconds(milliseconds)&&milliseconds>=0,shared.error.c_str());sampling.matches(immediate.Get());
    const auto generation=shared.sharedTextureGeneration();check(!shared.resize(0,240),"Invalid dimensions accepted");check(shared.sharedTextureGeneration()==generation,"Failed resize changed generation");
    check(shared.resize(512,384),shared.error.c_str());sampling.matches(immediate.Get());check(shared.sharedTextureGeneration()==generation+1,"Resize did not publish a new texture generation");
    shared.sharedTexture()->GetDesc(&desc);check(desc.Width==512&&desc.Height==384,"Resize dimensions");
    check(shared.draw(panel(),{0,0,0},{0,0,-1},false,false),shared.error.c_str());sampling.matches(immediate.Get());
    // Host state retains references independently of native renderer lifetime.
    shared=Renderer{};sampling.matches(immediate.Get());immediate->ClearState();externalView.Reset();
    for(const auto* file:{"shared.bmp","standalone.bmp","synthetic.idastex"})std::filesystem::remove(dir/file);
    std::filesystem::remove(dir);
    std::cout<<"Unity shared-device renderer passed "<<checks<<" checks; 12 exact framebuffer comparisons.\n";return 0;
}catch(const std::exception& e){std::cerr<<"Unity shared-device renderer failed: "<<e.what()<<'\n';return 1;}
