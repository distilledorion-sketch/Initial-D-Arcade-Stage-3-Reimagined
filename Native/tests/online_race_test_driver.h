#pragma once
#include "online_race_simulation.h"
#include "original_host_input.h"
#include <algorithm>
#include <bit>
#include <cmath>

// Deterministic ordinary controls used by numerical and App-level online
// fixtures. They follow the real path and brake/steer; no actor is relocated.
inline idas3::original::OriginalHostControls onlineRaceTestControls(
        const idas3::original::OnlineRaceSimulation& sim,unsigned slot,unsigned frame){
    const auto& car=sim.car(slot);const auto& d=car.vehicle().drive;const auto& points=car.path().points;
    std::size_t nearest=0;float best=1e30f;
    for(std::size_t i=0;i<points.size();++i){float dx=points[i][0]-d.f(0),dz=points[i][2]-d.f(8),dist=dx*dx+dz*dz;if(dist<best){best=dist;nearest=i;}}
    auto target=nearest;float length=0;const float look=std::max(10.f,d.f(0x238)*.65f);
    while(length<look&&target+1<points.size()){float x=points[target+1][0]-points[target][0],z=points[target+1][2]-points[target][2];length+=std::sqrt(x*x+z*z);++target;}
    const auto& p=points[target];const float wanted=std::atan2(-(p[0]-d.f(0)),-(p[2]-d.f(8)));
    float steer=std::clamp(-std::remainder(wanted-d.f(0x10),6.28318530718f)*.85f,-.8f,.8f),gas=1,brake=0;
    const auto cycle=(frame+slot*60)%360;
    if(cycle>=200&&cycle<220){gas=0;brake=.7f;}
    if(cycle>=240&&cycle<252)steer=(frame/360)%2?-.9f:.9f;
    if(cycle>=280&&cycle<295)gas=0;
    return {steer,gas,brake,frame%360==205,frame%110==90};
}

inline std::uint64_t onlineContactEffectsDigest(const idas3::original::OnlineRaceFrame& frame){
    std::uint64_t hash=14695981039346656037ull;
    const auto add=[&](std::uint64_t value){for(unsigned i=0;i<8;++i){hash^=(value>>(8*i))&255;hash*=1099511628211ull;}};
    add(frame.frame);
    for(const auto& driving:frame.driving){
        add(driving.bodyCollision.active);add(std::bit_cast<std::uint32_t>(driving.bodyCollision.x));add(std::bit_cast<std::uint32_t>(driving.bodyCollision.z));
        add(driving.feedback142460.size());for(auto cue:driving.feedback142460)add(cue);
        add(driving.completion.requestCue4);add(driving.newImpactRecords.size());
    }
    return hash;
}
