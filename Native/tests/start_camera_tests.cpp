#include "original_start_camera.h"
#include "original_start_grid.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace idas3::original;

namespace {
float distance(const std::array<float, 3>& a, const std::array<float, 3>& b) {
    const float x = a[0] - b[0], y = a[1] - b[1], z = a[2] - b[2];
    return std::sqrt(x * x + y * y + z * z);
}
}

int main() try {
    unsigned cases = 0;

    // Every shot is the same kind of node: a plain camera, no smoothing between
    // shots and no shake. If a future table breaks that, the consumer has to
    // learn those fields before it can be trusted.
    for (unsigned condition = 0; condition < 18; ++condition)
        for (unsigned shot = 0; shot < 2; ++shot) {
            const auto camera = originalStartCamera(condition, shot);
            if (camera.kind != 5) throw std::runtime_error("A start camera is not kind 5");
            if (camera.smoothJoint || camera.isShake)
                throw std::runtime_error("A start camera asks for smoothing or shake");
            if (!(camera.angleRadians > .7f && camera.angleRadians < 1.3f))
                throw std::runtime_error("A start camera field of view is outside the authored range");
            if (!(camera.wait > 0)) throw std::runtime_error("A start camera shot never holds");
            if (!(camera.objectSize > 0)) throw std::runtime_error("A start camera has no subject size");
        }
    ++cases;

    // The point of the table: these are the shots of the two cars waiting on
    // the line, so a condition's cameras belong to that condition's own start
    // line. Only the first shot is checked that way. The second stands 160 to
    // 800 units off, and what it frames from there is not established: every
    // node carries an offset and an objectSize and is kind 5, which reads like
    // a shot that tracks a subject rather than one aimed by its own yaw, and
    // that behaviour has not been recovered. Checking its stance against the
    // grid would reject authored data, and checking its yaw would assert a
    // convention nobody has confirmed, so it is left to whoever recovers
    // kind 5. Condition 12 repeats its close shot in both slots.
    float worst = 0;
    unsigned worstCondition = 0;
    for (unsigned condition = 0; condition < 18; ++condition) {
        const auto grid = originalStartPose(condition, 0).position;
        const auto reach = distance(originalStartCamera(condition, 0).position, grid);
        if (reach > worst) { worst = reach; worstCondition = condition; }
    }
    if (worst > 200)
        throw std::runtime_error("A close start camera stands too far from its own start line");
    ++cases;

    // Akina Snow drives Akina's road, and the source gives conditions 16/17 the
    // same cameras as 6/7 rather than a second copy of the data.
    for (unsigned shot = 0; shot < 2; ++shot) {
        const auto akina = originalStartCamera(6, shot), snow = originalStartCamera(16, shot);
        if (distance(akina.position, snow.position) != 0 || akina.objectSize != snow.objectSize)
            throw std::runtime_error("Akina Snow does not reuse Akina's start camera");
    }
    ++cases;

    // Out of range is refused rather than clamped.
    for (const auto bad : {std::pair<unsigned, unsigned>{18, 0}, {0, 2}}) {
        bool refused = false;
        try { originalStartCamera(bad.first, bad.second); }
        catch (const std::invalid_argument&) { refused = true; }
        if (!refused) throw std::runtime_error("An out-of-range start camera was accepted");
    }
    ++cases;

    std::cout << "PASS " << cases << " start camera cases: node shape, "
              << "close shots within " << worst << " units of their own start line (worst is "
              << "condition " << worstCondition << "), Akina Snow reuses Akina, and range checks. "
              << "The wide shot's framing is not covered; kind 5 is unrecovered.\n";
} catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
}
