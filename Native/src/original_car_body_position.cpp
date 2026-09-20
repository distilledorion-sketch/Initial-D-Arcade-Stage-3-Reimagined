#include "original_car_body_position.h"
#include "original_car_dimensions.h"
#include <bit>

namespace idas3 {
void OriginalCarBodyPosition::reset(){
    query_={};trace_={};surface_={};surfaceFound_=false;
    original::clearOriginalCollisionQuery(query_);
}
Vec3 OriginalCarBodyPosition::update(const original::OriginalCollisionData& collision,unsigned carId,Vec3 actor){
    query_.setf(32,actor.x);query_.setf(36,actor.y);query_.setf(40,actor.z);
    //034C20 calls022CE0 once. Failure emits a diagnostic but still uses the
    //query's retained normal, so neither reset nor a host fallback belongs here.
    surfaceFound_=original::queryOriginalCollisionSurface(collision,query_,trace_,surface_);
    const float height=originalCarRideHeight(carId);
    const float x=query_.f(0)*height,y=query_.f(4)*height,z=query_.f(8)*height;
    //034840 subtracts0.02 when building the actor matrix, BEFORE034C20
    //pre-multiplies the world-space normal translation. Preserve that rounding.
    const float baseY=actor.y-std::bit_cast<float>(0x3ca3d70au);
    return {actor.x+x,baseY+y,actor.z+z};
}
}
