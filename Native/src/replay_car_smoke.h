#pragma once
#include "replay_roadside_dust.h"

namespace idas3 {
// IDR2 revision3. Retain the revision2 owner/geometry prefix, then record the
// original intensity bytes, scene decisions and RNG immediately before the
// source tick. A pose alone cannot reproduce contact admission or draining.
struct ReplayCarSmokeFrame {
    ReplayRoadsideDustFrame owner;
    std::uint32_t intensity=0;
    // course:0..3, night:4, weather:5, profile course:6..9,
    // source player-request flag:10, alternate smoke parameters:11.
    std::uint32_t scene=0,randomBefore=0;
    static constexpr unsigned bytes=136,maxFrames=ReplayRoadsideDustFrame::maxFrames;
    static constexpr unsigned fixedSceneMask=0x83f;
    bool valid()const{
        return owner.valid()&&!(scene&~0xfffu)&&(scene&15)<9&&((scene>>6)&15)<9&&
            !((owner.flags&1)&&(scene&0x400));
    }
};
static_assert(sizeof(ReplayCarSmokeFrame)==ReplayCarSmokeFrame::bytes);
}
