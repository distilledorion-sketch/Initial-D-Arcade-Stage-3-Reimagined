#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>

namespace idas3::original {
// ARace's distinct brown roadside emitter,060B38..060C76. This is admission
// only: particle allocation, lifetime and drawing still belong to SmokeMgr.
// The low-nibble6/7 test in060964 is for the other smoke branch, not this one.
struct OriginalRoadsideDustInput {
    unsigned course=0,night=0,weather=0,effectIndex=0;
    std::array<std::uint8_t,4> wheelSurfaces{};
    bool emitterPresent=false,allocationAvailable=true;
    unsigned emitterKind=0;
    //0608B2 uses1 for the player;060940 uses.2 for the rival. This is
    // passed through140660 to the display, not the attachment/motion scale.
    float displayGain=1.f;
};
struct OriginalRoadsideDustDecision {
    enum class Action {None,Start,Stop};
    Action action=Action::None;
    bool replacesTireSmoke=false;
    unsigned wheel=0,emitterKind=0;
    float displayGain=0;
    std::uint32_t color=0;
};
inline OriginalRoadsideDustDecision originalRoadsideDustDecision(const OriginalRoadsideDustInput& in){
    if(in.effectIndex>=4)throw std::out_of_range("Original roadside dust wheel index");
    OriginalRoadsideDustDecision out;out.wheel=3-in.effectIndex;
    // ARace+674 course, +680 night, +684 weather. Day and dry are explicit
    // source requirements; do not replace them with a generic speed/slip gate.
    if(in.course!=7||in.night!=0||in.weather!=0)return out;
    out.replacesTireSmoke=(in.wheelSurfaces[out.wheel]&0x20u)!=0;
    if(out.replacesTireSmoke){
        if(!in.emitterPresent&&in.allocationAvailable){
            out.action=OriginalRoadsideDustDecision::Action::Start;
            out.emitterKind=2;out.displayGain=in.displayGain;out.color=0xff8b5439u;
        }
    }else if(in.emitterPresent&&in.emitterKind==2)
        out.action=OriginalRoadsideDustDecision::Action::Stop;
    return out;
}
//060F00..060FC2, after admission: stationary dust emitters remain attached,
// but get zero particle lifetime. The actor displacement magnitude is in
// source tick units. It must not be replaced by host km/h or render delta.
inline float originalRoadsideDustLifetimeScale(unsigned effectIndex,float actorDisplacementMagnitude){
    if(effectIndex>=4)throw std::out_of_range("Original roadside dust wheel index");
    if(!(actorDisplacementMagnitude>.1f))return 0.f;
    return effectIndex>1?.5f:.3f;
}
}
