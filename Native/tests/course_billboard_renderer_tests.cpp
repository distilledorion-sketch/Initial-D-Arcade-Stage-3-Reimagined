#include "renderer.h"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace idas3;
void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
Mesh quads(Vec3 eye,Vec3 f,Vec3 r,Vec3 u,bool billboard){
    Mesh mesh;
    for(unsigned i=0;i<3;++i){
        const auto anchor=eye+f*14.f+r*(float(i)*3.f-3.f);
        const Vec3 corners[]={{-.75f,1.25f,0},{1.25f,1.25f,0},{1.25f,-.75f,0},{-.75f,-.75f,0}};
        const auto begin=mesh.vertices.size();const auto point=[&](Vec3 p){return billboard?p:anchor+r*p.x+u*p.y;};
        mesh.quad(point(corners[0]),point(corners[1]),point(corners[2]),point(corners[3]),i==0?Color{1,0,0}:i==1?Color{0,1,0}:Color{0,0,1});
        if(billboard)for(auto j=begin;j<mesh.vertices.size();++j)mesh.vertices[j].normal=anchor;
    }
    for(auto& range:mesh.ranges){range.original=true;range.gmp=512;range.emissive=true;range.pcw=2;range.tsp=0x20800000;range.isp=0xe0000000;range.billboard=billboard;}
    return mesh;
}
int main()try{
    const auto path=std::filesystem::temp_directory_path()/("idas3-lamp-test-"+std::to_string(GetCurrentProcessId())+".bmp");
    std::uint64_t checked=0,visible=0;
    for(auto [w,h]:{std::pair{640,480},std::pair{1280,720},std::pair{2560,1004}}){
        Renderer renderer;require(renderer.initialize(nullptr,w,h,true),renderer.error.c_str());
        const auto capture=[&](const Mesh& mesh,Vec3 eye,Vec3 f,const OriginalRearViewFrame* rear=nullptr){
            require(renderer.draw(mesh,eye,eye+f,true,false,nullptr,false,nullptr,rear),renderer.error.c_str());
            require(renderer.saveBitmap(path.wstring()),renderer.error.c_str());
            std::ifstream file(path,std::ios::binary);BITMAPFILEHEADER header{};file.read(reinterpret_cast<char*>(&header),sizeof(header));file.seekg(header.bfOffBits);
            std::vector<std::uint32_t> pixels(std::size_t(w)*h);file.read(reinterpret_cast<char*>(pixels.data()),pixels.size()*4);require(bool(file),"Lamp readback");return pixels;
        };
        for(auto f:{Vec3{0,0,-1},Vec3{1,0,0},Vec3{0,0,1},Vec3{-1,0,0},Vec3{0,1,0}})for(unsigned roll=0;roll<2;++roll){
            const Vec3 eye{32,8,-16};Vec3 u=f.y?Vec3{0,0,1}:Vec3{0,1,0};if(roll)u=cross(f,u);
            const auto r=normalized(cross(f,u));renderer.cameraUp=u;
            const auto actual=capture(quads(eye,f,r,u,true),eye,f),expected=capture(quads(eye,f,r,u,false),eye,f);
            require(actual==expected,"Lamp quad differs from independently oriented world geometry");checked+=actual.size();
            std::size_t colors=0;for(auto pixel:actual)if((pixel&0xffffff)==0xff0000||(pixel&0xffffff)==0x00ff00||(pixel&0xffffff)==0x0000ff)++colors;
            require(colors>100,"Billboards disappeared or were dimmed by host headlights");visible+=colors;
        }
        OriginalRearViewFrame rear;rear.eye={0,0,0};rear.target={1,0,0};rear.up={0,0,1};
        const auto f=normalized(rear.target-rear.eye),r=normalized(cross(rear.up,f)),u=normalized(cross(f,r));
        renderer.cameraUp={0,1,0};
        const auto actual=capture(quads(rear.eye,f,r,u,true),{0,0,0},{0,0,-1},&rear);
        const auto expected=capture(quads(rear.eye,f,r,u,false),{0,0,0},{0,0,-1},&rear);
        const float fit=std::min(w/640.f,h/480.f),left=(w-640*fit)*.5f+160*fit,top=(h-480*fit)*.5f+32*fit;
        std::size_t rearColors=0;
        for(int y=0;y<h;++y)for(int x=0;x<w;++x)if(x+.5f>=left&&x+.5f<left+320*fit&&y+.5f>=top&&y+.5f<top+64*fit){
            const auto i=std::size_t(y)*w+x;require(actual[i]==expected[i],"Mirror used forward-view lamp orientation or incorrect range bounds");++checked;
            const auto color=actual[i]&0xffffff;if(color==0xff0000||color==0x00ff00||color==0x0000ff)++rearColors;
        }
        require(rearColors>30,"Mirror failed to show world-anchored lamps");visible+=rearColors;
    }
    std::filesystem::remove(path);
    std::cout<<"PASS 30 forward orientation/roll/resolution cases and3 independent mirror views; "<<checked<<" identical pixels, "<<visible<<" visible full-brightness lamp pixels.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
