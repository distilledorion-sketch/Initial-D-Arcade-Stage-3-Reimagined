#pragma once
#include "original_battle_profile.h"

namespace idas3::original {
// Native representation of ACar+2D4 and its constructor-owned variant map.
// Geometry selection and material rebuilding are separate consumers.
struct OriginalCarAppearanceConfig {
    std::uint32_t car=0,word=0,materialVariant=0;
    std::array<std::uint32_t,6> variants{0,1,2,3,4,5};
    bool paintDirty=false;
    explicit OriginalCarAppearanceConfig(std::uint32_t carId=0);
};
// The original result owner already records these ACar calls. This handles
// 0283C0..0287A0 setters; 029040 rebuild belongs to the assembly consumer.
void applyOriginalCarAppearanceCall(OriginalCarAppearanceConfig&,std::uint32_t address,std::uint32_t argument5,std::uint32_t argument6=0);
//0630B4..06316E: saved player parts, factory color, scene material variant,
// and both bits of profile166. Profile164 affects physics, not this word.
OriginalCarAppearanceConfig originalPlayerAppearanceConfig(const OriginalBattleProfile&,std::uint32_t materialVariant=0);
}
