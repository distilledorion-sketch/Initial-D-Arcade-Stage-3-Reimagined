#include "physics.h"
#include <algorithm>
#include <cmath>

namespace idas3 {
namespace {
constexpr float gravity = 9.80665f;
constexpr float fullCircle = 6.2831853071795864769f;
float finiteOr(float value, float fallback = 0) {
    return std::isfinite(value) ? value : fallback;
}
float bounded(float value, float low, float high) {
    return std::clamp(finiteOr(value), low, high);
}
float headingWrap(float a) {
    if (!std::isfinite(a)) return 0;
    // The historical code uses +/-2pi around +/-pi. A loop keeps the public
    // helper well defined for deliberately large replay initial states, too.
    while (a > pi) a -= fullCircle;
    while (a < -pi) a += fullCircle;
    return a;
}
float torqueFraction(float normalizedRpm) {
    // Uncalibrated smooth torque curve. It is deliberately outside the
    // recovered substeps so measured records can replace it independently.
    constexpr std::array<float, 7> values{0.62f, 0.71f, 0.84f, 0.96f, 1.0f, 0.93f, 0.78f};
    const float p = bounded(normalizedRpm, 0, 1) * 6.0f;
    const auto i = std::min(static_cast<std::size_t>(p), values.size() - 2);
    return values[i] + (values[i+1] - values[i]) * (p - static_cast<float>(i));
}
}

std::array<float, 6> signedSteeringPowers(float x) {
    x = bounded(x, -1, 1);
    std::array<float, 6> powers{};
    powers[0] = x;
    float magnitude = std::abs(x);
    for (std::size_t i = 1; i < powers.size(); ++i) {
        magnitude *= magnitude;
        powers[i] = std::copysign(magnitude, x);
    }
    return powers;
}

void updateRecoveredSteering(VehicleState& s, float x) {
    x = bounded(x, -1, 1);
    s.steering = x;
    s.steeringBasis = signedSteeringPowers(x);
    s.steeringDelta = x - s.previousSteering;
    s.previousSteering = x;
    s.steeredHeading = headingWrap(s.yaw + s.steeringBasis[1] * recoveredSteeringLimit);
}

void integrateRecoveredYaw(VehicleState& s, float contribution) {
    s.yawContribution = finiteOr(contribution);
    s.yawFrameVelocity += s.yawContribution;
    s.yawUnwrapped += s.yawFrameVelocity;
    s.yaw = headingWrap(s.yawUnwrapped);
    s.yawRate = s.yawFrameVelocity / physicsDt;
}

void updateRecoveredVelocity(VehicleState& s, float velocity) {
    s.previousSpeed = s.speed;
    s.speed = finiteOr(velocity);
    s.velocityDelta = s.speed - s.previousSpeed;
    s.accelProxy = std::clamp(20.0f * s.velocityDelta, -1.0f, 1.0f);
}

int decodeShiftEdges(bool down, bool up) {
    const unsigned request = (down ? 1u : 0u) | (up ? 2u : 0u);
    return request == 1 ? -1 : request == 2 ? 1 : 0;
}

void reset(VehicleState& s, const VehicleConfig& c, Vec3 p, float angle) {
    s = VehicleState{};
    s.position = p;
    s.yaw = headingWrap(angle);
    s.yawUnwrapped = s.yaw;
    s.rpm = c.idleRpm;
    s.cornerDots.fill(1);
    updateRecoveredSteering(s, 0);
}

void step(VehicleState& s, const DriverInput& input,
          const VehicleConfig& c, const RoadContact& road) {
    // Everything below, except calls to the explicitly recovered helpers,
    // is a continuous experimental four-contact vehicle model. No discrete
    // drift state, speed boost, rubber banding, or wall penalty is assumed.
    constexpr float dt = physicsDt;
    const float mass = std::max(c.mass, 100.0f);
    const float wheelbase = std::max(c.wheelbase, 1.0f);
    const float halfTrack = std::max(c.trackWidth, 0.5f) * 0.5f;
    const float frontWeight = std::clamp(c.frontWeight, 0.25f, 0.75f);
    const float frontArm = wheelbase * (1.0f - frontWeight);
    const float rearArm = wheelbase * frontWeight;
    const float wheelRadius = std::max(c.wheelRadius, 0.1f);
    const int gears = std::clamp(c.gearCount, 1, 6);
    s.gear = std::clamp(s.gear, 1, gears);
    s.throttle = bounded(input.throttle, 0, 1);
    s.brake = bounded(input.brake, 0, 1);
    updateRecoveredSteering(s, input.steer);
    s.wallContact = false;
    s.wallImpactSpeed = 0;

    const float cy = std::cos(s.yaw), sy = std::sin(s.yaw);
    const Vec3 forwardVector{sy, 0, cy};
    const Vec3 rightVector{cy, 0, -sy};
    float u = s.velocity.x * sy + s.velocity.z * cy;
    const float v = s.velocity.x * cy - s.velocity.z * sy;
    u = std::max(0.0f, finiteOr(u));
    const float ratio = std::max(0.1f, c.gearRatios[s.gear-1]) * c.finalDrive;
    const float wheelRpm = u / (fullCircle * wheelRadius) * 60.0f;
    const float coupledRpm = wheelRpm * ratio;
    // The clutch/engine relation and tach units remain unrecovered.
    const float launchRpm = c.idleRpm + s.throttle * 2700.0f;
    const float rpmTarget = std::clamp(std::max(launchRpm, coupledRpm), c.idleRpm, c.redlineRpm * 1.04f);
    s.rpm += (rpmTarget - s.rpm) * (1.0f - std::exp(-18.0f * dt));
    s.throttleRpmGate = s.throttle > 0.1f && s.rpm > 5000.0f;

    const bool downEdge = input.shiftDown && !s.heldShiftDown;
    const bool upEdge = input.shiftUp && !s.heldShiftUp;
    s.heldShiftDown = input.shiftDown;
    s.heldShiftUp = input.shiftUp;
    int shift = decodeShiftEdges(downEdge, upEdge);
    s.shiftSeconds = std::max(0.0f, s.shiftSeconds - dt);
    if (input.automatic && shift == 0 && s.shiftSeconds == 0) {
        if (coupledRpm > c.redlineRpm * 0.94f && s.gear < gears) shift = 1;
        else if (coupledRpm < c.redlineRpm * 0.39f && s.gear > 1) shift = -1;
    }
    if (shift && s.shiftSeconds == 0) {
        const int nextGear = std::clamp(s.gear + shift, 1, gears);
        if (nextGear != s.gear) {
            s.previousGear = s.gear;
            s.gear = nextGear;
            s.shiftSeconds = std::max(c.shiftDuration, dt);
        }
    }

    const float liveRatio = std::max(0.1f, c.gearRatios[s.gear-1]) * c.finalDrive;
    const float revLimiter = coupledRpm >= c.redlineRpm ? 0.0f : 1.0f;
    const float shiftClutch = s.shiftSeconds > 0 ? 0.0f : 1.0f;
    const float driveForce = c.maxTorque * torqueFraction(s.rpm / c.redlineRpm)
        * liveRatio * c.transmissionEfficiency / wheelRadius * s.throttle * shiftClutch * revLimiter;
    const float brakeForce = mass * c.brakeAcceleration * s.brake;
    const float engineBrakeForce = u > 0.2f
        ? mass * c.engineBraking * (1.0f - s.throttle) * std::clamp(s.rpm / c.redlineRpm, 0.0f, 1.0f) : 0;
    const float mu = std::max(0.05f, c.grip * bounded(road.surfaceGrip, 0.05f, 2.0f));
    const float normalTotal = mass * gravity / std::sqrt(1.0f + road.grade * road.grade);
    const float longitudinalTransfer = std::clamp(mass * s.acceleration * c.centreOfMassHeight / wheelbase,
        -normalTotal * 0.22f, normalTotal * 0.22f);
    const float frontNormal = normalTotal * frontWeight - longitudinalTransfer;
    const float rearNormal = normalTotal - frontNormal;
    const float rollTransfer = std::clamp(mass * s.lateralAcceleration * c.centreOfMassHeight / (2.0f * halfTrack),
        -normalTotal * 0.30f, normalTotal * 0.30f);
    const float steeringAngle = s.steering * recoveredSteeringLimit;
    float sumRight = 0, sumForward = 0, yawMoment = 0;
    float maximumSlip = 0;
    for (int corner = 0; corner < 4; ++corner) {
        // This module's ordering is FL, FR, RL, RR. It does NOT claim the
        // corresponding original +2A8/+2B4/+2C0/+2CC ordering is known.
        const bool frontAxle = corner < 2;
        const float side = (corner % 2 == 0) ? -1.0f : 1.0f;
        const float x = side * halfTrack;
        const float z = frontAxle ? frontArm : -rearArm;
        const float axleNormal = frontAxle ? frontNormal : rearNormal;
        const float axleWeight = frontAxle ? frontWeight : 1.0f - frontWeight;
        const float normal = std::max(normalTotal * 0.035f, axleNormal * 0.5f - side * rollTransfer * axleWeight);
        s.cornerLoad[corner] = normal;
        const float wheelSteer = frontAxle ? steeringAngle : 0;
        const float cs = std::cos(wheelSteer), ss = std::sin(wheelSteer);
        const float pointRight = v + s.yawRate * z;
        const float pointForward = u - s.yawRate * x;
        const float tireRight = pointRight * cs - pointForward * ss;
        const float tireForward = pointRight * ss + pointForward * cs;
        const float slipAngle = std::atan2(tireRight, std::max(2.5f, std::abs(tireForward)));
        s.cornerSlip[corner] = slipAngle;
        maximumSlip = std::max(maximumSlip, std::abs(slipAngle));
        // normalize+dot is structurally present in the recovered solver.
        // These particular vector producers are experimental contact velocities.
        const float magnitude = std::hypot(pointRight, pointForward);
        s.cornerDots[corner] = magnitude > 0.0001f ? (pointRight * ss + pointForward * cs) / magnitude : 1.0f;
        const float limit = mu * normal;
        float tireLateral = -limit * std::tanh(c.cornerStiffness * slipAngle / limit);
        float drivenShare = 0;
        if (c.drive == DriveLayout::All) drivenShare = 0.25f;
        else if ((c.drive == DriveLayout::Front) == frontAxle) drivenShare = 0.5f;
        const float brakeShare = (frontAxle ? c.frontBrakeBias : 1.0f - c.frontBrakeBias) * 0.5f;
        float tireLongitudinal = driveForce * drivenShare - brakeForce * brakeShare - engineBrakeForce * drivenShare;
        // Friction circle couples braking/drive and cornering continuously.
        tireLongitudinal = std::clamp(tireLongitudinal, -limit, limit);
        const float lateralCapacity = std::sqrt(std::max(0.0f, limit * limit - tireLongitudinal * tireLongitudinal));
        tireLateral = std::clamp(tireLateral, -lateralCapacity, lateralCapacity);
        s.cornerLateralForce[corner] = tireLateral;
        const float forceRight = tireLateral * cs + tireLongitudinal * ss;
        const float forceForward = tireLongitudinal * cs - tireLateral * ss;
        sumRight += forceRight;
        sumForward += forceForward;
        yawMoment += z * forceRight - x * forceForward;
    }

    const float rolling = u > 0.0f ? mass * c.rollingResistance : 0;
    const float drag = c.airDrag * u * u;
    const float slope = mass * gravity * road.grade / std::sqrt(1.0f + road.grade * road.grade);
    sumForward -= rolling + drag + slope;
    s.acceleration = sumForward / mass;
    s.lateralAcceleration = sumRight / mass;
    const float newU = std::max(0.0f, u + s.acceleration * dt);
    float newV = v + s.lateralAcceleration * dt;
    // Contact forces rotate world velocity. Use world coordinates when
    // integrating so the body's changing heading does not inject momentum.
    s.velocity.x += (sumForward * forwardVector.x + sumRight * rightVector.x) / mass * dt;
    s.velocity.z += (sumForward * forwardVector.z + sumRight * rightVector.z) / mass * dt;
    // Brakes/rolling loss may stop the car but cannot drive it backwards.
    if (newU <= 0) {
        newV *= std::exp(-14.0f * dt);
        s.velocity.x = newV * rightVector.x;
        s.velocity.z = newV * rightVector.z;
    }
    float yawAcceleration = yawMoment / std::max(c.yawInertia, 100.0f);
    // Low-speed stabilization is an exposed implementation limitation, not a
    // discovered original assist. Blend toward no yaw as the car comes to rest.
    if (u < 2.0f) yawAcceleration -= s.yawRate * (2.0f - u) * 6.0f;
    integrateRecoveredYaw(s, yawAcceleration * dt * dt);
    s.position.x += s.velocity.x * dt;
    s.position.z += s.velocity.z * dt;
    s.position.y = road.centre.y;
    s.velocity.y = road.grade * std::hypot(s.velocity.x, s.velocity.z);

    if (road.enforceWalls) {
        const float rh = finiteOr(road.heading);
        const Vec3 roadRight{std::cos(rh), 0, -std::sin(rh)};
        // Using the supplied current offset avoids distant segments in a
        // switchback being selected by an infinite-line approximation.
        const float predictedOffset = road.lateralOffset
            + (s.velocity.x * roadRight.x + s.velocity.z * roadRight.z) * dt;
        const float available = std::max(0.1f, road.halfWidth - c.contactHalfWidth);
        const float penetration = std::abs(predictedOffset) - available;
        if (penetration > 0) {
            const float side = std::copysign(1.0f, predictedOffset);
            const float outwardVelocity = (s.velocity.x * roadRight.x + s.velocity.z * roadRight.z) * side;
            s.position.x -= roadRight.x * side * penetration;
            s.position.z -= roadRight.z * side * penetration;
            s.wallContact = true;
            s.wallImpactSpeed = std::max(0.0f, outwardVelocity);
            if (outwardVelocity > 0) {
                const float impulse = outwardVelocity * (1.0f + c.wallRestitution);
                s.velocity.x -= roadRight.x * side * impulse;
                s.velocity.z -= roadRight.z * side * impulse;
                const Vec3 roadForward{std::sin(rh), 0, std::cos(rh)};
                const float along = s.velocity.x * roadForward.x + s.velocity.z * roadForward.z;
                const float loss = std::min(std::abs(along), outwardVelocity * c.wallTangentialFriction);
                s.velocity.x -= roadForward.x * std::copysign(loss, along);
                s.velocity.z -= roadForward.z * std::copysign(loss, along);
            }
        }
    }
    const float ny = std::sin(s.yaw), nz = std::cos(s.yaw);
    updateRecoveredVelocity(s, std::max(0.0f, s.velocity.x * ny + s.velocity.z * nz));
    s.lateralSpeed = s.velocity.x * nz - s.velocity.z * ny;
    s.slip = maximumSlip;
    s.steeredHeading = headingWrap(s.yaw + s.steeringBasis[1] * recoveredSteeringLimit);
    s.travel += std::hypot(s.velocity.x, s.velocity.z) * dt;
    s.simulatedSeconds += dt;
    ++s.tick;
}

} // namespace idas3
