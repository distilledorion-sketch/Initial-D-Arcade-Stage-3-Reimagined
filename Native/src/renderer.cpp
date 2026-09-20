#include "renderer.h"
#if !defined(IDAS3_PORTABLE_SCENE)
#include <d3dcompiler.h>
#include <DirectXMath.h>
#endif
#include <fstream>
#include <filesystem>
#include <sstream>
#include <cstring>
#include <limits>
#include <atomic>
#include <unordered_map>

namespace idas3 {
namespace {
void identifyCachedGeometry(Mesh& mesh){
    // Process-wide identities survive cache replacement/address reuse. This
    // is an identity for immutable bytes, not a probabilistic content hash.
    static std::atomic<std::uint64_t> next{1};
    for(auto& range:mesh.ranges){
        if(range.geometryId)continue;
        range.geometryId=next.fetch_add(1,std::memory_order_relaxed);
        if(!range.geometryId)throw std::overflow_error("Cached geometry identity exhausted");
    }
}
bool sameInstance(const NativeModelInstance& a,const NativeModelInstance& b){
    return a.chunk==b.chunk&&a.billboard==b.billboard&&a.transform==b.transform;
}
std::size_t instanceKey(const NativeModelInstance& instance){
    std::size_t key=instance.chunk*31u+unsigned(instance.billboard);
    // Hash float values rather than their representation so signed zero
    // follows the exact equality used by the existing assembly cache.
    for(float value:instance.transform)key^=std::hash<float>{}(value)+0x9e3779b9+(key<<6)+(key>>2);
    return key;
}
void appendCachedSlice(Mesh& destination,const Mesh& source,std::size_t first,std::size_t last){
    if(first==last)return;
    auto it=std::lower_bound(source.ranges.begin(),source.ranges.end(),first,
        [](const MeshRange& range,std::size_t vertex){return std::size_t(range.first)+range.count<=vertex;});
    for(;it!=source.ranges.end()&&it->first<last;++it){const auto& r=*it;
        const auto begin=std::max(first,std::size_t(r.first)),end=std::min(last,std::size_t(r.first)+r.count);
        // Cached object meshes have ordinary ownership while being assembled;
        // the complete group receives its course tag after coalescing.
        destination.beginRange(r.texture,r.tsp,r.pcw,r.isp,r.gmp,r.original,r.emissive,r.gloss,r.billboard,r.originalLightDirection,r.courseLighting,r.viewMask,r.carLighting,false,r.sourceFaceCulling);
        if(destination.ranges.back().count==0&&begin==r.first&&end==std::size_t(r.first)+r.count)
            destination.ranges.back().geometryId=r.geometryId;
        destination.ranges.back().count+=std::uint32_t(end-begin);
        destination.vertices.insert(destination.vertices.end(),source.vertices.begin()+begin,source.vertices.begin()+end);
    }
}
void rebuildObjectMesh(const NativeModel& model,const NativeAssembly& next,NativeAssembly& previous,
        Mesh& mesh,Mesh& rebuilt,std::vector<std::size_t>& offsets,bool courseGeometry){
    // A moving scenery window can add/remove just one tree. Reuse transformed
    // bytes for the other instances instead of transforming the entire group.
    // Keep only the current window, and preserve its exact instance/list order.
    // Reuse both buffers: allocating the replacement on every update costs
    // more than the transforms saved when the visibility window moves.
    std::unordered_multimap<std::size_t,std::size_t> lookup;
    if(offsets.size()==previous.instances.size()+1)
        for(std::size_t i=0;i<previous.instances.size();++i)lookup.emplace(instanceKey(previous.instances[i]),i);
    rebuilt.vertices.clear();rebuilt.ranges.clear();rebuilt.vertices.reserve(mesh.vertices.size());rebuilt.ranges.reserve(mesh.ranges.size());
    std::vector<std::size_t> boundaries;boundaries.reserve(next.instances.size()+1);boundaries.push_back(0);
    NativeAssembly single;single.instances.resize(1);
    for(const auto& instance:next.instances){
        bool reused=false;const auto [first,last]=lookup.equal_range(instanceKey(instance));
        for(auto it=first;it!=last;++it)if(sameInstance(instance,previous.instances[it->second])){
            appendCachedSlice(rebuilt,mesh,offsets[it->second],offsets[it->second+1]);reused=true;break;
        }
        if(!reused){single.instances[0]=instance;rebuilt.originalCar(model,single,{},0);}
        boundaries.push_back(rebuilt.vertices.size());
    }
    for(auto& range:rebuilt.ranges)range.courseGeometry=courseGeometry;
    identifyCachedGeometry(rebuilt);std::swap(mesh,rebuilt);offsets=std::move(boundaries);previous=next;
}
}
void Mesh::append(const Mesh& mesh){
    // Preserve both source order and the normal beginRange coalescing at the
    // boundary (background/course and course/car can share a material).
    for(const auto& range:mesh.ranges){
        beginRange(range.texture,range.tsp,range.pcw,range.isp,range.gmp,range.original,range.emissive,range.gloss,range.billboard,range.originalLightDirection,range.courseLighting,range.viewMask,range.carLighting,range.courseGeometry,range.sourceFaceCulling);
        if(ranges.back().count==0)ranges.back().geometryId=range.geometryId;
        ranges.back().count+=range.count;
        vertices.insert(vertices.end(),mesh.vertices.begin()+range.first,mesh.vertices.begin()+range.first+range.count);
    }
}
void CourseMeshCache::prepare(const NativeModel& model,const NativeAssembly& assembly,bool courseGeometry){
    if(model_!=&model||assembly_!=&assembly||courseGeometry_!=courseGeometry){
        invalidate();mesh_.vertices.clear();mesh_.ranges.clear();
        instanceVertices_.clear();instanceVertices_.push_back(0);
        NativeAssembly single;single.instances.resize(1);
        for(const auto& instance:assembly.instances){
            single.instances[0]=instance;mesh_.originalCar(model,single,{0,0,0},0);
            instanceVertices_.push_back(mesh_.vertices.size());
        }
        for(auto& range:mesh_.ranges)range.courseGeometry=courseGeometry;
        identifyCachedGeometry(mesh_);
        objectMeshes_.clear();
        model_=&model;assembly_=&assembly;
        courseGeometry_=courseGeometry;
    }
}
void CourseMeshCache::appendTo(Mesh& destination,const NativeModel& model,const NativeAssembly& assembly,bool courseGeometry){
    prepare(model,assembly,courseGeometry);
    destination.append(mesh_);
}
void CourseMeshCache::appendWithInsertions(Mesh& destination,const NativeModel& model,const NativeAssembly& assembly,
        std::span<const NativeAssemblyInsertion> insertions,bool courseGeometry){
    prepare(model,assembly,courseGeometry);
    auto appendSlice=[&](std::size_t first,std::size_t last){
        if(first==last)return;
        auto it=std::lower_bound(mesh_.ranges.begin(),mesh_.ranges.end(),first,
            [](const MeshRange& range,std::size_t vertex){return std::size_t(range.first)+range.count<=vertex;});
        for(;it!=mesh_.ranges.end()&&it->first<last;++it){const auto& r=*it;
            const auto begin=std::max(first,std::size_t(r.first)),end=std::min(last,std::size_t(r.first)+r.count);
            destination.beginRange(r.texture,r.tsp,r.pcw,r.isp,r.gmp,r.original,r.emissive,r.gloss,r.billboard,r.originalLightDirection,r.courseLighting,r.viewMask,r.carLighting,r.courseGeometry,r.sourceFaceCulling);
            if(destination.ranges.back().count==0&&begin==r.first&&end==std::size_t(r.first)+r.count)
                destination.ranges.back().geometryId=r.geometryId;
            destination.ranges.back().count+=std::uint32_t(end-begin);
            destination.vertices.insert(destination.vertices.end(),mesh_.vertices.begin()+begin,mesh_.vertices.begin()+end);
        }
    };
    objectMeshes_.resize(insertions.size());std::size_t cursor=0;
    for(std::size_t i=0;i<insertions.size();++i){const auto& addition=insertions[i];
        if(addition.before<cursor||addition.before>assembly.instances.size()||addition.replaceCount>assembly.instances.size()-addition.before)
            throw std::runtime_error("Original scenery insertion outside static assembly");
        appendSlice(instanceVertices_[cursor],instanceVertices_[addition.before]);
        auto& cached=objectMeshes_[i];
        if(cached.assembly.instances.size()!=addition.assembly.instances.size()||!std::equal(cached.assembly.instances.begin(),cached.assembly.instances.end(),addition.assembly.instances.begin(),sameInstance))
            rebuildObjectMesh(model,addition.assembly,cached.assembly,cached.mesh,cached.scratch,cached.instanceVertices,courseGeometry);
        destination.append(cached.mesh);cursor=addition.before+addition.replaceCount;
    }
    appendSlice(instanceVertices_[cursor],instanceVertices_.back());
}
void Mesh::beginRange(std::uint32_t texture,std::uint32_t tsp,std::uint32_t pcw,std::uint32_t isp,std::uint32_t gmp,bool original,bool emissive,std::uint32_t gloss,bool billboard,std::optional<std::array<float,3>> originalLightDirection,bool courseLighting,std::uint32_t viewMask,std::uint32_t carLighting,bool courseGeometry,bool sourceFaceCulling){
    if(ranges.empty()||ranges.back().texture!=texture||ranges.back().tsp!=tsp||ranges.back().pcw!=pcw||ranges.back().isp!=isp||ranges.back().gmp!=gmp||ranges.back().original!=original||ranges.back().emissive!=emissive||ranges.back().gloss!=gloss||ranges.back().billboard!=billboard||ranges.back().originalLightDirection!=originalLightDirection||ranges.back().courseLighting!=courseLighting||ranges.back().viewMask!=viewMask||ranges.back().carLighting!=carLighting||ranges.back().courseGeometry!=courseGeometry||ranges.back().sourceFaceCulling!=sourceFaceCulling){
        ranges.push_back({std::uint32_t(vertices.size()),0,texture,tsp,pcw,isp,gmp,original,emissive,gloss,billboard,originalLightDirection,courseLighting,viewMask,carLighting,courseGeometry,sourceFaceCulling});
    }
    // Even a matching material may receive new geometry. A caller copying a
    // complete immutable range can restore its identity only when count==0.
    ranges.back().geometryId=0;
}
void Mesh::triangle(Vec3 a,Vec3 b,Vec3 c,Color col) {
    beginRange();ranges.back().count+=3;
    auto n=normalized(cross(b-a,c-a));vertices.push_back({a,n,col});vertices.push_back({b,n,col});vertices.push_back({c,n,col});
}
void Mesh::originalCar(const NativeModel& model,const NativeAssembly& assembly,Vec3 pos,float yaw,std::uint32_t textureBase){
    originalCar(model,assembly,pos,yaw,0.0f,0.0f,textureBase);
}
void Mesh::originalCar(const NativeModel& model,const NativeAssembly& assembly,Vec3 pos,
        float yaw,float pitch,float roll,std::uint32_t textureBase,std::span<const std::uint32_t> illuminatedChunks,bool sourceFaceCulling,bool reflectedFaceCulling){
    const auto axisRight=right(yaw),axisForward=forward(yaw);
    const bool tilted=pitch!=0.0f||roll!=0.0f;
    const float cp=std::cos(pitch),sp=std::sin(pitch),cr=std::cos(roll),sr=std::sin(roll);
    auto rotate=[&](Vec3 p){
        // Column vectors: RY*RX*RZ applies roll first, then pitch, then yaw.
        // Retain the previous yaw-only arithmetic when there is no tilt.
        if(tilted){
            const Vec3 rolled{cr*p.x-sr*p.y,sr*p.x+cr*p.y,p.z};
            p={rolled.x,cp*rolled.y-sp*rolled.z,sp*rolled.y+cp*rolled.z};
        }
        return axisRight*p.x+Vec3{0,p.y,0}+axisForward*p.z;
    };
    for(const auto& instance:assembly.instances){const auto& m=instance.transform;
        const bool emissive=std::find(illuminatedChunks.begin(),illuminatedChunks.end(),instance.chunk)!=illuminatedChunks.end();
        // The outer Y-X-Z pose is a proper rotation. Only this local linear
        // transform can reverse winding; do not apply ordinary-car culling
        // to mirrored/degenerate instances or view-facing source quads.
        const double determinant=double(m[0])*(double(m[5])*m[10]-double(m[6])*m[9])
            -double(m[1])*(double(m[4])*m[10]-double(m[6])*m[8])
            +double(m[2])*(double(m[4])*m[9]-double(m[5])*m[8]);
        // Reflected menu owners explicitly invert source cull parity (1FB440).
        // Opt in separately: a negative transform alone does not establish the owner.
        const bool reflected=reflectedFaceCulling&&determinant<0;
        const bool cullInstance=!instance.billboard&&((sourceFaceCulling&&determinant>0)||reflected);
        auto point=[&](Vec3 p){return Vec3{m[0]*p.x+m[1]*p.y+m[2]*p.z+m[3],m[4]*p.x+m[5]*p.y+m[6]*p.z+m[7],m[8]*p.x+m[9]*p.y+m[10]*p.z+m[11]};};
        auto normal=[&](Vec3 p){return normalized(Vec3{m[0]*p.x+m[1]*p.y+m[2]*p.z,m[4]*p.x+m[5]*p.y+m[6]*p.z,m[8]*p.x+m[9]*p.y+m[10]*p.z});};
        for(const auto& batch:model.chunks.at(instance.chunk).batches){
            auto pcw=batch.ich[0],tsp=batch.ich[2];
            // GMP e0 changes effective material flags; the vertex shader applies
            // the original view-normal environment UV rule.
            if(batch.material[2]&(1u<<11)){pcw=(pcw|8u)&~4u;tsp=(tsp|(1u<<20))&~(1u<<19);}
            if(batch.ich[6]==2||batch.ich[6]==0x42) {
                if(!(batch.material[2]&(1u<<11)))pcw&=~8u;
            }
            auto isp=batch.ich[1];
            if(reflected&&((isp>>27)&3)>=2)isp^=1u<<27;
            beginRange(batch.material[9]==0xffffffff?0xffffffff:batch.material[9]+textureBase,tsp,pcw,isp,batch.material[2],true,emissive||instance.billboard,batch.material[1],instance.billboard,std::nullopt,false,3,0,false,cullInstance);
            ranges.back().count+=std::uint32_t(batch.indices.size());
            auto unpack=[](std::uint32_t bits){return Color{float((bits>>16)&255)/255,float((bits>>8)&255)/255,float(bits&255)/255,float(bits>>24)/255};};
            // VR/VUR's second packed color belongs to volume1; offset0 comes
            // from GMP specular0, never from that second diffuse color.
            Color offset=(batch.material[2]&2)?unpack(batch.material[4]):Color{0,0,0,0};
            for(std::size_t j=0;j<batch.indices.size();++j){auto i=batch.indices[j];const auto& v=batch.vertices[i];
                // Original flat shading uses the final strip vertex; the
                // extracted triangle's third index is that vertex for both
                // alternating strips and fans.
                const auto& colorVertex=(pcw&2)?v:batch.vertices[batch.indices[j-j%3+2]];
                const bool packedColors=batch.ich[6]==0x42||batch.ich[6]==0x4a;
                auto bits=(batch.material[2]&1)?batch.material[3]:(packedColors?colorVertex.color0:0xffffffffu);
                if(instance.billboard){
                    // Preserve the original packed normal of lit spectator
                    // cards in their unused planar Z coordinate. The exact
                    // 24-bit integer scaled by2^-24 fits a float losslessly.
                    // Unlit lamps/aura keep their existing local XYZ layout.
                    auto local=v.position;
                    if(!(batch.material[2]&512)){
                        if(local.z!=0)throw std::runtime_error("Lit billboard is not planar");
                        std::uint32_t packed=0;
                        for(unsigned component=0;component<3;++component){
                            const float value=component==0?v.normal.x:component==1?v.normal.y:v.normal.z;
                            const auto quantized=std::lround(value*127.f);
                            if(quantized< -128||quantized>127||float(quantized)/127.f!=value)
                                throw std::runtime_error("Lit billboard normal is not source packed-byte precision");
                            packed|=(std::uint32_t(quantized)&255u)<<(component*8);
                        }
                        local.z=float(packed)*(1.f/16777216.f);
                    }
                    vertices.push_back({local,pos+rotate({m[3],m[7],m[11]}),unpack(bits),v.u,v.v,offset});
                }else vertices.push_back({pos+rotate(point(v.position)),rotate(normal(v.normal)),unpack(bits),v.u,v.v,offset});
            }
        }
    }
}
void Mesh::originalWorldChunk(const NativeModel& model,std::uint32_t chunk,std::uint32_t textureBase){
    NativeAssembly assembly;NativeModelInstance instance;instance.chunk=chunk;instance.transform={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};assembly.instances.push_back(instance);
    originalCar(model,assembly,{0,0,0},0,textureBase);
}
void Mesh::quad(Vec3 a,Vec3 b,Vec3 c,Vec3 d,Color col){triangle(a,b,c,col);triangle(a,c,d,col);}
void Mesh::box(Vec3 center,Vec3 size,float yaw,Color col){
    Vec3 p[8];auto r=right(yaw),f=forward(yaw);
    for(int i=0;i<8;i++)p[i]=center+r*((i&1?1.f:-1.f)*size.x*.5f)+Vec3{0,(i&2?1.f:-1.f)*size.y*.5f,0}+f*((i&4?1.f:-1.f)*size.z*.5f);
    quad(p[0],p[1],p[3],p[2],col);quad(p[5],p[4],p[6],p[7],col);
    quad(p[4],p[0],p[2],p[6],col);quad(p[1],p[5],p[7],p[3],col);
    quad(p[2],p[3],p[7],p[6],col);quad(p[4],p[5],p[1],p[0],col);
}
void Mesh::tree(Vec3 p,float h){
    box(p+Vec3{0,h*.28f,0},{.35f,h*.56f,.35f},0,{.18f,.15f,.12f});
    for(int layer=0;layer<3;layer++){
        float bottom=h*(.24f+.20f*layer),top=h*(.64f+.17f*layer),radius=h*(.30f-.055f*layer);
        Vec3 tip=p+Vec3{0,top,0};
        for(int side=0;side<6;side++){
            float a=2*pi*side/6,b=2*pi*(side+1)/6;
            triangle(p+Vec3{std::cos(a)*radius,bottom,std::sin(a)*radius},tip,p+Vec3{std::cos(b)*radius,bottom,std::sin(b)*radius},{.08f+layer*.013f,.19f+layer*.025f,.12f+layer*.01f});
        }
    }
}
void Mesh::car(Vec3 pos,float yaw,Color col,bool braking,float steer){
    auto f=forward(yaw),r=right(yaw);auto at=[&](float x,float y,float z){return pos+r*x+Vec3{0,y,0}+f*z;};
    box(at(0,.58f,0),{1.64f,.55f,4.1f},yaw,col);
    box(at(0,.36f,0),{1.68f,.22f,4.15f},yaw,{.055f,.065f,.075f});
    // Sloped hatchback greenhouse is original development geometry.
    auto glass=Color{.07f,.12f,.17f};
    Vec3 bl=at(-.73f,.84f,-1.32f),br=at(.73f,.84f,-1.32f),fl=at(-.73f,.84f,1.02f),fr=at(.73f,.84f,1.02f);
    Vec3 tbl=at(-.66f,1.30f,-.73f),tbr=at(.66f,1.30f,-.73f),tfl=at(-.66f,1.30f,.37f),tfr=at(.66f,1.30f,.37f);
    quad(bl,br,tbr,tbl,glass);quad(fr,fl,tfl,tfr,glass);quad(fl,bl,tbl,tfl,glass);quad(br,fr,tfr,tbr,glass);quad(tbl,tbr,tfr,tfl,col);
    for(float side:{-1.f,1.f})box(at(side*.70f,1.08f,-.23f),{.07f,.47f,.075f},yaw,col);
    box(at(0,.83f,-1.74f),{1.63f,.08f,.20f},yaw,{.06f,.07f,.08f});
    for(float side:{-1.f,1.f}) {
        for(float z:{-1.30f,1.28f}){
            float wheelYaw=yaw+(z>0?steer*.3f:0);Vec3 hub=at(side*.80f,.32f,z);auto wf=forward(wheelYaw),wr=right(wheelYaw);
            for(int j=0;j<10;j++){
                float a=2*pi*j/10,b=2*pi*(j+1)/10;Vec3 va=wf*(std::sin(a)*.32f)+Vec3{0,std::cos(a)*.32f,0},vb=wf*(std::sin(b)*.32f)+Vec3{0,std::cos(b)*.32f,0};
                quad(hub+va-wr*.115f,hub+vb-wr*.115f,hub+vb+wr*.115f,hub+va+wr*.115f,{.024f,.027f,.032f});
                triangle(hub+wr*(side*.119f),hub+va+wr*(side*.119f),hub+vb+wr*(side*.119f),{.30f,.33f,.37f});
            }
        }
        box(at(side*.58f,.62f,-2.07f),{.46f,.15f,.025f},yaw,braking?Color{1,.09f,.035f}:Color{.48f,.045f,.024f});
        box(at(side*.55f,.65f,2.055f),{.41f,.14f,.02f},yaw,{.93f,.89f,.61f});
        box(at(side*.86f,.94f,.55f),{.21f,.13f,.24f},yaw,col);
    }
    box(at(0,.47f,-2.088f),{.45f,.15f,.015f},yaw,{.81f,.83f,.81f});
}
#if defined(IDAS3_PORTABLE_SCENE)
bool Renderer::initialize(HWND,int w,int h,bool){return initializeSceneCapture(w,h);}
bool Renderer::initializeSceneCapture(int w,int h){
    if(capturedScene){error="Renderer is already initialized";return false;}
    if(w<=0||h<=0||w>8192||h>8192){error="Invalid renderer dimensions";return false;}
    width=w;height=h;capturedScene.emplace();return true;
}
bool Renderer::resize(int w,int h){
    if(!capturedScene||w<=0||h<=0||w>8192||h>8192){error="Invalid scene output dimensions";return false;}
    width=w;height=h;return true;
}
bool Renderer::loadTextures(const NativeTextureBank& bank,bool append){
    if(!capturedScene){error="Renderer is not initialized";return false;}
    try{capturedScene->loadTextures(bank,append);return true;}catch(const std::exception& e){error=e.what();return false;}
}
bool Renderer::draw(const Mesh& mesh,Vec3 eye,Vec3 look,bool night,bool wet,const std::uint32_t* overlay,bool behind,const OriginalShowroomLighting* showroom,const OriginalRearViewFrame* rear,std::span<const OverlayPass> foreground){
    if(!capturedScene){error="Renderer is not initialized";return false;}
    try{capturedScene->capture(*this,mesh,eye,look,night,wet,overlay,behind,showroom,rear,foreground);return true;}catch(const std::exception& e){error=e.what();return false;}
}
bool Renderer::readGpuMilliseconds(double&){return false;}
bool Renderer::saveBitmap(const std::wstring&){error="Scene capture images are rendered by Unity";return false;}
#else
void Renderer::discardCommands(){
    if(sharedDevice&&context){Ptr<ID3D11CommandList> discard;context->FinishCommandList(FALSE,&discard);}
}
bool Renderer::fail(HRESULT hr,const char* where){discardCommands();std::ostringstream s;s<<where<<" (0x"<<std::hex<<static_cast<unsigned long>(hr)<<')';error=s.str();return false;}
bool Renderer::submitCommands(){
    if(!sharedDevice)return true;
    Ptr<ID3D11CommandList> commands;const auto hr=context->FinishCommandList(FALSE,&commands);
    if(FAILED(hr))return fail(hr,"finish shared-device frame");
    // TRUE restores ALL Unity immediate-context state, including stages that
    // this renderer never uses. Neither ClearState nor a manual partial restore
    // is performed on the host context. Command submission is ordered with the
    // host's subsequent texture sampling on this same device/render thread.
    hostImmediate->ExecuteCommandList(commands.Get(),TRUE);
    const auto removed=device->GetDeviceRemovedReason();
    return SUCCEEDED(removed)||fail(removed,"shared graphics device removed");
}
bool Renderer::initialize(HWND hwnd,int w,int h,bool softwareDiagnostic){
    if(device||capturedScene){error="Renderer is already initialized";return false;}
    if(w<=0||h<=0||w>7680||h>4320){error="Invalid renderer dimensions";return false;}
    diagnostic=softwareDiagnostic;UINT flags=D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL requested[]={D3D_FEATURE_LEVEL_11_0};D3D_FEATURE_LEVEL actual{};
    HRESULT hr;
    if(hwnd){
        DXGI_SWAP_CHAIN_DESC desc{};desc.BufferDesc.Width=w;desc.BufferDesc.Height=h;desc.BufferDesc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=2;desc.OutputWindow=hwnd;desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
        hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,flags,requested,1,D3D11_SDK_VERSION,&desc,&swapchain,&device,&actual,&context);
        if(FAILED(hr))return fail(hr,"D3D11 hardware initialization");
        Ptr<IDXGIDevice> dxgiDevice;Ptr<IDXGIAdapter> adapter;Ptr<IDXGIFactory> factory;
        if(SUCCEEDED(device.As(&dxgiDevice))&&SUCCEEDED(dxgiDevice->GetAdapter(&adapter))&&SUCCEEDED(adapter->GetParent(IID_PPV_ARGS(&factory))))factory->MakeWindowAssociation(hwnd,DXGI_MWA_NO_ALT_ENTER);
    }else{
        hr=D3D11CreateDevice(nullptr,diagnostic?D3D_DRIVER_TYPE_WARP:D3D_DRIVER_TYPE_HARDWARE,nullptr,flags,requested,1,D3D11_SDK_VERSION,&device,&actual,&context);
        if(FAILED(hr))return fail(hr,"D3D11 offscreen initialization");
    }
    return initializeResources(w,h);
}
bool Renderer::initializeSharedDevice(ID3D11Device* hostDevice,int w,int h){
    if(device||capturedScene){error="Renderer is already initialized";return false;}
    if(!hostDevice||hostDevice->GetFeatureLevel()<D3D_FEATURE_LEVEL_11_0){error="Unity renderer requires a D3D11 feature-level 11 device";return false;}
    if(w<=0||h<=0||w>7680||h>4320){error="Invalid renderer dimensions";return false;}
    device=hostDevice;device->GetImmediateContext(&hostImmediate);
    const auto hr=device->CreateDeferredContext(0,&context);
    if(FAILED(hr))return fail(hr,"Unity deferred graphics context");
    sharedDevice=true;
    return initializeResources(w,h);
}
bool Renderer::initializeSceneCapture(int w,int h){
    if(device||capturedScene){error="Renderer is already initialized";return false;}
    if(w<=0||h<=0||w>8192||h>8192){error="Invalid renderer dimensions";return false;}
    width=w;height=h;capturedScene.emplace();return true;
}
bool Renderer::initializeResources(int w,int h){
    HRESULT hr;
    const char* shader=R"HLSL(
cbuffer Frame:register(b0){row_major float4x4 viewProjection;float4 eye;float4 atmosphere;float4 lampPosition;float4 lampDirection;float4 cameraRight;float4 cameraUp;float4 showroomLight;float4 showroomTerms;float4 opponentPosition;float4 opponentDirection;float4 courseLamps[8];float4 sceneLightColor;};
cbuffer Material:register(b1){uint pcw;uint tsp;uint gmp;uint original;float alphaReference;uint emissive;float glossCoefficient;uint billboard;uint courseLightRange;uint3 materialPadding;};
cbuffer CourseFog:register(b3){float4 sourceFogColorDensity;float4 sourceVertexFogColorEnabled;uint4 sourceFogTable[32];};
cbuffer CourseLights:register(b4){row_major float4x4 sourceLightView;uint4 sourceGlm[2];uint4 sourceLightPackets[32];uint4 sourceLightInfo;};
float3 courseRgb(uint word){return float3((word>>16)&255,(word>>8)&255,word&255)/255.0;}
// Decode the source ELAN words, retaining quantized directions, separate
// diffuse/specular masks, material routing and upper-floatword attenuation.
void courseColors(inout float4 base,inout float4 offset,float3 viewPosition,float3 viewNormal,float gloss){
 float3 n=normalize(viewNormal),reflection=reflect(normalize(viewPosition),n);
 float3 diffuse=0,specular=0;float diffuseAlpha=0,specularAlpha=0;
 uint masks=sourceGlm[0].z,flags=sourceGlm[0].y;
 [loop]for(uint i=0;i<sourceLightInfo.y;++i){
  uint4 a=sourceLightPackets[i*2],b=sourceLightPackets[i*2+1];
  uint id=a.y&15,route=(a.z>>24)&15;
  bool useDiffuse=(masks&(1u<<id))!=0,useSpecular=(masks&(1u<<(id+16)))!=0;
  if(!useDiffuse&&!useSpecular)continue;
  int3 high=int3((a.z>>16)&255,(a.z>>8)&255,a.z&255);high=(high<<24)>>24;
  int3 low=int3((a.x>>16)&15,(a.x>>4)&15,a.x&15);
  float3 incoming=-float3((high<<4)|low)/2047.0;
  float3 position=asfloat(uint3(a.w,b.x,b.y));
  bool actualParallel=(a.x&(1u<<20))!=0;
  bool parallel=actualParallel||(all(position==0)&&b.z==0&&b.w==0);
  uint diffuseMode=actualParallel?((a.z>>28)&3):((a.y>>5)&7);
  uint specularMode=actualParallel?0:((a.z>>28)&3);
  float3 color=courseRgb(a.y>>8),toLight;
  if(parallel)toLight=normalize(incoming);
  else{
   float3 delta=position-viewPosition;float distance=length(delta);toLight=normalize(delta);
   float da=asfloat((b.z&65535)<<16),db=asfloat(b.z&0xffff0000);
   float aa=asfloat((b.w&65535)<<16),ab=asfloat(b.w&0xffff0000);
   if(da!=1||db!=0){float d=(a.z&(1u<<31))!=0?distance:1/distance;color*=saturate(db*d+da);}
   if(aa!=1||ab!=0)color*=saturate((1-max(0,dot(toLight,incoming)))*ab+aa);
  }
  float factor=(route&8)!=0?-2.0:2.0;
  if(useDiffuse){
   float amount=factor;
   if(diffuseMode==0)amount*=max(dot(n,toLight),0);
   else if(diffuseMode==1)amount*=abs(dot(n,toLight));
   if((route&4)!=0)diffuseAlpha+=color.r*amount;
   else if((route&2)!=0)specular+=color*amount*base.rgb;
   else diffuse+=color*amount*base.rgb;
  }
  if(useSpecular){
   float amount=factor;
   if(specularMode==0)amount*=saturate(pow(max(dot(toLight,reflection),0),gloss));
   else if(specularMode==1)amount*=saturate(pow(abs(dot(toLight,reflection)),gloss));
   if((route&4)!=0)specularAlpha+=color.r*amount;
   else if((route&1)!=0)specular+=color*amount*offset.rgb;
   else diffuse+=color*amount*offset.rgb;
  }
 }
 diffuse+=courseRgb(sourceGlm[0].w)*((flags&(1u<<5))!=0?base.rgb:float3(1,1,1));
 specular+=courseRgb(sourceGlm[1].x)*((flags&(1u<<6))!=0?offset.rgb:float3(1,1,1));
 base=float4(diffuse,base.a+diffuseAlpha);offset=float4(specular,offset.a+specularAlpha);
 if((flags&(1u<<9))!=0)offset+=max(base-1,0);
 base=saturate(base);offset=saturate(offset);
}
float sourceFogCoefficient(float reciprocalDepth){
 float z=clamp(sourceFogColorDensity.w*reciprocalDepth,1.0,255.9999);
 uint bits=asuint(z),exponent=(bits>>23)-127;
 uint index=exponent*16+((bits>>19)&15);
 float fraction=float(bits&0x7ffff)/524288.0;
 uint pair=sourceFogTable[index>>2][index&3];
 return lerp(float(pair>>8),float(pair&255),fraction)/255.0;
}
struct V{float3 p:POSITION;float3 n:NORMAL;float4 c:COLOR0;float2 uv:TEXCOORD0;float4 offsetColor:COLOR1;};
struct P{float4 p:SV_POSITION;float3 world:TEXCOORD0;float3 n:NORMAL;float4 c:COLOR0;float2 uv:TEXCOORD1;float4 offsetColor:COLOR1;noperspective float reciprocalDepth:TEXCOORD2;};
Texture2D surface:register(t0);SamplerState surfaceSampler:register(s0);
P mainVS(V v){P o;o.p=mul(float4(v.p,1),viewProjection);o.world=v.p;o.n=v.n;o.c=v.c;o.uv=v.uv;o.offsetColor=v.offsetColor;
 if(billboard!=0){float3 facing=cross(cameraRight.xyz,cameraUp.xyz);
  float localZ=v.p.z;float3 localNormal=float3(0,0,1);
  if((gmp&512)==0){
   // Lit source cards are planar. LocalZ carries their exact source normal
   // bytes, not a displaced coordinate; retain signed byte/127 semantics.
   uint packed=(uint)(v.p.z*16777216.0);
   int3 bytes=int3((int)(packed<<24),(int)(packed<<16),(int)(packed<<8))>>24;
   localNormal=float3(bytes)/127.0;localZ=0;
  }
  o.world=v.n+cameraRight.xyz*v.p.x+cameraUp.xyz*v.p.y+facing*localZ;
  o.p=mul(float4(o.world,1),viewProjection);
  o.n=cameraRight.xyz*localNormal.x+cameraUp.xyz*localNormal.y+facing*localNormal.z;}
 bool courseLit=sourceLightInfo.x!=0&&courseLightRange!=0&&showroomLight.w==0&&original!=0;
 if(courseLit){
  if((gmp&512)==0)courseColors(o.c,o.offsetColor,mul(float4(o.world,1),sourceLightView).xyz,mul(float4(o.n,0),sourceLightView).xyz,glossCoefficient);
  // ELAN adds untextured offset RGBA before pixel alpha selection.
  if((pcw&8)==0){o.c+=o.offsetColor;o.offsetColor=0;}
 }
 // Original 112260/053480 showroom GLM: one nonzero white parallel light,
 // ambient multiplied by base material, and specular routed to offset.
 // ELAN computes these colors per vertex before texturing. The b0 material
 // bit bypasses the entire light model, including ambient and highlights.
 if(showroomLight.w!=0&&original!=0&&(gmp&512)==0){
  float3 n=v.n*rsqrt(max(dot(v.n,v.n),1e-10));
  float3 toLight=normalize(showroomLight.xyz);
  float3 incident=v.p-eye.xyz;incident*=rsqrt(max(dot(incident,incident),1e-10));
  float diffuse=showroomTerms.y*max(dot(n,toLight),0);
  float specular=showroomTerms.y*pow(max(dot(toLight,reflect(incident,n)),0),glossCoefficient);
  o.c=saturate(float4(v.c.rgb*(showroomTerms.x+diffuse*sceneLightColor.rgb),v.c.a));
  o.offsetColor=saturate(float4(v.offsetColor.rgb*specular*sceneLightColor.rgb,v.offsetColor.a));
 }
 // ELAN's ordinary environment map adds view-normal XY/2+.5 to authored UV.
 if(original!=0&&(gmp&(1<<11))!=0){float3 n=normalize(v.n);o.uv=saturate(v.uv+float2(dot(n,cameraRight.xyz),dot(n,cameraUp.xyz))*.5+.5);}
 // Interpolate reciprocal clip depth linearly in screen space, matching the
 // PVR reciprocal-Z carrier without reapplying perspective correction.
 o.reciprocalDepth=1/o.p.w;return o;}
// ELAN flat shading uses the final source strip vertex after lighting. D3D's
// default provoking vertex differs, so select the recovered final vertex
// explicitly for showroom and course GLM passes.
[maxvertexcount(3)]
void showroomGeometry(triangle P input[3],inout TriangleStream<P> stream){
 for(uint i=0;i<3;++i){P o=input[i];
  if(original!=0&&(pcw&2)==0){o.c=input[2].c;o.offsetColor=input[2].offsetColor;}
  stream.Append(o);
 }
}
float4 mainPS(P v):SV_TARGET{
 float3 normal=v.n*rsqrt(max(dot(v.n,v.n),1e-10));
 // Cull Off draws both sides; face the normal at the viewer so the
 // away-facing half of crossed foliage is not shaded as a dark card.
 if(dot(normal,v.world-eye.xyz)>0)normal=-normal;
 float light=.32+.68*saturate(dot(normal,normalize(float3(-.35,.85,.4))));
 float4 color=v.c;float4 offsetColor=v.offsetColor;
 if(original==0){
  float4 texel=surface.Sample(surfaceSampler,v.uv);clip(texel.a*v.c.a-.05);
  color*=texel;color.rgb*=light;
 }else{
  // Source course and showroom colors were already lit per vertex.
  if(showroomLight.w==0&&!(sourceLightInfo.x!=0&&courseLightRange!=0)&&(gmp&512)==0&&emissive==0){color.rgb*=light;offsetColor=0;}
  if((tsp&(1<<20))==0)color.a=1;
  // PVR fog mode3 replaces the lit base before texture shading.
  if(sourceVertexFogColorEnabled.w!=0&&((tsp>>22)&3)==3){
   color=float4(sourceFogColorDensity.rgb,sourceFogCoefficient(v.reciprocalDepth));
  }
  if((pcw&8)!=0){
   float4 texel=surface.Sample(surfaceSampler,v.uv);
   if((tsp&(1<<19))!=0)texel.a=1;
   uint mode=(tsp>>6)&3;
   if(mode==0)color=texel;
   else if(mode==1)color=float4(color.rgb*texel.rgb,texel.a);
   else if(mode==2)color.rgb=lerp(color.rgb,texel.rgb,texel.a);
   else color*=texel;
   if((pcw&4)!=0)color.rgb+=offsetColor.rgb;
  }else color+=offsetColor;
  if(((pcw>>24)&7)==4){clip(floor(saturate(color.a)*255+.5)-alphaReference);color.a=1;}
  color=saturate(color);
 }
 // Original races use separate projected headlight geometry. Native scene
 // illumination remains for development/attract geometry and car ambient.
 if(showroomLight.w==0&&!(sourceLightInfo.x!=0&&courseLightRange!=0)&&atmosphere.w>0&&emissive==0){
  float3 delta=v.world-lampPosition.xyz;float ahead=dot(delta,lampDirection.xyz);
  float lateral=dot(delta,float3(lampDirection.z,0,-lampDirection.x));
  float beam=(1-smoothstep(.55,1,abs(lateral)/(2+max(ahead,0)*.13)))*smoothstep(0,3,ahead)*(1-smoothstep(70,155,ahead));
  float vertical=1-smoothstep(2.0,11.0,abs(delta.y+ahead*.018));
  float illumination=beam*vertical*lampPosition.w;
  if(opponentPosition.w>0){
   delta=v.world-opponentPosition.xyz;ahead=dot(delta,opponentDirection.xyz);
   lateral=dot(delta,float3(opponentDirection.z,0,-opponentDirection.x));
   beam=(1-smoothstep(.55,1,abs(lateral)/(2+max(ahead,0)*.13)))*smoothstep(0,3,ahead)*(1-smoothstep(70,155,ahead));
   vertical=1-smoothstep(2.0,11.0,abs(delta.y+ahead*.018));
   illumination=max(illumination,beam*vertical);
  }
  float3 streetLight=0;
  // Evaluate at the shaded surface, so road, barriers and moving car bodies
  // brighten under each fixture. Downward emission avoids lighting ceilings
  // above the bulb. Eight nearby lights bound the per-pixel cost; smooth finite
  // radius falloff avoids a hard pool edge. This is native presentation light,
  // not a claim of original ELAN attenuation or shadow-map parity.
  [unroll]for(uint i=0;i<8;++i){
   float3 toLamp=courseLamps[i].xyz-v.world;
   float d2=dot(toLamp,toLamp),invD=rsqrt(max(d2,1e-5));
   float3 direction=toLamp*invD;
   float edge=saturate(1-d2*courseLamps[i].w);
   float down=smoothstep(.05,.40,direction.y);
   float diffuse=.12+.88*saturate(dot(normal,direction));
   float strength=courseLamps[i].w>0?1.6*edge*edge*down*diffuse/(1+.018*d2):0;
   streetLight+=float3(1,.91,.76)*strength;
  }
  color.rgb*=min(float3(.22,.25,.34)+illumination*float3(.94,.86,.70)+streetLight,float3(1.30,1.24,1.16));
 }
 if(sourceVertexFogColorEnabled.w!=0&&original!=0){
  uint fogMode=(tsp>>22)&3;
  if(fogMode==0)color.rgb=lerp(color.rgb,sourceFogColorDensity.rgb,sourceFogCoefficient(v.reciprocalDepth));
  else if(fogMode==1&&(pcw&4)!=0)color.rgb=lerp(color.rgb,sourceVertexFogColorEnabled.rgb,v.offsetColor.a);
  return color;
 }
 // Development geometry and attract owners without a recovered fog setup
 // retain their native atmosphere; this is not used for source race meshes.
 float dist=length(v.world-eye.xyz);float fog=saturate((dist-85)/410);fog=fog*fog;
 if(showroomLight.w!=0 || (original!=0 && ((tsp>>22)&3)==2))fog=0;
 return float4(lerp(color.rgb,atmosphere.rgb,fog),color.a);
}
struct H{float4 p:SV_POSITION;float2 uv:TEXCOORD0;};
H hudVS(uint id:SV_VertexID){H o;float2 p=float2((id<<1)&2,id&2);o.uv=p;o.p=float4(p*float2(2,-2)+float2(-1,1),0,1);return o;}
Texture2D hud:register(t0);SamplerState smp:register(s0);
float4 hudPS(H v):SV_TARGET{return hud.Sample(smp,v.uv);}
cbuffer Fade:register(b2){float4 fadeColor;};
float4 fadePS(H v):SV_TARGET{return fadeColor;}
)HLSL";
    auto compile=[&](const char* entry,const char* profile,Ptr<ID3DBlob>& blob){Ptr<ID3DBlob> errors;HRESULT res=D3DCompile(shader,std::strlen(shader),"remake.hlsl",nullptr,nullptr,entry,profile,D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&blob,&errors);if(FAILED(res)){error=errors?std::string(static_cast<char*>(errors->GetBufferPointer()),errors->GetBufferSize()):"Shader compilation failed";return false;}return true;};
    Ptr<ID3DBlob> vb,pb,hvb,hpb,fadeBlob,showroomBlob;
    if(!compile("mainVS","vs_5_0",vb)||!compile("mainPS","ps_5_0",pb)||!compile("hudVS","vs_5_0",hvb)||!compile("hudPS","ps_5_0",hpb)||!compile("showroomGeometry","gs_5_0",showroomBlob))return false;
    if(FAILED(hr=device->CreateVertexShader(vb->GetBufferPointer(),vb->GetBufferSize(),nullptr,&vs)))return fail(hr,"world vertex shader");
    if(FAILED(hr=device->CreatePixelShader(pb->GetBufferPointer(),pb->GetBufferSize(),nullptr,&ps)))return fail(hr,"world pixel shader");
    if(FAILED(hr=device->CreateGeometryShader(showroomBlob->GetBufferPointer(),showroomBlob->GetBufferSize(),nullptr,&showroomGS)))return fail(hr,"showroom flat shading");
    if(FAILED(hr=device->CreateVertexShader(hvb->GetBufferPointer(),hvb->GetBufferSize(),nullptr,&hudVS)))return fail(hr,"HUD vertex shader");
    if(FAILED(hr=device->CreatePixelShader(hpb->GetBufferPointer(),hpb->GetBufferSize(),nullptr,&hudPS)))return fail(hr,"HUD pixel shader");
    if(!compile("fadePS","ps_5_0",fadeBlob))return false;
    if(FAILED(hr=device->CreatePixelShader(fadeBlob->GetBufferPointer(),fadeBlob->GetBufferSize(),nullptr,&fadePS)))return fail(hr,"screen fade shader");
    D3D11_INPUT_ELEMENT_DESC il[]={
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,40,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"COLOR",1,DXGI_FORMAT_R32G32B32A32_FLOAT,0,48,D3D11_INPUT_PER_VERTEX_DATA,0}};
    static_assert(sizeof(Vertex)==64);
    if(FAILED(hr=device->CreateInputLayout(il,5,vb->GetBufferPointer(),vb->GetBufferSize(),&layout)))return fail(hr,"vertex layout");
    D3D11_BUFFER_DESC cb{};cb.ByteWidth=368;cb.Usage=D3D11_USAGE_DEFAULT;cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    if(FAILED(hr=device->CreateBuffer(&cb,nullptr,&constants)))return fail(hr,"frame constants");
    cb.ByteWidth=48;
    if(FAILED(hr=device->CreateBuffer(&cb,nullptr,&materialConstants)))return fail(hr,"material constants");
    cb.ByteWidth=16;
    if(FAILED(hr=device->CreateBuffer(&cb,nullptr,&fadeConstants)))return fail(hr,"screen fade constants");
    cb.ByteWidth=544;
    if(FAILED(hr=device->CreateBuffer(&cb,nullptr,&fogConstants)))return fail(hr,"course fog constants");
    cb.ByteWidth=624;
    if(FAILED(hr=device->CreateBuffer(&cb,nullptr,&courseLightConstants)))return fail(hr,"course light constants");
    D3D11_RASTERIZER_DESC rs{};rs.FillMode=D3D11_FILL_SOLID;rs.CullMode=D3D11_CULL_NONE;rs.DepthClipEnable=TRUE;
    if(FAILED(hr=device->CreateRasterizerState(&rs,&raster)))return fail(hr,"raster state");
    D3D11_DEPTH_STENCIL_DESC ds{};ds.DepthEnable=FALSE;
    if(FAILED(hr=device->CreateDepthStencilState(&ds,&noDepth)))return fail(hr,"HUD depth state");
    D3D11_BLEND_DESC bs{};auto& b=bs.RenderTarget[0];b.BlendEnable=TRUE;b.SrcBlend=D3D11_BLEND_SRC_ALPHA;b.DestBlend=D3D11_BLEND_INV_SRC_ALPHA;b.BlendOp=D3D11_BLEND_OP_ADD;b.SrcBlendAlpha=D3D11_BLEND_ONE;b.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;b.BlendOpAlpha=D3D11_BLEND_OP_ADD;b.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
    if(FAILED(hr=device->CreateBlendState(&bs,&blend)))return fail(hr,"HUD blend state");
    b.SrcBlend=D3D11_BLEND_ONE;b.DestBlend=D3D11_BLEND_ONE;
    b.SrcBlendAlpha=D3D11_BLEND_ZERO;b.DestBlendAlpha=D3D11_BLEND_ONE;
    b.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_RED|D3D11_COLOR_WRITE_ENABLE_GREEN|D3D11_COLOR_WRITE_ENABLE_BLUE;
    if(FAILED(hr=device->CreateBlendState(&bs,&additiveOverlayBlend)))return fail(hr,"additive HUD blend state");
    D3D11_SAMPLER_DESC ss{};ss.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;ss.AddressU=ss.AddressV=ss.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;ss.MaxLOD=D3D11_FLOAT32_MAX;
    if(FAILED(hr=device->CreateSamplerState(&ss,&sampler)))return fail(hr,"HUD sampler");
    ss.AddressU=ss.AddressV=ss.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;
    if(FAILED(hr=device->CreateSamplerState(&ss,&modelSampler)))return fail(hr,"model sampler");
    D3D11_TEXTURE2D_DESC white{};white.Width=white.Height=white.MipLevels=white.ArraySize=1;white.Format=DXGI_FORMAT_B8G8R8A8_UNORM;white.SampleDesc.Count=1;white.Usage=D3D11_USAGE_IMMUTABLE;white.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    std::uint32_t whitePixel=0xffffffff;D3D11_SUBRESOURCE_DATA wd{&whitePixel,4,0};Ptr<ID3D11Texture2D> wt;
    if(FAILED(hr=device->CreateTexture2D(&white,&wd,&wt)))return fail(hr,"white texture");
    if(FAILED(hr=device->CreateShaderResourceView(wt.Get(),nullptr,&whiteTexture)))return fail(hr,"white texture view");
    return resize(w,h);
}
bool Renderer::loadTextures(const NativeTextureBank& bank,bool append){
    if(capturedScene){try{capturedScene->loadTextures(bank,append);return true;}catch(const std::exception& e){error=e.what();return false;}}
    if(!device){error="Renderer is not initialized";return false;}std::vector<Ptr<ID3D11ShaderResourceView>> next;next.reserve(bank.size());
    for(std::uint32_t i=0;i<bank.size();i++){const auto& image=bank.at(i);D3D11_TEXTURE2D_DESC d{};d.Width=image.width;d.Height=image.height;d.MipLevels=0;d.ArraySize=1;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;d.MiscFlags=D3D11_RESOURCE_MISC_GENERATE_MIPS;
        Ptr<ID3D11Texture2D> texture;Ptr<ID3D11ShaderResourceView> view;HRESULT hr;
        if(FAILED(hr=device->CreateTexture2D(&d,nullptr,&texture)))return fail(hr,"original texture import");
        context->UpdateSubresource(texture.Get(),0,nullptr,image.argb.data(),image.width*4,0);
        if(FAILED(hr=device->CreateShaderResourceView(texture.Get(),nullptr,&view)))return fail(hr,"original texture view");
        context->GenerateMips(view.Get());next.push_back(std::move(view));
    }if(!submitCommands())return false;
    if(append){for(auto& texture:next)modelTextures.push_back(std::move(texture));}else modelTextures=std::move(next);return true;
}
bool Renderer::resize(int w,int h){
    if(capturedScene){if(w<=0||h<=0||w>8192||h>8192){error="Invalid scene output dimensions";return false;}width=w;height=h;return true;}
    if(!device||w<=0||h<=0||w>7680||h>4320)return false;
    context->OMSetRenderTargets(0,nullptr,nullptr);ID3D11ShaderResourceView* none=nullptr;context->PSSetShaderResources(0,1,&none);
    target.Reset();rtv.Reset();depth.Reset();dsv.Reset();hudTexture.Reset();hudView.Reset();
    width=w;height=h;HRESULT hr;
    if(swapchain){if(FAILED(hr=swapchain->ResizeBuffers(0,w,h,DXGI_FORMAT_UNKNOWN,0)))return fail(hr,"swapchain resize");if(FAILED(hr=swapchain->GetBuffer(0,IID_PPV_ARGS(&target))))return fail(hr,"backbuffer");}
    else{D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=1;d.ArraySize=1;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.SampleDesc.Count=1;d.BindFlags=D3D11_BIND_RENDER_TARGET|(sharedDevice?D3D11_BIND_SHADER_RESOURCE:0);if(FAILED(hr=device->CreateTexture2D(&d,nullptr,&target)))return fail(hr,"offscreen target");}
    if(FAILED(hr=device->CreateRenderTargetView(target.Get(),nullptr,&rtv)))return fail(hr,"render target view");
    D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=1;d.ArraySize=1;d.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;d.SampleDesc.Count=1;d.BindFlags=D3D11_BIND_DEPTH_STENCIL;
    if(FAILED(hr=device->CreateTexture2D(&d,nullptr,&depth)))return fail(hr,"depth texture");if(FAILED(hr=device->CreateDepthStencilView(depth.Get(),nullptr,&dsv)))return fail(hr,"depth view");
    d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;d.Usage=D3D11_USAGE_DYNAMIC;d.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
    if(FAILED(hr=device->CreateTexture2D(&d,nullptr,&hudTexture)))return fail(hr,"HUD texture");if(FAILED(hr=device->CreateShaderResourceView(hudTexture.Get(),nullptr,&hudView)))return fail(hr,"HUD view");
    if(!submitCommands())return false;++textureGeneration;return true;
}
bool Renderer::draw(const Mesh& mesh,Vec3 eye,Vec3 look,bool night,bool wet,const std::uint32_t* overlay,bool overlayBehindScene,const OriginalShowroomLighting* showroomLighting,const OriginalRearViewFrame* rearView,std::span<const OverlayPass> foreground){
    if(capturedScene){try{capturedScene->capture(*this,mesh,eye,look,night,wet,overlay,overlayBehindScene,showroomLighting,rearView,foreground);return true;}catch(const std::exception& e){error=e.what();return false;}}
    using namespace DirectX;HRESULT hr;
    if(!context||!rtv||!dsv){error="Renderer target is not initialized";return false;}
    // Drop a previously failed, unsubmitted draw and begin from default state.
    // This affects only our private context, never Unity's bound pipeline.
    if(sharedDevice){discardCommands();context->ClearState();}
    if(measureGpuFrame){
        if(!gpuDisjoint){
            D3D11_QUERY_DESC query{D3D11_QUERY_TIMESTAMP_DISJOINT,0};
            if(FAILED(hr=device->CreateQuery(&query,&gpuDisjoint)))return fail(hr,"GPU timing frequency");
            query.Query=D3D11_QUERY_TIMESTAMP;
            if(FAILED(hr=device->CreateQuery(&query,&gpuStart)))return fail(hr,"GPU timing start");
            if(FAILED(hr=device->CreateQuery(&query,&gpuEnd)))return fail(hr,"GPU timing end");
        }
        context->Begin(gpuDisjoint.Get());context->End(gpuStart.Get());
    }
    context->GSSetShader(nullptr,nullptr,0);
    struct FogConstants{XMFLOAT4 colorDensity,vertexColorEnabled;std::array<XMUINT4,32> table;}fogFrame{};
    static_assert(sizeof(FogConstants)==544);
    if(courseFog&&!showroomLighting){
        const auto rgb=courseFog->colorRgb;
        fogFrame.colorDensity={float((rgb>>16)&255)/255,float((rgb>>8)&255)/255,float(rgb&255)/255,original::originalFogDecodedDensity(courseFog->packedDensity)};
        fogFrame.vertexColorEnabled={originalVertexFogColor.r,originalVertexFogColor.g,originalVertexFogColor.b,1};
        for(unsigned i=0;i<32;++i)fogFrame.table[i]={courseFog->table[i*4],courseFog->table[i*4+1],courseFog->table[i*4+2],courseFog->table[i*4+3]};
    }
    context->UpdateSubresource(fogConstants.Get(),0,nullptr,&fogFrame,0,0);
    context->PSSetConstantBuffers(3,1,fogConstants.GetAddressOf());
    float sky[4]={night?.025f:.34f,night?.045f:.43f,night?.075f:.50f,1};if(wet&&!night){sky[0]=.25f;sky[1]=.31f;sky[2]=.36f;}
    if(overrideClearColor){sky[0]=clearColor.r;sky[1]=clearColor.g;sky[2]=clearColor.b;sky[3]=clearColor.a;}
    context->ClearRenderTargetView(rtv.Get(),sky);context->ClearDepthStencilView(dsv.Get(),D3D11_CLEAR_DEPTH,1,0);
    context->OMSetRenderTargets(1,rtv.GetAddressOf(),dsv.Get());context->OMSetDepthStencilState(nullptr,0);context->OMSetBlendState(nullptr,nullptr,~0u);
    D3D11_VIEWPORT vp{0,0,float(width),float(height),0,1};context->RSSetViewports(1,&vp);context->RSSetState(raster.Get());
    if(overlay&&overlayBehindScene){
        if(!drawOverlay(overlay))return false;
        context->OMSetDepthStencilState(nullptr,0);context->OMSetBlendState(nullptr,nullptr,~0u);
    }
    if((overlay&&overlayBehindScene)||fitOriginalViewport||sceneViewport.width>0){
        const float fit=std::min(float(width)/640.f,float(height)/480.f);
        const float originX=(float(width)-640.f*fit)*.5f,originY=(float(height)-480.f*fit)*.5f;
        vp=sceneViewport.width>0&&sceneViewport.height>0
            ?D3D11_VIEWPORT{originX+sceneViewport.x*fit,originY+sceneViewport.y*fit,
                            sceneViewport.width*fit,sceneViewport.height*fit,0,1}
            :D3D11_VIEWPORT{originX,originY,640.f*fit,480.f*fit,0,1};
        context->RSSetViewports(1,&vp);
    }
    struct Frame{XMFLOAT4X4 matrix;XMFLOAT4 eye,sky,lampPosition,lampDirection,cameraRight,cameraUp,showroomLight,showroomTerms,opponentPosition,opponentDirection;std::array<XMFLOAT4,8> courseLamps;XMFLOAT4 sceneLightColor;}frame{};
    static_assert(sizeof(Frame)==368);
    if(night&&!showroomLighting&&courseLampLighting){
        std::array<std::pair<float,Vec3>,8> nearest;
        for(auto& slot:nearest)slot.first=std::numeric_limits<float>::max();
        for(const auto& position:courseLampPositions){
            const auto delta=position-eye;const float distance=dot(delta,delta);
            for(std::size_t i=0;i<nearest.size();++i)if(distance<nearest[i].first){
                for(std::size_t j=nearest.size()-1;j>i;--j)nearest[j]=nearest[j-1];
                nearest[i]={distance,position};break;
            }
        }
        for(std::size_t i=0;i<nearest.size();++i)if(nearest[i].first<std::numeric_limits<float>::max()){
            const auto p=nearest[i].second;frame.courseLamps[i]={p.x,p.y,p.z,1.f/(26.f*26.f)};
        }
    }
    frame.showroomLight={0,0,0,0};frame.showroomTerms={0,0,0,0};
    if(showroomLighting){const auto& setup=showroomLighting->parameters;const auto light=setup.directionToLight;
        frame.showroomLight={light.x,light.y,light.z,1};
        frame.showroomTerms={setup.ambientBase,setup.diffuseSpecularFactor,0,0};
        frame.sceneLightColor={setup.color.x,setup.color.y,setup.color.z,1};}
    const auto viewForward=normalized(look-eye),viewRight=normalized(cross(viewForward,cameraUp)),viewUp=normalized(cross(viewRight,viewForward));
    frame.lampPosition={vehiclePosition.x,vehiclePosition.y+.6f,vehiclePosition.z,vehicleLights?1.f:0.f};frame.lampDirection={vehicleForward.x,vehicleForward.y,vehicleForward.z,0};
    frame.opponentPosition={opponentPosition.x,opponentPosition.y+.6f,opponentPosition.z,opponentLights?1.f:0.f};
    frame.opponentDirection={opponentForward.x,opponentForward.y,opponentForward.z,0};
    frame.cameraRight={viewRight.x,viewRight.y,viewRight.z,0};frame.cameraUp={viewUp.x,viewUp.y,viewUp.z,0};
    // ELAN normal view uses negative camera Z and clip.w=-z. The LH variant
    // belongs to the original rear-view mirror, not the forward chase camera.
    XMMATRIX view=XMMatrixLookAtRH(XMVectorSet(eye.x,eye.y,eye.z,1),XMVectorSet(look.x,look.y,look.z,1),XMVectorSet(cameraUp.x,cameraUp.y,cameraUp.z,0));
    struct CourseLightConstants{XMFLOAT4X4 view;std::array<XMUINT4,2> glm;std::array<XMUINT4,32> lights;XMUINT4 info;};
    // Snapshot all three scopes once per view. Range changes select the
    // correct packet without rebuilding light descriptors for each material.
    std::array<CourseLightConstants,4> lightFrames{};
    const auto prepareSourceLighting=[&](const XMMATRIX& lightView){
        static_assert(sizeof(CourseLightConstants)==624);
        const std::array<const original::OriginalCourseLighting*,4> scopes{nullptr,courseLighting,playerLighting,rivalLighting};
        for(unsigned scope=0;scope<scopes.size();++scope){
            auto& lightFrame=lightFrames[scope];lightFrame={};
            XMStoreFloat4x4(&lightFrame.view,lightView);
            if(!scopes[scope]||showroomLighting)continue;
            original::OriginalLightMatrix transform;
            std::memcpy(transform.data(),&lightFrame.view,sizeof(lightFrame.view));
            const auto packet=original::originalCourseLightingPacket(*scopes[scope],transform);
            std::memcpy(lightFrame.glm.data(),packet.glm.data(),sizeof(lightFrame.glm));
            static_assert(sizeof(packet.lights)<=sizeof(lightFrame.lights));
            std::memcpy(lightFrame.lights.data(),packet.lights.data(),sizeof(packet.lights));
            lightFrame.info={1,packet.count,0,0};
        }
        context->VSSetConstantBuffers(4,1,courseLightConstants.GetAddressOf());
        context->PSSetConstantBuffers(4,1,courseLightConstants.GetAddressOf());
    };
    prepareSourceLighting(view);
    // Original Akina background radius is1974 with world-authored height;
    // retain its geometry and admit it through the native far plane.
    XMStoreFloat4x4(&frame.matrix,view*XMMatrixPerspectiveFovRH(verticalFieldOfView,projectionAspect>0?projectionAspect:vp.Width/vp.Height,nearClip,farClip));frame.eye={eye.x,eye.y,eye.z,1};frame.sky={sky[0],sky[1],sky[2],night?1.f:0.f};
    context->UpdateSubresource(constants.Get(),0,nullptr,&frame,0,0);context->VSSetConstantBuffers(0,1,constants.GetAddressOf());context->PSSetConstantBuffers(0,1,constants.GetAddressOf());
    if(!mesh.vertices.empty()){
        if(mesh.vertices.size()>2000000){error="Geometry budget exceeded";return false;}
        if(mesh.vertices.size()>capacity){capacity=mesh.vertices.size()+8192;vertexBuffer.Reset();D3D11_BUFFER_DESC d{};d.ByteWidth=UINT(capacity*sizeof(Vertex));d.Usage=D3D11_USAGE_DYNAMIC;d.BindFlags=D3D11_BIND_VERTEX_BUFFER;d.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;if(FAILED(hr=device->CreateBuffer(&d,nullptr,&vertexBuffer)))return fail(hr,"geometry buffer");}
        D3D11_MAPPED_SUBRESOURCE map{};if(FAILED(hr=context->Map(vertexBuffer.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&map)))return fail(hr,"map geometry");std::memcpy(map.pData,mesh.vertices.data(),mesh.vertices.size()*sizeof(Vertex));context->Unmap(vertexBuffer.Get(),0);
        const auto drawRanges=[&](const OriginalRearViewFrame* rear=nullptr)->bool{
        UINT stride=sizeof(Vertex),offset=0;context->IASetVertexBuffers(0,1,vertexBuffer.GetAddressOf(),&stride,&offset);context->IASetInputLayout(layout.Get());context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context->VSSetShader(vs.Get(),nullptr,0);context->PSSetShader(ps.Get(),nullptr,0);context->PSSetSamplers(0,1,modelSampler.GetAddressOf());
        context->PSSetConstantBuffers(1,1,materialConstants.GetAddressOf());
        context->VSSetConstantBuffers(1,1,materialConstants.GetAddressOf());
        bool flatStage=showroomLighting!=nullptr;
        context->GSSetShader(flatStage?showroomGS.Get():nullptr,nullptr,0);
        if(showroomLighting||courseLighting||playerLighting||rivalLighting)context->GSSetConstantBuffers(1,1,materialConstants.GetAddressOf());
        unsigned currentLightScope=~0u;
        // Original list classes have independent submission queues. Resolve
        // opaque, then punch-through, then translucent material ranges.
        for(unsigned list:{0u,4u,2u})for(const auto& range:mesh.ranges){
            if((range.viewMask&(rear?2u:1u))==0)continue;
            if((range.original?((range.pcw>>24)&7u):0u)!=list)continue;
            if(std::size_t(range.first)+range.count>mesh.vertices.size()){error="Invalid mesh draw range";return false;}
            if(range.carLighting>2){error="Invalid car light scope";return false;}
            const unsigned lightScope=range.carLighting?range.carLighting+1:range.courseLighting?1u:0u;
            if(lightScope!=currentLightScope){
                context->UpdateSubresource(courseLightConstants.Get(),0,nullptr,&lightFrames[lightScope],0,0);
                currentLightScope=lightScope;
            }
            const bool needsFlat=showroomLighting||(lightFrames[lightScope].info.x!=0&&range.original&&(range.pcw&2)==0);
            if(needsFlat!=flatStage){context->GSSetShader(needsFlat?showroomGS.Get():nullptr,nullptr,0);flatStage=needsFlat;}
            if(showroomLighting){
                const auto base=showroomLighting->parameters.directionToLight;
                const auto direction=range.originalLightDirection.value_or(std::array{base.x,base.y,base.z});
                if(direction[0]!=frame.showroomLight.x||direction[1]!=frame.showroomLight.y||direction[2]!=frame.showroomLight.z){
                    frame.showroomLight={direction[0],direction[1],direction[2],1};
                    context->UpdateSubresource(constants.Get(),0,nullptr,&frame,0,0);
                }
            }
            if(rear&&range.count){
                // Conservative world AABB culling avoids resubmitting road
                // and scenery ranges wholly outside the small rear frustum.
                auto lo=range.billboard?mesh.vertices[range.first].normal:mesh.vertices[range.first].position,hi=lo;
                for(unsigned i=0;i<range.count;++i){const auto& vertex=mesh.vertices[range.first+i];
                    const auto p=range.billboard?vertex.normal:vertex.position;
                    const auto local=(range.billboard&&!(range.gmp&512))?Vec3{vertex.position.x,vertex.position.y,0}:vertex.position;
                    const float radius=range.billboard?length(local):0;
                    lo={std::min(lo.x,p.x-radius),std::min(lo.y,p.y-radius),std::min(lo.z,p.z-radius)};
                    hi={std::max(hi.x,p.x+radius),std::max(hi.y,p.y+radius),std::max(hi.z,p.z+radius)};}
                const auto center=(lo+hi)*.5f-rear->eye,extent=(hi-lo)*.5f;
                const auto f=normalized(rear->target-rear->eye),r=normalized(cross(rear->up,f)),u=normalized(cross(f,r));
                const float ty=std::tan(rear->verticalFieldOfView*.5f),tx=ty*5.f;
                bool outside=false;
                for(auto plane:{f,f*tx+r,f*tx-r,f*ty+u,f*ty-u}){
                    const float radius=std::abs(plane.x)*extent.x+std::abs(plane.y)*extent.y+std::abs(plane.z)*extent.z;
                    if(dot(plane,center)+radius<-.01f){outside=true;break;}
                }
                if(outside)continue;
            }
            struct Material {std::uint32_t pcw,tsp,gmp,original;float alphaReference;std::uint32_t emissive;float glossCoefficient;std::uint32_t billboard,courseLightRange;std::array<std::uint32_t,3> padding;};
            static_assert(sizeof(Material)==48);
            Material material{range.pcw,range.tsp,range.gmp,range.original?1u:0u,float(originalAlphaReference),range.emissive?1u:0u,OriginalShowroomLighting::glossCoefficient(range.gloss),range.billboard?1u:0u,lightScope?1u:0u,{}};
            context->UpdateSubresource(materialConstants.Get(),0,nullptr,&material,0,0);
            if(range.original){
                const auto tsp=range.tsp;
                // Source mip0 is preserved; native generated mipmaps and
                // anisotropic filtering reduce road shimmer during movement.
                unsigned samplerIndex=((tsp>>16)&1)|(((tsp>>15)&1)<<1)|(((tsp>>18)&1)<<2)|(((tsp>>17)&1)<<3)|((((tsp>>13)&3)!=0)<<4);
                auto& state=originalSamplers[samplerIndex];
                // The TSP filter field selects point sampling for some materials.
                // That is stable at the cabinet 640x480; above it the minified
                // texels crawl as the camera moves. Keep point magnification and
                // filter the minification only.
                if(!state){D3D11_SAMPLER_DESC desc{};desc.Filter=(samplerIndex&16)?D3D11_FILTER_ANISOTROPIC:
                    height>480?D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR:D3D11_FILTER_MIN_MAG_MIP_POINT;desc.MaxAnisotropy=8;
                    desc.AddressU=(samplerIndex&1)?D3D11_TEXTURE_ADDRESS_CLAMP:(samplerIndex&4)?D3D11_TEXTURE_ADDRESS_MIRROR:D3D11_TEXTURE_ADDRESS_WRAP;
                    desc.AddressV=(samplerIndex&2)?D3D11_TEXTURE_ADDRESS_CLAMP:(samplerIndex&8)?D3D11_TEXTURE_ADDRESS_MIRROR:D3D11_TEXTURE_ADDRESS_WRAP;
                    desc.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;desc.MaxLOD=D3D11_FLOAT32_MAX;
                    if(FAILED(hr=device->CreateSamplerState(&desc,&state)))return fail(hr,"original material sampler");}
                context->PSSetSamplers(0,1,state.GetAddressOf());
                {
                    unsigned src=(tsp>>29)&7,dst=(tsp>>26)&7;auto& blendState=originalBlends[src*8+dst];
                    if(!blendState){
                        auto factor=[](unsigned code,bool source,bool alpha){switch(code){
                            case 0:return D3D11_BLEND_ZERO;case 1:return D3D11_BLEND_ONE;
                            case 2:return source?(alpha?D3D11_BLEND_DEST_ALPHA:D3D11_BLEND_DEST_COLOR):(alpha?D3D11_BLEND_SRC_ALPHA:D3D11_BLEND_SRC_COLOR);
                            case 3:return source?(alpha?D3D11_BLEND_INV_DEST_ALPHA:D3D11_BLEND_INV_DEST_COLOR):(alpha?D3D11_BLEND_INV_SRC_ALPHA:D3D11_BLEND_INV_SRC_COLOR);
                            case 4:return D3D11_BLEND_SRC_ALPHA;case 5:return D3D11_BLEND_INV_SRC_ALPHA;
                            case 6:return D3D11_BLEND_DEST_ALPHA;default:return D3D11_BLEND_INV_DEST_ALPHA;}};
                        D3D11_BLEND_DESC desc{};auto& b=desc.RenderTarget[0];b.BlendEnable=TRUE;b.SrcBlend=factor(src,true,false);b.DestBlend=factor(dst,false,false);
                        b.SrcBlendAlpha=factor(src,true,true);b.DestBlendAlpha=factor(dst,false,true);b.BlendOp=b.BlendOpAlpha=D3D11_BLEND_OP_ADD;b.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
                        if(FAILED(hr=device->CreateBlendState(&desc,&blendState)))return fail(hr,"original material blend");}
                    context->OMSetBlendState(blendState.Get(),nullptr,~0u);
                }
                // The original hardware auto-sorts the translucent list per pixel:
                // it compares greater-or-equal and does not write depth. Punch-through
                // is not auto-sorted -- it keeps the ISP's own comparison and its
                // depth write. Giving it greater-or-equal lets coplanar fragments
                // overwrite by submission order, which is what made signs flicker
                // under motion and let the road show through foliage.
                unsigned compare=list==2?6u:(range.isp>>29)&7,
                    write=list==2?0u:list==4?1u:unsigned(((range.isp>>26)&1)==0);
                auto& depthState=originalDepth[compare*2+write];
                if(!depthState){
                    // Original depth is reciprocal-Z (near is greater), while
                    // this native projection uses increasing far depth.
                    constexpr D3D11_COMPARISON_FUNC reversed[]={D3D11_COMPARISON_NEVER,D3D11_COMPARISON_GREATER,D3D11_COMPARISON_EQUAL,D3D11_COMPARISON_GREATER_EQUAL,D3D11_COMPARISON_LESS,D3D11_COMPARISON_NOT_EQUAL,D3D11_COMPARISON_LESS_EQUAL,D3D11_COMPARISON_ALWAYS};
                    D3D11_DEPTH_STENCIL_DESC desc{};desc.DepthEnable=TRUE;desc.DepthWriteMask=write?D3D11_DEPTH_WRITE_MASK_ALL:D3D11_DEPTH_WRITE_MASK_ZERO;desc.DepthFunc=reversed[compare];
                    if(FAILED(hr=device->CreateDepthStencilState(&desc,&depthState)))return fail(hr,"original material depth");}
                context->OMSetDepthStencilState(depthState.Get(),0);
            }else{
                context->PSSetSamplers(0,1,modelSampler.GetAddressOf());context->OMSetBlendState(nullptr,nullptr,~0u);context->OMSetDepthStencilState(nullptr,0);
            }
            auto* tex=range.texture<modelTextures.size()?modelTextures[range.texture].Get():whiteTexture.Get();
            context->PSSetShaderResources(0,1,&tex);context->Draw(range.count,range.first);
        }
        return true;
        };
        if(!drawRanges())return false;
        if(rearView&&!showroomLighting){
            const float fit=std::min(float(width)/640.f,float(height)/480.f);
            const float ox=(float(width)-640.f*fit)*.5f,oy=(float(height)-480.f*fit)*.5f;
            const D3D11_VIEWPORT mirrorViewport{ox+rearView->left*fit,oy+rearView->top*fit,rearView->width*fit,rearView->height*fit,0,1};
            context->RSSetViewports(1,&mirrorViewport);
            // Clear only the small color rectangle with a viewport triangle.
            // Main-view depth is no longer needed after its scene pass.
            context->ClearDepthStencilView(dsv.Get(),D3D11_CLEAR_DEPTH,1,0);
            context->UpdateSubresource(fadeConstants.Get(),0,nullptr,sky,0,0);
            context->GSSetShader(nullptr,nullptr,0);context->OMSetDepthStencilState(noDepth.Get(),0);context->OMSetBlendState(nullptr,nullptr,~0u);
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context->IASetInputLayout(nullptr);
            context->VSSetShader(hudVS.Get(),nullptr,0);context->PSSetShader(fadePS.Get(),nullptr,0);
            context->PSSetConstantBuffers(2,1,fadeConstants.GetAddressOf());context->Draw(3,0);
            const auto e=rearView->eye,t=rearView->target,u=rearView->up;
            const auto f=normalized(t-e),r=normalized(cross(u,f)),up=normalized(cross(f,r));
            const auto rearMatrix=XMMatrixLookAtLH(XMVectorSet(e.x,e.y,e.z,1),XMVectorSet(t.x,t.y,t.z,1),XMVectorSet(u.x,u.y,u.z,0));
            prepareSourceLighting(rearMatrix);
            XMStoreFloat4x4(&frame.matrix,rearMatrix*XMMatrixPerspectiveFovLH(rearView->verticalFieldOfView,5.f,.2f,5000.f));
            frame.eye={e.x,e.y,e.z,1};frame.cameraRight={r.x,r.y,r.z,0};frame.cameraUp={up.x,up.y,up.z,0};
            context->UpdateSubresource(constants.Get(),0,nullptr,&frame,0,0);
            // Reuse the same uploaded geometry. No readback or second mesh build.
            if(!drawRanges(rearView))return false;
            context->RSSetViewports(1,&vp);
        }
    }
    if(overlay&&!overlayBehindScene&&!drawOverlay(overlay))return false;
    for(const auto& pass:foreground)if(pass.pixels&&!drawOverlay(pass.pixels,pass.additive,pass.originalCanvas))return false;
    if(screenFadeArgb&0xff000000u)drawScreenFade();
    if(measureGpuFrame){context->End(gpuEnd.Get());context->End(gpuDisjoint.Get());}
    if(!submitCommands())return false;
    if(swapchain){hr=swapchain->Present(1,0);if(FAILED(hr))return fail(hr,"present");}
    return true;
}
bool Renderer::readGpuMilliseconds(double& milliseconds){
    if(!measureGpuFrame||!gpuDisjoint){error="GPU timing was not enabled";return false;}
    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT frequency{};UINT64 start=0,end=0;
    auto* readContext=sharedDevice?hostImmediate.Get():context.Get();
    readContext->Flush();const auto deadline=GetTickCount64()+2000;
    for(;;){
        const auto a=readContext->GetData(gpuDisjoint.Get(),&frequency,sizeof(frequency),D3D11_ASYNC_GETDATA_DONOTFLUSH);
        const auto b=readContext->GetData(gpuStart.Get(),&start,sizeof(start),D3D11_ASYNC_GETDATA_DONOTFLUSH);
        const auto c=readContext->GetData(gpuEnd.Get(),&end,sizeof(end),D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if(FAILED(a)||FAILED(b)||FAILED(c))return fail(FAILED(a)?a:FAILED(b)?b:c,"GPU timing readback");
        if(a==S_OK&&b==S_OK&&c==S_OK)break;
        if(GetTickCount64()>=deadline){error="GPU timing query timed out";return false;}
        SwitchToThread();
    }
    if(frequency.Disjoint||!frequency.Frequency||end<start){error="GPU timing interval is disjoint";return false;}
    milliseconds=double(end-start)*1000.0/double(frequency.Frequency);return true;
}
void Renderer::drawScreenFade(){
    const D3D11_VIEWPORT fullViewport{0,0,float(width),float(height),0,1};
    context->RSSetViewports(1,&fullViewport);
    const float color[]={float((screenFadeArgb>>16)&255)/255.f,float((screenFadeArgb>>8)&255)/255.f,
        float(screenFadeArgb&255)/255.f,float(screenFadeArgb>>24)/255.f};
    context->UpdateSubresource(fadeConstants.Get(),0,nullptr,color,0,0);
    context->GSSetShader(nullptr,nullptr,0);
    context->OMSetDepthStencilState(noDepth.Get(),0);context->OMSetBlendState(blend.Get(),nullptr,~0u);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context->IASetInputLayout(nullptr);
    context->VSSetShader(hudVS.Get(),nullptr,0);context->PSSetShader(fadePS.Get(),nullptr,0);
    context->PSSetConstantBuffers(2,1,fadeConstants.GetAddressOf());context->Draw(3,0);
}
bool Renderer::drawOverlay(const std::uint32_t* overlay,bool additive,bool originalCanvas){
    D3D11_MAPPED_SUBRESOURCE map{};HRESULT hr;
    if(originalCanvas&&!originalHudTexture){
        D3D11_TEXTURE2D_DESC desc{};desc.Width=640;desc.Height=480;desc.MipLevels=desc.ArraySize=1;
        desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DYNAMIC;
        desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;desc.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
        if(FAILED(hr=device->CreateTexture2D(&desc,nullptr,&originalHudTexture)))return fail(hr,"original canvas texture");
        if(FAILED(hr=device->CreateShaderResourceView(originalHudTexture.Get(),nullptr,&originalHudView)))return fail(hr,"original canvas view");
    }
    const int sourceWidth=originalCanvas?640:width,sourceHeight=originalCanvas?480:height;
    const float fit=originalCanvas?std::min(float(width)/640.f,float(height)/480.f):1.f;
    const D3D11_VIEWPORT viewport{(width-sourceWidth*fit)*.5f,(height-sourceHeight*fit)*.5f,sourceWidth*fit,sourceHeight*fit,0,1};
    context->RSSetViewports(1,&viewport);
    context->GSSetShader(nullptr,nullptr,0);
    const auto texture=originalCanvas?originalHudTexture.Get():hudTexture.Get();
    if(FAILED(hr=context->Map(texture,0,D3D11_MAP_WRITE_DISCARD,0,&map)))return fail(hr,"map HUD");
    for(int y=0;y<sourceHeight;y++)std::memcpy(static_cast<char*>(map.pData)+std::size_t(y)*map.RowPitch,overlay+std::size_t(y)*sourceWidth,sourceWidth*4);
    context->Unmap(texture,0);
    context->OMSetDepthStencilState(noDepth.Get(),0);context->OMSetBlendState(additive?additiveOverlayBlend.Get():blend.Get(),nullptr,~0u);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context->IASetInputLayout(nullptr);
    context->VSSetShader(hudVS.Get(),nullptr,0);context->PSSetShader(hudPS.Get(),nullptr,0);
    context->PSSetShaderResources(0,1,originalCanvas?originalHudView.GetAddressOf():hudView.GetAddressOf());context->PSSetSamplers(0,1,sampler.GetAddressOf());
    context->Draw(3,0);ID3D11ShaderResourceView* none=nullptr;context->PSSetShaderResources(0,1,&none);return true;
}
bool Renderer::saveBitmap(const std::wstring& path){
    if(!target){error="Renderer target is not initialized";return false;}
    D3D11_TEXTURE2D_DESC desc{};target->GetDesc(&desc);desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    Ptr<ID3D11Texture2D> stage;HRESULT hr=device->CreateTexture2D(&desc,nullptr,&stage);if(FAILED(hr))return fail(hr,"readback texture");context->CopyResource(stage.Get(),target.Get());if(!submitCommands())return false;
    auto* readContext=sharedDevice?hostImmediate.Get():context.Get();
    D3D11_MAPPED_SUBRESOURCE map{};if(FAILED(hr=readContext->Map(stage.Get(),0,D3D11_MAP_READ,0,&map)))return fail(hr,"readback");
    BITMAPFILEHEADER fh{};BITMAPINFOHEADER ih{};fh.bfType=0x4d42;fh.bfOffBits=sizeof(fh)+sizeof(ih);fh.bfSize=fh.bfOffBits+width*height*4;ih.biSize=sizeof(ih);ih.biWidth=width;ih.biHeight=-height;ih.biPlanes=1;ih.biBitCount=32;ih.biCompression=BI_RGB;
    std::ofstream out(std::filesystem::path(path),std::ios::binary);out.write(reinterpret_cast<char*>(&fh),sizeof(fh));out.write(reinterpret_cast<char*>(&ih),sizeof(ih));for(int y=0;y<height;y++)out.write(static_cast<char*>(map.pData)+std::size_t(y)*map.RowPitch,width*4);readContext->Unmap(stage.Get(),0);return bool(out);
}
#endif
}
