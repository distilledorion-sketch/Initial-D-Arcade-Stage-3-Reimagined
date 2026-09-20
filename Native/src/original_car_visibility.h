#pragma once
#include "original_car_appearance_config.h"

namespace idas3::original {
struct OriginalCarVisibility {
    //029040 resets ACar+358..6A4 to identity, then remaps selected slots.
    // Both1000 andFFFFFFFF are source sentinels; preserve their distinction.
    std::array<std::uint32_t,212> slots{};
    std::uint32_t maximumPopupPhase=0;
    bool popupMotorEnabled=false; // ACar+6C4, cleared by the FD front variant2.
};
OriginalCarVisibility originalCarVisibility(const OriginalCarAppearanceConfig&,std::int32_t enemyId=-1);
}
