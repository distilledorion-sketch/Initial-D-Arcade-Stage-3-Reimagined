#include "renderer.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace idas3;
namespace {
unsigned checks=0;
std::string fixture;
void check(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
Mesh panel(float depth,unsigned mode,bool offset=false,bool texture=false){
    Mesh mesh;mesh.quad({-depth*2,-depth*2,-depth},{depth*2,-depth*2,-depth},
        {depth*2,depth*2,-depth},{-depth*2,depth*2,-depth},{.2f,.4f,.8f,.6f});
    for(auto& v:mesh.vertices)v.offsetColor={0,0,0,.375f};
    for(auto& r:mesh.ranges){r.original=true;r.emissive=true;r.gmp=512;
        r.pcw=(offset?4:0)|(texture?8:0);r.texture=texture?0:0xffffffff;
        r.tsp=0x20100000|(mode<<22)|(texture?3u<<6:0);r.isp=0xc0000000;}
    return mesh;
}
void pixel(std::uint32_t actual,Color expected,bool alpha=false){
    for(unsigned i=0;i<(alpha?4u:3u);++i){
        const unsigned shift=i==0?16:i==1?8:i==2?0:24;
        const float value=i==0?expected.r:i==1?expected.g:i==2?expected.b:expected.a;
        if(std::abs(int((actual>>shift)&255)-int(std::lround(value*255)))>1)
            throw std::runtime_error("GPU fog pixel differs: "+fixture+" channel "+std::to_string(i)+" actual "+std::to_string((actual>>shift)&255)+" expected "+std::to_string(value*255));
        ++checks;
    }
}
Color mix(Color base,unsigned rgb,float amount){
    return {base.r+(float((rgb>>16)&255)/255-base.r)*amount,
        base.g+(float((rgb>>8)&255)/255-base.g)*amount,
        base.b+(float(rgb&255)/255-base.b)*amount,base.a};
}
}
int main()try{
    constexpr unsigned w=320,h=240;Renderer renderer;
    check(renderer.initialize(nullptr,w,h,true),renderer.error.c_str());
    const auto temporary=std::filesystem::temp_directory_path()/
        ("idas3-fog-renderer-"+std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directory(temporary);
    const auto path=temporary/"frame.bmp";
    auto capture=[&](const Mesh& mesh,const OriginalRearViewFrame* mirror=nullptr,const OriginalShowroomLighting* showroom=nullptr){
        check(renderer.draw(mesh,{0,0,0},{0,0,-1},false,false,nullptr,false,showroom,mirror),renderer.error.c_str());
        check(renderer.saveBitmap(path.wstring()),renderer.error.c_str());
        std::ifstream in(path,std::ios::binary);BITMAPFILEHEADER header{};in.read(reinterpret_cast<char*>(&header),sizeof(header));in.seekg(header.bfOffBits);
        std::vector<std::uint32_t> pixels(w*h);in.read(reinterpret_cast<char*>(pixels.data()),pixels.size()*4);check(bool(in),"Fog readback failed");return pixels;
    };
    const Color base{.2f,.4f,.8f,.6f};
    auto probe=original::originalCourseFog(0,false,false);renderer.courseFog=&probe;
    probe.table.fill(0xffff);fixture="constant table";pixel(capture(panel(250,0))[120*w+160],{1,1,1,1});
    probe.packedDensity=original::originalFogPackedDensity(1);probe.table.fill(0);probe.table[0]=0xffff;
    fixture="reciprocal depth";pixel(capture(panel(250,0))[120*w+160],{1,1,1,1});
    // Same camera-space depth must have identical fog on and off the view axis.
    // This also catches accidentally using normalized projection Z.
    for(unsigned course=0;course<9;++course)for(bool night:{false,true})for(bool wet:{false,true}){
        const auto fog=original::originalCourseFog(course,night,wet);renderer.courseFog=&fog;
        for(float depth:{2.f,50.f,250.f,1500.f}){
            fixture="course="+std::to_string(course)+" night="+std::to_string(night)+" wet="+std::to_string(wet)+" depth="+std::to_string(depth);
            const auto pixels=capture(panel(depth,0));
            const auto expected=mix(base,fog.colorRgb,original::originalFogTableCoefficient(fog,1/depth));
            for(unsigned y:{24u,120u,215u})for(unsigned x:{20u,160u,299u})pixel(pixels[y*w+x],expected);
        }
        fixture="mode2";pixel(capture(panel(1500,2))[120*w+160],base);
    }
    auto fog=original::originalCourseFog(2,false,true);renderer.courseFog=&fog;
    // A receding triangle detects double perspective correction of the
    // reciprocal-depth carrier; constant-depth panels cannot detect it.
    auto slope=panel(1,0);slope.vertices.resize(3);slope.ranges.resize(1);slope.ranges[0].count=3;
    const float tangent=std::tan(renderer.verticalFieldOfView*.5f),aspect=float(w)/h;
    const std::array<float,3> depths{50,500,1000};
    const std::array<Vec3,3> projected{{{-1,-1,0},{1,-1,0},{0,1,0}}};
    for(unsigned i=0;i<3;++i)slope.vertices[i].position={projected[i].x*depths[i]*tangent*aspect,projected[i].y*depths[i]*tangent,-depths[i]};
    const auto receding=capture(slope);fixture="receding triangle";
    for(unsigned y:{100u,160u,220u})for(unsigned x:{120u,160u,200u}){
        const float sx=2*(float(x)+.5f)/w-1,sy=1-2*(float(y)+.5f)/h;
        const float top=(sy+1)*.5f,left=(1-top-sx)*.5f,right=(1-top+sx)*.5f;
        const float reciprocal=left/depths[0]+right/depths[1]+top/depths[2];
        pixel(receding[y*w+x],mix(base,fog.colorRgb,original::originalFogTableCoefficient(fog,reciprocal)));
    }
    renderer.originalVertexFogColor={.8f,.2f,.1f,1};
    fixture="vertex-offset";pixel(capture(panel(250,1,true))[120*w+160],mix(base,0xcc331a,.375f));
    pixel(capture(panel(250,1,false))[120*w+160],base);
    // Mode3 must replace the base before modulate-alpha texturing, including
    // its fog-derived alpha. A final RGB fog blend cannot pass this fixture.
    const auto texturePath=temporary/"tex.idastex";
    {std::ofstream out(texturePath,std::ios::binary);out.write("IDAS3T1\0",8);
        const std::uint32_t words[]={1,1,0,1,1,4,0x80402080};out.write(reinterpret_cast<const char*>(words),sizeof(words));}
    check(renderer.loadTextures(NativeTextureBank::load(texturePath)),renderer.error.c_str());
    const float amount=original::originalFogTableCoefficient(fog,1/250.f);
    const Color mode3{float((fog.colorRgb>>16)&255)/255*128/255,
        float((fog.colorRgb>>8)&255)/255*32/255,float(fog.colorRgb&255)/255*64/255,amount*128/255};
    fixture="mode3-texture";pixel(capture(panel(250,3,false,true))[120*w+160],mode3,true);
    const auto frozen=capture(panel(250,0));check(frozen==capture(panel(250,0)),"Repeated draw consumed fog state");
    // The rear camera is500 source units away from the same panel. Using the
    // main camera's depth or radial distance would produce a different value.
    OriginalRearViewFrame rear;rear.eye={0,0,250};rear.target={0,0,-1};rear.up={0,1,0};
    const auto mirrored=capture(panel(250,0),&rear);
    fixture="mirror";pixel(mirrored[32*w+160],mix(base,fog.colorRgb,original::originalFogTableCoefficient(fog,1/500.f)));
    for(unsigned y=48;y<h;++y)for(unsigned x=0;x<w;++x)check(mirrored[y*w+x]==frozen[y*w+x],"Mirror fog changed main view outside mirror");
    OriginalShowroomLighting showroom;
    const auto showroomFog=capture(panel(250,0),nullptr,&showroom);renderer.courseFog=nullptr;
    check(showroomFog==capture(panel(250,0),nullptr,&showroom),"Course fog leaked into a showroom");
    std::filesystem::remove(path);std::filesystem::remove(texturePath);std::filesystem::remove(temporary);
    std::cout<<"PASS "<<checks<<" GPU fog checks:36 source states,4 depths,off-axis pixels,receding triangle,modes0/1/2/3,mode3 texture alpha,rear depth,showroom bypass and repaint.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
