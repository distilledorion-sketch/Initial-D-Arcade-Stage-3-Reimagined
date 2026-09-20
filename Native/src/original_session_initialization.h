#pragma once
#include "original_initialization.h"
#include "original_road_contact.h"

namespace idas3::original {

// The actual1595C0 call to222348 uses r6=17: its alternating unrolled
// entry copies37 words,148 bytes. These original pointer words are metadata
// identities, never pointers dereferenced by the native host.
struct OriginalSessionDefaults {
    std::array<std::uint32_t,37> words0C270B08{};
};
OriginalSessionDefaults verifiedOriginalSessionDefaults();

struct OriginalSessionInitializationState {
    std::uint32_t activeActorIdentity0C900954=0;
    std::int32_t elapsedFrames0C900E84=0;
    std::uint32_t steeringMask0C900EBC=0;
    std::uint32_t previousFlag0CAA94C4=0,state0C31FD44=0;
    // First16 copied words are vehicle.tail.statistics0C91FB0C; word16 is
    // vehicle.transmissionGlobals.flag91fb4c. Preserve the remaining20 too.
    std::array<std::uint32_t,20> statisticsRemainder0C91FB50{};
};

// Full1595C0 semantics, including15EE00 and all eight022940 query resets.
// Only writes the fields written by the original function. Side actor fields
// are synchronized from/to the authoritative actor record at this boundary.
// No blanket zero of actor, drive, collision scratch or output histories.
OriginalInitializationResult initializeOriginalSession(OriginalVehicleState& vehicle,
    OriginalVehicleParameters& parameters,OriginalInitializationSideState& initializationSide,
    OriginalActorState& actor,OriginalRoadContactState& roadContact,
    OriginalSessionInitializationState& session,const OriginalInitializationInputs& inputs,
    const OriginalSessionDefaults& defaults);

} // namespace idas3::original
