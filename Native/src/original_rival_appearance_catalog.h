#pragma once
#include <array>
#include <cstdint>
namespace idas3 {
// Exact31D8A8 car IDs and035F00/2F48CC number-plate digits.
struct OriginalRivalAppearance {unsigned car;std::array<std::uint8_t,5> digits;};
inline constexpr std::array<OriginalRivalAppearance,31> originalRivalAppearances{{
    {2, {1,1,0,0,9}}, // enemy0
    {13, {0,8,7,7,6}}, // enemy1
    {16, {4,6,0,3,7}}, // enemy2
    {25, {8,6,5,9,6}}, // enemy3
    {31, {9,5,0,8,5}}, // enemy4
    {14, {7,8,5,4,7}}, // enemy5
    {12, {3,4,6,2,8}}, // enemy6
    {10, {7,7,1,4,8}}, // enemy7
    {22, {6,3,8,8,7}}, // enemy8
    {9, {5,1,7,4,5}}, // enemy9
    {7, {2,6,0,3,7}}, // enemy10
    {19, {3,0,3,9,5}}, // enemy11
    {24, {1,3,1,3,7}}, // enemy12
    {0, {1,3,9,5,4}}, // enemy13
    {20, {4,6,6,3,7}}, // enemy14
    {19, {3,0,3,9,5}}, // enemy15
    {3, {3,7,5,9,7}}, // enemy16
    {6, {2,7,4,3,1}}, // enemy17
    {15, {5,6,8,3,8}}, // enemy18
    {17, {3,2,0,9,4}}, // enemy19
    {15, {1,0,4,5,7}}, // enemy20
    {5, {1,7,9,1,9}}, // enemy21
    {30, {3,5,2,1,8}}, // enemy22
    {1, {7,3,2,1,2}}, // enemy23
    {22, {0,4,8,4,2}}, // enemy24
    {24, {1,3,1,3,7}}, // enemy25
    {32, {0,5,8,5,1}}, // enemy26
    {33, {8,9,2,7,8}}, // enemy27
    {22, {6,3,8,8,7}}, // enemy28
    {0, {1,3,9,5,4}}, // enemy29
    {29, {1,3,6,0,0}}, // enemy30
}};
}
