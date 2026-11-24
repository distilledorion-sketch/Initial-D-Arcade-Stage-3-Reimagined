#pragma once
#include <algorithm>
#include <cmath>
#include "flycast_keyboard_steering.h"

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
    FlycastKeyboardSteering keyboard_;
    bool stickSampled_=false;
    std::int16_t lastX_=0,pairedAtX_=0;
public:
    void reset(){keyboardOwns_=false;keyboard_.reset();stickSampled_=false;}
    // Flycast's SDL path updates joyx only on a horizontal event. With snapshots,
    // changes to both axes in the same poll have no observable event order.
    std::int16_t pairedAtHorizontalChange(std::int16_t x,std::int16_t y,bool enabled=true){
        if(!enabled){stickSampled_=false;return y;}
        if(!stickSampled_||x!=lastX_){stickSampled_=true;lastX_=x;pairedAtX_=y;}
        return pairedAtX_;
    }
    void sampleKeyboard(std::uint64_t milliseconds,bool left,bool right){
        keyboard_.sample(milliseconds,left,right);
        // Events can arrive in a render frame with no simulation tick.
        if(left!=right)keyboardOwns_=true;
    }
    // A negative value selects the existing normalized/pad adapter. Only the
    // unsmoothed original keyboard path publishes a complete JVS ADC word.
    int keyboardAnalog()const{return keyboardOwns_?int(keyboard_.analog()):-1;}
    float advance(float current,float keyboard,float analog,bool connected,
                  bool originalHandling,float dt){
        const bool useStick=connected&&std::abs(analog)>.01f;
        if(useStick)keyboardOwns_=false;
        else if(keyboard!=0.f)keyboardOwns_=true;
        const float target=useStick?std::clamp(analog,-1.f,1.f):keyboard;
        if(originalHandling){
            if(useStick)current=target;
            else current=keyboardOwns_?keyboard_.steering():0.f;
        }
        else if(!useStick)current=advanceHostKeyboardSteering(current,target,dt);
        else current+=std::clamp(target-current,-12.f*dt,12.f*dt);
        // A raw value within the center byte can normalize to zero before it
        // reaches signed zero. Retain ownership through that final release.
        if(!useStick&&keyboard==0.f&&(originalHandling?keyboard_.raw()==0:current==0.f))keyboardOwns_=false;
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
