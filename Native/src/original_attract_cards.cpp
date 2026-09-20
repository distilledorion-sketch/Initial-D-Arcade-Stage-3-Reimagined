#include "unity_ui_capture.h"
#include "original_attract_cards.h"
#include <algorithm>
#include <bit>
#include <stdexcept>

namespace idas3::original {
namespace {
float word(unsigned value){return std::bit_cast<float>(value);}
unsigned fadeFraction(unsigned frame,unsigned duration){return unsigned(float(std::int32_t(frame))/float(duration)*255.f);}
}
void resetOriginalAttractCard(OriginalAttractCardState& s,unsigned child){
    if(child<3||child>5)throw std::invalid_argument("Original attract card child");
    s={};s.child=child;s.background=child==5?0:0xffffff;s.fade=child==4?0:255;
}
void stepOriginalAttractCard(OriginalAttractCardState& s,const OriginalAttractCardReadiness& ready){
    if(s.completed)return;s.displayedFrame=s.frame;
    if(s.child==3){
        const auto advance=[&]{++s.phase;s.frame=0;};
        switch(s.phase){
        case 0:s.fade=255-fadeFraction(std::min(s.frame,30u),30);s.fadeRgb=0;if(s.frame==33)advance();break;
        case 1:s.fade=s.fadeRgb=0;if(ready.resourcesReady)advance();break;
        case 2:s.fade=s.fadeRgb=0;if(!ready.networkWaiting)advance();break;
        case 3:s.fade=s.fadeRgb=0;if(s.frame==180)advance();break;
        case 4:s.fade=255-fadeFraction(30-std::min(s.frame,30u),30);s.fadeRgb=0xffffff;if(s.frame==33)advance();break;
        case 5:if(!ready.cardEnabled||ready.cardReady)advance();break;
        case 6:s.completed=true;break;
        default:throw std::invalid_argument("Original Sega phase");
        }
    }else if(s.child==4){
        //073960's authored polygon frame selection and white flash.
        s.fade=s.fadeRgb=0;s.logoPanel=false;s.meshFrame=0;
        if(s.frame<=139)s.meshFrame=s.frame;
        else if(s.frame<=169){s.meshFrame=s.frame;s.fade=fadeFraction(s.frame-140,30);s.fadeRgb=0xffffff;}
        else if(s.frame<=354){s.meshFrame=169;s.logoPanel=true;if(s.frame<=174){s.fade=fadeFraction(5-(s.frame-170),5);s.fadeRgb=0xffffff;}}
        else if(s.frame<=389){s.meshFrame=s.frame-180;const unsigned gray=((390-s.frame)*255)/40;s.background=gray|(gray<<8)|(gray<<16);}
        if(s.frame+1==390)s.completed=true;
    }else if(s.child==5){
        s.fade=s.fadeRgb=0;
        if(s.phase==0){if(s.frame<=30)s.fade=255-fadeFraction(s.frame,30);if(s.frame==30){s.phase=1;s.frame=0;}}
        else if(s.phase==1){if(s.frame==120){s.phase=2;s.frame=0;}}
        else if(s.phase==2){if(s.frame<=30)s.fade=fadeFraction(s.frame,30);if(s.frame>=30)s.completed=true;}
        else throw std::invalid_argument("Original Caution phase");
    }else throw std::invalid_argument("Original attract card child");
    ++s.frame;
}
std::uint32_t originalAttractCardFadeArgb(const OriginalAttractCardState& s){return(s.fade<<24)|s.fadeRgb;}
float originalAttractCardProjectionScale(){
    //1FA280(0x1000), the half-angle of1D0940's0x2000 camera. This is
    // the original rational tangent rather than the host tan() library.
    float angle=float(4096)*word(0x40c90fdb);angle*=word(0x37800000);
    float quarter=angle/word(0x3fc90fdb);quarter+=.5f;const int quadrant=int(quarter);
    angle-=float(quadrant)*word(0x3fc90fdb);const float square=angle*angle;float term=0;
    for(int divisor=13;divisor>2;divisor-=2){const float denominator=float(divisor)-term;term=square/denominator;}
    const float tangent=angle/(1.f-term),cotangent=1.f/tangent;
    // Source camera aspect1D0B34 and640-pixel viewport. Rosso translation
    // puts all selected source animation vertices at the same depth6.
    return ((cotangent/word(0x3faaaaab))*320.f)/6.f;
}
void OriginalAttractCards::load(const std::filesystem::path& root){
    const auto base=root/"data/original_assets/attract/cards";
    sega_=NativeModel::load(base/"segalogo/segalogo.idasmesh");
    rosso_=NativeModel::load(base/"segarosso/segarosso.idasmesh");
    rossoPanel_=NativeModel::load(base/"segarosso2d/segarosso2d.idasmesh");
    caution_=NativeModel::load(base/"adv_caution/adv_caution.idasmesh");
    segaTextures_=NativeTextureBank::load(base/"segalogo/textures/textures.idastex");
    rossoPanelTextures_=NativeTextureBank::load(base/"segarosso2d/textures/textures.idastex");
    cautionTextures_=NativeTextureBank::load(base/"adv_caution/textures/textures.idastex");
    if(sega_.chunks.size()!=1||rosso_.chunks.size()!=211||rossoPanel_.chunks.size()!=1||caution_.chunks.size()!=1)throw std::runtime_error("Unexpected original card model count");
    // Untextured Rosso animation is planar, so the projection has constant
    // depth. The existing source-material compositor is exact for that case.
    for(unsigned i=0;i<210;++i)for(const auto& b:rosso_.chunks[i].batches){if(b.material[9]!=0xffffffff)throw std::runtime_error("Unexpected Rosso texture");for(const auto& v:b.vertices)if(v.position.z!=0)throw std::runtime_error("Unexpected nonplanar Rosso animation");}
}
void OriginalAttractCards::paint(std::span<std::uint32_t> target,int width,int height)const{
    if(width<=0||height<=0||target.size()!=std::size_t(width)*height)throw std::invalid_argument("Invalid original card destination");
    std::fill(target.begin(),target.end(),0xff000000u|state_.background);unityUiClear(target.data(),width,height,0xff000000u|state_.background);
    const float fit=std::min(float(width)/640,float(height)/480),ox=(width-640*fit)*.5f,oy=(height-480*fit)*.5f;
    SpritePlacement p;p.scale=100*fit;p.offsetX=ox;p.offsetY=oy;p.invertY=true;p.authoredHeight=0;
    if(state_.child==3)compositeOriginalMenuChunk(target,width,height,segaTextures_,sega_.chunks.at(0),p);
    else if(state_.child==5)compositeOriginalMenuChunk(target,width,height,cautionTextures_,caution_.chunks.at(0),p);
    else if(state_.child==4){
        SpritePlacement projected=p;projected.scale=originalAttractCardProjectionScale()*fit;projected.offsetX=ox+320*fit;projected.offsetY=oy+240*fit;
        compositeOriginalMenuChunk(target,width,height,segaTextures_,rosso_.chunks.at(state_.meshFrame),projected);
        // Source2D panel is closer than Rosso's worldZ=-6, so it covers the
        // polygon animation even though the original submits it first.
        if(state_.logoPanel)compositeOriginalMenuChunk(target,width,height,rossoPanelTextures_,rossoPanel_.chunks.at(0),p);
    }
}
}
