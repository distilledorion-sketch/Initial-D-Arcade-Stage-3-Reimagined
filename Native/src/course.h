#pragma once
#include "math_types.h"
#include <cstddef>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

namespace idas3 {
struct CourseSample {
    Vec3 center{}, left{}, right{}, tangent{0,0,1};
    float distance=0, width=0, grade=0, curvature=0;
    std::size_t segmentIndex=0;
};
struct CourseProjection {
    CourseSample sample{};
    float lateral=0, squaredDistance=0;
    std::size_t segment=0;
};
struct Course {
    std::string id, name, sourceRoot, provenanceSha256;
    // Original positions are retained, without resampling or a unit conversion.
    // Left/right are ordered geometrically for right=(tangent.z,0,-tangent.x).
    std::vector<Vec3> points, left, right;
    std::vector<float> cumulative;
    float length=0;
    bool closed=false, reversed=false;
    bool sourceEdgesSwapped=false;

    // pathRoot accepts a folder containing the three files, HOSTFS, or driveA.
    // Files: <id>_path.bin, <id>_path_l.bin, <id>_path_r.bin.
    static Course load(const std::filesystem::path& pathRoot, const std::string& id,
                       const std::string& name="", bool reverse=false);
    // Distances clamp to the authored endpoints, including on closed courses.
    CourseSample sample(float distance) const;
    // A hint avoids jumping between neighboring hairpins. A globally distant
    // hint (>40 units from the road) triggers a full reacquisition except near
    // a closed lap's endpoints, where progress stays on that lap. A new lap or
    // a teleport across the seam must explicitly reset or omit the hint.
    CourseProjection project(Vec3 position,
        std::size_t hint=std::numeric_limits<std::size_t>::max(),
        std::size_t radius=100) const;
};
}
