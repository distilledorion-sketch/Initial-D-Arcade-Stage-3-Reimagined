#pragma once
#include "original_collision.h"
#include "original_contact.h"
#include "original_matrix.h"
#include <functional>

namespace idas3::original {
using OriginalContactPoint = std::array<float,3>;
struct OriginalImpactRecord {
    OriginalContactPoint position{};
    std::uint32_t tick=0;
    float magnitude=0;
};
struct OriginalRoadContactState {
    std::array<OriginalCollisionQuery,4> surfaces0CAA9518{};
    std::array<OriginalCollisionQuery,4> sweeps0CAA9618{};
    std::array<OriginalContactPoint,4> normals0CAA94C8{};
    std::array<std::uint32_t,4> flags0CAA94F8{};
    std::array<float,4> impacts0CAA9508{};
    float impact0C900E5C=0,impact0C900E60=0;
    std::uint32_t tick0C92DE30=0;
    // Typed outputs replace the three original append-only guest pointers in
    // 0C91FB0C. The original signed counter <=99 still gates record emission.
    std::vector<OriginalImpactRecord> impactRecords;
    std::vector<std::uint32_t> feedback142460;
    std::uint32_t invalidScalarDiagnostics=0;
};
struct OriginalRoadContactParameters {
    // Entire 44-byte record at 0C2700F4 + originalVehicleId*44.
    std::array<float,11> geometry0C2700F4{};
};
struct OriginalRoadContactServices {
    // Required original actor matrix boundary: identity, translation, Y/X/Z
    // rotations, then 1F6260. No host transform is silently substituted.
    std::function<OriginalContactPoint(const OriginalActorState&,
        const OriginalContactPoint&)> transformPoint;
    std::function<void(OriginalCollisionQuery&)> surface022CE0;
    std::function<void(OriginalCollisionQuery&)> swept022D20;
};
// Binds the actual native collision routines; state/scratch lifetimes must
// outlive the returned services. Caller supplies the original matrix helper.
OriginalRoadContactServices bindOriginalRoadContactQueries(
    const OriginalCollisionData& data,OriginalTriangleSearchTrace& trace,
    OriginalSurfaceScratch& scratch,
    std::function<OriginalContactPoint(const OriginalActorState&,
        const OriginalContactPoint&)> transformPoint);
// Fully original numerical services, including FSCA/FTRV actor transforms.
// The immutable FSCA table must also outlive the returned services.
OriginalRoadContactServices bindOriginalRoadContactServices(
    const OriginalCollisionData& data,OriginalTriangleSearchTrace& trace,
    OriginalSurfaceScratch& scratch,const OriginalFscaTable& fsca);

// Original 157EE0 / 158000. Corner index is 0..3. Query return booleans are
// intentionally ignored, as in the original callers; cached output survives.
OriginalContactPoint queryOriginalWheelSurface(OriginalDriveState& drive,
    OriginalRoadContactState& state,std::size_t corner,
    const OriginalContactPoint& current,const OriginalRoadContactServices& services);
void queryOriginalWheelSweep(OriginalDriveState& drive,OriginalRoadContactState& state,
    std::size_t corner,const OriginalContactPoint& current,
    const OriginalContactPoint& previous,const OriginalRoadContactServices& services);
// Original 1594A0 weighted contact normal / decay tail.
void aggregateOriginalWallImpacts(OriginalDriveState& drive,const OriginalRoadContactState& state);
// Complete ordinary C++ body 158200 through 1593C0/1594A0. Executes once per
// original frame after prepareOriginalContactFrame, not per renderer substep.
void updateOriginalRoadContact(OriginalDriveState& drive,OriginalActorState& actor,
    OriginalRoadContactState& state,const OriginalRoadContactParameters& parameters,
    const OriginalRoadContactServices& services);
} // namespace idas3::original
