#include "renderer.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace idas3;
int main()try{
    std::size_t unchanged=0,red=0,green=0;
    for(auto [w,h]:{std::pair{640,480},std::pair{1280,720},std::pair{2560,1004}}){
        Renderer renderer;if(!renderer.initialize(nullptr,w,h,true))throw std::runtime_error(renderer.error);
        Mesh mesh;mesh.box({0,0,-6},{2,2,2},0,{.05f,.1f,1});
        mesh.box({-2,.8f,14},{1,1,1},0,{1,.02f,.01f});mesh.box({2,.8f,14},{1,1,1},0,{.01f,1,.01f});
        const auto rear=originalRearViewFrame(original::originalIdentityMatrix());
        const auto path=std::filesystem::temp_directory_path()/("idas3-rear-test-"+std::to_string(GetCurrentProcessId())+".bmp");
        const auto capture=[&](bool mirror,const std::uint32_t* overlay=nullptr){
            if(!renderer.draw(mesh,{0,0,0},{0,0,-1},false,false,overlay,false,nullptr,mirror?&rear:nullptr)||!renderer.saveBitmap(path.wstring()))throw std::runtime_error(renderer.error);
            std::ifstream in(path,std::ios::binary);BITMAPFILEHEADER fh{};in.read(reinterpret_cast<char*>(&fh),sizeof(fh));in.seekg(fh.bfOffBits);
            std::vector<std::uint32_t> pixels(std::size_t(w)*h);in.read(reinterpret_cast<char*>(pixels.data()),pixels.size()*4);if(!in)throw std::runtime_error("Rear readback failed");return pixels;
        };
        const auto baseline=capture(false),mirror=capture(true);
        const float fit=std::min(float(w)/640.f,float(h)/480.f),left=(w-640*fit)*.5f+160*fit,top=(h-480*fit)*.5f+32*fit,right=left+320*fit,bottom=top+64*fit;
        std::size_t rCount=0,gCount=0;
        for(int y=0;y<h;++y)for(int x=0;x<w;++x){const auto i=std::size_t(y)*w+x;
            if(x+.5f<left||x+.5f>=right||y+.5f<top||y+.5f>=bottom){++unchanged;if(mirror[i]!=baseline[i])throw std::runtime_error("Mirror modified pixels outside its viewport");continue;}
            const auto c=mirror[i];const unsigned r=(c>>16)&255,g=(c>>8)&255,b=c&255;
            if(r>80&&r>g*3&&r>b*3){++rCount;if(x>(left+right)/2)throw std::runtime_error("Rear red landmark on wrong side");}
            if(g>80&&g>r*3&&g>b*3){++gCount;if(x<(left+right)/2)throw std::runtime_error("Rear green landmark on wrong side");}
        }
        if(!rCount||!gCount)throw std::runtime_error("Rear view missed landmarks behind player");red+=rCount;green+=gCount;
        if(capture(true)!=mirror)throw std::runtime_error("Repeated mirror draw changed pixels");
        std::vector<std::uint32_t> overlay(std::size_t(w)*h,0xffa041dc);
        const auto covered=capture(true,overlay.data());for(auto c:covered)if((c&0xffffff)!=0xa041dc)throw std::runtime_error("Mirror leaked viewport into HUD pass");
        std::filesystem::remove(path);
    }
    std::cout<<"PASS rear geometry, mirrored side order, repeated render and HUD restoration at3 resolutions; "<<unchanged<<" unchanged outside pixels, "<<red<<" red and "<<green<<" green rear landmark pixels.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
