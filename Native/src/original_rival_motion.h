#pragma once
#include "original_rival.h"
#include "original_collision.h"
#include "original_matrix.h"

namespace idas3::original {
// Exact15B7A8..15BE50 following updateOriginalRivalPace. Profile-specific
// lookahead, yaw filters, drift presentation, XYZ advancement and wheel spin.
// Caller must skip this stage when the pace stage reports an inactive actor.
void advanceOriginalRivalMotion(OriginalRivalState& rival,OriginalActorState& publicActor,
    const OriginalRivalPath& path,const OriginalRivalPaceInputs& inputs,
    std::uint32_t previousSurfaceValid0CAA9864);

struct OriginalRivalRoadState {
    std::array<OriginalCollisionQuery,4> surfaces0CAA9764{};
    std::uint32_t surfaceValid0CAA9864=0;
    OriginalTriangleSearchTrace trace;
    OriginalSurfaceScratch surface;
};
// Original15CDE0: retain current/previous query points and return exactly
// {normalX, queryY-height, normalZ}; publishes the low material byte.
std::array<float,3> queryOriginalRivalWheel(OriginalActorState& publicActor,
    unsigned corner,const std::array<float,3>& point,OriginalRivalRoadState& road,
    const OriginalCollisionData& collision);
//15BE50..15CDA4: original transformed footprint, four surface queries,
// suspension, published pose and finite-scalar guard. The external enclosing
// matrix stack is preserved by using a local native matrix.
void finishOriginalRivalRoadContact(OriginalRivalState& rival,OriginalActorState& publicActor,
    std::uint32_t carIndex0C9015F8,const OriginalRivalData& data,
    OriginalRivalRoadState& road,const OriginalCollisionData& collision,
    const OriginalFscaTable& fsca);
bool updateOriginalRival(OriginalRivalState& rival,OriginalActorState& publicActor,
    std::uint32_t& frameCounter0CAA986C,const OriginalRivalData& data,
    const OriginalRivalPath& path,const OriginalRivalPaceInputs& inputs,
    const OriginalDriveState& playerDrive,const OriginalActorState& playerActor,
    std::uint32_t carIndex0C9015F8,OriginalRivalRoadState& road,
    const OriginalCollisionData& collision,const OriginalFscaTable& fsca);
} // namespace idas3::original
