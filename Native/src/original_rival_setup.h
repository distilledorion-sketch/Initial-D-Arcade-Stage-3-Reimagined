#pragma once
#include "original_rival.h"

namespace idas3::original {
// Complete sparse reset15ADC0. Public records, road queries and every other
// actor word survive a new race; initialization15AE00 owns its separate writes.
inline void disableOriginalRivals(std::span<OriginalRivalState,8> actors,
    std::uint32_t& frame0CAA986C) {
    for(auto& actor:actors)actor.setu(0,0);
    frame0CAA986C=0;
}
} // namespace idas3::original
