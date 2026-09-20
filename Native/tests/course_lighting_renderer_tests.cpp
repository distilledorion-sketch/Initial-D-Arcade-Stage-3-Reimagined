#include "renderer.h"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace idas3;
void require(bool pass,const char* message){if(!pass)throw std::runtime_error(message);}
int main()try{
    Renderer renderer;require(renderer.initialize(nullptr,640,480,true),renderer.error.c_str());
    renderer.vehiclePosition=renderer.opponentPosition={10000,10000,10000};
    const auto path=std::filesystem::temp_directory_path()/"idas3-course-lighting-check.bmp";
    auto capture=[&](const Mesh& mesh,bool lights,bool night=true,const OriginalRearViewFrame* rear=nullptr){
        renderer.courseLampLighting=lights;
        require(renderer.draw(mesh,{0,8,18},{0,0,0},night,false,nullptr,false,nullptr,rear),renderer.error.c_str());
        require(renderer.saveBitmap(path.wstring()),renderer.error.c_str());
        std::ifstream f(path,std::ios::binary);BITMAPFILEHEADER head{};f.read(reinterpret_cast<char*>(&head),sizeof(head));f.seekg(head.bfOffBits);
        std::vector<std::uint32_t> pixels(640*480);f.read(reinterpret_cast<char*>(pixels.data()),pixels.size()*4);require(bool(f),"Light pixel readback failed");return pixels;
    };
    auto litPixels=[](const auto& a,const auto& b){unsigned brighter=0;for(std::size_t i=0;i<a.size();++i){
        const auto luminance=[](unsigned c){return int((c>>16)&255)*3+int((c>>8)&255)*6+int(c&255);};
        if(luminance(b[i])-luminance(a[i])>25)++brighter;
    }return brighter;};
    renderer.courseLampPositions={{0,7,0}};
    Mesh ground;ground.quad({-20,0,-20},{-20,0,20},{20,0,20},{20,0,-20},{.55f,.55f,.55f});
    auto off=capture(ground,false),on=capture(ground,true);
    require(litPixels(off,on)>3000,"Lamp does not illuminate road geometry");
    require(capture(ground,false,false)==capture(ground,true,false),"Street lighting changed daytime rendering");
    renderer.courseLampPositions={{0,-7,0}};
    require(capture(ground,false)==capture(ground,true),"Lamp shines upward through the ground");
    renderer.courseLampPositions={{0,60,0}};
    require(capture(ground,false)==capture(ground,true),"Lamp finite-radius falloff is broken");
    renderer.courseLampPositions={{0,7,0}};
    Mesh body;body.box({0,.8f,0},{1.8f,1.5f,4.4f},.3f,{.8f,.1f,.1f});
    require(litPixels(capture(body,false),capture(body,true))>100,"Lamp does not illuminate car body geometry");
    for(auto& range:body.ranges){range.original=true;range.emissive=true;range.pcw=0;range.isp=0xc0000000;range.tsp=0x20000000;}
    require(capture(body,false)==capture(body,true),"Lamp changed emissive vehicle parts");
    renderer.courseLampPositions.clear();
    require(capture(ground,false)==capture(ground,true),"Empty course leaks light from a prior scene");
    renderer.courseLampPositions={{0,7,0}};
    OriginalRearViewFrame rear;rear.eye={0,5,12};rear.target={0,0,0};rear.up={0,1,0};
    off=capture(ground,false,true,&rear);on=capture(ground,true,true,&rear);unsigned mirrorLit=0;
    for(unsigned y=32;y<96;++y)for(unsigned x=160;x<480;++x)mirrorLit+=off[y*640+x]!=on[y*640+x];
    require(mirrorLit>100,"Street lighting missing from mirror view");
    std::filesystem::remove(path);
    std::cout<<"PASS localized road/body illumination, daytime, downward emission, finite range, emissive exclusion, scene reset and rear-view lighting\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
