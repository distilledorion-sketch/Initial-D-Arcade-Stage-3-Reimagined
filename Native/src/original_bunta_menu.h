#pragma once
#include "original_choice_menu.h"
#include <array>

namespace idas3::original {
// Original iSelBunta 19A980. The caller advances frame440 at 60Hz and
// resets it when the selected course changes (19A940); saved level436 is
// the cleared challenge count, not the next opponent number.
struct OriginalBuntaMenuDraws {
    std::uint32_t backdropChunk=1;
    std::array<std::uint32_t,3> progressColors{};
    std::vector<OriginalChoiceDraw> stars;
};
OriginalBuntaMenuDraws originalBuntaMenuDraws(std::int32_t level,std::uint32_t frame);
NativeModelChunk materializeOriginalBuntaColors(const NativeModelChunk& chunk,
    const std::array<std::uint32_t,3>& progressColors);
}
