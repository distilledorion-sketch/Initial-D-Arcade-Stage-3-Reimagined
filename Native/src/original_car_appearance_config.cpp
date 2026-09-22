#include "original_car_appearance_config.h"
#include "original_car_color_catalog.h"
#include <stdexcept>

namespace idas3::original {
OriginalCarAppearanceConfig::OriginalCarAppearanceConfig(std::uint32_t carId):car(carId){
    if(car>=35)throw std::out_of_range("Original appearance car");
    if(car==22)variants={0,1,4,2,3,5};
}
void applyOriginalCarAppearanceCall(OriginalCarAppearanceConfig& s,std::uint32_t address,std::uint32_t value,std::uint32_t argument6){
    if(address==0x0c0287a0){
        constexpr std::array<std::uint32_t,11> methods{0x0c0283c0,0x0c028480,0x0c028400,0x0c0284c0,0x0c028500,0x0c028540,0x0c028580,0x0c0285c0,0,0x0c0286c0,0x0c028720};
        if(value<methods.size()&&methods[value])applyOriginalCarAppearanceCall(s,methods[value],argument6);
        return;
    }
    const auto mapped=[&](std::uint32_t v){
        if(std::int32_t(v)<0)throw std::out_of_range("Negative original part variant");
        return v<=5?s.variants[v]:v;
    };
    const auto field=[&](unsigned shift,unsigned mask,std::uint32_t v){s.word=(s.word&~(mask<<shift))|((v&mask)<<shift);};
    switch(address){
    case 0x0c0283c0:field(0,7,mapped(value));break;
    case 0x0c028400:field(3,7,mapped(value));break;
    case 0x0c028440:field(6,1,mapped(value));break;
    case 0x0c028480:field(7,7,mapped(value));break;
    case 0x0c0284c0:field(10,7,mapped(value));break;
    case 0x0c028500:field(13,7,mapped(value));break;
    case 0x0c028540:field(16,7,mapped(value));break;
    case 0x0c028580:field(19,7,mapped(value));break;
    case 0x0c0285c0:{
        constexpr std::array<std::uint32_t,7> car19{0,1,2,6,3,4,5},car27{0,1,2,5,3,4,6};
        if(s.car==19)value=car19.at(value);
        if(s.car==27)value=car27.at(value);
        field(22,7,mapped(value));break;
    }
    case 0x0c028660:field(25,7,value);s.paintDirty=true;break;
    case 0x0c0286a0:s.materialVariant=value;break;
    case 0x0c0286c0:
        // Values1/2 add a bit to the installed pair;0/3 replace the pair.
        if(value==1||value==2)value=(s.word>>28)|(1u<<(value-1));
        field(28,3,value);break;
    case 0x0c028720:field(30,1,value);break;
    case 0x0c028760:field(31,1,value);break;
    default:throw std::invalid_argument("Unsupported original appearance setter");
    }
}
OriginalCarAppearanceConfig originalPlayerAppearanceConfig(const OriginalBattleProfile& profile,std::uint32_t materialVariant){
    OriginalCarAppearanceConfig out(profile.u(16));
    for(unsigned slot:{0u,2u,1u,3u,4u,5u,6u,7u,9u})applyOriginalCarAppearanceCall(out,0x0c0287a0,slot,profile.byte(156+slot));
    applyOriginalCarAppearanceCall(out,0x0c028660,originalCarPresentationColor(out.car,profile.u(64)));
    applyOriginalCarAppearanceCall(out,0x0c0286a0,materialVariant);
    applyOriginalCarAppearanceCall(out,0x0c028720,profile.byte(166)&1);
    applyOriginalCarAppearanceCall(out,0x0c028760,(profile.byte(166)>>1)&1);
    return out;
}
}
