#include "original_stream_gain.h"
#include <algorithm>
#include <array>
#include <cmath>
namespace idas3::original {
namespace {
// Exact128 bytes at ARM7EB0 in AICADRV.bin SHA256
//6b1a51d7576b6333a0aa741ce3b0b3ca6c3818d6de2b092d37b9196672478f67.
constexpr std::array<std::uint8_t,128> levelTable{
0,7,14,21,28,36,43,50,57,64,71,75,79,82,86,90,
94,97,101,105,109,112,116,120,122,125,127,130,132,135,137,140,
142,145,147,150,152,155,157,159,162,164,167,169,173,174,176,177,
179,180,181,183,184,186,187,189,190,191,193,194,196,197,198,200,
201,203,204,205,207,209,210,211,212,213,214,216,217,218,219,220,
221,222,223,224,225,227,228,229,230,231,232,233,234,235,237,238,
238,239,239,240,240,241,241,242,243,243,244,244,245,245,246,247,
247,248,248,249,249,250,251,251,252,252,253,253,254,254,254,254};
}
std::uint8_t originalStreamTotalLevel(std::uint8_t volume,std::uint8_t master){
    return levelTable[std::clamp(int(volume)+int(master)-64,0,127)]^255;
}
std::int32_t originalStreamGainQ15(std::uint8_t volume,std::uint8_t master){
    const auto level=originalStreamTotalLevel(volume,master);
    return level==255?0:std::int32_t(32768.0*std::exp2(-double(level)/16));
}
float originalStreamGain(std::uint8_t volume,std::uint8_t master){return float(originalStreamGainQ15(volume,master))/32768.f;}
}

