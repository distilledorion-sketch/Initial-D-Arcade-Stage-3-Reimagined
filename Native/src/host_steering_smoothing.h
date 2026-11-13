#pragma once
#include <algorithm>
#include <cmath>

namespace idas3 {
// Digital steering needs a short ramp for taps, not the old 0.67-second lag.
// This host-only response does not change pad/wheel axes or the car solver.
inline float advanceHostKeyboardSteering(float current,float target,float dt){
    const float rate=target==0.f?10.f:current*target<0.f?12.f:6.f;
    return current+std::clamp(target-current,-rate*dt,rate*dt);
}
// Keep a keyboard release on the keyboard's existing return ramp even when
// an unused controller is connected. A real stick still takes ownership
// immediately, including its next neutral sample.
class HostSteeringSource {
    bool keyboardOwns_=false;
public:
    void reset(){keyboardOwns_=false;}
    float advance(float current,float keyboard,float analog,bool connected,
                  bool originalHandling,float dt){
        const bool useStick=connected&&std::abs(analog)>.01f;
        if(useStick)keyboardOwns_=false;
        else if(keyboard!=0.f)keyboardOwns_=true;
        const float target=useStick?std::clamp(analog,-1.f,1.f):keyboard;
        if(originalHandling&&connected&&(useStick||(keyboard==0.f&&!keyboardOwns_)))current=target;
        else if(!useStick)current=advanceHostKeyboardSteering(current,target,dt);
        else current+=std::clamp(target-current,-12.f*dt,12.f*dt);
        if(!useStick&&keyboard==0.f&&current==0.f)keyboardOwns_=false;
        return current;
    }
};
// Optional host preference after the existing input conditioning. This is not
// part of the recovered car solver. The bypass returns its input unchanged.
class HostSteeringSmoothing {
    float amount_=0;
    double filtered_=0;
public:
    float amount()const{return amount_;}
    bool setAmount(float value){
        if(!std::isfinite(value)||value<0.f||value>1.f)return false;
        if(amount_!=value){amount_=value;reset();}
        return true;
    }
    void reset(){filtered_=0;}
    float advance(float target,float dt,bool blocked=false){
        if(blocked){reset();return 0.f;}
        if(amount_==0.f){filtered_=target;return target;}
        if(!std::isfinite(target)){reset();return 0.f;}
        target=std::clamp(target,-1.f,1.f);
        if(!std::isfinite(dt)||dt<=0.f)return float(filtered_);
        const double alpha=-std::expm1(-double(dt)/(.2*double(amount_)));
        filtered_=std::clamp(filtered_+(double(target)-filtered_)*alpha,
                             std::min(filtered_,double(target)),std::max(filtered_,double(target)));
        return float(filtered_);
    }
};
}
