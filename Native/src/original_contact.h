#pragma once
#include "original_dynamics.h"

namespace idas3::original {
// Original actor fields mirrored by157AE0 before its158200 contact call.
// This is a typed fixed record, separate from the native render transform.
struct OriginalActorState {
    std::array<std::uint32_t,0x100/4> words{};
    float f(std::size_t offset) const{return std::bit_cast<float>(words.at(offset/4));}
    std::uint32_t u(std::size_t offset) const{return words.at(offset/4);}
    void setf(std::size_t offset,float value){words.at(offset/4)=std::bit_cast<std::uint32_t>(value);}
    void setu(std::size_t offset,std::uint32_t value){words.at(offset/4)=value;}
    void setByte(std::size_t offset,std::uint8_t value){auto& word=words.at(offset/4);const auto shift=(offset&3)*8;word=(word&~(0xFFu<<shift))|(std::uint32_t(value)<<shift);}
};
struct OriginalWheelHistory {
    std::array<std::uint32_t,4> rotationCounters0CAA94B4{};
};

// Exact[157AF4,157D66) stage afterCEC0, stopping before158200 executes.
// Copies actor pose/history/flags, computes contact-derived next-frame224/228,
// updates wheel rotation accumulators. It does not query a replacement road.
void prepareOriginalContactFrame(OriginalDriveState& drive,OriginalActorState& actor,
    OriginalWheelHistory& wheelHistory);
} // namespace idas3::original
