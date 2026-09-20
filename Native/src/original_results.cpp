#include "original_results.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace idas3 {
namespace {
constexpr float lit(std::uint32_t bits){return std::bit_cast<float>(bits);}
void validate(const OriginalResultsState& s){
    if(s.carId>=35||s.condition>=22||s.sectionCapacity<2||s.sectionCapacity>4||s.sectionCount>s.sectionCapacity||(s.recordFlags&~0x78000000u))throw std::invalid_argument("Unsupported original Time Attack results state");
    std::uint32_t previous=0;
    for(unsigned i=0;i<s.sectionCount;i++){if(s.sectionTimes6000[i]<previous||s.sectionTimes6000[i]>s.totalTicks6000)throw std::invalid_argument("Results require cumulative section timestamps");previous=s.sectionTimes6000[i];}
}
}
void completeOriginalResultSections(OriginalBattleResultsState& s){
    if(s.resultStatus==0 && s.sectionCapacity<=4 && s.sectionCount+1==s.sectionCapacity &&
       (!s.sectionCount || s.totalTicks6000>=s.sectionTimes6000[s.sectionCount-1]))
        s.sectionTimes6000[s.sectionCount++]=s.totalTicks6000;
}
OriginalBattleResults OriginalBattleResults::load(const std::filesystem::path& root){
    OriginalBattleResults out;const auto path=root/"data/original_assets/results/result";
    out.model_=NativeModel::load(path/"result.idasmesh");
    out.textures_=NativeTextureBank::load(path/"textures/textures.idastex");
    if(out.model_.chunks.size()!=43||out.textures_.size()!=10)throw std::runtime_error("Original battle results bank identity mismatch");
    return out;
}
std::vector<OriginalHudDraw> OriginalBattleResults::drawList(const OriginalBattleResultsState& s)const{
    if(s.resultStatus>2||s.sectionCapacity>4||s.sectionCount>s.sectionCapacity||!std::isfinite(s.signedAdvantage))throw std::invalid_argument("Invalid original battle results state");
    std::uint32_t previous=0;
    for(unsigned i=0;i<s.sectionCount;++i){if(s.sectionTimes6000[i]<previous||s.sectionTimes6000[i]>s.totalTicks6000)throw std::invalid_argument("Battle results require cumulative section timestamps");previous=s.sectionTimes6000[i];}
    std::vector<OriginalHudDraw> out;
    const auto draw=[&](unsigned index,float x=0,float y=0){auto m=original::originalIdentityMatrix();original::translateOriginalMatrix(m,{x,y,0});out.push_back({OriginalHudDraw::Kind::polygon,index,0xffffffff,m});};
    //0EC480 clock construction,0ECD20 draw and0DCB80 digit-count selection.
    // Preserve each original single-precision subtraction, including gaps.
    std::array<float,7> clockX{};float x=5.05f;
    for(unsigned i=0;i<7;++i){clockX[i]=x;x-=.16f;if(i==2)x-=.09f;if(i==4)x-=.08f;}
    float y=-.99f;previous=0;
    for(unsigned row=0;row<=s.sectionCapacity;++row){
        std::uint32_t time=s.totalTicks6000;
        if(row){y-=row==1?.34f:.28f;time=row<=s.sectionCount?s.sectionTimes6000[row-1]-previous:0;if(row<=s.sectionCount)previous=s.sectionTimes6000[row-1];}
        const std::array<unsigned,7> digits{(time/6)%10,(time/60)%10,(time/600)%10,(time/6000)%10,(time/60000)%6,(time/360000)%10,(time/3600000)%6};
        if(time){draw(15,clockX[0],y);for(unsigned i=0;i<(digits[6]?7u:6u);++i)draw(19+digits[i],clockX[i],y);}
        else {for(unsigned i=0;i<6;++i)draw(17,clockX[i],y);draw(15,clockX[0],y);
            //0DD380 leaves separator scale+56/+60 untouched. This native
            // owner uses zero-initialized storage, so that separator collapses.
            out.back().matrix.elements[0]=0;out.back().matrix.elements[5]=0;}
    }
    //0ECF60/0EDFE0 points, including deduction and highlighted balance.
    // Each value has a separate high-digit widget beyond eight decimal places.
    const bool timeAttack=s.profileMode==1;
    const float pointsY=timeAttack?lit(0xc0370a3e):-3.2f;
    y=pointsY;
    for(unsigned row=0;row<5;++row){
        if(row)y=row==4?pointsY-1.18f:y-.28f;
        if(row==4&&s.balanceHighlighted&&!s.balanceVisible)continue;
        auto value=s.points[row];x=4.65f;
        if(row==3&&s.deduction){unsigned digits=0;for(auto n=value;n;n/=10)++digits;draw(18,1.47f-float(digits+1)*.12f,-1.638f);}
        const auto base=(row==3&&s.deduction)||(row==4&&s.balanceHighlighted)?29u:19u;
        do{draw(base+value%10,x,y);value/=10;x-=.16f;}while(value);
        if(row==3&&s.deduction)draw(41);
    }
    if(!s.balanceHighlighted||s.balanceVisible)draw(s.balanceHighlighted?40:39,4.79f,timeAttack?lit(0xc081999a):-4.39f); //0ECF60 object+252 unit anchor.
    y=-1.33f;
    for(unsigned row=0;row<s.sectionCapacity;++row){draw(20+row,3.1f,y);y-=.28f;}
    //0EEFC0 shares the clocks and points, then submits the authored TA panel.
    // It has no WIN/LOSE heading or opponent advantage widget.
    if(timeAttack){draw(s.circuitLayout?5:4);return out;}
    //0EEC80: result heading, WIN/LOSE/TIME UP, signed advantage.
    draw(s.circuitLayout?3:2);draw(9+s.resultStatus);
    std::array<float,7> advantageX{};advantageX[0]=4.87f;x=4.64f;
    for(unsigned i=1;i<7;++i){advantageX[i]=x;x-=.16f;}
    if(s.resultStatus>1){for(unsigned i=0;i<2;++i)draw(17,advantageX[i],-2.51f);}
    else{
        const float scaled=s.signedAdvantage*10.f;
        // SH4 FTRC saturation, followed by156720's signed magnitude/BCD.
        const auto signedValue=scaled>=2147483648.f?INT32_MAX:scaled<=-2147483648.f?INT32_MIN:std::int32_t(scaled);
        std::uint32_t value=signedValue<0?0u-std::uint32_t(signedValue):std::uint32_t(signedValue);
        //156720 returns eight packed nibbles; the seven-digit widget trims
        // within its own capacity. The sign tests that packed result.
        const auto packedMagnitude=value%100000000u;value=packedMagnitude%10000000u;
        unsigned digit=0;do{draw(19+value%10,advantageX[digit++],-2.51f);value/=10;}while((value||digit<2)&&digit<7);
        draw(16);draw(packedMagnitude==0?14:s.signedAdvantage>0?12:13);
    }
    return out;
}
void OriginalBattleResults::paint(std::span<std::uint32_t> argb,int width,int height,const OriginalBattleResultsState& state,bool straightAlphaOverlay)const{
    if(width<=0||height<=0||argb.size()!=std::size_t(width)*height)throw std::invalid_argument("Invalid battle results destination");
    auto commands=drawList(state);
    // Original depth keeps the full authored panel behind the number widgets.
    std::stable_partition(commands.begin(),commands.end(),[](const auto& d){return d.index>=2&&d.index<=5;});
    const float fit=std::min(float(width)/640.f,float(height)/480.f);
    SpritePlacement placement;placement.scale=100.f*fit;placement.invertY=true;placement.authoredHeight=0;placement.defaultOriginalUiColors=true;
    placement.straightAlphaOverlay=straightAlphaOverlay;
    placement.offsetX=(float(width)-640.f*fit)*.5f;placement.offsetY=(float(height)-480.f*fit)*.5f;
    for(const auto& d:commands){
        auto chunk=model_.chunks.at(d.index);
        if(d.index>=2&&d.index<=5){
            const auto depth=[](const NativeModelBatch& batch){float z=0;for(const auto& v:batch.vertices)z+=v.position.z;return batch.vertices.empty()?0:z/float(batch.vertices.size());};
            // The authored panel mixes foreground text and farther opaque
            // backing batches. Preserve its depth without recoloring artwork.
            std::stable_sort(chunk.batches.begin(),chunk.batches.end(),[&](const auto& a,const auto& b){return depth(a)<depth(b);});
        }
        auto matrix=d.matrix;
        if(d.index==18){
            // The deduction setter146720 receives a bank-space position;
            // ordinary digits instead carry the bank origin in their widget.
            //145220 initializes that origin from145408/40C. Remove it for the
            // same authored640x480 coordinates used by the native compositor.
            matrix.elements[12]-=lit(0xc04ccccc);matrix.elements[13]-=lit(0x40199999);
        }
        for(auto& batch:chunk.batches)for(auto& vertex:batch.vertices){const auto p=original::transformOriginalPoint(matrix,{vertex.position.x,vertex.position.y,vertex.position.z});vertex.position={p[0],p[1],p[2]};}
        compositeOriginalMenuChunk(argb,width,height,textures_,chunk,placement);
    }
}
OriginalTimeAttackResults OriginalTimeAttackResults::load(const std::filesystem::path& root){
    OriginalTimeAttackResults out;out.hud_=OriginalRaceHud::load(root);
    const auto directory=root/"data/original_assets/hud";
    out.main_=NativeModel::load(directory/"game2d/game2d.idasmesh");
    out.mainTextures_=NativeTextureBank::load(directory/"game2d/textures/textures.idastex");
    out.timeAttack_=NativeModel::load(directory/"game2d_ta/game2d_ta.idasmesh");
    out.timeAttackTextures_=NativeTextureBank::load(directory/"game2d_ta/textures/textures.idastex");
    if(out.main_.chunks.size()!=212||out.mainTextures_.size()!=76||out.timeAttack_.chunks.size()!=24)throw std::runtime_error("Original results bank identity mismatch");
    // These authored widgets submit their foreground label before their black
    // backing strip. The arcade resolves that with Z; our 2D compositor uses
    // submission order. Sort once at load, keeping the original artwork intact.
    for(unsigned index:{15u,16u,17u,18u,19u}){
        auto& batches=out.timeAttack_.chunks[index].batches;
        const auto depth=[](const NativeModelBatch& batch){float z=0;for(const auto& v:batch.vertices)z+=v.position.z;return batch.vertices.empty()?0.f:z/float(batch.vertices.size());};
        std::stable_sort(batches.begin(),batches.end(),[&](const auto& a,const auto& b){return depth(a)<depth(b);});
    }
    return out;
}
std::vector<OriginalResultsDraw> OriginalTimeAttackResults::drawList(const OriginalResultsState& s)const{
    validate(s);using Bank=OriginalResultsDraw::Bank;
    std::vector<OriginalResultsDraw> out;const auto identity=original::originalIdentityMatrix();
    const auto draw=[&](Bank bank,unsigned index,const auto& matrix){out.push_back({bank,index,matrix});};
    const auto announcement=[&]{
        if(s.recordFlags&OriginalResultsState::newRecord)draw(Bank::timeAttack,20,identity);
        if(s.recordFlags&OriginalResultsState::courseRecord)draw(Bank::timeAttack,21,identity);
        else if(s.recordFlags&OriginalResultsState::modelRecord)draw(Bank::timeAttack,23,identity);
        else if(s.recordFlags&OriginalResultsState::personalBest)draw(Bank::timeAttack,22,identity);
    };
    if(s.announcementOnly){announcement();return out;}
    auto m=identity;
    // 0CED20..0CEDEE: normal-layout labels and independent +D0/+D4 slides,
    // global31CE38 bit512 clear. Keep cumulative matrix arithmetic and order.
    original::translateOriginalMatrix(m,{s.livePanel?s.slide208:0.f,lit(0xbc23d70a),0});draw(Bank::race,142,m);
    original::translateOriginalMatrix(m,{0,lit(0xbd75c28f),0});draw(Bank::race,138,m);draw(Bank::race,140,m);
    original::translateOriginalMatrix(m,{0,lit(0x3db851ec),0});for(unsigned i:{15u,16u,17u})draw(Bank::timeAttack,i,m);
    original::translateOriginalMatrix(m,{0,lit(0xbd23d70a),0});for(unsigned i:{18u,19u})draw(Bank::timeAttack,i,m);
    m=identity;original::translateOriginalMatrix(m,{s.livePanel?s.slide212:0.f,0,lit(0xb8d1b717)});draw(Bank::race,58,m);
    original::translateOriginalMatrix(m,{0,lit(0xbd75c28f),0});draw(Bank::race,139,m);draw(Bank::race,141,m);
    // 0CEEA0..0CF3EE: original1568A0 packed-clock digits and missing-record
    // dashes. MODEL availability is +60==1, not whether its time is nonzero.
    m=identity;original::translateOriginalMatrix(m,{lit(0x40a47ae1),lit(0xbeb851ec),0});
    constexpr std::array<std::uint32_t,6> advances{0x3e0f5c29,0x3e4ccccd,0x3e0f5c29,0x3e75c28f,0x3e0f5c29,0x3e0f5c29};
    for(unsigned row=0;row<3;row++){
        if(row)original::translateOriginalMatrix(m,{lit(0xbf800000),lit(0xbe23d70a),0});
        original::translateOriginalMatrix(m,{lit(0x3e19999a),0,0});draw(Bank::race,75,m);
        original::translateOriginalMatrix(m,{lit(0xbe19999a),0,0});
        const bool available=s.condition>=18||s.suppliedRecordTargets?s.bestTimes6000[row]!=0:row==0||(row==1?s.modelBestAvailable:s.bestTimes6000[2]!=0);
        const auto t=s.bestTimes6000[row];
        const std::array<unsigned,7> digit{(t/3600000)%6,(t/360000)%10,(t/60000)%6,(t/6000)%10,(t/600)%10,(t/60)%10,(t/6)%10};
        if(available&&digit[0])draw(Bank::race,77+digit[0],m);
        for(unsigned i=1;i<7;i++){original::translateOriginalMatrix(m,{lit(advances[i-1]),0,0});draw(Bank::race,available?77+digit[i]:76,m);}
    }
    // Live normal-layout difference rows. No comparison remains dashes;
    // the owner only supplies measured checkpoint/finish comparisons.
    if(s.livePanel)for(unsigned row=0;row<2;++row){
        m=identity;original::translateOriginalMatrix(m,{5.14f,-1.14f-.16f*row,0});
        const auto value=s.differences6000[row];
        const auto t=std::uint32_t(std::min<std::int64_t>(3594000,std::abs(std::int64_t(value))));
        const std::array<unsigned,7> digit{0,(t/360000)%10,(t/60000)%6,(t/6000)%10,(t/600)%10,(t/60)%10,(t/6)%10};
        auto sign=m;original::translateOriginalMatrix(sign,{-.04f,.01125f,0});
        sign.elements[0]=sign.elements[5]=1.15f;
        if(s.differenceAvailable[row])draw(value<0?Bank::timeAttack:Bank::race,value<0?0u:73u,sign);
        const bool negative=s.differenceAvailable[row]&&value<0;
        for(unsigned i=1;i<7;++i){original::translateOriginalMatrix(m,{lit(advances[i-1]),0,0});draw(negative?Bank::timeAttack:Bank::race,s.differenceAvailable[row]?(negative?5:77)+digit[i]:76,m);}
        if(s.differenceAvailable[row]){auto sep=identity;original::translateOriginalMatrix(sep,{5.29f,-1.14f-.16f*row,0});draw(negative?Bank::timeAttack:Bank::race,negative?3:75,sep);}
    }
    // 0CF4D6..0CF532: original record banner priority, authored coordinates.
    announcement();
    return out;
}
void OriginalTimeAttackResults::paint(std::span<std::uint32_t> argb,int width,int height,const OriginalResultsState& state)const{
    if(width<=0||height<=0||argb.size()!=std::size_t(width)*height)throw std::invalid_argument("Invalid results destination");
    auto commands=drawList(state);
    OriginalHudState hud;hud.flags=4;hud.timePanel=true;hud.edgeAnchored=state.edgeAnchored;
    hud.elapsedTicks6000=state.totalTicks6000;hud.finishTicks6000=state.totalTicks6000;hud.remainingTicks6000=state.remainingTicks6000;
    hud.sectionTimes6000=state.sectionTimes6000;hud.sectionCount=state.sectionCount;hud.sectionCapacity=state.sectionCapacity;
    if(!state.livePanel&&!state.announcementOnly)hud_.paint(argb,width,height,hud);
    // Original z separates backing strips from foreground labels. The CPU
    // compositor has no depth buffer, so those strips are submitted first.
    std::stable_partition(commands.begin(),commands.end(),[](const auto& d){return d.bank==OriginalResultsDraw::Bank::race&&(d.index==58||d.index==139||d.index==141);});
    const float fit=std::min(float(width)/640.f,float(height)/480.f);
    const float centeredX=(float(width)-640.f*fit)*.5f,centeredY=(float(height)-480.f*fit)*.5f;
    for(const auto& d:commands){
        const bool ta=d.bank==OriginalResultsDraw::Bank::timeAttack,banner=ta&&d.index>=20;
        auto chunk=(ta?timeAttack_:main_).chunks.at(d.index);
        for(auto& batch:chunk.batches)for(auto& vertex:batch.vertices){const auto p=original::transformOriginalPoint(d.matrix,{vertex.position.x,vertex.position.y,vertex.position.z});vertex.position={p[0],p[1],p[2]};}
        SpritePlacement placement;placement.scale=100.f*fit;placement.invertY=true;placement.authoredHeight=0;
        placement.offsetX=state.edgeAnchored&&!banner?float(width)-640.f*fit:centeredX;
        placement.offsetY=state.edgeAnchored&&!banner?0.f:centeredY;
        compositeOriginalMenuChunk(argb,width,height,ta?timeAttackTextures_:mainTextures_,chunk,placement);
    }
}
}
