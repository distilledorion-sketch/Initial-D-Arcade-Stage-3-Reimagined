#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace idas3 {
// Host-output protection, after the original instruments, shared DSP and user
// gains. No lookahead or audio latency. One gain preserves the stereo balance.
// This limits sample peaks; it is not an intersample/true-peak limiter.
class AudioPeakLimiter {
public:
    static constexpr float ceiling=.98f;
    void reset(){reduction_=0;}
    void process(std::array<float,2>& frame){
        const float peak=std::max(std::abs(frame[0]),std::abs(frame[1]));
        // Instant attack, 50 ms exponential release on the 44100 Hz output
        // clock. Store reduction so recovery cannot get stuck below unity.
        constexpr float release=.999546588f;
        reduction_=std::max(peak>ceiling?1.f-ceiling/peak:0.f,reduction_*release);
        if(reduction_<.000001f)reduction_=0;
        if(reduction_>0){const float gain=1.f-reduction_;frame[0]*=gain;frame[1]*=gain;}
    }
private:
    float reduction_=0;
};
}
