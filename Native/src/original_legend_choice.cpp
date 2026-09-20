#include "original_legend_choice.h"

namespace idas3::original {
namespace {
using Bank=OriginalLegendChoiceBank;
using Draw=OriginalLegendChoiceDraw;
constexpr float dimDepth=-0.15f,panelDepth=-0.145f,promptDepth=-0.144f;
constexpr float optionDepth=-0.144f,markDepth=-0.1445f;
constexpr float timerLabelDepth=-0.149f,timerDigitDepth=-0.14f;
constexpr float optionX=320.f,optionY=128.f,optionSmall=.8f;
constexpr float digitLeft=520.f,digitRight=568.f,digitCentre=544.f,digitY=16.f;
constexpr std::uint32_t dimColour=0x7f000000u;

Draw layer(Bank bank,std::uint32_t chunk,float x,float y,float z){
    Draw draw;draw.bank=bank;draw.chunk=chunk;draw.x=x;draw.y=y;draw.z=z;return draw;
}
// 0F0600: a label plus one or two digit parts, each part being its digit + 1.
void appendTimer(std::vector<Draw>& draws,std::uint32_t timerTicks){
    draws.push_back(layer(Bank::Continue,11,0,0,timerLabelDepth));
    const auto seconds=timerTicks/60u;
    if(seconds>9u){
        const auto tens=seconds/10u,units=seconds-tens*10u;
        draws.push_back(layer(Bank::Continue,tens+1u,digitLeft,digitY,timerDigitDepth));
        draws.push_back(layer(Bank::Continue,units+1u,digitRight,digitY,timerDigitDepth));
    }else draws.push_back(layer(Bank::Continue,seconds+1u,digitCentre,digitY,timerDigitDepth));
}
// The two option labels trade places, so the chosen entry sits on the near row.
void appendOptions(std::vector<Draw>& draws,std::uint32_t first,std::uint32_t second,bool selected){
    // Both entries share one anchor. They differ only in size: every traced
    // capture draws one of the pair at 0.8, and the pair never moves apart.
    auto a=layer(Bank::Prompt,first,optionX,optionY,optionDepth);
    auto b=layer(Bank::Prompt,second,optionX,optionY,optionDepth);
    a.color=0;a.tinted=true;b.color=0;b.tinted=true;
    (selected?a:b).scale=optionSmall;
    draws.push_back(a);draws.push_back(b);
}
void appendPanels(std::vector<Draw>& draws){
    draws.push_back(layer(Bank::Prompt,4,0,0,panelDepth));
    draws.push_back(layer(Bank::Continue,11,0,0,panelDepth));
}
}

std::vector<OriginalLegendChoiceDraw> originalLegendChoiceDraws(OriginalLegendChoiceKind kind,
    std::uint32_t selected,std::uint32_t timerTicks){
    std::vector<Draw> draws;
    const bool chosen=selected!=0;
    auto dim=layer(Bank::Continue,13,0,0,dimDepth);
    dim.color=dimColour;dim.tinted=true;
    draws.push_back(dim);
    switch(kind){
    case OriginalLegendChoiceKind::Continue:
        appendTimer(draws,timerTicks);
        appendPanels(draws);
        draws.push_back(layer(Bank::Prompt,5,0,0,promptDepth));
        appendOptions(draws,1,0,chosen);
        draws.push_back(layer(Bank::Prompt,chosen?2u:3u,0,-2.f,markDepth));
        break;
    case OriginalLegendChoiceKind::ContinueConfirm:
        appendTimer(draws,timerTicks);
        appendPanels(draws);
        draws.push_back(layer(Bank::Prompt,5,0,0,promptDepth));
        appendOptions(draws,1,0,chosen);
        draws.push_back(layer(Bank::Prompt,chosen?2u:3u,0,-2.f,markDepth));
        break;
    case OriginalLegendChoiceKind::NextRival:
        appendPanels(draws);
        draws.push_back(layer(Bank::Prompt,16,0,0,promptDepth));
        appendOptions(draws,11,10,chosen);
        draws.push_back(layer(Bank::Prompt,chosen?13u:14u,0,0,markDepth));
        draws.push_back(layer(Bank::Prompt,chosen?7u:9u,0,0,markDepth));
        appendTimer(draws,timerTicks);
        break;
    default:
        draws.push_back(layer(Bank::Prompt,17,0,0,promptDepth));
        appendPanels(draws);
        appendOptions(draws,12,10,chosen);
        draws.push_back(layer(Bank::Prompt,chosen?13u:15u,0,0,markDepth));
        draws.push_back(layer(Bank::Prompt,chosen?7u:9u,0,0,markDepth));
        appendTimer(draws,timerTicks);
        break;
    }
    return draws;
}
}
