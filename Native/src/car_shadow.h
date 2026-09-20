#pragma once
#include "math_types.h"
#include <array>

namespace idas3 {
class Mesh;
// Presentation-only grounding shadow. Supply actual WORLD road positions,
// not suspended wheel centers or actor-space points. This adds no physics.
struct CarShadowFootprint {
    // Front-left, front-right, rear-left, rear-right. Either side convention
    // works if both axles agree; front/rear should follow the car's pose.
    std::array<Vec3,4> roadPoints{};
    float widthScale=1.12f;
    float lengthScale=1.55f;
    float opacity=.34f;
    float surfaceLift=.012f;
    // Extra airborne separation, excluding the normal body ride height.
    // Opacity fades smoothly to zero by1.5 road units.
    float separation=0;
};
// Appends one untextured, soft-alpha translucent range with depth testing and
// no depth writes. Returns false for invisible or invalid footprints.
// Existing renderer material metadata is used for state selection; the shape
// and opacity are native presentation choices, not recovered original assets.
bool appendCarContactShadow(Mesh& mesh,const CarShadowFootprint& footprint);
} // namespace idas3
