// Standalone reference: execute the locally supplied Flycast DX11 lighting
// function unchanged. This does not include the game, its renderer or saves.
#define NOMINMAX
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
using Microsoft::WRL::ComPtr;
struct F4 {float x{},y{},z{},w{};};
struct I4 {int x{},y{},z{},w{};};
struct Light {F4 color,direction,position;int parallel{},routing{},dmode{},smode{};I4 masks;float distanceA{1},distanceB{},angleA{1},angleB{};I4 distanceMode;};
struct Lights {std::array<Light,16> light{};F4 ambientBase[2]{},ambientOffset[2]{};I4 ambientMaterial;int count{},overflow{},bump0{-1},bump1{-1};};
struct Poly {F4 gloss{4,8,0,0};I4 constant;};
struct Input {F4 base{.13f,.27f,.39f,.43f},offset{.07f,.09f,.11f,.17f},position{1,-2,-6,1},normal{.3f,.7f,1,0};I4 options;};
static_assert(sizeof(Light)==112&&sizeof(Lights)==1888&&sizeof(Poly)==32&&sizeof(Input)==80);
void require(bool okay,const char* what){if(!okay)throw std::runtime_error(what);}
void hr(HRESULT result,const char* what){if(FAILED(result))throw std::runtime_error(what);}
F4 normalize(F4 v){const auto l=std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);return {v.x/l,v.y/l,v.z/l,0};}
float dot(F4 a,F4 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
float clamp(float v){return std::clamp(v,0.f,1.f);}
std::array<F4,2> expected(const Lights& s,const Poly& p,const Input& in){
    const unsigned vol=unsigned(in.options.x);auto base=in.base,offset=in.offset;
    if((&p.constant.x)[vol]==1)return {base,offset};
    const auto n=normalize(in.normal),v=normalize(in.position);
    const auto nd=dot(n,v);const F4 reflection{v.x-2*nd*n.x,v.y-2*nd*n.y,v.z-2*nd*n.z,0};
    float diffuse[3]{},specular[3]{},da=0,sa=0;
    for(int i=0;i<s.count;++i){const auto& l=s.light[i];F4 direction=l.direction,color=l.color;
        if(l.parallel)direction=normalize(direction);
        else{
            direction={l.position.x-in.position.x,l.position.y-in.position.y,l.position.z-in.position.z,0};
            float distance=std::sqrt(dot(direction,direction));direction=normalize(direction);
            float attenuation=1;
            if(l.distanceA!=1||l.distanceB!=0){if(!l.distanceMode.x)distance=1/distance;attenuation*=clamp(l.distanceB*distance+l.distanceA);}
            if(l.angleA!=1||l.angleB!=0)attenuation*=clamp((1-std::max(0.f,dot(direction,l.direction)))*l.angleB+l.angleA);
            color.x*=attenuation;color.y*=attenuation;color.z*=attenuation;
        }
        const float sign=(l.routing&8)?-2.f:2.f;
        if((&l.masks.x)[vol]){
            float factor=sign;
            if(l.dmode==0)factor*=std::max(0.f,dot(n,direction));
            else if(l.dmode==1)factor*=std::abs(dot(n,direction));
            if(l.routing&4)da+=color.x*factor;
            else for(unsigned c=0;c<3;++c)((l.routing&2)?specular:diffuse)[c]+=(&color.x)[c]*factor*(&in.base.x)[c];
        }
        if((&l.masks.x)[2+vol]){
            float factor=sign;
            if(l.smode==0)factor*=clamp(std::pow(std::max(0.f,dot(direction,reflection)),(&p.gloss.x)[vol]));
            else if(l.smode==1)factor*=clamp(std::pow(std::abs(dot(direction,reflection)),(&p.gloss.x)[vol]));
            if(l.routing&4)sa+=color.x*factor;
            else for(unsigned c=0;c<3;++c)((l.routing&1)?specular:diffuse)[c]+=(&color.x)[c]*factor*(&in.offset.x)[c];
        }
    }
    for(unsigned c=0;c<3;++c){
        (&base.x)[c]=diffuse[c]+(&s.ambientBase[vol].x)[c]*((&s.ambientMaterial.x)[vol]?(&in.base.x)[c]:1.f);
        (&offset.x)[c]=specular[c]+(&s.ambientOffset[vol].x)[c]*((&s.ambientMaterial.x)[2+vol]?(&in.offset.x)[c]:1.f);
    }
    base.w+=da;offset.w+=sa;
    for(unsigned c=0;c<4;++c){if(s.overflow)(&offset.x)[c]+=std::max(0.f,(&base.x)[c]-1);(&base.x)[c]=clamp((&base.x)[c]);(&offset.x)[c]=clamp((&offset.x)[c]);}
    return {base,offset};
}
int main(int argc,char** argv)try{
    require(argc==2,"Supply local primary core/rend/dx11/dx11_naomi2.cpp");
    std::ifstream f(std::filesystem::path(argv[1]),std::ios::binary);require(bool(f),"Cannot open supplied primary source");
    const std::string source{std::istreambuf_iterator<char>(f),{}};
    const auto anchor=source.find("const char * const DX11N2ColorShader");require(anchor!=source.npos,"Missing original color shader declaration");
    const auto begin=source.find("R\"(",anchor),end=source.find(")\";",begin);require(begin!=source.npos&&end!=source.npos,"Missing original shader source boundaries");
    const std::string prefix="cbuffer polyConstants:register(b1){float4 glossCoef;int4 constantColor;};\n";
    const std::string suffix=R"(
cbuffer fixtureInput:register(b0){float4 inputBase,inputOffset,inputPosition,inputNormal;int4 fixtureOptions;};
RWStructuredBuffer<float4> result:register(u0);
[numthreads(1,1,1)]void mainCS(){float4 b=inputBase,s=inputOffset;computeColors(b,s,fixtureOptions.x,inputPosition.xyz,normalize(inputNormal.xyz));result[0]=b;result[1]=s;}
)";
    const auto shader=prefix+source.substr(begin+3,end-(begin+3))+suffix;
    ComPtr<ID3DBlob> bytecode,error;const auto compiled=D3DCompile(shader.data(),shader.size(),"supplied DX11N2ColorShader",nullptr,nullptr,"mainCS","cs_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&bytecode,&error);
    if(FAILED(compiled)){if(error)std::cerr<<static_cast<const char*>(error->GetBufferPointer());hr(compiled,"Original lighting shader compile failed");}
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level;
    hr(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context),"WARP creation failed");
    ComPtr<ID3D11ComputeShader> compute;hr(device->CreateComputeShader(bytecode->GetBufferPointer(),bytecode->GetBufferSize(),nullptr,&compute),"Compute shader creation failed");
    context->CSSetShader(compute.Get(),nullptr,0);
    auto constant=[&](UINT bytes){ComPtr<ID3D11Buffer> buffer;D3D11_BUFFER_DESC desc{};desc.ByteWidth=bytes;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;hr(device->CreateBuffer(&desc,nullptr,&buffer),"Constant buffer failed");return buffer;};
    auto inputBuffer=constant(sizeof(Input)),polyBuffer=constant(sizeof(Poly)),lightBuffer=constant(sizeof(Lights));
    ID3D11Buffer* bindings[]{inputBuffer.Get(),polyBuffer.Get(),lightBuffer.Get()};context->CSSetConstantBuffers(0,3,bindings);
    ComPtr<ID3D11Buffer> output,readback;D3D11_BUFFER_DESC desc{};desc.ByteWidth=32;desc.StructureByteStride=16;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_UNORDERED_ACCESS;desc.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    hr(device->CreateBuffer(&desc,nullptr,&output),"Output buffer failed");desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;hr(device->CreateBuffer(&desc,nullptr,&readback),"Readback buffer failed");
    ComPtr<ID3D11UnorderedAccessView> uav;hr(device->CreateUnorderedAccessView(output.Get(),nullptr,&uav),"Output view failed");
    unsigned cases=0,comparisons=0;float maximumError=0;
    auto run=[&](const Lights& lights,const Poly& poly,const Input& in){
        context->UpdateSubresource(inputBuffer.Get(),0,nullptr,&in,0,0);context->UpdateSubresource(polyBuffer.Get(),0,nullptr,&poly,0,0);context->UpdateSubresource(lightBuffer.Get(),0,nullptr,&lights,0,0);
        context->CSSetUnorderedAccessViews(0,1,uav.GetAddressOf(),nullptr);context->Dispatch(1,1,1);
        ID3D11UnorderedAccessView* nullView=nullptr;context->CSSetUnorderedAccessViews(0,1,&nullView,nullptr);context->CopyResource(readback.Get(),output.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};hr(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped),"Readback map failed");
        std::array<F4,2> actual;std::copy_n(static_cast<const F4*>(mapped.pData),2,actual.begin());context->Unmap(readback.Get(),0);
        const auto wanted=expected(lights,poly,in);for(unsigned row=0;row<2;++row)for(unsigned c=0;c<4;++c){
            const auto a=(&actual[row].x)[c],e=(&wanted[row].x)[c],difference=std::abs(a-e);maximumError=std::max(maximumError,difference);++comparisons;
            if(!std::isfinite(a)||difference>0.00002f){std::cerr<<"case="<<cases<<" row="<<row<<" component="<<c<<" actual="<<a<<" expected="<<e<<'\n';throw std::runtime_error("Original shader differs from documented formula");}
        }++cases;
    };
    for(unsigned mode=0;mode<4;++mode)for(unsigned route=0;route<16;++route)for(unsigned masks=0;masks<4;++masks)for(unsigned ambient=0;ambient<16;++ambient)for(unsigned vol=0;vol<2;++vol){
        Input in;in.options.x=int(vol);Poly poly;Lights lights;lights.count=1;lights.overflow=int((route+ambient)&1);
        for(unsigned c=0;c<4;++c)(&lights.ambientMaterial.x)[c]=(ambient>>c)&1;
        lights.ambientBase[0]={.21f,.31f,.11f,1};lights.ambientBase[1]={.33f,.17f,.23f,1};lights.ambientOffset[0]={.04f,.07f,.08f,0};lights.ambientOffset[1]={.1f,.03f,.06f,0};
        auto& light=lights.light[0];light.color={.71f,.59f,.83f,0};light.direction={-.17f,.23f,.57f,0};light.position={2,3,-2,1};light.parallel=mode<2;light.routing=int(route);light.dmode=int(mode&1);light.smode=int((mode+1)&1);
        light.masks={int(masks&1),int(masks&1),int((masks>>1)&1),int((masks>>1)&1)};
        light.distanceMode.x=int(mode&1);light.distanceA=.2f;light.distanceB=mode&1?.09f:2.f;light.angleA=1.1f;light.angleB=-.3f;
        run(lights,poly,in);
    }
    // Explicit accumulation/overflow/subtraction, alpha and constant bypass.
    for(unsigned bypass=0;bypass<2;++bypass)for(unsigned vol=0;vol<2;++vol)for(unsigned route=0;route<16;++route){
        Input in;in.options.x=int(vol);in.base={1.2f,.4f,.8f,1.3f};in.offset={.2f,.1f,.4f,.7f};Poly poly;poly.constant={int(bypass),int(bypass),0,0};Lights lights;lights.count=16;lights.overflow=1;
        for(unsigned i=0;i<16;++i){auto& light=lights.light[i];light.color={.1f,.05f,.07f,0};light.direction={0,0,1,0};light.parallel=1;light.routing=int(route);light.masks={1,1,1,1};}
        run(lights,poly,in);
    }
    std::cout<<"PASS "<<cases<<" unmodified DX11 shader cases / "<<comparisons<<" float comparisons; max error "<<maximumError<<" (tolerance2e-5). Both volumes,16 routes,4 ambient flags, single/double modes, point/parallel attenuation,16-light accumulation, overflow and constant bypass. Offscreen WARP only; no game or user data.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
