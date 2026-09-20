// Standalone raw-packet shipping-HLSL versus unchanged primary-HLSL fixture.
// Reuse only standalone WARP utility/type definitions, not game renderer code.
#define main unusedPrimaryFixtureMain
#include "check_elan_lighting_reference.cpp"
#undef main
#include "../src/original_course_lighting.h"
#include <bit>
#include <random>
struct RawLights {std::array<float,16> view{};std::array<unsigned,8> glm{};std::array<std::array<unsigned,8>,16> lights{};std::array<unsigned,4> info{};};
static_assert(sizeof(RawLights)==624);
std::string read(const std::filesystem::path&p){std::ifstream f(p,std::ios::binary);require(bool(f),"Missing source input");return {std::istreambuf_iterator<char>(f),{}};}
F4 color(unsigned p){return {float((p>>16)&255)/255,float((p>>8)&255)/255,float(p&255)/255,float(p>>24)/255};}
Lights decode(const RawLights&r){
 Lights out;out.ambientBase[0]=color(r.glm[3]);out.ambientOffset[0]=color(r.glm[4]);out.ambientMaterial.x=(r.glm[1]>>5)&1;out.ambientMaterial.z=(r.glm[1]>>6)&1;out.overflow=(r.glm[1]>>9)&1;
 // Primary sendLights scans original hardware IDs, not submitted vector order.
 for(unsigned id=0;id<16;++id){const auto masks=r.glm[2];if(!(masks&(0x10001u<<id)))continue;
  const std::array<unsigned,8>*packet=nullptr;for(unsigned i=0;i<r.info[1];++i)if((r.lights[i][1]&15)==id)packet=&r.lights[i];if(!packet)continue;
  const auto&p=*packet;auto&l=out.light[out.count++];l.color=color(p[1]>>8);l.routing=(p[2]>>24)&15;l.masks={int((masks>>id)&1),0,int((masks>>(16+id))&1),0};
  auto component=[&](unsigned highShift,unsigned lowShift){const int hi=std::int8_t((p[2]>>highShift)&255);return -float(hi*16+int((p[0]>>lowShift)&15))/2047.f;};
  l.direction={component(16,16),component(8,4),component(0,0),0};l.position={std::bit_cast<float>(p[3]),std::bit_cast<float>(p[4]),std::bit_cast<float>(p[5]),0};
  const bool actualParallel=p[0]&(1u<<20);l.parallel=actualParallel||(l.position.x==0&&l.position.y==0&&l.position.z==0&&p[6]==0&&p[7]==0);
  l.dmode=actualParallel?(p[2]>>28)&3:(p[1]>>5)&7;l.smode=actualParallel?0:(p[2]>>28)&3;
  l.distanceA=std::bit_cast<float>((p[6]&65535)<<16);l.distanceB=std::bit_cast<float>(p[6]&0xffff0000);l.angleA=std::bit_cast<float>((p[7]&65535)<<16);l.angleB=std::bit_cast<float>(p[7]&0xffff0000);l.distanceMode.x=p[2]>>31;
 }
 return out;
}
int main(int argc,char**argv)try{
 require(argc==3||argc==4,"Primary dx11_naomi2.cpp and shipping renderer.cpp required; optional Unity fixture output");
 const auto primary=read(argv[1]),native=read(argv[2]);const auto anchor=primary.find("const char * const DX11N2ColorShader");require(anchor!=primary.npos,"Primary shader missing");
 const auto pb=primary.find("R\"(",anchor)+3,pe=primary.find(")\";",pb);require(pe!=primary.npos,"Primary shader end missing");
 const auto nb=native.find("cbuffer CourseLights:register(b4)"),ne=native.find("float sourceFogCoefficient",nb);require(nb!=native.npos&&ne!=native.npos,"Shipping shader scope changed");require(native.substr(nb,ne-nb).find("sourceLightPackets[32]")!=std::string::npos,"Fixture requires released16-light layout");
 const auto shader=std::string("cbuffer polyConstants:register(b1){float4 glossCoef;int4 constantColor;};\n")+primary.substr(pb,pe-pb)+native.substr(nb,ne-nb)+R"(
cbuffer fixtureInput:register(b0){float4 inputBase,inputOffset,inputPosition,inputNormal;int4 fixtureOptions;};
RWStructuredBuffer<float4> result:register(u0);
[numthreads(1,1,1)]void mainCS(){
 float4 rb=inputBase,rs=inputOffset,nb=inputBase,ns=inputOffset;
 computeColors(rb,rs,0,inputPosition.xyz,normalize(inputNormal.xyz));
 if(constantColor.x!=1)courseColors(nb,ns,inputPosition.xyz,inputNormal.xyz,glossCoef.x);
 result[0]=rb;result[1]=rs;result[2]=nb;result[3]=ns;
})";
 ComPtr<ID3DBlob> code,error;auto compiled=D3DCompile(shader.data(),shader.size(),"primary and shipping lighting",nullptr,nullptr,"mainCS","cs_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&code,&error);
 if(FAILED(compiled)){if(error)std::cerr<<static_cast<const char*>(error->GetBufferPointer());hr(compiled,"Shader compilation failed");}
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> ctx;D3D_FEATURE_LEVEL level;hr(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&ctx),"WARP unavailable");
 ComPtr<ID3D11ComputeShader> cs;hr(device->CreateComputeShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&cs),"Shader unavailable");ctx->CSSetShader(cs.Get(),nullptr,0);
 auto constant=[&](unsigned bytes){D3D11_BUFFER_DESC d{};d.ByteWidth=bytes;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_CONSTANT_BUFFER;ComPtr<ID3D11Buffer>b;hr(device->CreateBuffer(&d,nullptr,&b),"Constant buffer");return b;};
 auto input=constant(sizeof(Input)),poly=constant(sizeof(Poly)),lights=constant(sizeof(Lights)),raw=constant(sizeof(RawLights));ID3D11Buffer*bindings[]{input.Get(),poly.Get(),lights.Get(),nullptr,raw.Get()};ctx->CSSetConstantBuffers(0,5,bindings);
 D3D11_BUFFER_DESC d{};d.ByteWidth=64;d.StructureByteStride=16;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_UNORDERED_ACCESS;d.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;ComPtr<ID3D11Buffer>output,staging;hr(device->CreateBuffer(&d,nullptr,&output),"Output buffer");d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;d.MiscFlags=0;hr(device->CreateBuffer(&d,nullptr,&staging),"Staging buffer");ComPtr<ID3D11UnorderedAccessView>uav;hr(device->CreateUnorderedAccessView(output.Get(),nullptr,&uav),"Output view");
 unsigned cases=0,comparisons=0;float maximumError=0;
 std::ofstream fixture;
 if(argc==4){fixture.open(argv[3],std::ios::binary);require(bool(fixture),"Cannot write Unity lighting fixture");const unsigned header[]{0x3147544c,0};fixture.write(reinterpret_cast<const char*>(header),sizeof(header));}
 auto run=[&](const RawLights&r,const Input&i,const Poly&p){const auto reference=decode(r);ctx->UpdateSubresource(input.Get(),0,nullptr,&i,0,0);ctx->UpdateSubresource(poly.Get(),0,nullptr,&p,0,0);ctx->UpdateSubresource(lights.Get(),0,nullptr,&reference,0,0);ctx->UpdateSubresource(raw.Get(),0,nullptr,&r,0,0);ctx->CSSetUnorderedAccessViews(0,1,uav.GetAddressOf(),nullptr);ctx->Dispatch(1,1,1);ID3D11UnorderedAccessView*none=nullptr;ctx->CSSetUnorderedAccessViews(0,1,&none,nullptr);ctx->CopyResource(staging.Get(),output.Get());D3D11_MAPPED_SUBRESOURCE map{};hr(ctx->Map(staging.Get(),0,D3D11_MAP_READ,0,&map),"Readback");std::array<F4,4>values;std::copy_n(static_cast<const F4*>(map.pData),4,values.begin());ctx->Unmap(staging.Get(),0);
  for(unsigned a=0;a<2;++a)for(unsigned b=0;b<4;++b){const auto x=(&values[a].x)[b],y=(&values[a+2].x)[b],diff=std::abs(x-y);maximumError=std::max(maximumError,diff);++comparisons;if(!std::isfinite(x)||!std::isfinite(y)||diff>2e-5f){std::cerr<<"case"<<cases<<" row"<<a<<" component"<<b<<" primary"<<x<<" native"<<y<<'\n';throw std::runtime_error("Shipping shader differs from primary");}}
  if(fixture.is_open()){
   fixture.write(reinterpret_cast<const char*>(&r),sizeof(r));fixture.write(reinterpret_cast<const char*>(&i),sizeof(i));fixture.write(reinterpret_cast<const char*>(&p),sizeof(p));
   // Expected colors come from the unmodified primary HLSL, not the host shader.
   fixture.write(reinterpret_cast<const char*>(values.data()),2*sizeof(F4));require(bool(fixture),"Fixture write failed");
  }
  ++cases;
 };
 using namespace idas3::original;std::mt19937 random(0x0538a0);auto number=[&](){return float(random()%20001)/10000.f-1;};
 for(unsigned row=0;row<36;++row)for(unsigned view=0;view<8;++view){auto state=originalCourseLighting(row/4,(row/2)%2,row%2);auto matrix=originalLightIdentityMatrix;const float angle=view*.39f,co=std::cos(angle),si=std::sin(angle);matrix[0]=co;matrix[2]=-si;matrix[8]=si;matrix[10]=co;matrix[12]=view*100.f;matrix[13]=view*-70.f;matrix[14]=view*200.f;updateOriginalCourseRelativeDirections(state,matrix);auto packet=originalCourseLightingPacket(state,matrix);RawLights r;r.glm=packet.glm;std::copy(packet.lights.begin(),packet.lights.end(),r.lights.begin());r.info={1,packet.count,0,0};
  for(unsigned v=0;v<16;++v){Input in;in.position={number()*300,number()*200,-10-std::abs(number())*900,1};in.normal={number(),number(),number(),0};in.base={.4f+number()*.3f,.4f+number()*.3f,.4f+number()*.3f,.7f};in.offset={.2f,.3f,.4f,.1f};Poly p;p.gloss.x=.5f+v*2.f;run(r,in,p);}
 }
 // Synthetic packet corners independently exercise raw IDs/masks/routes,
 // both point distance modes, supported single/double-sided modes and material flags.
 for(unsigned mode=0;mode<4;++mode)for(unsigned route=0;route<16;++route)for(unsigned flags=0;flags<8;++flags)for(unsigned mask=0;mask<4;++mask){
  RawLights r;r.info={1,1,0,0};const unsigned id=7;r.glm={0x08000400,((flags&1)<<5)|((flags&2)<<5)|((flags&4)<<7),((mask&1)?1u<<id:0)|((mask&2)?1u<<(id+16):0),0xff123456,0xff345678,0,0,0};
  auto&p=r.lights[0];p={0x080304a7|(mode==0?1u<<20:0),0x6d456700|id|((mode&1)<<5),0x1071a251|(route<<24)|((mode&1)<<31),std::bit_cast<unsigned>(2.f),std::bit_cast<unsigned>(3.f),std::bit_cast<unsigned>(-2.f),0x3e803f00,0xbf004000};
  p[2]=(p[2]&0xf0ffffff)|(route<<24);if(mode==3){p[3]=p[4]=p[5]=p[6]=p[7]=0;}
  Input in;Poly po;po.gloss.x=flags*.5f+.5f;run(r,in,po);
 }
 for(unsigned bypass=0;bypass<2;++bypass){RawLights r;r.info={1,16,0,0};r.glm={0x08000400,0x2e0,0xffffffff,0xff151515,0xff101010,0,0,0};for(unsigned id=0;id<16;++id)r.lights[id]={0x08100400,0x030a1700|id,0x017f7f7f,0,0,0,0,0};Input in;Poly p;p.constant.x=bypass;in.base={1.2f,.4f,.8f,1.3f};run(r,in,p);}
 if(fixture.is_open()){fixture.seekp(4);fixture.write(reinterpret_cast<const char*>(&cases),sizeof(cases));require(bool(fixture),"Fixture count write failed");}
 std::cout<<"PASS "<<cases<<" raw-packet cases / "<<comparisons<<" primary-versus-shipping WARP comparisons; max error "<<maximumError<<" (tolerance2e-5).36 source rows,8 transforms,16 vertices each plus2048 packet/routing/ambient/attenuation and2 sixteen-light/bypass cases. Volume0 only; independent CPU packet decoder; no renderer/game/userdata/device audio.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
