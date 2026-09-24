#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace idas3 {

struct HudAnalogSample {
    std::uint64_t tick=0;
    float speed=0,rpm=0;
};

// A read-only display of the two most recent fixed-step samples. There is no
// frame-rate-dependent easing and no extrapolation past the latest solver RPM.
// Gear and all simulation owners retain current state. RPM-based display
// effects follow the presented needle; discrete telemetry flags are unchanged.
class HudAnalogPresentation {
public:
    void reset(){*this={};}
    HudAnalogSample sample(const HudAnalogSample& previous,const HudAnalogSample& current,
        float alpha,bool interpolate){
        alpha=std::isfinite(alpha)?std::clamp(alpha,0.f,1.f):1.f;
        const bool adjacent=current.tick>0&&previous.tick==current.tick-1;
        // A paused/held frame or a clock reset must not briefly replay the old
        // needle interval on resuming. Wait for one genuinely new solver tick.
        if(!initialized_||!interpolate||!adjacent||current.tick<lastTick_||
            (current.tick==lastTick_&&alpha+1e-6f<lastAlpha_))blockedTick_=current.tick;
        initialized_=true;lastTick_=current.tick;lastAlpha_=alpha;
        HudAnalogSample result=current;
        result.speed=nonnegative(current.speed);result.rpm=nonnegative(current.rpm);
        if(interpolate&&adjacent&&current.tick!=blockedTick_){
            if(std::isfinite(previous.speed))result.speed=std::lerp(nonnegative(previous.speed),result.speed,alpha);
            if(std::isfinite(previous.rpm))result.rpm=std::lerp(nonnegative(previous.rpm),result.rpm,alpha);
        }
        return result;
    }
private:
    static float nonnegative(float value){return std::isfinite(value)?std::max(0.f,value):0.f;}
    std::uint64_t lastTick_=0,blockedTick_=0;
    float lastAlpha_=0;
    bool initialized_=false;
};

}
