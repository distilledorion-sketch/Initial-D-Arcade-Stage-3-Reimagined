#pragma once
#include "math_types.h"
namespace idas3 {
struct CarBodyAngles {float pitch=0,roll=0;};
// Original road contact publishes pitch/roll in wrapped [0,2pi) form.
// Crossing zero must not send the displayed body through a half turn.
inline CarBodyAngles interpolateCarBodyAngles(CarBodyAngles previous,CarBodyAngles current,float alpha){
    return {lerpAngle(previous.pitch,current.pitch,alpha),lerpAngle(previous.roll,current.roll,alpha)};
}
}
