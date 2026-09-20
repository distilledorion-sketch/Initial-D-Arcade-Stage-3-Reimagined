#pragma once
#include <algorithm>
#include <cstdint>
#include <stdexcept>
namespace idas3 {
// Ordinary ACar fields from026436/027080. State advances once per original
// presentation frame, independently of the host display refresh rate.
struct OriginalHeadlightState {
    std::int32_t counter=-1;
    std::uint32_t maximumPhase=0,phase=0;
    bool visible=false;
    float fraction=0;
    void advance(bool lightsOn){
        if(maximumPhase==0){counter=lightsOn?1:0;phase=0;visible=lightsOn;fraction=lightsOn?1.f:0.f;return;}
        if(counter<0)counter=lightsOn?40:0;
        counter=std::clamp(counter+(lightsOn?1:-1),0,40);
        phase=(maximumPhase*std::uint32_t(40-counter))/40u;
        fraction=float(counter)/40.f;visible=counter!=0;
    }
    void reset(){counter=-1;phase=0;visible=false;fraction=0;}
};
}
