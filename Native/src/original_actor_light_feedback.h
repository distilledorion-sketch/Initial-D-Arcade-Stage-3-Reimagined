#pragma once
#include <cstdint>

namespace idas3::original {
// Complete035220: ACar byte81 is the lamp gate, pointer+095C selects its
// live actor, and actor+50 bit16 records OFF. Every other bit is preserved.
constexpr std::uint32_t originalActorLightFeedback(std::uint32_t flags,bool enabled){
    return enabled?(flags&0xfffeffffu):(flags|0x00010000u);
}
}
