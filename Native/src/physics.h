#pragma once
#include "math_types.h"
#include <array>
#include <cstdint>

namespace idas3 {

// Original guest simulation runs at 60 Hz. Presentation must never change this.
inline constexpr float physicsDt = 1.0f / 60.0f;
inline constexpr float recoveredSteeringLimit = 0.523598790f;

struct DriverInput {
    float steer = 0;       // -1 left, +1 right in the new renderer's coordinates
    float throttle = 0;
    float brake = 0;
    bool shiftUp = false;  // held host state; the solver derives rising edges
    bool shiftDown = false;
    bool automatic = true;
};

enum class DriveLayout { Rear, Front, All };

// These are exposed ENGINEERING DEFAULTS, not extracted Sega car parameters.
// The archive identifies an 88-byte transmission record, but has not decoded
// its fields. This model does not invent a correspondence to that record.
struct VehicleConfig {
    const char* name = "Research FR";
    DriveLayout drive = DriveLayout::Rear;
    float mass = 1100.0f;
    float wheelbase = 2.4f;
    float trackWidth = 1.45f;
    float frontWeight = 0.53f;
    float centreOfMassHeight = 0.46f;
    float yawInertia = 1550.0f;
    float wheelRadius = 0.30f;
    float maxTorque = 205.0f;
    float finalDrive = 4.30f;
    float transmissionEfficiency = 0.90f;
    int gearCount = 5;
    std::array<float, 6> gearRatios{3.25f, 2.12f, 1.52f, 1.17f, 0.92f, 0.76f};
    float idleRpm = 1100.0f;
    float redlineRpm = 8200.0f;
    float shiftDuration = 0.13f;
    float grip = 1.55f;
    float cornerStiffness = 28500.0f; // N/radian, per corner
    float brakeAcceleration = 13.0f;
    float frontBrakeBias = 0.64f;
    float rollingResistance = 0.22f; // m/s^2
    float airDrag = 0.36f;          // force coefficient N/(m/s)^2
    float engineBraking = 0.65f;    // m/s^2 at high rpm
    float contactHalfWidth = 0.78f;
    float wallRestitution = 0.08f;
    float wallTangentialFriction = 0.23f;
};

// Geometry is sampled by the caller. The imported course currently preserves
// native coordinates 1:1; interpreting one native unit as one metre is unverified.
struct RoadContact {
    Vec3 centre{};
    float heading = 0;
    float curvature = 0;
    float grade = 0;         // rise / horizontal run; positive uphill
    float halfWidth = 5.5f;
    float surfaceGrip = 1.0f;
    float lateralOffset = 0; // +right of road centre at the start of the step
    bool enforceWalls = true;
};

struct VehicleState {
    Vec3 position{};
    Vec3 velocity{};
    float yaw = 0;
    float yawRate = 0;       // radians/second, convenient host-facing form
    float speed = 0;         // forward body velocity; m/s convention
    float lateralSpeed = 0;
    float rpm = 1100;
    int gear = 1;
    int previousGear = 1;
    float steering = 0;
    float throttle = 0;
    float brake = 0;
    std::array<float, 6> steeringBasis{}; // recovered +1CC,+1D0,...,+1E0
    float steeringDelta = 0;            // recovered +1E8
    float previousSteering = 0;         // recovered +1EC
    float steeredHeading = 0;
    float previousSpeed = 0;            // recovered +23C
    float velocityDelta = 0;            // recovered +234
    float accelProxy = 0;               // recovered +24C
    float yawContribution = 0;          // per-frame contribution, analogous +D8
    float yawFrameVelocity = 0;         // per-frame accumulator, analogous +DC
    float yawUnwrapped = 0;             // analogous +110
    std::array<float, 4> cornerDots{};
    std::array<float, 4> cornerSlip{};
    std::array<float, 4> cornerLoad{};
    std::array<float, 4> cornerLateralForce{};
    float slip = 0;
    float acceleration = 0;
    float lateralAcceleration = 0;
    float shiftSeconds = 0;
    float travel = 0;
    float simulatedSeconds = 0;
    bool throttleRpmGate = false; // observed throttle >0.1 and tach-like >5000
    bool wallContact = false;
    float wallImpactSpeed = 0;
    std::uint64_t tick = 0;
    bool heldShiftUp = false;
    bool heldShiftDown = false;
    float speedKmh() const { return speed * 3.6f; }
};

// Independently recoverable substeps. These preserve the known arithmetic
// structure; host sin/cos and surrounding dynamics do not claim bit parity.
std::array<float, 6> signedSteeringPowers(float x);
void updateRecoveredSteering(VehicleState& state, float workingSteering);
void integrateRecoveredYaw(VehicleState& state, float perFrameContribution);
void updateRecoveredVelocity(VehicleState& state, float velocity);
int decodeShiftEdges(bool downEdge, bool upEdge); // simultaneous press => no shift

void reset(VehicleState& state, const VehicleConfig& config,
           Vec3 position = {}, float yaw = 0);
void step(VehicleState& state, const DriverInput& input,
          const VehicleConfig& config, const RoadContact& road);

} // namespace idas3
