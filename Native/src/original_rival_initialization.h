#pragma once
#include "original_rival.h"
#include "original_collision.h"

namespace idas3::original {
struct OriginalRivalInitializationInputs {
    std::array<float,3> position{},angles{};
    std::uint32_t condition0C9015CC{},enemyId0C9015E0{},profileMode0C901648{},level0C9015D0{};
    std::int32_t actorSlot=1,field8=1;
};
struct OriginalRivalInitializationResult {
    std::uint32_t enemyId0C9015E0{},profile0CAA9868{},level0C9015D0{},frame0CAA986C{};
    unsigned actorSlot{},carIndex{};
    bool alternatePath{};
};
unsigned originalRivalCarIndex(unsigned profile);
// Exact15AE00 effects on the caller-selected716-byte actor,168-byte public
// actor and four query records. Unwritten state and surface-valid are retained.
// Caller binds the returned slot/path/global selections to native ownership.
OriginalRivalInitializationResult initializeOriginalRival(OriginalRivalState& rival,
    OriginalActorState& publicActor,std::array<OriginalCollisionQuery,4>& queries,
    const OriginalRivalInitializationInputs& inputs);
}
