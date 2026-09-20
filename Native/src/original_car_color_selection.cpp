#include "original_car_color_selection.h"
#include <bit>
#include <cmath>
#include <stdexcept>

namespace idas3::original {
namespace {
std::int32_t signedWord(std::uint32_t n){return std::bit_cast<std::int32_t>(n);}
void validSelection(const OriginalCarColorSelection& s,std::uint32_t selected){
    if(s.colorCounts680.size()!=s.rememberedColors676.size()||selected>=s.colorCounts680.size())
        throw std::out_of_range("Original color selection outside initialized maker roster");
}
}
void initializeOriginalCarColorSelection(OriginalCarColorSelection& s,
        std::span<const std::uint32_t> counts,std::uint32_t selected,std::uint32_t profileColor){
    if(selected>=counts.size())throw std::out_of_range("Original initial color selection outside maker roster");
    s.colorCounts680.assign(counts.begin(),counts.end());s.rememberedColors676.assign(counts.size(),0);
    s.selected480=selected;s.previous484=selected;s.currentColor684=profileColor;s.profileColor64=profileColor;
}
OriginalCarColorSelectionEvents stepOriginalCarColorSelection(OriginalCarColorSelection& s,
        const OriginalCarColorSelectionInput& input){
    validSelection(s,input.selectedLocalIndex);OriginalCarColorSelectionEvents out;
    s.selected480=input.selectedLocalIndex;
    out.selectionChanged=s.selected480!=s.previous484;
    if(out.selectionChanged)s.currentColor684=s.rememberedColors676[s.selected480];
    s.previous484=s.selected480;
    out.gearDelta=(input.gearButtons92ED40&0x20)?1:(input.gearButtons92ED40&0x10)?-1:0;
    if(out.gearDelta){
        s.currentColor684+=std::uint32_t(out.gearDelta);
        const auto count=s.colorCounts680[s.selected480];
        if(signedWord(s.currentColor684)>=signedWord(count))s.currentColor684=0;
        if(signedWord(s.currentColor684)<0)s.currentColor684=count-1;
        s.rememberedColors676[s.selected480]=s.currentColor684;
        out.showroomColorApplyRequested=true;
    }
    if(input.confirmOrTimeout){s.profileColor64=s.currentColor684;out.profileColorWritten=true;}
    return out;
}
std::vector<OriginalCarColorIndicatorDraw> originalCarColorIndicatorDraws(
        std::uint32_t car,std::uint32_t selected,std::span<const std::uint32_t> rgb){
    std::vector<OriginalCarColorIndicatorDraw> out;out.reserve(2+rgb.size());out.push_back({0x000d0025});
    for(std::uint32_t i=0;i<rgb.size();++i){
        std::uint32_t argb=rgb[i]|0xff000000u;
        if((car==30&&(i==3||i==4))||(car==34&&i==7)||(car==22&&i==4)||(car==3&&i==3)){
            const auto masked=rgb[i]&0x003f3f3fu;argb=masked*3u|0xff000000u;
        }
        const float offset=std::fma(float(i),20.f,180.f);
        const float y=-(offset*std::bit_cast<float>(0x3c23d70au));
        out.push_back({i==selected?0x000d0026u:0x000d0027u,
            std::bit_cast<float>(0x3eae147bu),y,std::bit_cast<float>(0x3a83126fu),true,argb});
        if(i==selected)out.push_back({0x000d0028u,std::bit_cast<float>(0x3eae147bu),y,0.f,false,0});
    }
    return out;
}
}
