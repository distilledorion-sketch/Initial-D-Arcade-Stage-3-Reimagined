#include "renderer.h"
#include <d3dcompiler.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static void checkScreenFade(){
    using namespace idas3;
    constexpr unsigned width=256,height=144;
    Renderer renderer;
    if(!renderer.initialize(nullptr,width,height,true))throw std::runtime_error("WARP initialization: "+renderer.error);
    Mesh empty,scene;scene.box({0,0,0},{2,2,2},0,{.85f,.25f,.12f});
    std::vector<std::uint32_t> hud(width*height);
    for(unsigned y=8;y<32;++y)for(unsigned x=8;x<56;++x)hud[y*width+x]=0xff20e070;
    const auto path=std::filesystem::temp_directory_path()/("idas3-fade-test-"+std::to_string(GetCurrentProcessId())+".bmp");
    const auto capture=[&](const Mesh& mesh,bool overlay,bool behind,std::span<const OverlayPass> foreground=std::span<const OverlayPass>{}){
        if(!renderer.draw(mesh,{0,0,5},{0,0,0},false,false,overlay?hud.data():nullptr,behind,nullptr,nullptr,foreground))throw std::runtime_error("WARP fade draw: "+renderer.error);
        if(!renderer.saveBitmap(path.wstring()))throw std::runtime_error("WARP fade readback: "+renderer.error);
        std::vector<std::uint32_t> pixels(width*height);
        std::ifstream in(path,std::ios::binary);BITMAPFILEHEADER file{};BITMAPINFOHEADER info{};
        in.read(reinterpret_cast<char*>(&file),sizeof(file));in.read(reinterpret_cast<char*>(&info),sizeof(info));
        require(file.bfType==0x4d42&&info.biWidth==width&&info.biHeight==-int(height)&&info.biBitCount==32,"Unexpected fade readback format");
        in.seekg(file.bfOffBits);in.read(reinterpret_cast<char*>(pixels.data()),std::streamsize(pixels.size()*sizeof(pixels[0])));
        require(bool(in),"Truncated fade readback");in.close();std::filesystem::remove(path);return pixels;
    };
    const auto clear=capture(empty,false,false);
    for(bool behind:{false,true}){
        renderer.screenFadeArgb=0;
        const auto baseline=capture(scene,true,behind);
        require((baseline[16*width+16]&0xffffff)==0x20e070,"HUD test patch missing");
        require(baseline[(height/2)*width+width/2]!=clear[(height/2)*width+width/2],"3D test geometry missing");
        // An alpha-zero colored source must leave both layers byte-identical.
        renderer.screenFadeArgb=0x003e94ff;
        require(capture(scene,true,behind)==baseline,"Zero-alpha fade changed scene/HUD");
        renderer.screenFadeArgb=0x80000000;
        const auto half=capture(scene,true,behind);
        for(std::size_t i=0;i<half.size();++i)for(unsigned shift:{0u,8u,16u}){
            const auto expected=int((((baseline[i]>>shift)&255)*127+127)/255);
            const auto actual=int((half[i]>>shift)&255);
            require(std::abs(actual-expected)<=1,"Half fade must cover all scene/HUD pixels and widescreen margins");
        }
        // Rendering repeatedly while the caller pauses must not advance or
        // clear the fade. Resuming is an explicit caller property change.
        require(renderer.screenFadeArgb==0x80000000,"Renderer consumed caller fade state");
        require(capture(scene,true,behind)==half,"Paused/repeated fade changed pixels");
        renderer.screenFadeArgb=0x803060a0;
        const auto colored=capture(scene,true,behind);
        for(std::size_t i=0;i<colored.size();++i)for(unsigned shift:{0u,8u,16u}){
            const auto source=(0x3060a0u>>shift)&255,destination=(baseline[i]>>shift)&255;
            const auto expected=int((source*128+destination*127+127)/255);
            require(std::abs(int((colored[i]>>shift)&255)-expected)<=1,"Colored fade must use straight source alpha, not premultiplied/additive blending");
        }
        renderer.screenFadeArgb=0xff000000;
        const auto black=capture(scene,true,behind);
        require(std::all_of(black.begin(),black.end(),[](auto p){return (p&0xffffff)==0;}),"Opaque fade must cover the complete viewport including HUD");
        require(renderer.screenFadeArgb==0xff000000,"Renderer reset persistent fade");
        renderer.screenFadeArgb=0;
        require(capture(scene,true,behind)==baseline,"Unfade leaked GPU blend/depth/viewport state");
    }
    // An opaque menu may have no3D geometry. Fade must still be independent of
    // the vertex buffer or previous scene's material state.
    renderer.screenFadeArgb=0xff172b49;
    const auto solid=capture(empty,true,false);
    require(std::all_of(solid.begin(),solid.end(),[](auto p){return (p&0xffffff)==0x172b49;}),"Opaque colored fade failed with empty scene");
    // Name-slot light must add over both scene and backing before the glyph
    // layer. Its carrier alpha is deliberately zero: RGB is preweighted.
    renderer.screenFadeArgb=0;
    const auto underlay=capture(scene,true,true);
    std::vector<std::uint32_t> slots(width*height,0x00102030),glyph(width*height),cursor(width*height,0x00050403);
    for(unsigned y=12;y<28;++y)for(unsigned x=12;x<42;++x)glyph[y*width+x]=0x80406080;
    const std::array<OverlayPass,3> layers{{{slots.data(),true},{glyph.data(),false},{cursor.data(),true}}};
    const auto layered=capture(scene,true,true,layers);
    for(std::size_t i=0;i<layered.size();++i)for(unsigned shift:{0u,8u,16u}){
        const unsigned lit=std::min(255u,((underlay[i]>>shift)&255)+((slots[i]>>shift)&255));
        const unsigned alpha=glyph[i]>>24;
        const unsigned letter=((((glyph[i]>>shift)&255)*alpha+lit*(255-alpha)+127)/255);
        const unsigned expected=std::min(255u,letter+((cursor[i]>>shift)&255));
        require(std::abs(int((layered[i]>>shift)&255)-int(expected))<=1,"Ordered additive/alpha overlays lost the scene, multiplied alpha twice or covered glyphs out of order");
    }
    require(capture(scene,true,true,layers)==layered,"Repeated ordered overlay changed pixels");
    renderer.screenFadeArgb=0xff000000;
    const auto fadedLayers=capture(scene,true,true,layers);
    require(std::all_of(fadedLayers.begin(),fadedLayers.end(),[](auto p){return (p&0xffffff)==0;}),"Source screen fade must cover the additive name UI too");
    renderer.screenFadeArgb=0;
    std::vector<std::uint32_t> originalCanvas(640*480,0xff345678);
    const std::array<OverlayPass,1> originalPass{{{originalCanvas.data(),false,true}}};
    const auto fitted=capture(scene,true,true,originalPass);
    require((fitted[72*width+128]&0xffffff)==0x345678,"Original canvas was not fitted to the screen");
    require(fitted[72*width+16]==underlay[72*width+16]&&fitted[72*width+240]==underlay[72*width+240],"Original canvas stretched into widescreen margins");
    require(capture(scene,true,true)==underlay,"Original canvas leaked viewport/texture state into the next frame");
}
int main(int argc,char**argv){try{
    using namespace idas3;
    require(argc==2,"Project root required");const std::filesystem::path root=argv[1];
    // Compile exactly the shipping HLSL without creating a graphics device.
    std::ifstream source(root/"src/renderer.cpp");std::string text((std::istreambuf_iterator<char>(source)),{});
    auto start=text.find("R\"HLSL(");require(start!=text.npos,"Shader marker missing");start+=7;
    auto end=text.find(")HLSL\"",start);require(end!=text.npos,"Shader end missing");auto shader=text.substr(start,end-start);
    for(auto [entry,profile]:{std::pair{"mainVS","vs_5_0"},{"mainPS","ps_5_0"},{"hudVS","vs_5_0"},{"hudPS","ps_5_0"},{"fadePS","ps_5_0"}}){
        Microsoft::WRL::ComPtr<ID3DBlob> blob,errors;
        auto hr=D3DCompile(shader.data(),shader.size(),"renderer.cpp",nullptr,nullptr,entry,profile,D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&blob,&errors);
        if(FAILED(hr))throw std::runtime_error(errors?std::string(static_cast<char*>(errors->GetBufferPointer()),errors->GetBufferSize()):"Shader compile failed");
    }
    NativeModel model;model.chunks.resize(1);NativeModelBatch batch;
    batch.ich[0]=0x0a00070e;batch.ich[1]=0x93800000;batch.ich[2]=0x94002452;batch.ich[6]=0x4a;
    batch.material[2]=0x202;batch.material[4]=0x40201008;batch.material[9]=7;
    batch.vertices={
        {0,{0,0,0},{0,1,0},0,0,0x80402010,0xffff0000},
        {0,{1,0,0},{0,1,0},1,0,0x80604020,0xffff0000},
        {0,{0,0,1},{0,1,0},0,1,0x80806040,0xffff0000}};
    batch.indices={0,1,2};model.chunks[0].batches.push_back(batch);
    Mesh mesh;mesh.originalWorldChunk(model,0,211);
    require(mesh.ranges.size()==1&&mesh.ranges[0].original,"Original material range missing");
    require(mesh.ranges[0].texture==218&&mesh.ranges[0].gmp==0x202,"Material metadata changed");
    require(mesh.vertices[0].color.r==64.f/255,"Packed diffuse did not survive");
    require(mesh.vertices[0].offsetColor.r==32.f/255&&mesh.vertices[0].offsetColor.b==8.f/255,"Offset must come from GMP, not second-volume diffuse");
    // Same texture/TSP must retain differing offset/Gouraud/lighting controls.
    model.chunks[0].batches[0].ich[0]&=~2u;mesh.originalWorldChunk(model,0,211);
    require(mesh.ranges.size()==2,"Distinct PCW merged into one range");
    for(std::size_t i=3;i<6;++i)require(mesh.vertices[i].color.r==128.f/255,"Flat color must use original last provoking vertex");
    model.chunks[0].batches[0].material[2]|=1;model.chunks[0].batches[0].material[3]=0xff112233;
    mesh.originalWorldChunk(model,0,211);
    require(mesh.vertices[6].color.g==34.f/255,"GMP diffuse override ignored");
    mesh.triangle({0,0,0},{1,0,0},{0,0,1},{1,0,0});
    require(!mesh.ranges.back().original&&mesh.ranges.back().count==3,"Procedural draw behavior changed");
    // Contact tilt is a presentation-coordinate conversion. Validate known
    // quarter-turn geometry, world translation, normals and the original
    // negative-Z/body-to-positive-Z/model change of basis independently.
    NativeAssembly poseAssembly;NativeModelInstance instance;instance.chunk=0;
    instance.transform={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};poseAssembly.instances.push_back(instance);
    const auto closePoint=[](Vec3 actual,Vec3 expected){return length(actual-expected)<0.00002f;};
    Mesh pitched;pitched.originalCar(model,poseAssembly,{10,20,30},0,-pi*.5f,0,211);
    require(closePoint(pitched.vertices[2].position,{10,21,30}),"Host negative pitch must lift model+Z nose");
    require(closePoint(pitched.vertices[2].normal,{0,0,-1}),"Tilt must rotate normals without translation");
    Mesh rolled;rolled.originalCar(model,poseAssembly,{10,20,30},0,0,-pi*.5f,211);
    require(closePoint(rolled.vertices[1].position,{10,19,30}),"Host negative roll must lower model+X side");
    const auto rotateAxis=[](Vec3 v,Vec3 axis,float angle){
        const float c=std::cos(angle),s=std::sin(angle);
        return v*c+cross(axis,v)*s+axis*(dot(axis,v)*(1-c));
    };
    for(const Vec3 originalAngles:std::array<Vec3,4>{{{.18f,.7f,.06f},{-.12f,-2.1f,-.07f},{6.15f,3.1f,6.22f},{0,-.9f,0}}}){
        Mesh posed;posed.originalCar(model,poseAssembly,{10,20,30},originalAngles.y+pi,-originalAngles.x,-originalAngles.z,211);
        const auto originalBody=[&](Vec3 v){
            v={-v.x,v.y,-v.z}; // exact local half-turn; model+Z -> original-Z
            v=rotateAxis(v,{0,0,1},originalAngles.z);
            v=rotateAxis(v,{1,0,0},originalAngles.x);
            return rotateAxis(v,{0,1,0},originalAngles.y);
        };
        for(std::size_t i=0;i<3;++i){
            require(closePoint(posed.vertices[i].position,Vec3{10,20,30}+originalBody(batch.vertices[i].position)),"Original actor/model pose basis mismatch");
            require(closePoint(posed.vertices[i].normal,originalBody(batch.vertices[i].normal)),"Original actor/model normal basis mismatch");
        }
        require(posed.ranges[0].texture==218,"Pose overload changed texture binding");
    }
    Mesh oldPose,zeroTilt;oldPose.originalCar(model,poseAssembly,{10,20,30},.61f,211);
    zeroTilt.originalCar(model,poseAssembly,{10,20,30},.61f,0,0,211);
    for(std::size_t i=0;i<3;++i){
        require(length(oldPose.vertices[i].position-zeroTilt.vertices[i].position)==0,"Yaw-only overload changed positions");
        require(length(oldPose.vertices[i].normal-zeroTilt.vertices[i].normal)==0,"Yaw-only overload changed normals");
    }
    auto real=NativeModel::load(root/"data/original_models/courses/k_df/k_df.idasmesh");
    auto assembly=NativeAssembly::load(root/"data/original_models/courses/k_df/assembly/sector_00.idasasm",real.chunks.size());
    Mesh road;road.originalCar(real,assembly,{0,0,0},0);
    std::size_t vertices=0;unsigned classes=0;
    for(const auto& range:road.ranges){require(range.original,"Missing original course state");require(std::size_t(range.first)+range.count<=road.vertices.size(),"Invalid original range");vertices+=range.count;classes|=1u<<((range.pcw>>24)&7);}
    require(vertices==road.vertices.size()&&(classes&21)==21,"Original scene list coverage changed");
    checkScreenFade();
    std::cout<<"5 shipping shaders compiled, original material/flat-color/procedural isolation and actor/model tilt-coordinate tests passed, Akina "<<road.ranges.size()<<" ranges validated. WARP-only full-viewport fade checks passed at alpha0/128/255, both HUD orders, pause/unfade and empty scene.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
