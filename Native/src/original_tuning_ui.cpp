#include "original_tuning_ui.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace idas3 {
namespace {
constexpr float f(std::uint32_t word){return std::bit_cast<float>(word);}
constexpr std::array<std::uint32_t,31> extraPictures{47,50,59,65,41,53,63,51,49,46,54,61,56,62,58,61,48,0xffffffff,43,60,64,55,57,66,52,56,44,45,49,62,42};
}
OriginalTuningUi OriginalTuningUi::load(const std::filesystem::path& root){
    OriginalTuningUi out;const auto base=root/"data/original_assets";
    const std::array<std::filesystem::path,4> paths{base/"menus/v3/v3sS07tune",base/"tuning/v3sK12shop",base/"menus/v3/v3sS00common",base/"tuning/v3sK17continue"};
    const std::array<const char*,4> names{"v3sS07tune","v3sK12shop","v3sS00common","v3sK17continue"};
    for(unsigned i=0;i<4;++i){out.models_[i]=NativeModel::load(paths[i]/(std::string(names[i])+".idasmesh"));out.textures_[i]=NativeTextureBank::load(paths[i]/"textures/textures.idastex");}
    const auto fontPath=base/"tuning/option";out.font_=NativeTextureBank::load(fontPath/"textures/textures.idastex");
    std::ifstream map(fontPath/"font.bin",std::ios::binary),spacing(fontPath/"spacing.bin",std::ios::binary);
    map.read(reinterpret_cast<char*>(out.glyphMap_.data()),sizeof(out.glyphMap_));
    for(auto& [code,trim]:out.spacing_){std::uint16_t padding;spacing.read(reinterpret_cast<char*>(&code),2);spacing.read(reinterpret_cast<char*>(&padding),2);spacing.read(reinterpret_cast<char*>(&trim),4);}
    if(!map||!spacing||out.font_.size()!=40)throw std::runtime_error("Invalid original option-font data");
    return out;
}
std::vector<OriginalTuningUiDraw> OriginalTuningUi::drawList(const original::OriginalTuningChild& s,const original::OriginalTuningData& data,std::uint32_t sharedCountdown)const{
    using Kind=original::OriginalTuningChildKind;using Bank=OriginalTuningUiDraw::Bank;
    if(s.kind!=Kind::basic&&s.kind!=Kind::performance&&s.kind!=Kind::optionalPart)throw std::invalid_argument("Missing tuning child artwork");
    std::vector<OriginalTuningUiDraw> out;const auto& d=data.car(s.car);
    const auto draw=[&](unsigned index,float x=0,float y=0,float z=0,Bank bank=Bank::shop){out.push_back({bank,index,x,y,z,1,1});};
    if(s.kind==Kind::optionalPart){
        //118100: ordering, positions, digit spacing and copied YES/NO colors.
        const auto& row=d.optional.at(s.optionalIndex).words;const float z=f(0x3ca3d70a),step=f(0x3e051eb8);
        draw(5,0,f(0xbecccccd));draw(4,0,f(0xbecccccd));draw(22,0,f(0xbed1eb85),f(0x3c23d70a));
        const int timer=std::bit_cast<std::int32_t>(sharedCountdown)/60;
        float x=timer>9?f(0x40a33333):f(0x40a8a3d7);
        const auto timerDigit=[&](unsigned n){draw(n,x,f(0xc0533334),z,Bank::common);out.back().scaleX=out.back().scaleY=f(0x3f4ccccd);};
        if(timer>9){timerDigit(unsigned((timer/10)%10));x+=f(0x3eae147b);}
        timerDigit(unsigned(timer%10));
        draw(31,0,f(0xbecccccd),f(0x3c23d70a));
        x=0;x+=f(0x4094cccd);const float balanceY=f(0xbecccccd)-f(0x4083d70a);
        if(std::bit_cast<std::int32_t>(s.balance)>=0)for(unsigned divisor:{100000000u,10000000u,1000000u,100000u,10000u,1000u,100u,10u,1u}){
            if(s.balance>=divisor||divisor==1)draw(7+(s.balance/divisor)%10,x,balanceY,f(0x3c23d70a));x+=step;
        }
        x=f(0x40800000);const float priceY=f(0xc0866666);draw(6,x,priceY,z);x+=1;
        if(row[3]){
            if(row[3]>=10000){draw(7+(row[3]/10000)%10,x,priceY,z);x+=step;}
            for(unsigned divisor:{1000u,100u,10u,1u}){draw(7+(row[3]/divisor)%10,x,priceY,z);x+=step;}
            draw(32,x,priceY,z);
        }
        draw(row[5]==27&&row[1]==5?26:24,f(0x3e4ccccd),f(0xc07eb852),z);
        if(row[2]<=41)draw(row[2]+8,f(0x3fd9999a),f(0xc0800000),z,Bank::tune);
        if(row[5]<extraPictures.size()&&std::int32_t(extraPictures[row[5]])>=0)draw(extraPictures[row[5]],f(0x4019999a),f(0xc06f5c29),z);
        const auto choice=[&](unsigned index,float px,bool selected){
            draw(index,px,f(0xc08ccccd),z,Bank::continuation);
            out.back().scaleX=out.back().scaleY=selected?f(0x3ecccccd):f(0x3ea3d70a);out.back().choiceColors=selected?1:0;
        };
        choice(1,f(0x402e147b),s.choice==0);
        if(s.choice==0){draw(3,f(0x3fb851ec),f(0xc07851ec),z,Bank::continuation);out.back().scaleX=out.back().scaleY=f(0x3ecccccd);}
        choice(0,f(0x402147ae),s.choice==1);
        if(s.choice==1){draw(2,f(0x3f9eb852),f(0xc07851ec),z,Bank::continuation);out.back().scaleX=out.back().scaleY=f(0x3ecccccd);}
        return out;
    }
    const bool basic=s.kind==Kind::basic;
    if(basic){draw(33);draw(40,0,0,f(0x3c23d70a));draw(s.package==d.packages.size()-1?38:s.package+34,0,0,f(0x3c23d70a));}
    else{draw(0);draw(1,0,0,f(0x3c23d70a));draw(3,0,0,f(0x3c23d70a));}
    const auto currentAmount=[&](){return basic?d.packages.at(s.package).steps.at(s.current).words[2]:d.performance.at(s.current).words[1];};
    if(s.flags&4){
        float x=f(0x3e4ccccd);const float y=f(0xc0633333),z=f(0x3ca3d70a),step=f(0x3e051eb8);
        draw(29,x,y,z);x+=f(0x3f99999a);const auto value=currentAmount();
        for(unsigned divisor:{100000u,10000u,1000u,100u,10u,1u})if(divisor<=1000||value>=divisor){draw(7+(value/divisor)%10,x,y,z);x+=step;}
        draw(basic?30:28,x,y,z);
    }
    if(basic&&(s.flags&2)&&s.extraIndex<extraPictures.size()&&std::int32_t(extraPictures[s.extraIndex])>=0)draw(extraPictures[s.extraIndex],f(0x4019999a),f(0xc06e147b),f(0x3ca3d70a));
    if(basic&&(s.flags&8)&&s.current>=0){const auto picture=d.packages.at(s.package).steps.at(s.current).words[1];if(picture<=41)draw(picture+8,f(0x3fd9999a),f(0xc07ae148),f(0x3ca3d70a),Bank::tune);}
    if(s.flags&16){
        if(basic){const auto slot=d.packages.at(s.package).steps.at(s.current).words[0];float x=f(0x3e4ccccd);unsigned index=slot==8?23:25;
            if(s.extraIndex==27&&s.package==0&&slot==5)index=26;
            else if(slot==8&&s.car==19&&s.package==0&&s.selected==6)x-=f(0x3d75c28f);
            draw(index,x,f(0xc07d70a4),f(0x3ca3d70a));
        }else draw(23,0,f(0xc07d70a4),f(0x3ca3d70a));
    }
    if(s.flags&32){
        const float z=f(0x3ca3d70a);draw(basic?39:2,0,0,z);
        const unsigned value=basic?s.nextThreshold:d.performance.at(s.next).words[1];
        if(std::int32_t(value)>0){float x=f(0x409f0a3d);const float y=f(0xc063d70a),step=f(0x3e051eb8);
            for(unsigned divisor:{100000u,10000u,1000u,100u,10u,1u}){if(divisor<=1000||value>=divisor)draw(7+(value/divisor)%10,x,y,z);x+=step;}
        }
        if(basic)draw(39,0,0,z);
    }
    if(basic&&(s.flags&64))draw(s.package==d.packages.size()-1?21:s.package+17,s.completionX,s.completionY,s.completionZ);
    return out;
}
std::vector<OriginalTuningUiGlyph> OriginalTuningUi::descriptionGlyphs(const original::OriginalTuningData& data,const OriginalTuningUiDescription& desc)const{
    std::vector<OriginalTuningUiGlyph> out;if(!desc.address)return out;const auto& text=data.descriptions.at(desc.address);float x=desc.x,y=desc.y;
    for(std::size_t i=0;i<text.size();){
        if(text[i]=='\n'){x=desc.x;y+=desc.size;++i;continue;}
        const auto a=std::uint8_t(text[i]),b=i+1<text.size()?std::uint8_t(text[i+1]):std::uint8_t(0);const unsigned index=std::uint8_t(a-160)*96u+std::uint8_t(b-160);
        if(index>9216){++i;continue;} //0C6A82 invalid pair skips one source byte.
        if(index==9216)throw std::runtime_error("Original JIS table boundary is outside this asset");
        const auto glyph=glyphMap_[index];if(glyph!=0xffff)out.push_back({glyph,x,y,f(0xbc656042),desc.size});
        float trim=0;const unsigned code=a|(unsigned(b)<<8);for(const auto& entry:spacing_)if(entry.first==code){trim=entry.second;break;}
        x=std::fma(desc.size,1.0f-trim,x);i+=2;
    }return out;
}
void OriginalTuningUi::applyChoiceColors(NativeModelChunk& chunk,bool selected){
    unsigned vertex=0;
    for(auto& batch:chunk.batches){
        batch.material[2]=0x600;batch.ich[2]|=0x100000;
        for(auto& v:batch.vertices){
            //115426/1154A0 stops recoloring after the first four vertices.
            if(vertex<4){
                if(batch.ich[6]==0x4a)v.color0=selected?0xffffffffu:(vertex&1)?0xff333333u:0xffccccccu;
                ++vertex;
            }
        }
    }
}
OriginalTuningUiDescription OriginalTuningUi::optionalDescription(const original::OriginalTuningChild& s,const original::OriginalTuningData& data){
    //117B60 constructor: option row+16, size26, position(250,390).
    if(s.kind!=original::OriginalTuningChildKind::optionalPart)throw std::invalid_argument("Optional description requires an optional-parts child");
    return {data.car(s.car).optional.at(s.optionalIndex).words[4],250,390,26};
}
void OriginalTuningUi::paint(std::span<std::uint32_t> argb,int width,int height,const original::OriginalTuningChild& s,const original::OriginalTuningData& data,const OriginalTuningUiDescription& desc,std::uint32_t sharedCountdown)const{
    paintImpl(argb,width,height,s,data,desc,sharedCountdown,false);
}
void OriginalTuningUi::paintOverlay(std::span<std::uint32_t> argb,int width,int height,const original::OriginalTuningChild& s,const original::OriginalTuningData& data,const OriginalTuningUiDescription& desc,std::uint32_t sharedCountdown)const{
    paintImpl(argb,width,height,s,data,desc,sharedCountdown,true);
}
void OriginalTuningUi::paintImpl(std::span<std::uint32_t> argb,int width,int height,const original::OriginalTuningChild& s,const original::OriginalTuningData& data,const OriginalTuningUiDescription& desc,std::uint32_t sharedCountdown,bool straightAlphaOverlay)const{
    if(width<=0||height<=0||argb.size()!=std::size_t(width)*height)throw std::invalid_argument("Invalid original tuning canvas");
    auto draws=drawList(s,data,sharedCountdown);
    const float fit=std::min(float(width)/640,float(height)/480);
    SpritePlacement place;place.scale=100*fit;place.offsetX=(float(width)-640*fit)*.5f;place.offsetY=(float(height)-480*fit)*.5f;place.invertY=true;place.authoredHeight=0;place.defaultOriginalUiColors=true;
    place.straightAlphaOverlay=straightAlphaOverlay;
    for(const auto& draw:draws){auto chunk=models_[unsigned(draw.bank)].chunks.at(draw.index);
        if(draw.choiceColors>=0)applyChoiceColors(chunk,draw.choiceColors==1);
        for(auto& batch:chunk.batches)for(auto& v:batch.vertices){v.position.x=v.position.x*draw.scaleX+draw.x;v.position.y=v.position.y*draw.scaleY+draw.y;v.position.z+=draw.z;}
        const auto depth=[](const NativeModelBatch& b){float total=0;for(const auto& v:b.vertices)total+=v.position.z;return b.vertices.empty()?0:total/float(b.vertices.size());};
        std::stable_sort(chunk.batches.begin(),chunk.batches.end(),[&](const auto& a,const auto& b){return depth(a)<depth(b);});
        auto placement=place;if(draw.choiceColors>=0)placement.defaultOriginalUiColors=false;
        compositeOriginalMenuChunk(argb,width,height,textures_[unsigned(draw.bank)],chunk,placement);
    }
    const auto textDescription=s.kind==original::OriginalTuningChildKind::optionalPart&&!desc.address?optionalDescription(s,data):desc;
    if((s.flags&16)||s.kind==original::OriginalTuningChildKind::optionalPart)for(const auto& glyph:descriptionGlyphs(data,textDescription)){
        //0C58DA..599E builds UVs(0,0),(0,-1),(1,0),(1,-1).
        // Repeat sampling of the negative-V interval is1-v for interior
        // pixels. Keep the original texture bytes and reproduce that UVs.
        const float x=place.offsetX+glyph.x*fit,y=place.offsetY+glyph.y*fit,size=glyph.size*fit;
        OriginalSprite sprite;sprite.vertices={OriginalSpriteVertex{x,y,0,0,1,0xffffffff,0},{x,y+size,0,0,0,0xffffffff,0},{x+size,y,0,1,1,0xffffffff,0},{x+size,y+size,0,1,0,0xffffffff,0}};
        compositeOriginalSprite(argb,width,height,font_.at(glyph.index),sprite,{});
    }
}
}
