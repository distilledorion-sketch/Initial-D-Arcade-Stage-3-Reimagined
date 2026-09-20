#include "original_choice_menu.h"
#include <array>
#include <cmath>
#include <stdexcept>

namespace idas3::original {
namespace {
// Exact2A2358: two labels then two highlights for each original course.
constexpr std::array<std::array<std::uint32_t,4>,9> route={{{19,21,26,28},{19,21,26,28},{18,17,25,24},{18,17,25,24},{22,16,29,23},{18,20,25,27},{22,16,29,23},{22,16,29,23},{18,17,25,24}}};
constexpr float x=3.1999998092651367f,y=-1.2799999713897705f;
constexpr std::array<std::array<std::uint32_t,3>,9> underlay={{{25,36,6},{29,39,9},{20,31,1},{22,33,3},{23,34,4},{24,35,5},{27,37,7},{28,38,8},{21,32,2}}};
}
std::vector<OriginalChoiceDraw> originalChoiceCourseUnderlay(int course) {
    if(course<0||course>8)throw std::out_of_range("Original TA underlay course outside table");
    const auto& row=underlay[course];
    return {{0x2001e},{0x20000|row[0]},{0x20000|row[1],0,-1.6799999475479126f,.009999999776482582f},{0x40000|row[2]}};
}
std::vector<OriginalChoiceDraw> originalChoiceMenuDraws(const OriginalChoiceMenuState& state) {
    if(state.selected<0||state.selected>1||state.course<0||state.course>8)throw std::out_of_range("Original choice selection outside source table");
    if(!std::isfinite(state.confirmationPhase)||state.confirmationPhase<0||state.confirmationPhase>1)throw std::invalid_argument("Original choice phase outside0..1");
    std::vector<OriginalChoiceDraw> draws;
    auto draw=[&](std::uint32_t selector,float px=0,float py=0,float pz=0,float scale=1,OriginalChoiceColors colors=OriginalChoiceColors::Source){draws.push_back({selector,px,py,pz,scale,colors});};
    const bool glow=state.confirmationPhase<.009999999776482582f || (state.frame&6u);
    draw(0x80036);
    if(state.screen==OriginalChoiceScreen::Transmission) {
        draw(0x8002e);draw(0xe0005);draw(0xe0004);
        if(glow)draw(0xe0002+std::uint32_t(state.selected),x,y,.009999999776482582f);
        draw(0xe0000+std::uint32_t(state.selected),x,y,.019999999552965164f,1,OriginalChoiceColors::Selected);
        draw(0xe0000+std::uint32_t(1-state.selected),x,y,.019999999552965164f,.800000011920929f,OriginalChoiceColors::InactiveTransmission);
    } else {
        std::uint32_t bank=23,left=0,right=0,leftGlow=0,rightGlow=0;
        OriginalChoiceColors inactive=OriginalChoiceColors::InactiveRoute;
        if(state.screen==OriginalChoiceScreen::Route) {
            draw(0x17001f);draw(0x17001e);
            left=route[state.course][0];right=route[state.course][1];leftGlow=route[state.course][2];rightGlow=route[state.course][3];
        } else if(state.screen==OriginalChoiceScreen::Weather) {
            bank=24;draw(0x180000);draw(0x180001);left=2;right=3;leftGlow=4;rightGlow=5;inactive=OriginalChoiceColors::InactiveWeatherTime;
        } else {
            bank=25;draw(0x190005);draw(0x190000,state.selected==1?-1.2799999713897705f:0.f);left=1;right=2;leftGlow=3;rightGlow=4;inactive=OriginalChoiceColors::InactiveWeatherTime;
        }
        const float z=state.screen==OriginalChoiceScreen::Route?.019999999552965164f:.05000000074505806f;
        const float glowZ=state.screen==OriginalChoiceScreen::Route?z+(-.009999999776482582f):z+(-.019999999552965164f);
        for(int i=0;i<2;++i) {
            const bool selected=i==state.selected;
            const auto selectedColors=state.screen==OriginalChoiceScreen::Route?OriginalChoiceColors::Selected:OriginalChoiceColors::SelectedWeatherTime;
            draw((bank<<16)|(i==0?left:right),x,y,z,selected?1.f:.800000011920929f,selected?selectedColors:inactive);
            if(selected && glow)draw((bank<<16)|(i==0?leftGlow:rightGlow),x,y,glowZ);
        }
    }
    return draws;
}
NativeModelChunk materializeOriginalChoiceColors(const NativeModelChunk& chunk,OriginalChoiceColors colors) {
    auto out=chunk;
    if(colors==OriginalChoiceColors::Source)return out;
    std::size_t vertexIndex=0;
    const std::size_t count=colors==OriginalChoiceColors::InactiveWeatherTime || colors==OriginalChoiceColors::SelectedWeatherTime?9:4;
    for(auto& batch:out.batches) {
        batch.material[2]=0x600u;
        for(auto& vertex:batch.vertices) {
            if(vertexIndex>=count)break;
            if(batch.ich[6]==0x4au) {
                std::uint32_t color=0xffffffffu;
                if(colors==OriginalChoiceColors::InactiveTransmission)color=(vertexIndex&1)?0xffccccccu:0xff000000u;
                if(colors==OriginalChoiceColors::InactiveRoute || colors==OriginalChoiceColors::InactiveWeatherTime)color=vertexIndex<2?0xffccccccu:0xff000000u;
                vertex.color0=color;vertex.color1=0;
            }
            ++vertexIndex;
        }
    }
    return out;
}
const char* originalChoiceBankName(std::uint32_t bank) {
    switch(bank){case 2:return "v3sK00common";case 4:return "v3sK01course";case 8:return "v3sS00common";case 14:return "v3sS06mission";case 23:return "v3sT02route";case 24:return "v3sT03weather";case 25:return "v3sT04time";default:throw std::out_of_range("Unknown original choice bank");}
}
}
