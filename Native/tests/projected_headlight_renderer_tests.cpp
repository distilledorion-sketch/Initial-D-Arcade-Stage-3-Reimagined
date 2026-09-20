#include "renderer.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace idas3;
namespace {
unsigned checks=0;
void check(bool okay,const char*why){++checks;if(!okay)throw std::runtime_error(why);}
void pixel(std::uint32_t actual,Color expected){
    const std::array<float,3> channels{expected.b,expected.g,expected.r};
    for(unsigned channel=0;channel<3;++channel){const auto wanted=int(std::lround(std::clamp(channels[channel],0.f,1.f)*255));
        ++checks;if(std::abs(int((actual>>(8*channel))&255)-wanted)>1)
            throw std::runtime_error("Projected blend channel "+std::to_string(channel)+" expected "+std::to_string(wanted)+" actual "+std::to_string((actual>>(8*channel))&255));}
}
Mesh panel(float z,Color color,bool projection){
    Mesh mesh;mesh.quad({-10000,-10000,z},{10000,-10000,z},{10000,10000,z},{-10000,10000,z},color);
    auto&range=mesh.ranges[0];range.original=true;range.emissive=true;range.gmp=0x222;
    range.pcw=projection?0x8a00071e:2;range.tsp=projection?0x4489a464:0x20900000;
    range.isp=projection?0x93800000:0xc0000000;range.texture=projection?0:0xffffffff;
    for(auto&vertex:mesh.vertices){vertex.normal={.8f,.3f,-.2f};vertex.u=vertex.v=.5f;
        if(projection)vertex.offsetColor={.02f,.04f,.06f,.7f};}
    return mesh;
}
}
int main(int argc,char**argv)try{
    check(argc==2,"Native project root required");const std::filesystem::path root=argv[1];
    const auto bank=NativeModel::load(root/"data/original_assets/headlight_projection/lightobj.idasmesh");
    check(bank.chunks.size()==8,"Imported lightobj must retain all eight source chunks");
    const auto&chunk=bank.chunks[7];check(chunk.index==7&&chunk.batches.size()==1,"Source chunk7 identity");const auto&batch=chunk.batches[0];
    check(batch.ich[0]==0x8a00071e&&batch.ich[1]==0x93800000&&batch.ich[2]==0x4489a464&&batch.ich[6]==0x4a,"Actual projected PCW/ISP/TSP/VUR words");
    check(batch.material[2]==0x222&&batch.material[9]==0&&batch.vertices.size()==12&&batch.indices.size()==24,"Actual projected noLight/texture0/twelve-vertex strip");
    const auto actualTextures=NativeTextureBank::load(root/"data/original_assets/headlight_projection/textures/textures.idastex");
    check(actualTextures.size()==2&&!actualTextures.at(0).argb.empty(),"Actual source texture0 bank");
    const auto directory=std::filesystem::temp_directory_path()/("idas3-projected-material-"+std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(directory);const auto textureFile=directory/"constant.idastex",imageFile=directory/"frame.bmp";
    {
        // Independent constant texel, RGBA bytes64,128,192,32. Its alpha is
        // deliberately low: source TSP ignores texture alpha, and DST_COLOR
        // blending must not be replaced by source-alpha additive blending.
        std::ofstream out(textureFile,std::ios::binary);out.write("IDAS3T1\0",8);
        const std::array<std::uint32_t,7> words{1,1,0,1,1,4,0x20c08040};out.write(reinterpret_cast<const char*>(words.data()),sizeof(words));
    }
    constexpr unsigned w=320,h=240;Renderer renderer;check(renderer.initialize(nullptr,w,h,true),renderer.error.c_str());
    check(renderer.loadTextures(NativeTextureBank::load(textureFile)),renderer.error.c_str());
    auto fog=original::originalBootstrapFog();fog.colorRgb=0xff0020;fog.table.fill(0xffff);renderer.courseFog=&fog;
    auto lighting=original::originalCourseLighting(3,true,false);lighting.ambient={0,0,0};renderer.courseLighting=&lighting;
    renderer.vehicleLights=true;renderer.opponentLights=true;renderer.vehicleForward=renderer.opponentForward={0,0,-1};renderer.courseLampPositions={{0,3,-10}};
    auto capture=[&](const Mesh&mesh,bool night,const OriginalRearViewFrame*rear=nullptr,const OriginalShowroomLighting*showroom=nullptr){
        check(renderer.draw(mesh,{0,0,0},{0,0,-1},night,false,nullptr,false,showroom,rear),renderer.error.c_str());
        check(renderer.saveBitmap(imageFile.wstring()),renderer.error.c_str());
        std::ifstream in(imageFile,std::ios::binary);BITMAPFILEHEADER head{};in.read(reinterpret_cast<char*>(&head),sizeof(head));in.seekg(head.bfOffBits);
        std::vector<std::uint32_t> pixels(w*h);in.read(reinterpret_cast<char*>(pixels.data()),pixels.size()*4);check(bool(in),"Projected material readback");return pixels;
    };
    const Color destination{.2f,.4f,.6f,1},source{.4f,.3f,.2f,.1f};
    const Color shaded{source.r*64.f/255+.02f,source.g*128.f/255+.04f,source.b*192.f/255+.06f,1};
    // The expected color comes directly from the original factor equation,
    // independently of the renderer's factor-switch implementation.
    const Color expected{destination.r*(1+shaded.r),destination.g*(1+shaded.g),destination.b*(1+shaded.b),1};
    OriginalRearViewFrame rear;rear.eye={};rear.target={0,0,-1};rear.up={0,1,0};
    for(float depth:{10.f,400.f})for(bool night:{false,true})for(unsigned mask=0;mask<4;++mask){
        auto mesh=panel(-depth,destination,false),projected=panel(-depth+.1f,source,true);projected.ranges[0].viewMask=mask;mesh.append(projected);
        check(mesh.ranges.size()==2&&mesh.ranges[1].viewMask==mask,"Append lost source view mask");
        const auto frame=capture(mesh,night,&rear);
        for(unsigned y:{90u,120u,180u})for(unsigned x:{90u,160u,230u})pixel(frame[y*w+x],mask&1?expected:destination);
        for(unsigned y:{21u,32u,42u})for(unsigned x:{100u,160u,220u})pixel(frame[y*w+x],mask&2?expected:destination);
        check(capture(mesh,night,&rear)==frame,"Frozen projected material/view mask changed on repaint");
        // Change all competing native/source light terms; noLight+emissive
        // source projection and fog-off surface must keep the same equation.
        lighting.ambient={1,0,1};renderer.vehicleLights=!renderer.vehicleLights;renderer.opponentLights=!renderer.opponentLights;
        for(auto&vertex:mesh.vertices)vertex.normal=-vertex.normal;
        check(capture(mesh,night,&rear)==frame,"Projected noLight material changed under ambient/normal/native-beam changes");
        lighting.ambient={0,0,0};
    }
    auto joined=panel(-10,source,true);joined.ranges[0].viewMask=1;auto second=joined;second.ranges[0].viewMask=2;joined.append(second);
    check(joined.ranges.size()==2&&joined.ranges[0].viewMask==1&&joined.ranges[1].viewMask==2,"Adjacent projected scopes incorrectly merged");
    auto mesh=panel(-10,destination,false);mesh.append(panel(-9.9f,source,true));OriginalShowroomLighting showroom;
    pixel(capture(mesh,false,nullptr,&showroom)[120*w+160],expected);
    std::filesystem::remove(imageFile);std::filesystem::remove(textureFile);std::filesystem::remove(directory);
    std::cout<<"PASS "<<checks<<" projected material framebuffer checks: original chunk7 words/texture0, independent DST_COLOR+ONE equation, texture/offset/no-alpha contribution, noLight/no-fog, depth10/400, day/night, viewMask0/1/2/3, main/rear, replay and append scope. WARP only.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
