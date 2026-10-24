#pragma once
#include "online_race_simulation.h"
#include "original_host_input.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

// Read-only guidance for isolated scored-race acceptance. It generates normal
// controls; it cannot advance the clock, place a car, extend time or set a result.
// This deliberately simple path follower is not a reference for skilled driving.
inline idas3::original::OriginalHostControls onlineScoredDriver(
        const idas3::original::OnlineRaceSimulation& sim,unsigned slot){
    const auto& car=sim.car(slot);const auto& d=car.vehicle().drive;
    const auto& points=car.path().points;
    if(points.empty())throw std::logic_error("Scored diagnostic needs a course path");
    std::size_t nearest=0;float best=1e30f;
    for(std::size_t i=0;i<points.size();++i){
        const float x=points[i][0]-d.f(0),z=points[i][2]-d.f(8),distance=x*x+z*z;
        if(distance<best){best=distance;nearest=i;}
    }
    auto target=nearest;float length=0;const float look=std::max(10.f,d.f(0x238)*.65f);
    const bool closed=!sim.setup().imported&&sim.setup().condition<4;
    for(std::size_t step=0;step<points.size()&&length<look;++step){
        auto next=target+1;
        if(next==points.size()){if(!closed)break;next=0;}
        const float x=points[next][0]-points[target][0],z=points[next][2]-points[target][2];
        length+=std::sqrt(x*x+z*z);target=next;
    }
    const auto& p=points[target];const float wanted=std::atan2(-(p[0]-d.f(0)),-(p[2]-d.f(8)));
    return {std::clamp(-std::remainder(wanted-d.f(0x10),6.28318530718f)*.85f,-.8f,.8f),1,0,false,false};
}
