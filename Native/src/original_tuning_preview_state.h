#pragma once
#include "original_tuning.h"
#include "original_matrix.h"
#include <optional>
namespace idas3::original {
struct OriginalTuningPreviewState {
    std::uint32_t angle=8192,focus=11;
    std::uint64_t frames=0;
};
// 0717C0. nullopt preserves the existing focus for the performance owner.
std::optional<unsigned> originalTuningPreviewFocus(const OriginalBattleProfile&,
    const OriginalTuningData&,OriginalTuningChildKind);
// 078680 runs after this frame's 078720 draw, once per original60Hz owner tick.
void advanceOriginalTuningPreview(OriginalTuningPreviewState&);
//0365C0 reads five32-bit name codes, limited by saved profile word76.
std::array<std::uint8_t,5> originalTuningPreviewPlateDigits(const OriginalBattleProfile&);
//077EEE ->1F6CF0, under the existing finite FIPR/FSRRA reference model.
std::array<float,3> originalTuningPreviewIncomingLight();
struct OriginalTuningPreviewScene {
    OriginalMatrix background,shadow,car,reflection;
};
// Parent scene0=result070E4A,1=post-race ranking07F3D4,2=Continue081530.
// Each parent supplies its own matrix to shared preview draw078720.
OriginalTuningPreviewScene originalTuningPreviewScene(unsigned car,std::uint32_t angle,const OriginalFscaTable&,unsigned scene=0);
}
