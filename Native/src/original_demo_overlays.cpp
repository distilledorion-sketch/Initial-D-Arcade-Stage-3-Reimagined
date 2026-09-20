#include "unity_ui_capture.h"
#include "original_demo_overlays.h"
#include <algorithm>
#include <bit>
#include <fstream>
#include <stdexcept>

namespace idas3::original {
namespace {
unsigned u32(std::istream& in){unsigned n=0;in.read(reinterpret_cast<char*>(&n),4);if(!in)throw std::runtime_error("Truncated original demo timeline");return n;}
float f32(std::istream& in){return std::bit_cast<float>(u32(in));}
float interpolate(float a,float b,unsigned elapsed,unsigned duration){float d=b-a;d*=float(elapsed);d/=float(duration);return a+d;}
unsigned alpha(unsigned frame,unsigned start,unsigned in,unsigned out,unsigned end,bool fade){
    if(fade?frame<=in:frame<in)return std::min(255u,unsigned(float(frame-start)/float(in-start)*255.f));
    if(fade?frame<=out:frame<out)return 255;
    return unsigned(std::max(0,int((1.f-float(frame-out)/float(end-out))*255.f)));
}
unsigned fadeOver(unsigned p,unsigned color){const unsigned a=color>>24;if(!a)return p;
    const unsigned da=p>>24,total=a*255+da*(255-a);unsigned value=((total+127)/255)<<24;
    // Straight-alpha output for the HUD blend state. Dividing by the exact
    // accumulated alpha preserves the original opaque-canvas integer result.
    for(unsigned shift:{0u,8u,16u}){
        const unsigned source=(color>>shift)&255,destination=(p>>shift)&255;
        value|=((source*a*255+destination*da*(255-a))/total)<<shift;
    }
    return value;
}
void fillFade(std::span<std::uint32_t> dst,std::uint32_t color){if(!(color>>24))return;
    for(auto& p:dst)p=fadeOver(p,color);
}
}
void OriginalDemoOverlays::load(const std::filesystem::path& root){
    const auto base=root/"data/original_assets/attract/demo_overlays";
    model_=NativeModel::load(base/"adv2d_v3/adv2d_v3.idasmesh");
    textures_=NativeTextureBank::load(base/"adv2d_v3/textures/textures.idastex");
    std::ifstream in(base/"timeline.bin",std::ios::binary);std::array<char,8> magic{};in.read(magic.data(),8);
    if(magic!=std::array<char,8>{'I','D','O','2','D','0','0','1'})throw std::runtime_error("Original demo timeline format");
    const auto count=u32(in),fadeCount=u32(in);if(count!=206||fadeCount!=6||model_.chunks.size()!=46||textures_.size()!=23)throw std::runtime_error("Unexpected original demo assets");
    cues_.clear();fades_.clear();
    for(unsigned i=0;i<count;++i){OriginalDemoOverlayCue c;c.chunk=u32(in);c.start=u32(in);c.fadeInEnd=u32(in);c.fadeOutStart=u32(in);c.end=u32(in);
        for(auto* group:{&c.from,&c.to,&c.scaleFrom,&c.scaleTo})for(auto& f:*group)f=f32(in);
        if(c.chunk>=model_.chunks.size()||c.start>c.fadeInEnd||c.fadeInEnd>c.fadeOutStart||c.fadeOutStart>c.end||c.start==c.end)throw std::runtime_error("Invalid original demo cue");cues_.push_back(c);
    }
    for(unsigned i=0;i<fadeCount;++i){OriginalDemoFadeCue c;c.rgb=u32(in);c.start=u32(in);c.fadeInEnd=u32(in);c.fadeOutStart=u32(in);c.end=u32(in);c.depth=f32(in);fades_.push_back(c);}
}
std::vector<OriginalDemoOverlayDraw> OriginalDemoOverlays::draws(unsigned frame)const{
    std::vector<OriginalDemoOverlayDraw> result;result.reserve(24);
    for(unsigned i=0;i<cues_.size();++i){const auto& c=cues_[i];if(frame<c.start||frame>=c.end)continue;
        const unsigned elapsed=frame-c.start,duration=c.end-c.start;OriginalDemoOverlayDraw d;
        d.row=i;d.chunk=c.chunk;d.alpha=alpha(frame,c.start,c.fadeInEnd,c.fadeOutStart,c.end,false);
        d.translation.x=interpolate(c.from[0],c.to[0],elapsed,duration)*.01f-3.2f;
        d.translation.y=interpolate(c.from[1],c.to[1],elapsed,duration)*.01f+2.4f;
        d.translation.z=-interpolate(c.from[2],c.to[2],elapsed,duration)*.01f-.15f;
        d.scaleX=interpolate(c.scaleFrom[0],c.scaleTo[0],elapsed,duration);d.scaleY=interpolate(c.scaleFrom[1],c.scaleTo[1],elapsed,duration);
        d.materialOverride=c.start!=c.fadeInEnd||c.fadeOutStart!=c.end;result.push_back(d);
    }return result;
}
std::vector<OriginalDemoFadeDraw> OriginalDemoOverlays::fades(unsigned frame)const{
    std::vector<OriginalDemoFadeDraw> result;result.reserve(2);if(frame==0)result.push_back({0xff000000u,-.16f});
    for(const auto& c:fades_)if(frame>c.start&&frame<c.end)result.push_back({c.rgb|(alpha(frame,c.start,c.fadeInEnd,c.fadeOutStart,c.end,true)<<24),c.depth});return result;
}
void OriginalDemoOverlays::paint(std::span<std::uint32_t> target,int width,int height,unsigned frame)const{
    paintImpl(target,width,height,frame,false);
}
void OriginalDemoOverlays::paintOverlay(std::span<std::uint32_t> target,int width,int height,unsigned frame)const{
    paintImpl(target,width,height,frame,true);
}
void OriginalDemoOverlays::paintImpl(std::span<std::uint32_t> target,int width,int height,unsigned frame,bool straightAlphaOverlay)const{
    if(width<=0||height<=0||target.size()!=std::size_t(width)*height)throw std::invalid_argument("Invalid original demo overlay destination");
    // Retain the recovered source depth ordering; fades at -.16 lie behind
    // the title/credit quads at -.15 and in front of the 3D course.
    struct Layer{NativeModelChunk chunk;float depth=0;std::uint32_t fade=0;bool isFade=false;};std::vector<Layer> layers;
    for(const auto& d:draws(frame))for(auto batch:model_.chunks[d.chunk].batches){float depth=0;
        for(auto& v:batch.vertices){v.position.x=v.position.x*d.scaleX+d.translation.x+3.2f;v.position.y=v.position.y*d.scaleY+d.translation.y-2.4f;v.position.z+=d.translation.z;depth+=v.position.z;}
        // Source Init0E69A0 enables alpha and both material-color bits through
        //0D7E20; Main0E7866 applies0D7E80's diffuse and specular color writes.
        if(d.materialOverride){batch.ich[2]|=0x001000c0;batch.material[3]=batch.material[5]=0xffffffu|(d.alpha<<24);}
        depth/=float(batch.vertices.size());Layer layer;layer.depth=depth;layer.chunk.batches.push_back(std::move(batch));layers.push_back(std::move(layer));
    }
    for(const auto& f:fades(frame)){Layer l;l.depth=f.depth;l.fade=f.argb;l.isFade=true;layers.push_back(std::move(l));}
    std::stable_sort(layers.begin(),layers.end(),[](const auto& a,const auto& b){return a.depth<b.depth;});
    // Same explicit flat model-to-canvas presentation boundary as the native
    // menu compositor; source geometry/material/UV and cue transforms remain.
    const float fit=std::min(float(width)/640,float(height)/480);SpritePlacement p;p.scale=100*fit;p.offsetX=(width-640*fit)*.5f;p.offsetY=(height-480*fit)*.5f;p.invertY=true;p.authoredHeight=0;
    p.straightAlphaOverlay=straightAlphaOverlay;
    for(const auto& l:layers)if(l.isFade){if(unityUiEnabled())unityUiSolid(target.data(),width,height,0,0,float(width),float(height),l.fade);else fillFade(target,l.fade);}else compositeOriginalMenuChunk(target,width,height,textures_,l.chunk,p);
}
void OriginalDemoOverlays::extendBackdrop(std::span<std::uint32_t> target,int width,int height,unsigned frame)const{
    if(width<=0||height<=0||target.size()!=std::size_t(width)*height)throw std::invalid_argument("Invalid original demo backdrop destination");
    auto commands=fades(frame);if(commands.empty())return;
    std::stable_sort(commands.begin(),commands.end(),[](const auto& a,const auto& b){return a.depth<b.depth;});
    // Match Frontend's integer fitted rectangle, including odd resolutions.
    const float fit=std::min(float(width)/640.f,float(height)/480.f);
    const int drawWidth=std::max(1,int(640.f*fit)),drawHeight=std::max(1,int(480.f*fit));
    const int left=(width-drawWidth)/2,top=(height-drawHeight)/2;
    if(unityUiEnabled()){for(const auto& fade:commands){
        unityUiSolid(target.data(),width,height,0,0,float(width),float(top),fade.argb);
        unityUiSolid(target.data(),width,height,0,float(top+drawHeight),float(width),float(height-top-drawHeight),fade.argb);
        unityUiSolid(target.data(),width,height,0,float(top),float(left),float(drawHeight),fade.argb);
        unityUiSolid(target.data(),width,height,float(left+drawWidth),float(top),float(width-left-drawWidth),float(drawHeight),fade.argb);
    }return;}
    for(const auto& fade:commands)for(int y=0;y<height;++y){auto row=target.subspan(std::size_t(y)*width,width);
        if(y<top||y>=top+drawHeight)fillFade(row,fade.argb);
        else {fillFade(row.first(left),fade.argb);fillFade(row.subspan(left+drawWidth),fade.argb);}
    }
}
}
