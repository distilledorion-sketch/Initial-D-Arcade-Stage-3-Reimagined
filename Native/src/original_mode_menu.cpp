#include "unity_ui_capture.h"
#include "original_mode_menu.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <map>
#include <stdexcept>

namespace idas3::original {
namespace {
struct ModeRecord {float x,y,z;std::uint32_t scene,title,glow,confirmation;};
// Original2A1478 three28-byte records, bank19selectors stripped to chunk IDs.
constexpr std::array<ModeRecord,3> records={{
    {3.919999837875366f,-1.4399999380111694f,.009999999776482582f,16,7,11,12},
    {2.879999876022339f,-2.3999998569488525f,.009999999776482582f,17,8,13,14},
    {4.f,-3.359999895095825f,.009999999776482582f,15,6,9,10}}};
std::size_t index(OriginalGameMode mode){const auto i=std::uint32_t(mode);if(i>=3)throw std::out_of_range("Original game mode outside0..2");return i;}
std::uint32_t shade(float multiplier) {
    const auto v=std::uint32_t(std::int32_t(255.f*multiplier));
    return 0xff000000u|v|(v<<8)|(v<<16);
}
// Same nearest-integer operation as lround for these bounded color channels.
// The double addition is intentional: float(value+.5f) can round a value just
// below a half upward before conversion, changing the existing raster output.
std::uint32_t channel(float value){return std::uint32_t(std::clamp(int(double(value)+.5),0,255));}
std::uint32_t preparedBlend(std::uint32_t dst,std::uint32_t src,std::uint32_t tsp){
    const unsigned source=tsp>>29,destination=(tsp>>26)&7;
    if(source==1&&destination==0)return src;
    if(source==4&&destination==5){if((src>>24)==255)return src;if((src>>24)==0)return dst;}
    // Adding two normalized eight-bit channels and rounding back always gives
    // their saturating integer sum; no multiplication or variable alpha here.
    if(source==1&&destination==1){std::uint32_t result=0;for(int shift:{0,8,16,24})result|=std::min(255u,((src>>shift)&255)+((dst>>shift)&255))<<shift;return result;}
    const float sa=float(src>>24)/255,da=float(dst>>24)/255;
    auto factor=[&](unsigned mode,float opposite){switch(mode){case 0:return 0.f;case 1:return 1.f;case 2:return opposite;case 3:return 1-opposite;case 4:return sa;case 5:return 1-sa;case 6:return da;default:return 1-da;}};
    std::uint32_t result=0;
    for(int shift:{0,8,16,24}){float sc=float((src>>shift)&255)/255,dc=float((dst>>shift)&255)/255;if(shift==24)sc=sa;
        const float value=sc*factor(source,dc)+dc*factor(destination,sc);result|=channel(value*255)<<shift;}
    return result;
}
struct PreparedVertex {float x,y,u,v;std::uint32_t color,offset;};
float preparedEdge(const PreparedVertex& a,const PreparedVertex& b,float x,float y){return (b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);}
bool preparedTopLeft(const PreparedVertex& a,const PreparedVertex& b){return a.y>b.y||(a.y==b.y&&a.x<b.x);}
std::uint32_t preparedShade(std::uint32_t texel,float wa,float wb,float wc,
    std::uint32_t ca,std::uint32_t cb,std::uint32_t cc,std::uint32_t offset,std::uint32_t tsp,std::uint32_t pcw){
    const unsigned env=(tsp>>6)&3;
    if((pcw&8)&&env!=2&&!offset&&(env==0||(ca==0xffffffffu&&cb==ca&&cc==ca)))return texel;
    const float alpha=float(texel>>24)/255;
    std::uint32_t result=0;
    for(int shift:{0,8,16,24}){
        float vertex=wa*((ca>>shift)&255)+wb*((cb>>shift)&255)+wc*((cc>>shift)&255);
        if(shift==24&&!(tsp&(1u<<20)))vertex=255;
        const float sample=float((texel>>shift)&255);float value=vertex;
        if(pcw&8){if(env==0)value=sample;else if(env==1)value=shift==24?sample:vertex*sample/255;
            else if(env==2)value=shift==24?vertex:vertex*(1-alpha)+sample*alpha;else value=vertex*sample/255;}
        if(shift!=24&&((pcw&4)||!(pcw&8)))value+=wa*((offset>>shift)&255)+wb*((offset>>shift)&255)+wc*((offset>>shift)&255);
        result|=channel(value)<<shift;
    }
    return result;
}
}
struct OriginalModeMenu::RasterCache {
    struct Pixel {std::uint32_t index,sample;};
    struct ColorPixel {std::uint32_t index,sample;float wa,wb,wc;};
    struct Batch {std::uint32_t pcw,tsp,offset;std::vector<Pixel> fixed;std::vector<ColorPixel> tinted;};
    struct Entry {std::vector<Batch> batches;std::uint64_t used=0;std::size_t bytes=0;};
    using Key=std::array<std::uint32_t,6>;
    std::map<Key,Entry> entries;std::uint64_t serial=0;std::size_t bytes=0;
    static constexpr std::size_t budget=32u*1024u*1024u;
    // Prepared fragments retain primitive order and exact barycentric values.
    // Only coverage/UV sampling is cached; tint and blending still run for each
    // current source color. Fixed-color fragments may retain their source RGBA.
    const Entry& prepare(const OriginalModeMenuDraw& draw,const NativeModelChunk& chunk,const NativeTextureBank& textures){
        const Key key={draw.bank,draw.chunk,std::bit_cast<std::uint32_t>(draw.x),std::bit_cast<std::uint32_t>(draw.y),std::bit_cast<std::uint32_t>(draw.scale),std::uint32_t(draw.replaceVertexColors)|(std::uint32_t(draw.replaceMaterialDiffuse)<<1)};
        if(auto found=entries.find(key);found!=entries.end()){found->second.used=++serial;return found->second;}
        Entry entry;entry.used=++serial;
        static const NativeImage white{1,1,{0xffffffffu}};
        for(const auto& source:chunk.batches){
            Batch batch{source.ich[0],source.ich[2],0};
            const auto params=draw.replaceVertexColors?0x600u:source.material[2];
            const bool tint=(draw.replaceVertexColors&&source.ich[6]==0x4a)||(draw.replaceMaterialDiffuse&&(params&1));
            batch.offset=(params&2)?source.material[4]:0u;
            const auto& image=source.material[9]==0xffffffffu?white:textures.at(source.material[9]);
            std::vector<PreparedVertex> vertices;
            const float scale=100.f*draw.scale,ox=draw.x*100.f,oy=-draw.y*100.f;
            for(const auto& v:source.vertices)vertices.push_back({v.position.x*scale+ox,(0.f-v.position.y)*scale+oy,v.u,v.v,
                tint?0xffffffffu:(params&1)?source.material[3]:v.color0,batch.offset});
            for(std::size_t i=0;i<source.indices.size();i+=3){
                auto a=vertices[source.indices[i]],b=vertices[source.indices[i+1]],c=vertices[source.indices[i+2]];
                float area=preparedEdge(a,b,c.x,c.y);if(std::abs(area)<1e-7f)continue;if(area<0){std::swap(b,c);area=-area;}
                const int minx=std::max(0,int(std::floor(std::min({a.x,b.x,c.x})))),maxx=std::min(640,int(std::ceil(std::max({a.x,b.x,c.x}))));
                const int miny=std::max(0,int(std::floor(std::min({a.y,b.y,c.y})))),maxy=std::min(480,int(std::ceil(std::max({a.y,b.y,c.y}))));
                for(int y=miny;y<maxy;++y)for(int x=minx;x<maxx;++x){
                    const float px=x+.5f,py=y+.5f,ea=preparedEdge(b,c,px,py),eb=preparedEdge(c,a,px,py),ec=preparedEdge(a,b,px,py);
                    if(ea<0||eb<0||ec<0||(ea==0&&!preparedTopLeft(b,c))||(eb==0&&!preparedTopLeft(c,a))||(ec==0&&!preparedTopLeft(a,b)))continue;
                    const float wa=ea/area,wb=eb/area,wc=ec/area;
                    const float u=wa*a.u+wb*b.u+wc*c.u,v=wa*a.v+wb*b.v+wc*c.v;
                    auto coord=[](float uv,std::uint32_t size,bool clamp,bool mirror){if(clamp)return std::uint32_t(std::clamp(int(std::floor(uv*size)),0,int(size)-1));
                        const float base=std::floor(uv);float fraction=uv-base;if(mirror&&(int(base)&1))fraction=1-fraction;
                        return std::uint32_t(std::clamp(int(std::floor(fraction*size)),0,int(size)-1));};
                    const auto tx=coord(u,image.width,(batch.tsp&(1u<<16))!=0,(batch.tsp&(1u<<18))!=0),ty=coord(v,image.height,(batch.tsp&(1u<<15))!=0,(batch.tsp&(1u<<17))!=0);
                    auto sample=image.argb[std::size_t(ty)*image.width+tx];if(batch.tsp&(1u<<19))sample|=0xff000000u;
                    const auto index=std::uint32_t(y*640+x);
                    if(tint)batch.tinted.push_back({index,sample,wa,wb,wc});
                    else batch.fixed.push_back({index,preparedShade(sample,wa,wb,wc,a.color,b.color,c.color,batch.offset,batch.tsp,batch.pcw)});
                }
            }
            batch.fixed.shrink_to_fit();batch.tinted.shrink_to_fit();
            entry.bytes+=batch.fixed.capacity()*sizeof(Pixel)+batch.tinted.capacity()*sizeof(ColorPixel)+sizeof(Batch);
            entry.batches.push_back(std::move(batch));
        }
        if(entry.bytes>budget)throw std::runtime_error("Mode prepared raster exceeds bounded cache");
        while(bytes+entry.bytes>budget){auto oldest=std::min_element(entries.begin(),entries.end(),[](const auto&a,const auto&b){return a.second.used<b.second.used;});bytes-=oldest->second.bytes;entries.erase(oldest);}
        bytes+=entry.bytes;return entries.emplace(key,std::move(entry)).first->second;
    }
    void paint(std::span<std::uint32_t> target,const Entry& entry,std::uint32_t color)const{
        for(const auto& b:entry.batches){
            for(const auto& p:b.fixed)target[p.index]=preparedBlend(target[p.index],p.sample,b.tsp);
            for(const auto& p:b.tinted){const auto src=preparedShade(p.sample,p.wa,p.wb,p.wc,color,color,color,b.offset,b.tsp,b.pcw);target[p.index]=preparedBlend(target[p.index],src,b.tsp);}
        }
    }
};
std::size_t OriginalModeMenu::preparedRasterBytes()const{return raster?raster->bytes:0;}
void selectOriginalGameMode(OriginalModeMenuState& state,OriginalGameMode mode) {
    const auto i=index(mode);
    if(state.selected==mode)return;
    state.selected=mode;state.focusFrames[i]=0;state.focusValues[i]=0;state.selectedFrames=0;
}
void setOriginalModeConfirmation(OriginalModeMenuState& state,float phase) {
    if(!std::isfinite(phase))throw std::invalid_argument("Nonfinite original mode phase");
    state.confirmationPhase=std::clamp(phase,0.f,1.f);
}
void stepOriginalModeMenu(OriginalModeMenuState& state) {
    const auto i=index(state.selected);
    state.focusFrames[i]=std::min(state.focusFrames[i]+1,20);
    state.focusValues[i]=float(state.focusFrames[i])*.05000000074505806f;
    ++state.selectedFrames;
}
std::vector<OriginalModeMenuDraw> originalModeMenuDraws(const OriginalModeMenuState& state) {
    const auto selected=index(state.selected);
    if(!std::isfinite(state.confirmationPhase)||state.confirmationPhase<0||state.confirmationPhase>1)throw std::invalid_argument("Original mode phase outside0..1");
    std::vector<OriginalModeMenuDraw> out;
    auto draw=[&](std::uint32_t chunk,float x=0,float y=0,float scale=1,std::uint32_t color=0xffffffffu,bool replace=false,std::uint32_t bank=19){out.push_back({bank,chunk,x,y,scale,color,replace});};
    draw(54,0,0,1,0xffffffffu,false,8);draw(0);draw(18);
    const float phase=state.confirmationPhase;
    const float twice=phase+phase;
    const auto sceneShade=shade(std::max((1.f-twice)*.30000001192092896f,0.f));
    const auto titleShade=shade(std::max((1.f-twice)*.5f,0.f));
    const float alpha=std::min(2.f-twice,1.f);
    const auto alphaByte=std::uint32_t(std::int32_t(255.f*alpha));
    const auto confirmationColor=alphaByte|(alphaByte<<8)|(alphaByte<<16)|(alphaByte<<24);
    const float selectedScale=std::min(std::fma(float(state.selectedFrames)/6.f,.019999999552965164f,.9800000190734863f),1.f);
    for(std::size_t i=0;i<records.size();++i) {
        const auto& r=records[i];
        if(i==selected) {
            draw(r.scene,0,0,1,0xffffffffu,true);
            if(phase<.009999999776482582f || phase>.5f || (state.selectedFrames&6u))draw(r.glow,r.x,r.y,selectedScale);
            draw(r.title,r.x,r.y,selectedScale,0xffffffffu,true);
            if(phase>0 && (phase<.009999999776482582f || phase>.5f || (state.selectedFrames&6u)))
                {draw(r.confirmation,r.x,r.y,1,confirmationColor,false);out.back().replaceMaterialDiffuse=true;}
        } else {
            draw(r.scene,0,0,1,sceneShade,true);
            draw(r.title,r.x,r.y,.699999988079071f,titleShade,true);
        }
    }
    draw(4);draw(5);draw(std::uint32_t(selected)+1);
    return out;
}
void OriginalModeMenu::load(const std::filesystem::path& root) {
    const auto folder=root/"data/original_assets/menus/v3";
    common=NativeModel::load(folder/"v3sS00common/v3sS00common.idasmesh");
    commonTextures=NativeTextureBank::load(folder/"v3sS00common/textures/textures.idastex");
    mode=NativeModel::load(folder/"v3sS11mode/v3sS11mode.idasmesh");
    modeTextures=NativeTextureBank::load(folder/"v3sS11mode/textures/textures.idastex");
    const std::array<const char*,2> names={"select0402","mode_bunta"};
    for(std::size_t i=0;i<names.size();++i){
        const auto p=root/"data/original_assets/menus/warnings"/names[i];
        warnings[i]=NativeModel::load(p/(std::string(names[i])+".idasmesh"));
        warningTextures[i]=NativeTextureBank::load(p/"textures/textures.idastex");
    }
    previousKey.clear();pixels.clear();originalCanvas.clear();
    raster=std::make_shared<RasterCache>();
}
const std::vector<std::uint32_t>& OriginalModeMenu::paint(int width,int height,const OriginalModeMenuState& state,std::array<int,2> warningChunks) {
    if(width<=0||height<=0||width>16384||height>16384)throw std::invalid_argument("Invalid original mode menu dimensions");
    const std::vector<std::uint32_t> key={std::uint32_t(width),std::uint32_t(height),std::uint32_t(state.selected),std::bit_cast<std::uint32_t>(state.confirmationPhase),std::min(state.selectedFrames,6u),state.confirmationPhase>0?(state.selectedFrames&6u):0u,std::uint32_t(warningChunks[0]),std::uint32_t(warningChunks[1])};
    if(key==previousKey)return pixels;
    // Rasterize at the original640x480 resolution. Repeated full-HD software
    // material blending is unnecessary; scale the finished original frame once.
    // Settled states reuse this complete image without an animation repaint.
    renderOriginalCanvas(state,warningChunks);
    pixels.assign(std::size_t(width)*height,0xff000000u);unityUiClear(pixels.data(),width,height,0xff000000);
    const float fit=std::min(float(width)/640.f,float(height)/480.f);
    const int drawWidth=std::max(1,int(640.f*fit)),drawHeight=std::max(1,int(480.f*fit));
    const int left=(width-drawWidth)/2,top=(height-drawHeight)/2;
    unityUiCopy(pixels.data(),originalCanvas.data(),width,height,float(drawWidth)/640,float(drawHeight)/480,float(left),float(top),true);
    if(!unityUiEnabled()){
    std::vector<int> columns(std::size_t(drawWidth),0);
    for(int x=0;x<drawWidth;++x)columns[std::size_t(x)]=x*640/drawWidth;
    for(int y=0;y<drawHeight;++y) {
        const auto* row=originalCanvas.data()+std::size_t(y*480/drawHeight)*640;
        auto* destination=pixels.data()+std::size_t(top+y)*width+left;
        for(int x=0;x<drawWidth;++x)destination[x]=row[columns[std::size_t(x)]];
    }
    }
    previousKey=key;
    return pixels;
}
void OriginalModeMenu::renderOriginalCanvas(const OriginalModeMenuState& state,std::array<int,2> warningChunks) {
    constexpr int width=640,height=480;
    originalCanvas.assign(width*height,0xff000000u);unityUiClear(originalCanvas.data(),width,height,0xff000000);
    auto draws=originalModeMenuDraws(state);
    for(const auto& command:draws) {
        const auto& model=command.bank==8?common:mode;
        const auto& textures=command.bank==8?commonTextures:modeTextures;
        if(command.chunk>=model.chunks.size())throw std::out_of_range("Original mode artwork bank missing");
        if(unityUiEnabled()){
            auto chunk=model.chunks[command.chunk];
            for(auto& batch:chunk.batches){const auto params=command.replaceVertexColors?0x600u:batch.material[2];
                const bool tint=(command.replaceVertexColors&&batch.ich[6]==0x4a)||(command.replaceMaterialDiffuse&&(params&1));
                batch.material[2]=params;if(tint){if(params&1)batch.material[3]=command.color;else for(auto& v:batch.vertices)v.color0=command.color;}}
            SpritePlacement p;p.scale=100.f*command.scale;p.offsetX=100.f*command.x;p.offsetY=-100.f*command.y;p.invertY=true;p.authoredHeight=0;
            compositeOriginalMenuChunk(originalCanvas,width,height,textures,chunk,p);continue;
        }
        const auto& prepared=raster->prepare(command,model.chunks[command.chunk],textures);
        raster->paint(originalCanvas,prepared,command.color);
    }
    // Legacy145920 warning banks retain their authored positions, negative
    // UVs and material state. Use the same flat640x480 presentation boundary
    // as the other legacy HUD/menu banks; keep the source bank order.
    SpritePlacement p;p.scale=100;p.invertY=true;p.authoredHeight=0;
    for(std::size_t i=0;i<warningChunks.size();++i)if(warningChunks[i]>=0)
        compositeOriginalMenuChunk(originalCanvas,width,height,warningTextures[i],warnings[i].chunks.at(std::size_t(warningChunks[i])),p);
}
}
