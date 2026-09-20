#include "original_tuning_preview_state.h"
#include "original_ranking_data.h"
#include <bit>
#include <cmath>
#include <stdexcept>
namespace idas3::original {
std::array<float,3> originalTuningPreviewIncomingLight(){
    constexpr float x=std::bit_cast<float>(0x3ecccccdu);
    const float squared=float(double(x)*double(x)+2.0);
    const float inverse=1.f/std::sqrt(squared);
    return{x*inverse,-inverse,-inverse};
}
std::array<std::uint8_t,5> originalTuningPreviewPlateDigits(const OriginalBattleProfile& p){
    constexpr std::array<std::uint32_t,5> factors{0xf6b0,31673,0x145e3,0x11ced,0x17f4d};
    constexpr std::array<std::uint32_t,5> addends{17706,0xd6b1,0x14573,1507,0xffa7};
    std::uint32_t sum=0;const auto length=std::bit_cast<std::int32_t>(p.u(76));
    for(unsigned i=0;i<5;++i)sum+=(int(i)<length?p.u(44+i*4):0u)*factors[i]+addends[i];
    sum%=100000;std::array<std::uint8_t,5> out{};
    for(unsigned i=0,divisor=10000;i<5;++i,divisor/=10)out[i]=std::uint8_t((sum/divisor)%10);
    return out;
}
std::optional<unsigned> originalTuningPreviewFocus(const OriginalBattleProfile& p,
    const OriginalTuningData& data,OriginalTuningChildKind kind){
    const auto car=p.u(16),package=unsigned(p.byte(152));const auto& d=data.car(car);unsigned slot;
    if(kind==OriginalTuningChildKind::basic)slot=d.packages.at(package).steps.at(p.byte(153)).words[0];
    else if(kind==OriginalTuningChildKind::optionalPart)slot=d.optional.at(p.byte(154)).words[1];
    else return std::nullopt;
    if(slot==9){
        if(car==19)return 5;
        if(car==20)return p.byte(165)==1?5:1;
        if(car==10)return p.byte(165)==1?9:5;
        if(car==2||car==31)return 1;
        return 9;
    }
    if(slot==8){
        const bool exception=car==4||car==18||(car==22&&package==1)||(car==25&&package!=0)||car==26||car==30;
        if(exception&&d.packages.at(package).steps.at(p.byte(153)).words[1]==4)return 5;
    }
    return slot;
}
void advanceOriginalTuningPreview(OriginalTuningPreviewState& s){
    static constexpr std::array<std::int32_t,12> targets{61896,61896,57344,45056,28672,39141,28672,53248,-1,16384,-1,-1};
    const auto target=targets.at(s.focus);
    if(target<0)s.angle+=128;
    else{
        const auto current=std::bit_cast<std::int32_t>(s.angle);
        const auto delta=std::bit_cast<std::int32_t>(std::uint32_t(target)-s.angle);
        std::int32_t destination=target;
        if((delta>=0&&delta>32768)||(delta<0&&std::bit_cast<std::int32_t>(s.angle-std::uint32_t(target))>32768))destination-=65536;
        const float tail=float(destination)*std::bit_cast<float>(0x3dcccccdu);
        const float mixed=std::fma(float(current),std::bit_cast<float>(0x3f666666u),tail);
        s.angle=std::uint32_t(std::int32_t(mixed))&65535u;
    }
    ++s.frames;
}
OriginalTuningPreviewScene originalTuningPreviewScene(unsigned car,std::uint32_t angle,const OriginalFscaTable& trig,unsigned scene){
    if(car>=35||scene>2)throw std::out_of_range("Original tuning preview car/scene");
    OriginalTuningPreviewScene out;auto base=originalIdentityMatrix();
    if(scene==2){
        //081530..08155C supplies fixed eye(0,1,2.8),target(0,.65,0)
        // to1FC2A0. These are its exact column-major matrix words, verified
        // against those instructions (including FIPR/FSRRA), not a host
        // look-at approximation. No runtime variable affects this matrix.
        constexpr std::array<std::uint32_t,16> words{
            0x3f800000u,0u,0u,0u,0u,0x3f7e05ecu,0x3dfe05eeu,0u,
            0u,0xbdfe05eeu,0x3f7e05ecu,0u,0u,0xbf251d72u,0xc039c121u,0x3f800000u};
        for(unsigned i=0;i<16;++i)base.elements[i]=std::bit_cast<float>(words[i]);
    }else{
        // ARankinTA07F3D6 supplies-.8 before RX1024; AResult supplies-.4.
        translateOriginalMatrix(base,{0,std::bit_cast<float>(scene==1?0xbf4ccccdu:0xbecccccdu),-6});
        rotateOriginalMatrixPhase(base,0,1024,trig);
    }
    out.background=base;
    auto shadow=base;translateOriginalMatrix(shadow,{0,std::bit_cast<float>(0x3c23d70au),0});rotateOriginalMatrixPhase(shadow,1,std::uint16_t(angle),trig);
    const auto dimensions=ranking_data::dimensions[car];
    scaleOriginalMatrix(shadow,{std::bit_cast<float>(dimensions[0])*std::bit_cast<float>(0x3fcccccdu),1,
        std::bit_cast<float>(dimensions[1])*std::bit_cast<float>(0x3fe66666u)});out.shadow=shadow;
    auto body=base;float height=std::bit_cast<float>(ranking_data::heights[car]);
    translateOriginalMatrix(body,{0,height,0});rotateOriginalMatrixPhase(body,1,std::uint16_t(angle),trig);out.car=body;
    height*= -2;translateOriginalMatrix(body,{0,height,0});scaleOriginalMatrix(body,{1,-1,1});out.reflection=body;return out;
}
}
