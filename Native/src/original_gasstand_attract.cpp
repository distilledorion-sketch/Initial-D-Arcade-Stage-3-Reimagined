#include "unity_ui_capture.h"
#include "original_menu_backdrop.h"
#include "original_gasstand_attract.h"
#include "original_gasstand_data.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace idas3::original {
namespace {
using namespace gasstand_data;
std::span<const Message> script(unsigned id){switch(id){case 0:return script0;case 1:return script1;case 2:return script2;default:throw std::invalid_argument("Invalid original gasstand script");}}
int glyph(char c){const auto it=std::find(fontCharacters.begin(),fontCharacters.end(),c);return it==fontCharacters.end()?-1:int(it-fontCharacters.begin());}
unsigned lineCount(const Message& m){return !m.lines[2].empty()?3:!m.lines[1].empty()?2:1;}
void validate(const OriginalGasstandState& s){const auto rows=script(s.script);if(s.phase>3||s.message>rows.size()||s.speaker>2||s.fade<0||s.fade>15||!std::isfinite(s.brandX)||!std::isfinite(s.bubbleX)||!std::isfinite(s.bubbleY))throw std::invalid_argument("Invalid original gasstand state");}

}
void resetOriginalGasstand(OriginalGasstandState& s,unsigned id){(void)script(id);s={};s.script=id;s.brandX=gasstand_data::lit_0C0D095C;}
unsigned nextOriginalGasstandScript(unsigned previous,bool skipBattle,bool skipCustomization){if(previous>2)throw std::invalid_argument("Invalid prior gasstand script");unsigned next=(previous+1)%3;if(skipCustomization&&next==0)next=1;if(skipBattle&&next==1)next=2;return next;}
void stepOriginalGasstand(OriginalGasstandState& s){
    validate(s);if(s.completed)return;const auto rows=script(s.script);
    //0D0D20 four-state Main. The renderer also owns two source animation
    // writes, so advance those once here rather than at host refresh rate.
    switch(s.phase){
    case 0:if(--s.fade<0){s.fade=0;s.fadeEnabled=false;++s.phase;}break;
    case 1:{
        const auto& row=rows[s.message];s.speaker=row.speaker;s.bubble=true;
        s.bubbleX=gasstand_data::speakerXY[s.speaker][0]*gasstand_data::lit_0C0D0F30-gasstand_data::lit_0C0D0F34;
        s.bubbleY=std::fma(-gasstand_data::speakerXY[s.speaker][1],gasstand_data::lit_0C0D0F30,gasstand_data::lit_0C0D0F38);
        if(s.script==0&&s.message==6)s.brands=true;
        if(s.script==2&&s.message==7)s.ranking=true;
        ++s.phase;break;}
    case 2:{unsigned count=0;for(const auto line:rows[s.message].lines)count+=unsigned(line.size());
        if(++s.elapsed>count*3+40){s.elapsed=0;++s.message;s.bubble=false;
            if(s.message>=rows.size()){++s.phase;s.fadeEnabled=true;}else s.phase=1;}
        break;}
    case 3:if(++s.fade>15){s.fade=15;s.completed=true;}break;
    }
    s.displayedBackgroundPhase=s.backgroundPhase;
    if(float(++s.backgroundPhase)*gasstand_data::lit_0C112E28>=gasstand_data::lit_0C112F88)s.backgroundPhase=0;
    if(s.script==0&&s.brands)s.brandX-=gasstand_data::lit_0C0D14F0;
}
std::uint32_t originalGasstandFadeArgb(const OriginalGasstandState& s){validate(s);return s.fadeEnabled?std::uint32_t(float(s.fade)/15.f*255.f)<<24:0;}
float originalAlphabetTextWidth(std::string_view text){float sum=0;for(const char c:text){const int i=glyph(c);sum=std::fma(i<0?5.f:gasstand_data::word(gasstand_data::fontWidthWords[std::size_t(i)]),1.f,sum);}return sum;}
std::vector<OriginalGasstandDraw> originalGasstandDraws(const OriginalGasstandState& s){
    validate(s);using namespace gasstand_data;using Bank=OriginalGasstandDraw::Bank;
    std::vector<OriginalGasstandDraw> out;out.reserve(180);
    const auto draw=[&](Bank bank,unsigned i,Vec3 pos=Vec3{},float sx=1,float sy=1){out.push_back({bank,i,pos,sx,sy});};
    const float phase=float(s.displayedBackgroundPhase)*lit_0C112E28;
    //112E60 repeats original tile57: columns-1..2, rows-1..1. Its setter
    // uses Z=-100, with145920's depth-compensating scale at draw time.
    for(int row=-1;row<2;++row)for(int col=-1;col<3;++col){
        const float x=std::fma(float(col),lit_0C112F88,phase),y=std::fma(float(row),lit_0C112F88,phase);
        // Keep getter/setter single-precision roundoff at the source origin.
        const float originX=word(0xc04ccccc),originY=word(0x40199999);
        draw(Bank::gasstand,57,{(originX+x)-originX,(originY-y)-originY,-100.f-word(0xbe19999a)});
    }
    draw(Bank::gasstand,13,{0,lit_0C0D1130,0});
    const auto rows=script(s.script);
    const Message* message=s.message<rows.size()?&rows[s.message]:nullptr;
    const unsigned lines=message?lineCount(*message):1;
    if(s.phase==2&&message){
        constexpr std::array<std::array<float,3>,3> lineOffsets{{{-10,0,0},{-36,-10,0},{-36,-10,16}}};
        for(unsigned line=0;line<lines;++line){
            float advance=0;const float x=speakerXY[s.speaker][0],y=speakerXY[s.speaker][1]+lineOffsets[lines-1][line]+24.f;
            for(const char c:message->lines[line]){const int i=glyph(c);
                if(i>=0){draw(Bank::alphabet,unsigned(i),{x+advance,y,gasstand_data::word(0x3727c5ac)});
                    //1CB200 color4 with dimming argument-1 yieldsRGB010101;
                    //1E7E80 restores full alpha from descriptor opacity1.
                    out.back().color=0xff010101;}
                advance=std::fma(i<0?5.f:word(fontWidthWords[std::size_t(i)]),1.f,advance);
            }
        }
    }
    if(s.bubble&&message){
        const unsigned left=lines==1?0:lines==2?3:7,middle=lines==1?1:lines==2?4:8,right=lines==1?5:lines==2?6:9;
        Vec3 p{s.bubbleX,s.bubbleY,0};if(lines==1)p.y-=lit_0C0D114C;
        draw(Bank::gasstand,left,p,lit_0C0D1208,lit_0C0D1208);
        Vec3 tail{s.bubbleX,s.bubbleY-lit_0C0D1210,0};
        if(lines==1)tail.y+=lit_0C0D1214;else if(lines==3)tail.y-=lit_0C0D13B0;
        tail.x+=lit_0C0D13B4;if(s.speaker==1)tail.x+=lit_0C0D13B8;
        draw(Bank::gasstand,2,tail,lit_0C0D13BC,lit_0C0D13C0);
        p.x+=lit_0C0D13C4;float widest=0;for(auto line:message->lines)widest=std::max(widest,originalAlphabetTextWidth(line));
        draw(Bank::gasstand,middle,p,lit_0C0D13CC*widest,lit_0C0D13C0);
        p.x=std::fma(widest*lit_0C0D13D0,lit_0C0D13D4,p.x);
        draw(Bank::gasstand,right,p,lit_0C0D13C0,lit_0C0D13C0);
    }
    draw(Bank::etc,0);draw(Bank::etc,1,{0,lit_0C0D13D8,0});
    switch(s.script){
    case 0:
        if(s.brands)for(unsigned i=0;i<brandOrder.size();++i){
            const float x=std::fma(float(i/2),lit_0C0D14F8,s.brandX);
            const float y=-float(i%2)*lit_0C0D14FC-lit_0C0D1500;
            draw(Bank::gasstand,brandOrder[i]+14,{x,y,0});
        }
        draw(Bank::etc,2);break;
    case 1:draw(Bank::etc,3);break;
    case 2:if(s.ranking)draw(Bank::gasstand,56);draw(Bank::etc,4);break;
    }
    return out;
}
bool OriginalGasstandAttract::available(const std::filesystem::path& root){
    for(const char* file:{"data/original_assets/attract/gasstand/gasstand.idasmesh","data/original_assets/attract/gasstand/textures/textures.idastex","data/original_assets/attract/alphabet/textures/textures.idastex","data/original_assets/menus/v3/v3sA00etc/v3sA00etc.idasmesh","data/original_assets/menus/v3/v3sA00etc/textures/textures.idastex"})
        if(!std::filesystem::is_regular_file(root/file))return false;
    return true;
}
void OriginalGasstandAttract::load(const std::filesystem::path& root){
    const auto gas=root/"data/original_assets/attract/gasstand",etc=root/"data/original_assets/menus/v3/v3sA00etc";
    gasstand_=NativeModel::load(gas/"gasstand.idasmesh");gasstandTextures_=NativeTextureBank::load(gas/"textures/textures.idastex");
    etc_=NativeModel::load(etc/"v3sA00etc.idasmesh");etcTextures_=NativeTextureBank::load(etc/"textures/textures.idastex");
    alphabet_=NativeTextureBank::load(root/"data/original_assets/attract/alphabet/textures/textures.idastex");
    if(gasstand_.chunks.size()!=58||gasstandTextures_.sourceSize()!=15||alphabet_.size()!=62||etc_.chunks.size()<5)throw std::runtime_error("Original gasstand asset identity mismatch");reset();
    logoBackdrop_=buildMenuLogoBackdrop(gasstandTextures_,gasstand_.chunks.at(57));
    preparedAlphabet_.clear();preparedAlphabet_.reserve(alphabet_.size());
    for(unsigned i=0;i<alphabet_.size();++i){auto glyph=alphabet_.at(i);
        for(auto& color:glyph.argb){if((color&0xffffff)!=0xffffff)throw std::runtime_error("Original alphabet expects white source glyphs");color=(color&0xff000000)|0x010101;}
        preparedAlphabet_.push_back(std::move(glyph));}
}
void OriginalGasstandAttract::paint(std::span<std::uint32_t> target,int width,int height)const{paintImpl(target,width,height,true);}
void OriginalGasstandAttract::paintReference(std::span<std::uint32_t> target,int width,int height)const{paintImpl(target,width,height,false);}
void OriginalGasstandAttract::paintText(std::span<std::uint32_t> target,int width,int height,std::string_view text,float x,float y,float scale,std::uint32_t color)const{
    SpritePlacement p;p.scale=scale;p.offsetX=x;p.offsetY=y;
    float advance=0;
    for(const char c:text){const int i=glyph(c);if(i>=0){
        OriginalSprite sprite;sprite.vertices={OriginalSpriteVertex{advance,0,0,0,1,color,0},{advance,32,0,0,0,color,0},{advance+32,0,0,1,1,color,0},{advance+32,32,0,1,0,color,0}};
        compositeOriginalSprite(target,width,height,alphabet_.at(unsigned(i)),sprite,p);
        advance+=gasstand_data::word(gasstand_data::fontWidthWords[unsigned(i)]);
    }else advance+=5;}
}
void OriginalGasstandAttract::paintMenuMargins(std::span<std::uint32_t> target,int width,int height,std::uint64_t frame)const{
    paintMenuLogoBackdrop(target,width,height,logoBackdrop_,frame,true);
}
void OriginalGasstandAttract::paintImpl(std::span<std::uint32_t> target,int width,int height,bool optimized)const{
    if(width<=0||height<=0||target.size()!=std::size_t(width)*height)throw std::invalid_argument("Invalid gasstand destination");
    if(optimized)paintMenuLogoBackdrop(target,width,height,logoBackdrop_,state_.displayedBackgroundPhase);
    using Bank=OriginalGasstandDraw::Bank;auto commands=originalGasstandDraws(state_);
    // These source depth bands put the moving tile behind the source full
    // panel, and text at1e-5 in front. Preserve the original list separately.
    const float fit=std::min(float(width)/640.f,float(height)/480.f),ox=(width-640.f*fit)*.5f,oy=(height-480.f*fit)*.5f;
    struct Batch{NativeModelChunk chunk;const NativeTextureBank* textures;float depth;};std::vector<Batch> batches;
    for(const auto& d:commands){if(d.bank==Bank::alphabet||(optimized&&d.bank==Bank::gasstand&&d.index==57))continue;
        const auto& model=d.bank==Bank::gasstand?gasstand_:etc_;const auto* textures=d.bank==Bank::gasstand?&gasstandTextures_:&etcTextures_;
        for(auto batch:model.chunks.at(d.index).batches){float depth=0;
            for(auto& v:batch.vertices){v.position.x=v.position.x*d.scaleX+d.position.x;v.position.y=v.position.y*d.scaleY+d.position.y;v.position.z+=d.position.z;depth+=v.position.z;}
            // The independent reference mesh uses the same pixel-center tie
            // convention as the baked texture. Source strip roundoff can
            // otherwise select either adjacent texel on a half-pixel phase.
            if(!optimized&&d.bank==Bank::gasstand&&d.index==57)
                for(auto& v:batch.vertices){v.position.x-=.00001f;v.position.y+=.00001f;}
            // The title and conversation stay fitted to the cabinet canvas.
            // Only the horizontal backing strips and their glow span the screen.
            if(d.bank==Bank::etc&&d.index<=1&&ox>0)
                for(auto& v:batch.vertices)v.position.x=(v.position.x-3.2f)*width/(640*fit)+3.2f;
            depth/=float(batch.vertices.size());NativeModelChunk chunk;chunk.batches.push_back(std::move(batch));batches.push_back({std::move(chunk),textures,depth});
        }
    }
    std::stable_sort(batches.begin(),batches.end(),[](const auto& a,const auto& b){return a.depth<b.depth;});
    SpritePlacement p;p.scale=100.f*fit;p.offsetX=ox;p.offsetY=oy;p.invertY=true;p.authoredHeight=0;
    for(const auto& b:batches)compositeOriginalMenuChunk(target,width,height,*b.textures,b.chunk,p);
    p={};p.scale=fit;p.offsetX=ox;p.offsetY=oy;
    for(const auto& d:commands)if(d.bank==Bank::alphabet){const float x=d.position.x,y=d.position.y;
        const auto color=optimized?0xffffffffu:d.color;
        OriginalSprite glyphSprite;glyphSprite.vertices={OriginalSpriteVertex{x,y,0,0,1,color,0},{x,y+32,0,0,0,color,0},{x+32,y,0,1,1,color,0},{x+32,y+32,0,1,0,color,0}};
        compositeOriginalSprite(target,width,height,optimized?preparedAlphabet_.at(d.index):alphabet_.at(d.index),glyphSprite,p);
    }
}
}
