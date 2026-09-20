#pragma once
#include "math_types.h"
#include "original_collision.h"

namespace idas3 {
// One persistent instance per rendered ACar, corresponding to its query at+AA0.
class OriginalCarBodyPosition {
public:
    OriginalCarBodyPosition(){reset();}
    void reset();
    Vec3 update(const original::OriginalCollisionData& collision,unsigned carId,Vec3 actorPosition);
    const original::OriginalCollisionQuery& query()const{return query_;}
    bool surfaceFound()const{return surfaceFound_;}
private:
    original::OriginalCollisionQuery query_{};
    original::OriginalTriangleSearchTrace trace_{};
    original::OriginalSurfaceScratch surface_{};
    bool surfaceFound_=false;
};
}
