#pragma once
#include "math_types.h"
#include <array>
#include <cstdint>

namespace idas3::original {
struct OriginalTimeAttackBackgroundDraw {
    unsigned chunk=45;
    // Bank-space displacement, including the original getter/setter rounding.
    // A menu compositor multiplies X by100 and Y by-100; Z is source depth.
    Vec3 position;
};
// HLecture owns common-select tile45 through112DC0/112E60. Its animation
// advances on original60Hz updates, never on repeated paint calls.
std::uint32_t advanceOriginalTimeAttackBackground(std::uint32_t frame);
std::array<OriginalTimeAttackBackgroundDraw,12> originalTimeAttackBackgroundDraws(std::uint32_t displayedFrame);
}
