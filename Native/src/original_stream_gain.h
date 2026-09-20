#pragma once
#include <cstdint>
namespace idas3::original {
// ARM5A00/5A74 ->5D34 ->34C0 ->3368. Driver16E4 initializes
// each of the eight stream descriptors' master byte+1 to64.
std::uint8_t originalStreamTotalLevel(std::uint8_t volume,std::uint8_t streamMaster=64);
// Same integer AICA attenuation convention as OriginalIcsPlayer; analog
// output/cabinet gain and the native mix headroom remain separate.
std::int32_t originalStreamGainQ15(std::uint8_t volume,std::uint8_t streamMaster=64);
float originalStreamGain(std::uint8_t volume,std::uint8_t streamMaster=64);
}
