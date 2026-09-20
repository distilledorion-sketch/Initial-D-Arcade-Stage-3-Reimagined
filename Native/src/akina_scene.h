#pragma once
#include <array>
#include <cstddef>

namespace idas3 {
// Original 0C29EE58 table, returned by 0C191CE0 for course 3.
// These are indices in the 4089-point forward k_df_path, not the shorter
// physics PATH_dfi_0 route. 0C03AA80 advances at equality with each boundary.
inline constexpr std::array<std::size_t,29> originalAkinaSectorBoundaries{
    105,233,339,457,617,721,857,1001,1249,1377,1529,1665,1769,1953,
    2081,2193,2409,2513,2617,2897,3081,3161,3297,3433,3561,3689,
    3801,3897,4019
};
inline constexpr std::size_t akinaSectorForPathIndex(std::size_t originalForwardPathIndex) {
    std::size_t sector=0;
    while(sector<originalAkinaSectorBoundaries.size() &&
          originalForwardPathIndex>=originalAkinaSectorBoundaries[sector]) ++sector;
    return sector;
}
static_assert(akinaSectorForPathIndex(0)==0 && akinaSectorForPathIndex(104)==0);
static_assert(akinaSectorForPathIndex(105)==1 && akinaSectorForPathIndex(4019)==29);
static_assert(akinaSectorForPathIndex(4088)==29);
}
