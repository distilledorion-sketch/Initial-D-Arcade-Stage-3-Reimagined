#pragma once
#include <array>
#include <cmath>
#include <cstdint>

namespace idas3 {
// IDR2 revision2 appends an optional source-input timeline. Positions and the
// footprint are captured before host interpolation; no contact re-query or
// subtraction of final car poses can replace these values exactly.
struct ReplayRoadsideDustFrame {
    std::uint32_t flags=0; // bit0: source owner (0 player/1 rival), bit1: available
    std::uint32_t surfaces=0;
    float displacement=0;
    std::array<float,16> footprint{};
    std::array<std::array<float,3>,4> positions{};
    static constexpr unsigned bytes=124,maxFrames=108000+1024;
    bool valid()const{
        if(flags>3||!std::isfinite(displacement)||displacement<0||displacement>10000)return false;
        for(float v:footprint)if(!std::isfinite(v)||std::abs(v)>1000000)return false;
        for(const auto& p:positions)for(float v:p)if(!std::isfinite(v)||std::abs(v)>1000000)return false;
        return true;
    }
};
static_assert(sizeof(ReplayRoadsideDustFrame)==ReplayRoadsideDustFrame::bytes);
}
