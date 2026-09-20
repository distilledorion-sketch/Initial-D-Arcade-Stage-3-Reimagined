#include "original_car_wheel_offsets.h"
#include "original_car_wheel_offset_catalog.h"
#include <bit>
#include <stdexcept>
namespace idas3::original {
OriginalCarWheelOffsets originalCarWheelOffsets(const OriginalCarAppearanceConfig& config){
    if(config.car>=35)throw std::out_of_range("Wheel-fit car ID outside original catalog");
    unsigned front=0,rear=0;
    if(config.car<=2)front=rear=(config.word>>10)&7;
    else if(config.car==22||config.car==23)front=rear=config.word&7;
    else{front=config.word&7;rear=(config.word>>13)&7;}
    const auto select=[&](unsigned variant,unsigned axle){
        if(!variant)return 0.0f;
        const unsigned index=variant-1>4?0:variant-1;
        return std::bit_cast<float>(originalCarWheelOffsetWords[config.car][index][axle]);
    };
    return {select(front,0),select(rear,1)};
}
}
