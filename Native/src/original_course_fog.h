#pragma once
#include <array>
#include <cstdint>

namespace idas3::original {
struct OriginalFogRegisterWrite {
    std::uint32_t offset{},value{};
    bool operator==(const OriginalFogRegisterWrite&) const=default;
};
struct OriginalCourseFog {
    std::uint32_t sourceRow{},sourceAddress{};
    float density{},maximum{};
    std::uint32_t colorRgb{};
    std::uint16_t packedDensity{};
    // Original1AD254..26C float samples; PVR table entries contain adjacent
    // quantized values, high byte=current, low byte=next. Last repeats itself.
    std::array<float,128> samples{};
    std::array<std::uint16_t,128> table{};
};
//042700 publishes course=(sceneIndex/2), original time and weather separately.
// Daytime rain's night-model constructor does not change the time selection.
// Direction does not select a fog row. Source roster is0..8 in game order.
OriginalCourseFog originalCourseFog(unsigned course,bool night,bool wet);
// Original1CEB40 graphics reset: full-range linear table, density100000,
// white table fog. Retain this state when Happo writes only B8 and B0.
OriginalCourseFog originalBootstrapFog();
// Actual Happo040D60/03C5E0 writes density/color only. Preserve the existing
// table and samples; source initial/global table history is separate.
void applyHappoFogRegisters(OriginalCourseFog&,bool night,bool wet);
// Source1ACC40 order: density B8, table offsets3FC..200, table color B0.
std::array<OriginalFogRegisterWrite,130> originalCourseFogWrites(const OriginalCourseFog&);
// Exact1FACC0 float-bit packing, for finite positive source density values.
std::uint16_t originalFogPackedDensity(float);
// PVR B8: unsigned 8-bit mantissa /128, signed 8-bit base-two exponent.
// Decode the written register, not the unquantized authoring density.
float originalFogDecodedDensity(std::uint16_t packedDensity);
// Table lookup at reciprocal positive camera depth in original scene units.
// This is clip.w's reciprocal, not the projection's normalized Z or radial
// eye distance. Positive infinity reaches the near end; zero reaches far.
// Negative/NaN inputs are rejected because they are not visible scene depth.
float originalFogTableCoefficient(const OriginalCourseFog&,float reciprocalDepth);
} // namespace idas3::original
