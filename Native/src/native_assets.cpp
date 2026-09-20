#include "native_assets.h"
#include "unity_ui_capture.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>

namespace idas3 {
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
std::uint32_t pvrBlend(std::uint32_t dst,std::uint32_t src,std::uint32_t tsp,float opacity,bool straightAlphaOverlay=false){
    // TSP factors follow the original eight PVR source/destination modes.
    const unsigned sourceMode=tsp>>29,destMode=(tsp>>26)&7;
    if(straightAlphaOverlay){
        // A scene overlay is carried in a straight-alpha HUD texture and
        // blended once more by Renderer. Preserve that carrier, rather than
        // writing PVR's already-premultiplied RGB and squared framebuffer A.
        if(sourceMode==4&&destMode==5)return blend(dst,src,opacity);
        if(sourceMode==1&&destMode==0)return src|0xff000000u;
        // Original tuning YES/NO glow is additive over its opaque shop panel.
        // Its composed RGB is a valid opaque carrier; an additive layer over
        // a transparent3D aperture cannot be represented by this carrier.
        if(sourceMode==1&&destMode==1&&(dst>>24)==255)
            return pvrBlend(dst,src,tsp,opacity,false)|0xff000000u;
        throw std::runtime_error("Unsupported original transparent overlay blend factors");
    }
    // Exact identity/endpoints avoid four float conversions and rounded
    // channels for the opaque pixels covering most original menu artwork.
    if(opacity==1.f){
        if(sourceMode==1&&destMode==0)return src;
        if(sourceMode==4&&destMode==5){if((src>>24)==255)return src;if((src>>24)==0)return dst;}
    }
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
    ScreenVertex a,ScreenVertex b,ScreenVertex c,float opacity,std::uint32_t tsp=(1u<<15)|(1u<<16)|(1u<<20)|(3u<<6),bool originalMaterial=false,std::uint32_t pcw=0x0a,bool straightAlphaOverlay=false,bool softwareOnly=false){
    float area=edge(a,b,c.x,c.y);if(std::abs(area)<1e-7f)return;
    if(!softwareOnly&&unityUiTriangle(target.data(),width,height,image,{a.x,a.y,a.u,a.v,a.color,a.offset},
        {b.x,b.y,b.u,b.v,b.color,b.offset},{c.x,c.y,c.u,c.v,c.color,c.offset},opacity,tsp,originalMaterial,pcw))return;
    if(area<0){std::swap(b,c);area=-area;}
    const int minx=std::max(0,int(std::floor(std::min({a.x,b.x,c.x}))));
    const int maxx=std::min(width,int(std::ceil(std::max({a.x,b.x,c.x}))));
    const int miny=std::max(0,int(std::floor(std::min({a.y,b.y,c.y}))));
    const int maxy=std::min(height,int(std::ceil(std::max({a.y,b.y,c.y}))));
    const unsigned env=(tsp>>6)&3;
    // White diffuse with zero offset reproduces the texel's integer channels
    // after rounding, so neither interpolation nor color modulation is needed.
    // Keep the full path for decal interpolation and every varying color.
    const bool texelColor=(pcw&8)&&env!=2&&a.offset==0&&b.offset==0&&c.offset==0&&
        (env==0||(a.color==0xffffffffu&&b.color==0xffffffffu&&c.color==0xffffffffu));
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
        const float texAlpha=float(color>>24)/255;
        std::uint32_t modulated=texelColor?color:0;
        if(!texelColor)for(int shift:{0,8,16,24}){
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
        auto& dest=target[std::size_t(y)*width+x];dest=originalMaterial?pvrBlend(dest,modulated,tsp,opacity,straightAlphaOverlay):blend(dest,modulated,opacity);
    }
}
}
NativeTextureBank NativeTextureBank::load(const std::filesystem::path& path){
    Reader reader(path);std::array<char,8>magic{};reader.bytes(magic.data(),8);
    if(magic!=std::array<char,8>{'I','D','A','S','3','T','1',0}||reader.u32()!=1)throw std::runtime_error("Unsupported native texture format");
    auto count=reader.u32();if(!count||count>4096)throw std::runtime_error("Invalid native texture count");
    NativeTextureBank bank;bank.images.resize(count);std::size_t total=0;
    for(std::uint32_t i=0;i<count;i++){
        auto index=reader.u32(),w=reader.u32(),h=reader.u32(),size=reader.u32();
        if(index>=count||!bank.images[index].argb.empty()||!w||!h||w>2048||h>2048||std::uint64_t(w)*h*4!=size)throw std::runtime_error("Invalid native texture record");
        total+=size;if(total>256*1024*1024)throw std::runtime_error("Native texture bank budget exceeded");
        auto& im=bank.images[index];im.width=w;im.height=h;im.argb.resize(std::size_t(w)*h);
        for(auto& px:im.argb){auto rgba=reader.u32();px=(rgba&0xff00ff00u)|((rgba&255)<<16)|((rgba>>16)&255);}
    }
    if(!reader.end())throw std::runtime_error("Unexpected native texture trailing bytes");return bank;
}
const NativeImage& NativeTextureBank::at(std::uint32_t index)const{return images.at(index);}
NativeModel NativeModel::load(const std::filesystem::path& path){
    if(std::filesystem::file_size(path)>512*1024*1024)throw std::runtime_error("Native model file budget exceeded");
    Reader reader(path);std::array<char,8> magic{};reader.bytes(magic.data(),8);
    if(magic!=std::array<char,8>{'I','D','A','S','3','M','1',0}||reader.u32()!=1)throw std::runtime_error("Unsupported native model format");
    auto count=reader.u32();if(!count||count>8192)throw std::runtime_error("Invalid native model chunk count");
    NativeModel model;model.chunks.resize(count);std::size_t totalVertices=0,totalIndices=0,totalRaw=0;
    for(std::uint32_t i=0;i<count;i++){
        auto& chunk=model.chunks[i];chunk.index=reader.u32();chunk.sourceOffset=reader.u32();chunk.sourceSize=reader.u32();auto batches=reader.u32();
        if(chunk.index!=i||batches>32768)throw std::runtime_error("Invalid native model chunk");
        for(auto& w:chunk.header)w=reader.u32();
        const bool emptyMarker=chunk.sourceSize==8&&batches==0&&chunk.header[0]==0xffffffff&&chunk.header[1]==3&&std::all_of(chunk.header.begin()+2,chunk.header.end(),[](auto word){return word==0;});
        if(!emptyMarker&&(chunk.header[0]!=0x100||chunk.header[6]!=chunk.sourceSize))throw std::runtime_error("Original model header mismatch");
        chunk.batches.resize(batches);
        for(auto& batch:chunk.batches){
            batch.sourceOffset=reader.u32();auto nv=reader.u32(),ni=reader.u32(),rawSize=reader.u32();
            totalVertices+=nv;totalIndices+=ni;totalRaw+=rawSize;
            if(ni%3||nv>1000000||ni>3000000||rawSize>64000000||totalVertices>4000000||totalIndices>12000000||totalRaw>256*1024*1024)throw std::runtime_error("Native model geometry budget exceeded");
            for(auto& w:batch.ich)w=reader.u32();for(auto& w:batch.material)w=reader.u32();
            if(batch.ich[7]!=nv||((batch.ich[0]>>8)&15)!=7||((batch.material[0]>>8)&15)!=5)throw std::runtime_error("Original model batch mismatch");
            std::uint32_t stride=0;
            switch(batch.ich[6]){case 0x002:stride=16;break;case 0x00A:case 0x042:stride=24;break;case 0x04A:stride=32;break;case 0x00E:case 0x10A:stride=40;break;default:throw std::runtime_error("Unsupported original vertex layout");}
            if(std::uint64_t(nv)*stride!=rawSize)throw std::runtime_error("Original model raw vertex length mismatch");
            batch.vertices.resize(nv);batch.indices.resize(ni);
            for(auto& v:batch.vertices){v.header=reader.u32();v.position={reader.f32(),reader.f32(),reader.f32()};v.normal={reader.f32(),reader.f32(),reader.f32()};v.u=reader.f32();v.v=reader.f32();v.color0=reader.u32();v.color1=reader.u32();}
            for(auto& index:batch.indices){index=reader.u32();if(index>=nv)throw std::runtime_error("Native model triangle index out of bounds");}
            std::vector<std::uint8_t> original(rawSize);reader.bytes(original.data(),original.size());
            // Validate independent preserved source XYZ/header and UV words on import.
            for(std::size_t vi=0;vi<nv;vi++){
                auto readWord=[&](std::size_t off){const auto* p=original.data()+vi*stride+off;return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);};
                const auto& v=batch.vertices[vi];
                if(v.header!=readWord(0)||std::bit_cast<std::uint32_t>(v.position.x)!=readWord(4)||std::bit_cast<std::uint32_t>(v.position.y)!=readWord(8)||std::bit_cast<std::uint32_t>(v.position.z)!=readWord(12))throw std::runtime_error("Native vertex disagrees with original source");
                if(batch.ich[6]==0x00A||batch.ich[6]==0x04A||batch.ich[6]==0x10A){if(std::bit_cast<std::uint32_t>(v.u)!=readWord(16)||std::bit_cast<std::uint32_t>(v.v)!=readWord(20))throw std::runtime_error("Native UV disagrees with original source");}
            }
        }
    }
    if(!reader.end())throw std::runtime_error("Unexpected native model trailing bytes");return model;
}
NativeAssembly NativeAssembly::load(const std::filesystem::path& path,std::size_t chunkCount){
    Reader reader(path);std::array<char,8> magic{};reader.bytes(magic.data(),8);
    if(magic!=std::array<char,8>{'I','D','A','S','3','A','1',0}||reader.u32()!=1)throw std::runtime_error("Unsupported native assembly format");
    auto count=reader.u32();if(!count||count>8192)throw std::runtime_error("Invalid native assembly count");
    NativeAssembly assembly;assembly.instances.resize(count);
    for(auto& instance:assembly.instances){instance.chunk=reader.u32();if(instance.chunk>=chunkCount)throw std::runtime_error("Assembly references missing model chunk");for(auto& f:instance.transform)f=reader.f32();
        if(instance.transform[12]!=0||instance.transform[13]!=0||instance.transform[14]!=0||instance.transform[15]!=1)throw std::runtime_error("Original assembly matrix is not affine");
    }
    if(!reader.end())throw std::runtime_error("Unexpected native assembly trailing bytes");return assembly;
}
NativeSpriteBank NativeSpriteBank::load(const std::filesystem::path& table,const std::filesystem::path& payload){
    auto bytes=std::filesystem::file_size(table);if(!bytes||bytes%16||bytes/16>32768)throw std::runtime_error("Invalid original RIP table size");
    if(std::filesystem::file_size(payload)!=bytes/16*112)throw std::runtime_error("Original RIP payload/table mismatch");
    Reader records(table),vertices(payload);NativeSpriteBank bank;bank.sprites.reserve(std::size_t(bytes/16));
    for(std::size_t i=0;i<bytes/16;i++){
        OriginalSprite sprite;sprite.texture=records.u32();auto count=records.u32();sprite.tagA=records.u32();sprite.tagB=records.u32();
        if(count!=4||sprite.texture>65535||sprite.tagA>255||sprite.tagB>255)throw std::runtime_error("Unsupported original RIP sprite record");
        for(auto& v:sprite.vertices){v={vertices.f32(),vertices.f32(),vertices.f32(),vertices.f32(),vertices.f32(),vertices.u32(),vertices.u32()};
            if(std::abs(v.x)>65536||std::abs(v.y)>65536||std::abs(v.u)>256||std::abs(v.v)>256)throw std::runtime_error("Original sprite bounds exceeded");
            if(v.descriptor!=(sprite.texture|(sprite.tagA<<24)|(sprite.tagB<<16)))throw std::runtime_error("Original RIP descriptor mismatch");
        }
        bank.sprites.push_back(sprite);
    }
    return bank;
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
    if(unityUiCopyImage(target.data(),width,height,image,x,y,drawWidth,drawHeight,opacity))return;
    OriginalSprite sprite;sprite.vertices={OriginalSpriteVertex{x,y,0,0,0,0xffffffff,0},{x,y+drawHeight,0,0,1,0xffffffff,0},{x+drawWidth,y,0,1,0,0xffffffff,0},{x+drawWidth,y+drawHeight,0,1,1,0xffffffff,0}};
    SpritePlacement placement;placement.opacity=opacity;compositeOriginalSprite(target,width,height,image,sprite,placement);
}
void compositeOriginalMenuChunk(std::span<std::uint32_t> target,int width,int height,
    const NativeTextureBank& textures,const NativeModelChunk& chunk,const SpritePlacement& placement,
    std::span<const Vec3> transformedPositions){
    if(width<=0||height<=0||target.size()!=std::size_t(width)*height)throw std::runtime_error("Invalid original menu destination");
    if(!transformedPositions.empty()){
        std::size_t count=0;for(const auto& batch:chunk.batches)count+=batch.vertices.size();
        if(transformedPositions.size()!=count)throw std::invalid_argument("Transformed menu positions do not match source vertices");
    }
    static const NativeImage white{1,1,{0xffffffff}};
    std::size_t vertexIndex=0;
    // Reuse capacity across material batches; the source geometry/indices stay immutable.
    std::vector<ScreenVertex> transformed;
    for(const auto& batch:chunk.batches){
        const auto tex=batch.material[9];const NativeImage& image=tex==0xffffffff?white:textures.at(tex);
        // Original UI helper 1B8240 writes GMP params=0x600 and replaces
        // VUR colors from its buffer. Constructor 1B7F80 initializes that
        // buffer to {0xFFFFFFFF,0}; raw black colors are placeholders.
        const bool uiColors=placement.defaultOriginalUiColors&&batch.ich[6]==0x4a;
        const auto params=uiColors?0x600u:batch.material[2];
        transformed.clear();transformed.reserve(batch.vertices.size());
        for(const auto& v:batch.vertices){
            const auto& position=transformedPositions.empty()?v.position:transformedPositions[vertexIndex++];
            const auto color=uiColors?0xffffffffu:(params&1)?batch.material[3]:v.color0;
            // VUR color1 is second-volume diffuse, never volume0 offset.
            const auto offset=(params&2)?batch.material[4]:0u;
            transformed.push_back({position.x*placement.scale+placement.offsetX,
                (placement.invertY?placement.authoredHeight-position.y:position.y)*placement.scale+placement.offsetY,v.u,v.v,color,offset});
        }
        for(std::size_t i=0;i<batch.indices.size();i+=3)triangle(target,width,height,image,
            transformed[batch.indices[i]],transformed[batch.indices[i+1]],transformed[batch.indices[i+2]],placement.opacity,batch.ich[2],true,batch.ich[0],placement.straightAlphaOverlay,placement.softwareOnly);
    }
}
}
