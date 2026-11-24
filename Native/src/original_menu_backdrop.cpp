#include "original_menu_backdrop.h"
#include "original_gasstand_data.h"
#include "unity_ui_capture.h"
#include <algorithm>
#include <cmath>

namespace idas3::original {
NativeImage buildMenuLogoBackdrop(const NativeTextureBank& textures,const NativeModelChunk& chunk){
    // The cabinet steps 254 pixels between overlapping 256-pixel meshes.
    // Each mesh rearranges the same small logo in several different UV strips.
    // A 128-unit strip samples a 256-texel source. Bake both samples
    // per cabinet pixel so fractional scrolling keeps the original detail.
    constexpr unsigned period=508;
    std::vector<std::uint32_t> pattern(period*period);
    SpritePlacement p;p.scale=200;p.invertY=true;p.authoredHeight=0;
    p.defaultOriginalUiColors=true;p.softwareOnly=true;
    compositeOriginalMenuChunk(pattern,period,period,textures,chunk,p);
    NativeImage image{2*period,period};
    image.argb.resize(image.width*image.height);
    for(unsigned y=0;y<image.height;++y){
        const auto row=pattern.data()+(y%period)*period;
        for(unsigned x=0;x<image.width;x+=period)
            std::copy_n(row,period,image.argb.data()+y*image.width+x);
    }
    unityUiForgetTexture(image);
    return image;
}

void paintMenuLogoBackdrop(std::span<std::uint32_t> pixels,int w,int h,const NativeImage& image,
                          std::uint64_t frame,bool marginsOnly){
    const float fit=std::min(w/640.f,h/480.f);
    const float left=(w-640*fit)*.5f,top=(h-480*fit)*.5f;
    const unsigned cycle=unsigned(std::ceil(gasstand_data::lit_0C112F88/gasstand_data::lit_0C112E28));
    // Choose the texel in the direction of increasing screen coordinates
    // at exact half-pixel boundaries, consistently across CPU/GPU triangles.
    const float scroll=float(frame%cycle)*gasstand_data::lit_0C112E28*100.f-.001f;
    constexpr float texelsPerUnit=2.f;
    const auto rect=[&](int x0,int y0,int x1,int y1){
        if(x0>=x1||y0>=y1)return;
        if(unityUiEnabled()){
            const auto vertex=[&](float x,float y){
                return UnityUiVertex{x,y,((x-left)/fit-scroll)*texelsPerUnit/image.width,((y-top)/fit-scroll)*texelsPerUnit/image.height,0xffffffff,0};
            };
            const auto a=vertex(float(x0),float(y0)),b=vertex(float(x1),float(y0));
            const auto c=vertex(float(x0),float(y1)),d=vertex(float(x1),float(y1));
            constexpr unsigned tsp=(1u<<29)|(1u<<19); // Opaque, repeating texture.
            unityUiTriangle(pixels.data(),w,h,image,a,b,c,1,tsp,true,8);
            unityUiTriangle(pixels.data(),w,h,image,c,b,d,1,tsp,true,8);
            return;
        }
        const auto wrap=[](float coord,int size){int i=int(std::floor(coord))%size;return i<0?i+size:i;};
        std::vector<unsigned> columns(x1-x0);
        for(int x=x0;x<x1;++x)columns[x-x0]=wrap(((x+.5f-left)/fit-scroll)*texelsPerUnit,image.width);
        for(int y=y0;y<y1;++y){
            const auto row=image.argb.data()+wrap(((y+.5f-top)/fit-scroll)*texelsPerUnit,image.height)*image.width;
            auto dest=pixels.data()+std::size_t(y)*w+x0;
            for(auto x:columns)*dest++=row[x];
        }
    };
    if(!marginsOnly){rect(0,0,w,h);return;}
    const int dw=std::max(1,int(640*fit)),dh=std::max(1,int(480*fit));
    const int x=(w-dw)/2,y=(h-dh)/2;
    rect(0,0,x,h);rect(x+dw,0,w,h);rect(x,0,x+dw,y);rect(x,y+dh,x+dw,h);
}
}
