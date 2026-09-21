#pragma once
#include "math_types.h"

namespace idas3 {
// Presentation only. A rollback replaces the predicted position underneath the
// offset. Retain correction velocity so another packet cannot restart a sharp
// exponential catch-up. The exact critically damped solution is independent of
// render rate and converges without adding a delayed physics/collision state.
template<class T> inline void advanceOnlineVisualCorrection(T& offset,T& velocity,float dt){
    constexpr float omega=18.f;
    const float decay=std::exp(-omega*dt);
    const T impulse=velocity+offset*omega;
    offset=(offset+impulse*dt)*decay;
    velocity=(velocity-impulse*(omega*dt))*decay;
}
}
