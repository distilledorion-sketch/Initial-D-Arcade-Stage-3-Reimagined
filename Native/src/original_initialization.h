#pragma once
#include "original_vehicle.h"

namespace idas3::original {

// The remaining globals belong to the enclosing contact/platform pipeline.
// Address labels identify original data fields, not a guest memory interface.
inline constexpr std::array<std::uint32_t,12> initializationContactMultiplierAddresses{
    0x0C90088C,0x0C900E7C,0x0C900950,0x0C90089C,0x0C90096C,0x0C900E3C,
    0x0C900ECC,0x0C900898,0x0C900E70,0x0C9008A0,0x0C900E38,0x0C900E78};
inline constexpr std::array<std::uint32_t,18> initializationOtherGlobalAddresses{
    0x0C900EFC,0x0CAA98A4,0x0CAA98B4,0x0CAA98B8,0x0CAA98BC,0x0CAA98C0,
    0x0CAA98C4,0x0CAA98C8,0x0CAA9890,0x0CAA98CC,0x0CAA98D4,0x0CAA98D8,
    0x0CAA98DC,0x0C8FF37C,0x0CAA9CF8,0x0CAA9CF4,0x0CAA987C,0x0C91FB40};

struct OriginalInitializationSideState {
    std::array<float,3> actorPosition{}; // *[900954]+00..08
    std::uint32_t actorFlags50=0,randomSeed0C37C778=0;
    std::array<float,12> contactMultipliers{};
    std::array<std::uint32_t,18> otherGlobals{};
};
struct OriginalInitializationInputs {
    std::array<float,3> position{},angles{};
    std::uint32_t vehicleIndex0C901654=0;
    std::uint32_t throttleHistoryCount0C285098=0;
    std::uint32_t vehicleType0C284EF4=0,modeMask0C283E08=0;
    std::uint32_t mode0C9015FC=0,mode0C9015C0=0;
};
struct OriginalInitializationResult {
    // Original15A380 resets the separate platform digital-input state.
    // Exposed for the platform adapter; it has no hidden host implementation.
    bool resetPlatformDigitalInput=true;
};

// Complete native drive/global semantics of15EE00, including1F9E60 RNG.
// Preserves fields not written by that function, including unused ring slots.
// Selected table words and input pose must come from the original data/caller.
OriginalInitializationResult initializeOriginalVehicle(OriginalVehicleState& state,
    OriginalVehicleParameters& parameters,OriginalInitializationSideState& side,
    const OriginalInitializationInputs& inputs);

} // namespace idas3::original
