#include "original_battle_hud.h"
#include "unity_ui_capture.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace idas3 {
namespace {
constexpr float lit(std::uint32_t bits){return std::bit_cast<float>(bits);}
void scaleXY(original::OriginalMatrix& m,float value){for(unsigned i=0;i<8;++i)m.elements[i]*=value;}
}
std::vector<OriginalBattleHudDraw> drawOriginalBattleHud(const OriginalBattleHudState& s,
        OriginalBattleHudAnimation& animation,const original::OriginalMatrix& base){
    if(!std::isfinite(s.validity96)||!std::isfinite(s.signedAdvantage100)||!std::isfinite(s.slide208)||!std::isfinite(s.slide212))
        throw std::invalid_argument("Original battle HUD requires finite source fields");
    using Kind=OriginalBattleHudDraw::Kind;std::vector<OriginalBattleHudDraw> out;
    auto m=base;
    const auto emit=[&](Kind kind,unsigned index=0){out.push_back({kind,{OriginalHudDraw::Kind::polygon,index,0xffffffff,m}});};
    const auto polygon=[&](unsigned index){emit(Kind::game2d,index);};
    const auto translate=[&](float x,float y=0.f,float z=0.f){original::translateOriginalMatrix(m,{x,y,z});};
    emit(Kind::baseHud);
    if(std::bit_cast<std::int32_t>(s.flags104)<0)return out;
    emit(Kind::beginProjection6);
    if(s.flags104&1)polygon(186);
    const auto finish=[&](){m=base;emit(Kind::endProjection);return out;};
    if(s.alternateLayout96)return finish();
    const bool portrait=s.profileMode0C31C99C==0||s.profileMode0C31C99C==2;
    if(s.flags104&2048){
        m=base;translate(s.slide208,lit(0xbc23d70a));
        if(!portrait)translate(0,lit(0x3f0ccccd));
        polygon(60);
        if(portrait){scaleXY(m,lit(0x3c23d70a));emit(Kind::portrait228,0);}
        m=base;translate(s.slide212);
        if(!portrait)translate(0,lit(0x3f0ccccd));
        polygon(61);m=base;
        if(s.frame204>40){emit(Kind::playerName232);emit(Kind::opponentName236);}
    }
    if(!(s.flags104&2))return finish();
    m=base;translate(s.slide212);polygon(98);if(portrait)polygon(99);
    polygon(s.signedAdvantage100<0.f?59:58);
    m=base;translate(s.slide208,lit(0xbc23d70a));polygon(10);polygon(96);
    if(s.frame204<=40)return finish();
    m=base;
    if(s.signedAdvantage100<0.f){
        ++animation.negativeBlink0CA9B548;
        if(std::bit_cast<std::int32_t>(animation.negativeBlink0CA9B548)>60)animation.negativeBlink0CA9B548=0;
    }else animation.negativeBlink0CA9B548=0;
    translate(5.f,lit(0xbea3d70a));
    if(s.validity96<0.f){
        translate(lit(0x3f7ae148));polygon(25);
        translate(lit(0xbf7ae148));translate(lit(0x3f333333));polygon(22);
        translate(lit(0xbf333333));polygon(24);
        for(unsigned i=0;i<3;++i){translate(lit(0x3e3851ec));polygon(24);}
        translate(lit(0x3e75c28f));polygon(24);return finish();
    }
    unsigned magnitude=0,digitBase=37,sign=23,point=21,unit=26;
    if(s.signedAdvantage100>lit(0x461c3f9a))magnitude=99999;
    else if(s.signedAdvantage100<lit(0xc61c3f9a)){magnitude=99999;digitBase=47;sign=25;point=22;unit=24;}
    else if(s.signedAdvantage100<lit(0x3dcccccd)&&s.signedAdvantage100>lit(0xbdcccccd))unit=57;
    else if(s.signedAdvantage100<0.f){magnitude=unsigned(-s.signedAdvantage100*10.f);digitBase=47;sign=25;point=22;unit=24;}
    else magnitude=unsigned(s.signedAdvantage100*10.f);
    if(std::bit_cast<std::int32_t>(animation.negativeBlink0CA9B548)>49)return finish();
    translate(lit(0x3f7ae148));polygon(sign);
    translate(lit(0xbf7ae148));translate(lit(0x3f333333));polygon(point);translate(lit(0xbf333333));
    if(magnitude>9999){translate(lit(0xbe0f5c29));polygon(unit);translate(lit(0x3e0f5c29));}
    else if(magnitude>999){translate(lit(0x3d23d70a));polygon(unit);translate(lit(0xbd23d70a));}
    else if(magnitude>99){translate(lit(0x3e6147ae));polygon(unit);translate(lit(0xbe6147ae));}
    else{translate(lit(0x3eb851ec));polygon(unit);translate(lit(0xbeb851ec));}
    if(magnitude>9999)polygon(digitBase+(magnitude/10000)%10);
    translate(lit(0x3e3851ec));if(magnitude>999)polygon(digitBase+(magnitude/1000)%10);
    translate(lit(0x3e3851ec));if(magnitude>99)polygon(digitBase+(magnitude/100)%10);
    translate(lit(0x3e3851ec));polygon(digitBase+(magnitude/10)%10);
    translate(lit(0x3e75c28f));polygon(digitBase+magnitude%10);
    return finish();
}
OriginalBattleHudAssets OriginalBattleHudAssets::load(const std::filesystem::path& root){
    OriginalBattleHudAssets out;const auto directory=root/"data/original_assets/hud/game2d";
    out.model_=NativeModel::load(directory/"game2d.idasmesh");out.textures_=NativeTextureBank::load(directory/"textures/textures.idastex");
    if(out.model_.chunks.size()!=212||out.textures_.size()!=76)throw std::runtime_error("Original battle HUD game2d identity mismatch");return out;
}
void OriginalBattleHudAssets::paintGame2d(std::span<std::uint32_t> argb,int width,int height,
        std::span<const OriginalBattleHudDraw> commands,bool edgeAnchored)const{
    if(width<=0||height<=0||argb.size()!=std::size_t(width)*height)throw std::invalid_argument("Invalid battle HUD destination");
    const float fit=std::min(float(width)/640.f,float(height)/480.f);
    struct Prepared {NativeModelChunk chunk;float depth;bool centered;};std::vector<Prepared> prepared;
    for(const auto& command:commands){
        if(command.kind!=OriginalBattleHudDraw::Kind::game2d)continue;
        auto chunk=model_.chunks.at(command.draw.index);
        for(auto& batch:chunk.batches)for(auto& v:batch.vertices){const auto p=original::transformOriginalPoint(command.draw.matrix,{v.position.x,v.position.y,v.position.z});v.position={p[0],p[1],p[2]};}
        const float depth=chunk.batches.empty()||chunk.batches.front().vertices.empty()?0.f:chunk.batches.front().vertices.front().position.z;
        prepared.push_back({std::move(chunk),depth,command.draw.index==186});
    }
    // The authored label planes areZ=+1e-6; backing planes areZ=0.
    // Original depth testing keeps labels visible even when submitted before
    // their backing. Order these flat HUD planes for the CPU compositor;
    // retain the source drawList order separately for instruction comparison.
    std::stable_sort(prepared.begin(),prepared.end(),[](const auto& a,const auto& b){return a.depth<b.depth;});
    for(const auto& item:prepared){
        const UnityUiHudScope hudGroup(item.centered?4:unityUiHudGroup());
        SpritePlacement placement;placement.scale=100.f*fit;placement.invertY=true;placement.authoredHeight=0;
        placement.offsetX=edgeAnchored&&!item.centered?float(width)-640.f*fit:(float(width)-640.f*fit)*.5f;
        placement.offsetY=edgeAnchored&&!item.centered?0.f:(float(height)-480.f*fit)*.5f;
        compositeOriginalMenuChunk(argb,width,height,textures_,item.chunk,placement);
    }
}
}
