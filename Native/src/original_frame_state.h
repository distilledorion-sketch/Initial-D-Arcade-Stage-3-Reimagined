#pragma once
#include "original_contact.h"

namespace idas3::original {
struct OriginalPublishedActors {
    std::array<std::uint32_t,42> player0C8FF388{},secondary0C8FF430{};
};
struct OriginalRecoveryState {
    std::array<std::uint32_t,42> actor0C8FF580{};
    OriginalDriveState drive0C9009F0;
};
// Original222372(count20) copies42 words; count135 copies272 words. These
// counts are verified against actual instructions, not interpreted as bytes.
void publishOriginalActors(const OriginalActorState& player,const OriginalActorState& secondary,
    std::uint32_t mode0C9015E4,OriginalPublishedActors& published);
// Original159920 tail: save valid four-wheel state or restore the prior valid
// state when >2 current wheel upper nibbles are zero. Returns true on restore.
bool applyOriginalRecovery(OriginalDriveState& drive,OriginalActorState& actor,
    OriginalPublishedActors& published,OriginalRecoveryState& recovery);

struct OriginalBodyCollisionResult {
    std::uint32_t active=0; // original pair solver result+40
    float x=0,z=0;        // original result+76/+84
};
struct OriginalBodyCollisionEffects {
    std::uint32_t cue=0,invalidScalarDiagnostics=0;
};
// Original1578C4..157A06 after the separate car-pair query. A single-car
// session supplies an inactive pair result; this still runs original decay,
// clamping and flag writes. Rival-car collision detection is a separate stage.
OriginalBodyCollisionEffects applyOriginalBodyCollisionResponse(OriginalDriveState& drive,
    const OriginalBodyCollisionResult& collision,std::uint32_t& latch0C31FD44,
    std::uint32_t& randomSeed0C37C778);
}
