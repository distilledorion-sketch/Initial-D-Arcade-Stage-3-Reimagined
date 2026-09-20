#pragma once
#include "math_types.h"

namespace idas3 {
struct ChaseView { Vec3 eye,target; };
// Host presentation matched to the user's close chase-view reference.
// Translation follows the interpolated car exactly; only orbit heading eases.
// These are explicit presentation choices, not recovered original constants.
class ChaseCamera {
public:
    static constexpr float distance=5.1f,height=1.45f,lookAhead=7.f;
    static constexpr float verticalFieldOfView=.85f;
    void reset(){ready=false;}
    ChaseView update(Vec3 car,float heading,double seconds){
        if(!ready){yaw=heading;ready=true;}
        else yaw=lerpAngle(yaw,heading,float(1-std::exp(-std::clamp(seconds,0.0,.1)*18)));
        const auto facing=forward(yaw);
        return {car-facing*distance+Vec3{0,height,0},car+facing*lookAhead+Vec3{0,height,0}};
    }
private:
    bool ready=false;
    float yaw=0;
};
}
