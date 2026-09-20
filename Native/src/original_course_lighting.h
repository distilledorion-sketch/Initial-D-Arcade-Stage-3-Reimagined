#pragma once
#include <array>
#include <cstdint>

namespace idas3::original {
using OriginalLightVector = std::array<float,3>;
// SH4 XF/FTRV order: element[column*4+row], including translation at12..14.
using OriginalLightMatrix = std::array<float,16>;
inline constexpr OriginalLightMatrix originalLightIdentityMatrix{
    1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
enum class OriginalCourseLightKind : unsigned { Parallel, RelativeParallel, Spot, Point };
struct OriginalCourseLight {
    OriginalCourseLightKind kind{};
    unsigned sourceSlot{}; // Index within the authored group, not compacted ELAN ID.
    bool enabled{true};
    OriginalLightVector position{},incomingDirection{},color{};
    float distance0{},distance1{};
    std::uint16_t angle0{},angle1{};
    // Identity-transform source output retained for diagnostics. Distance
    // coefficients are static; angular coefficients are rebuilt after the
    // source's quantized direction-length correction on each submission.
    std::array<std::uint32_t,2> coefficientWords{};
    std::array<float,2> angleCosines{}; // Actual source FSCA outputs.
};
struct OriginalCourseLighting {
    unsigned sourceRow{},sourceAddress{},count{};
    OriginalLightVector ambient{};
    float gain{1.f}; // ARRAY+88 multiplies light RGB, not ambient.
    // Registration19AFA0: relative, authored parallel, then spots.
    std::array<OriginalCourseLight,16> lights{};
};
struct OriginalCourseLightingPacket {
    std::array<std::uint32_t,8> glm{};
    unsigned count{};
    std::array<std::array<std::uint32_t,8>,16> lights{};
};
// Same original course/time/weather selection as fog; no direction remapping.
// This returns constructor/reset state. Course-specific positional updates
// and enabling/disabling must be applied before submitting an animated scene.
OriginalCourseLighting originalCourseLighting(unsigned course,bool night,bool wet);
// Explicit generic19ACC0/19AFA0 table state, including unused Happo rows.
OriginalCourseLighting originalCourseLightingTableRow(unsigned course,bool night,bool wet);
// Actual Happo041320: wet relative direction/color update, then night-only
// nearest-two selection. Reference position is the source path query result.
void updateOriginalHappoLighting(OriginalCourseLighting&,const OriginalLightMatrix&,const OriginalLightVector& referencePosition);
struct OriginalIrohazakaLightUpdate {
    std::array<unsigned,4> authoredIndices{};
    bool coordinatesUpdated{},sourceEnableIndexOutsideFourSlots{};
};
// Source1A5580 position selection/copy. Out-of-four-slot enable writes are
// reported and omitted; they never become unchecked native memory accesses.
// Missing authored spot slots also fail closed with coordinatesUpdated=false.
OriginalIrohazakaLightUpdate updateOriginalIrohazakaLighting(OriginalCourseLighting&,const OriginalLightMatrix&,const OriginalLightVector& referencePosition);
// Actual course virtual+80 dispatcher. Irohazaka diagnostics report omitted
// unregistered/unsafe source writes; only registered lights are represented.
OriginalIrohazakaLightUpdate updateOriginalCourseLighting(OriginalCourseLighting&,const OriginalLightMatrix&,const OriginalLightVector& referencePosition);
// Exact1AD580 direction update. The argument is the matrix referenced by
// course+80: prior published player ACar+2404 visual world matrix, including
// its source half-turn. No normalization occurs.
void updateOriginalCourseRelativeDirections(OriginalCourseLighting&,const OriginalLightMatrix&);
// Source0538A0→1CF0A0→1D3860/1D3D00. Matrix must be the current source view
// transform used for course geometry. Implements the12-bit direction branch
// (ELAN revision!=1, including the primary reference's revision0x10).
// Use for the course's own light scope, never globally for player/rival cars.
OriginalCourseLightingPacket originalCourseLightingPacket(const OriginalCourseLighting&,const OriginalLightMatrix&);
// Quantizer205680/205DA0: source input*65535,FTRC, then signed12 extraction.
// Kept public for independent boundary tests and renderer packet inspection.
std::int16_t originalLightDirection12(float);
} // namespace idas3::original
