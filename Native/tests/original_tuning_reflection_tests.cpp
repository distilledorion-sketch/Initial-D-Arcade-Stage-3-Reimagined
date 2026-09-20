#include "renderer.h"
#include <fstream>
#include <iostream>
using namespace idas3;
int main()try{
    auto require=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    Mesh combined;
    for(unsigned i=0;i<3;++i){
        Mesh piece;const float x=-.9f+.6f*float(i);
        piece.beginRange(0xffffffffu,1u<<29,0,6u<<29,0,true);
        for(const auto point:std::array<Vec3,6>{{{x,-.3f,-2},{x+.5f,-.3f,-2},{x,.3f,-2},{x,.3f,-2},{x+.5f,-.3f,-2},{x+.5f,.3f,-2}}})
            piece.vertices.push_back({point,{0,1,0},{1,1,1,1}});
        piece.ranges[0].count=6;if(i==1)piece.ranges[0].originalLightDirection=std::array{0.f,-1.f,0.f};
        combined.append(piece);
    }
    require(combined.ranges.size()==3,"Append merged distinct light states");
    Renderer renderer;require(renderer.initialize(nullptr,192,96,true),"WARP initialization");
    renderer.overrideClearColor=true;renderer.clearColor={.2f,0,.2f,1};renderer.verticalFieldOfView=pi/4;
    OriginalShowroomLighting light;light.parameters={{0,1,0},{1,1,1},0,1};
    const auto file=std::filesystem::temp_directory_path()/("idas3-tuning-light-"+std::to_string(GetCurrentProcessId())+".bmp");
    const auto draw=[&](){
        require(renderer.draw(combined,{0,0,0},{0,0,-1},false,false,nullptr,false,&light),"Light render");
        require(renderer.saveBitmap(file.wstring()),"Light readback");
        std::ifstream in(file,std::ios::binary);BITMAPFILEHEADER header{};BITMAPINFOHEADER info{};
        in.read(reinterpret_cast<char*>(&header),sizeof header);in.read(reinterpret_cast<char*>(&info),sizeof info);
        require(header.bfType==0x4d42&&info.biWidth==192&&info.biHeight==-96&&info.biBitCount==32,"Light readback format");
        std::vector<unsigned> pixels(192*96);in.seekg(header.bfOffBits);in.read(reinterpret_cast<char*>(pixels.data()),std::streamsize(pixels.size()*4));
        require(bool(in),"Truncated light readback");in.close();std::filesystem::remove(file);return pixels;
    };
    const auto pixels=draw();
    require((pixels[48*192+59]&0xffffff)==0xffffff,"Default source light missing");
    require((pixels[48*192+93]&0xffffff)==0,"Reflected source light ignored");
    require((pixels[48*192+128]&0xffffff)==0xffffff,"Source light was not restored after reflection");
    require(draw()==pixels,"Repeated rendering changed source light state");
    std::cout<<"Original reflected light: append boundaries, bright/dark/restore pixels and repeat frame exact under WARP.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
