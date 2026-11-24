#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace idas3 {
// Flycast v2.5 GamepadDevice::buttonToAnalogInput/rampAnalog, specialized to
// the first horizontal axis. This is a host input owner, not a physics tick.
// os_UpdateInputState ramps the previously held buttons before polling the
// next events. Retain the signed raw axis past the cabinet's calibrated lock.
class FlycastKeyboardSteering {
    std::int16_t raw_=0;
    std::uint64_t lastMilliseconds_=0;
    bool started_=false,ramping_=false,left_=false,right_=false;
public:
    void reset(){*this={};}
    void sample(std::uint64_t milliseconds,bool left,bool right){
        // The production clock is monotonic. A reset/replaced test clock must
        // not turn an unsigned time difference into a full-lock input.
        if(started_&&milliseconds<lastMilliseconds_)reset();
        if(started_&&ramping_){
            // 201ms spans the entire signed axis. Bounding only this already
            // saturated case avoids integer overflow after a very long sleep.
            const auto elapsed=std::min<std::uint64_t>(milliseconds-lastMilliseconds_,201);
            const int delta=int(std::round(float(elapsed)*(32767.f/100.f)));
            const int raw=raw_;
            if(left_!=right_)
                raw_=std::int16_t(right_?std::min(32767,raw+delta):std::max(-32768,raw-delta));
            else if(raw>0)raw_=std::int16_t(std::max(0,raw-delta));
            else if(raw<0)raw_=std::int16_t(std::min(0,raw+delta));
            else ramping_=false;
        }
        lastMilliseconds_=milliseconds;started_=true;
        if(left!=left_||right!=right_){left_=left;right_=right;ramping_=true;}
    }
    std::int16_t raw()const{return raw_;}
    // maple_jvs.cpp read_analog_axis and 0xff7f overflow bound. The guest
    // reverses Flycast's signed-low-byte wire compensation before publication.
    static std::uint16_t analogFromRaw(std::int16_t raw){return std::uint16_t(std::min(int(raw)+32768,0xff7f));}
    std::uint16_t analog()const{return analogFromRaw(raw_);}
    float steering()const{return std::clamp((int(analog()>>8)-128)/80.f,-1.f,1.f);}
};
}
