#pragma once
#include <cstdint>
namespace idas3::original {
// Native lifts of the game's own rational sine/cosine implementation. These
// are not calls to the platform C library. Normal finite-F32 FPSCR contract;
// exceptional FPU modes and NaN payload behavior are outside this interface.
float originalSinF32(float angle);
float originalCosF32(float angle);
// Original inverse trig returns integer angle units (65536 per turn), not
// radians. Preserve the full u32 return: asin's negative out-of-domain branch
// sign-extends -16384, while the exact -1 endpoint returns 0x0000c000.
std::uint32_t originalAtanAngle(float value);                 // 0C1F9700
std::uint32_t originalAsinAngle(float value);                 // 0C1F9640
std::uint32_t originalAtan2Angle(float first,float second);   // 0C1F97C0
}
