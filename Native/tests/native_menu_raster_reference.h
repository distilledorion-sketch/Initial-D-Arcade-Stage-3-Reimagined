#pragma once
// Frozen pre-optimization native compositor, for exact pixel regression tests.
#include "native_assets.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>

namespace idas3::menu_reference {
namespace {
class Reader {
    std::ifstream input;
public:
    explicit Reader(const std::filesystem::path& path):input(path,std::ios::binary){
        if(!input)throw std::runtime_error("Cannot open native asset: "+path.string());
    }
    void bytes(void* out,std::size_t size){if(!input.read(static_cast<char*>(out),std::streamsize(size)))throw std::runtime_error("Truncated native asset");}
    std::uint32_t u32(){std::array<unsigned char,4>b{};bytes(b.data(),4);return b[0]|(std::uint32_t(b[1])<<8)|(std::uint32_t(b[2])<<16)|(std::uint32_t(b[3])<<24);}
    float f32(){float f=std::bit_cast<float>(u32());if(!std::isfinite(f))throw std::runtime_error("Non-finite original sprite attribute");return f;}
    bool end(){return input.peek()==std::char_traits<char>::eof();}
};
std::uint32_t blend(std::uint32_t dst,std::uint32_t src,float alpha){
    const auto sa=std::uint32_t(std::clamp(std::lround(((src>>24)&255)*alpha),0l,255l));
    if(!sa)return dst;if(sa==255)return src|0xff000000u;
    const auto da=dst>>24;
    const auto oa=sa+(da*(255-sa)+127)/255;
    if(!oa)return 0;
    std::uint32_t result=oa<<24;
    for(int shift:{0,8,16}){
        auto sc=(src>>shift)&255,dc=(dst>>shift)&255;
        auto premul=sc*sa+(dc*da*(255-sa)+127)/255;
        result|=std::min(255u,(premul+oa/2)/oa)<<shift;
    }
    return result;
}
struct ScreenVertex{float x,y,u,v;std::uint32_t color,offset=0;};
std::uint32_t pvrBlend(std::uint32_t dst,std::uint32_t src,std::uint32_t tsp,float opacity){
    // TSP factors follow the original eight PVR source/destination modes.
    const unsigned sourceMode=tsp>>29,destMode=(tsp>>26)&7;
    const float sa=float(src>>24)/255*opacity,da=float(dst>>24)/255;
    auto factor=[&](unsigned mode,float opposite){switch(mode){case 0:return 0.f;case 1:return 1.f;case 2:return opposite;case 3:return 1-opposite;case 4:return sa;case 5:return 1-sa;case 6:return da;default:return 1-da;}};
    std::uint32_t output=0;
    for(int shift:{0,8,16,24}){
        float sc=float((src>>shift)&255)/255,dc=float((dst>>shift)&255)/255;
        if(shift==24)sc=sa;
        else if(sourceMode==1)sc*=opacity;
        float value=sc*factor(sourceMode,dc)+dc*factor(destMode,sc);
        output|=std::uint32_t(std::clamp(std::lround(value*255),0l,255l))<<shift;
    }
    return output;
}
float edge(const ScreenVertex& a,const ScreenVertex& b,float x,float y){return (b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);}
bool topLeft(const ScreenVertex& a,const ScreenVertex& b){return a.y>b.y||(a.y==b.y&&a.x<b.x);}
void triangle(std::span<std::uint32_t> target,int width,int height,const NativeImage& image,
    ScreenVertex a,ScreenVertex b,ScreenVertex c,float opacity,std::uint32_t tsp=(1u<<15)|(1u<<16)|(1u<<20)|(3u<<6),bool originalMaterial=false,std::uint32_t pcw=0x0a){
    float area=edge(a,b,c.x,c.y);if(std::abs(area)<1e-7f)return;
    if(area<0){std::swap(b,c);area=-area;}
    const int minx=std::max(0,int(std::floor(std::min({a.x,b.x,c.x}))));
    const int maxx=std::min(width,int(std::ceil(std::max({a.x,b.x,c.x}))));
    const int miny=std::max(0,int(std::floor(std::min({a.y,b.y,c.y}))));
    const int maxy=std::min(height,int(std::ceil(std::max({a.y,b.y,c.y}))));
    for(int y=miny;y<maxy;y++)for(int x=minx;x<maxx;x++){
        float px=x+.5f,py=y+.5f;
        float ea=edge(b,c,px,py),eb=edge(c,a,px,py),ec=edge(a,b,px,py);
        if(ea<0||eb<0||ec<0||(ea==0&&!topLeft(b,c))||(eb==0&&!topLeft(c,a))||(ec==0&&!topLeft(a,b)))continue;
        float wa=ea/area,wb=eb/area,wc=ec/area;
        float u=wa*a.u+wb*b.u+wc*c.u,v=wa*a.v+wb*b.v+wc*c.v;
        auto sampleCoord=[](float uv,std::uint32_t size,bool clamp,bool mirror){
            if(clamp)return std::uint32_t(std::clamp(int(std::floor(uv*size)),0,int(size)-1));
            float base=std::floor(uv),fraction=uv-base;
            if(mirror&&(int(base)&1))fraction=1-fraction;
            return std::uint32_t(std::clamp(int(std::floor(fraction*size)),0,int(size)-1));
        };
        auto tx=sampleCoord(u,image.width,(tsp&(1u<<16))!=0,(tsp&(1u<<18))!=0);
        auto ty=sampleCoord(v,image.height,(tsp&(1u<<15))!=0,(tsp&(1u<<17))!=0);
        auto color=image.argb[std::size_t(ty)*image.width+tx];
        if(tsp&(1u<<19))color|=0xff000000u;
        const unsigned env=(tsp>>6)&3;const float texAlpha=float(color>>24)/255;
        std::uint32_t modulated=0;
        for(int shift:{0,8,16,24}){
            float vertex=(wa*((a.color>>shift)&255)+wb*((b.color>>shift)&255)+wc*((c.color>>shift)&255));
            if(originalMaterial&&shift==24&&!(tsp&(1u<<20)))vertex=255;
            const float texel=float((color>>shift)&255);
            float value=vertex;
            if(pcw&8){
                if(env==0)value=texel;
                else if(env==1)value=shift==24?texel:vertex*texel/255;
                else if(env==2)value=shift==24?vertex:vertex*(1-texAlpha)+texel*texAlpha;
                else value=vertex*texel/255;
            }
            if(shift!=24&&((pcw&4)||!(pcw&8)))value+=wa*((a.offset>>shift)&255)+wb*((b.offset>>shift)&255)+wc*((c.offset>>shift)&255);
            modulated|=std::uint32_t(std::clamp(std::lround(value),0l,255l))<<shift;
        }
        auto& dest=target[std::size_t(y)*width+x];dest=originalMaterial?pvrBlend(dest,modulated,tsp,opacity):blend(dest,modulated,opacity);
    }
}
}
void compositeOriginalSprite(std::span<std::uint32_t> target,int width,int height,
    const NativeImage& image,const OriginalSprite& sprite,const SpritePlacement& placement){
    if(width<=0||height<=0||target.size()!=std::size_t(width)*height||!image.width||!image.height||image.argb.size()!=std::size_t(image.width)*image.height)throw std::runtime_error("Invalid sprite destination/source");
    if(!std::isfinite(placement.scale)||placement.scale<=0||placement.scale>32||!std::isfinite(placement.offsetX)||!std::isfinite(placement.offsetY)||std::abs(placement.offsetX)>65536||std::abs(placement.offsetY)>65536)throw std::runtime_error("Invalid sprite placement");
    std::array<ScreenVertex,4> screen{};
    for(int i=0;i<4;i++){const auto& v=sprite.vertices[i];screen[i]={(v.x*placement.scale)+placement.offsetX,((placement.invertY?placement.authoredHeight-v.y:v.y)*placement.scale)+placement.offsetY,v.u,v.v,v.color};}
    triangle(target,width,height,image,screen[0],screen[1],screen[2],placement.opacity);
    triangle(target,width,height,image,screen[2],screen[1],screen[3],placement.opacity);
}
void compositeImage(std::span<std::uint32_t> target,int width,int height,const NativeImage& image,float x,float y,float drawWidth,float drawHeight,float opacity){
    OriginalSprite sprite;sprite.vertices={OriginalSpriteVertex{x,y,0,0,0,0xffffffff,0},{x,y+drawHeight,0,0,1,0xffffffff,0},{x+drawWidth,y,0,1,0,0xffffffff,0},{x+drawWidth,y+drawHeight,0,1,1,0xffffffff,0}};
    SpritePlacement placement;placement.opacity=opacity;menu_reference::compositeOriginalSprite(target,width,height,image,sprite,placement);
}
void compositeOriginalMenuChunk(std::span<std::uint32_t> target,int width,int height,
    const NativeTextureBank& textures,const NativeModelChunk& chunk,const SpritePlacement& placement){
    if(width<=0||height<=0||target.size()!=std::size_t(width)*height)throw std::runtime_error("Invalid original menu destination");
    static const NativeImage white{1,1,{0xffffffff}};
    for(const auto& batch:chunk.batches){
        const auto tex=batch.material[9];const NativeImage& image=tex==0xffffffff?white:textures.at(tex);
        // Original UI helper 1B8240 writes GMP params=0x600 and replaces
        // VUR colors from its buffer. Constructor 1B7F80 initializes that
        // buffer to {0xFFFFFFFF,0}; raw black colors are placeholders.
        const bool uiColors=placement.defaultOriginalUiColors&&batch.ich[6]==0x4a;
        const auto params=uiColors?0x600u:batch.material[2];
        std::vector<ScreenVertex> transformed;transformed.reserve(batch.vertices.size());
        for(const auto& v:batch.vertices){
            const auto color=uiColors?0xffffffffu:(params&1)?batch.material[3]:v.color0;
            // VUR color1 is second-volume diffuse, never volume0 offset.
            const auto offset=(params&2)?batch.material[4]:0u;
            transformed.push_back({v.position.x*placement.scale+placement.offsetX,
                (placement.invertY?placement.authoredHeight-v.position.y:v.position.y)*placement.scale+placement.offsetY,v.u,v.v,color,offset});
        }
        for(std::size_t i=0;i<batch.indices.size();i+=3)triangle(target,width,height,image,
            transformed[batch.indices[i]],transformed[batch.indices[i+1]],transformed[batch.indices[i+2]],placement.opacity,batch.ich[2],true,batch.ich[0]);
    }
}
}
