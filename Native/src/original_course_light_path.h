#pragma once
#include "math_types.h"
#include <span>
#include <stdexcept>
namespace idas3 {
//099460 samples the authored centre point, with period=count-1. Course::load
//already reverses all count points, matching the original reverse index
//period-wrappedIndex (distinct from the cell index period-index-1).
inline Vec3 originalCourseLightReference(std::span<const Vec3> orientedPoints,int index){
    if(orientedPoints.size()<2||orientedPoints.size()>1000000)
        throw std::invalid_argument("Original course light path extent");
    const int period=int(orientedPoints.size()-1);
    int wrapped=index%period;if(wrapped<0)wrapped+=period;
    return orientedPoints[unsigned(wrapped)];
}
}
