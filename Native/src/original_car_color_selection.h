#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace idas3::original {
struct OriginalCarColorSelection {
    std::uint32_t selected480{},previous484{},currentColor684{},profileColor64{};
    std::vector<std::uint32_t> rememberedColors676,colorCounts680;
};
struct OriginalCarColorSelectionInput {
    std::uint32_t selectedLocalIndex{};
    std::uint8_t gearButtons92ED40{}; //0x20 increments; otherwise0x10 decrements.
    bool confirmOrTimeout{};
};
struct OriginalCarColorSelectionEvents {
    bool selectionChanged{},showroomColorApplyRequested{},profileColorWritten{};
    std::int32_t gearDelta{};
};
//12E142..12E1CE. Counts must follow the original maker roster. The selected
// current color copies profile64, but ALL remembered entries start at0.
void initializeOriginalCarColorSelection(OriginalCarColorSelection&,
    std::span<const std::uint32_t> counts,std::uint32_t selectedLocalIndex,
    std::uint32_t profileColor64);
// Call only on the owner's active phase1 update, before acting on its
// transition events. This reproduces source single-step wrap, not modulo.
OriginalCarColorSelectionEvents stepOriginalCarColorSelection(
    OriginalCarColorSelection&,const OriginalCarColorSelectionInput&);

struct OriginalCarColorIndicatorDraw {
    std::uint32_t selector{}; //bank13=v3sS05cars;37background,38/39swatch,40marker.
    float x{},y{},z{};
    bool replaceColors{};
    std::uint32_t argb{}; //1B8540 mode2: replace ONLY batch.material[3] (GMP+12).
};
//1B4660 command arithmetic. RGB inputs are source1911A0 values in original
//color order. Geometry, material replacement and UI camera remain caller-owned.
std::vector<OriginalCarColorIndicatorDraw> originalCarColorIndicatorDraws(
    std::uint32_t globalCarId,std::uint32_t selectedColor,
    std::span<const std::uint32_t> originalRgb);
}
