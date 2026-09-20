#pragma once
#include "math_types.h"
#include <array>
namespace idas3 {
struct CarWheelPose {
    // Original actor3C,40..4C,60..6C. Order: FR, FL, RR, RL.
    float steeringRadians=0;
    std::array<float,4> suspensionY{},rotationRadians{};
};
inline CarWheelPose interpolateCarWheels(const CarWheelPose& previous,const CarWheelPose& current,float alpha){
    CarWheelPose out;
    // Rivals publish steering as slip+pi. Crossing the slip wrap switches
    // between near-zero and near-2pi values for the same wheel direction.
    out.steeringRadians=lerpAngle(previous.steeringRadians,current.steeringRadians,alpha);
    for(unsigned i=0;i<4;++i){out.suspensionY[i]=previous.suspensionY[i]+(current.suspensionY[i]-previous.suspensionY[i])*alpha;
        out.rotationRadians[i]=lerpAngle(previous.rotationRadians[i],current.rotationRadians[i],alpha);}
    return out;
}
}
