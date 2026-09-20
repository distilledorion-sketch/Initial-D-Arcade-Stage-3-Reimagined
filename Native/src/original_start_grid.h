#pragma once
#include <array>
#include <cstdint>

namespace idas3::original {
struct OriginalStartGridPose {
    std::array<float,3> position{};
    std::array<float,3> angles{};
    std::array<float,3> authoredDirection{};
    std::array<float,3> horizontalDirection{};
    std::uint32_t gridSlot=0;
};
// Original course table getters191C20/191C80 and race0629F2..062A48.
// The two grid slots are authored positions, not a host lateral offset.
// All18 conditions accepted:2*course+direction, original course order
// Myogi,Usui,Akagi,Akina,Happogahara,Irohazaka,Shomaru,Tsuchisaka,AkinaSnow.
OriginalStartGridPose originalStartPose(std::uint32_t conditionCode,std::uint32_t gridSlot);
// Backwards-compatible Akina-only entry point.
OriginalStartGridPose originalAkinaStartPose(std::uint32_t conditionCode,std::uint32_t gridSlot);
// Original raceState+668 selection at062706..0629F2. Network mode1
// requires peer ordering and is deliberately outside this solo interface.
// Numeric modes are retained until their menu-name binding is established.
std::uint32_t originalSoloStartGridSlot(std::uint32_t raceMode0C062668);
}
