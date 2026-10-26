#pragma once
#include <array>
#include <cstdint>
#include <string>
namespace idas3::original {
struct OriginalNetPasswordInput {
    unsigned course=0,direction=0,weather=0,night=0;
    std::uint32_t ticks6000=0;
    unsigned car=0,manual=0;
    std::array<std::uint8_t,5> name{221,221,221,221,221};
    std::uint32_t salt=0;
};
// 0DE100 / 0DDCE0 / 0DDEC0, ordinary native arithmetic. Cabinet IR format,
// not an account credential or a submission to the community leaderboard.
std::array<unsigned,14> originalNetPassword(const OriginalNetPasswordInput&);
std::string originalNetPasswordText(const OriginalNetPasswordInput&);
unsigned originalNetPasswordGlyph(unsigned symbol);
}
