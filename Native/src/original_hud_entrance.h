#pragma once
#include <algorithm>
#include <array>
namespace idas3::original {
struct HudEntrance { float labels=0,backings=0; };
inline HudEntrance originalHudEntrance(unsigned frame60){
    // 0C0C8082..80E8 initializes both slides at 3. The backing filter
    // (1AE520/1AE560, response 3) targets zero after frame 1; the label
    // and portrait filter (1AE640/1AE6A0, .8/.5) does so after frame 7.
    // 0C0C8AE6..8BB6 publishes +D4/+D0. Evaluate once into an age table:
    // repeated paints, pause and high refresh cannot advance the filters.
    static const auto frames=[] {
        std::array<HudEntrance,241> out{};out[0]={3,3};
        float priorBacking=3,backing=3,input1=3,input2=3,output1=3,output2=3;
        const float squared=.5f*.5f;
        float damping=.8f*4.f;damping*=.5f;
        float a=squared-damping;a+=4.f;
        float b=squared-4.f;b+=b;
        float c=squared+damping;c+=4.f;
        for(unsigned tick=1;tick<out.size();++tick){
            const float targetBacking=tick>1?0.f:3.f;
            backing=(priorBacking+targetBacking-(-2.f*backing))*.25f;
            priorBacking=targetBacking;
            const float targetLabel=tick>7?0.f:3.f;
            float label=input1+input1;label=input2+label;label+=targetLabel;
            label*=squared;label-=a*output2;label-=b*output1;label/=c;
            input2=input1;input1=targetLabel;output2=output1;output1=label;
            out[tick]={label,backing};
        }
        return out;
    }();
    return frames[std::min(frame60,240u)];
}
}
