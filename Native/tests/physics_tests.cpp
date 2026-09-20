#include "physics.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace idas3;
namespace {
int checks = 0;
void check(bool value, const char* message) {
    ++checks;
    if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
bool near(float a, float b, float epsilon = 1e-5f) { return std::abs(a-b) < epsilon; }
RoadContact openRoad() { RoadContact r; r.enforceWalls = false; return r; }
void run(VehicleState& s, int ticks, DriverInput input, const VehicleConfig& c, RoadContact r = openRoad()) {
    for (int i = 0; i < ticks; ++i) step(s,input,c,r);
}
}

int main() {
    VehicleConfig c;
    VehicleState s;
    reset(s,c);

    // Recovered nonlinear basis: odd symmetry, full-lock identity, successive
    // magnitude reduction; catches lost-sign repeated-squaring translations.
    for (float x : {0.0f, 0.125f, 0.5f, 0.875f, 1.0f}) {
        auto p = signedSteeringPowers(x), n = signedSteeringPowers(-x);
        for (std::size_t i = 0; i < p.size(); ++i) {
            check(near(p[i],-n[i]), "signed basis preserves odd symmetry");
            check(p[i] >= 0 && p[i] <= 1, "basis remains bounded");
            if (i) check(p[i] <= p[i-1], "higher steering powers reduce centre response");
        }
    }
    updateRecoveredSteering(s,0.5f);
    check(near(s.steeredHeading, recoveredSteeringLimit * 0.25f), "half-lock direction is 7.5 degrees");
    updateRecoveredSteering(s,-0.5f);
    check(near(s.steeringDelta,-1.0f), "steering delta retains reversal");
    check(near(s.steeredHeading,-recoveredSteeringLimit * 0.25f), "direction changes sign");
    s.yawUnwrapped = pi - 0.01f;
    s.yawFrameVelocity = 0.02f;
    integrateRecoveredYaw(s,0.0f);
    check(s.yaw < -3.0f && s.yaw >= -pi, "positive wrap crosses to negative pi");
    check(near(s.yawRate,1.2f), "frame yaw velocity converts at 60 Hz");
    s.speed = 10.0f;
    updateRecoveredVelocity(s,10.01f);
    check(near(s.accelProxy,0.2f,0.0001f), "observed dv proxy scale is twenty");
    updateRecoveredVelocity(s,0);
    check(s.accelProxy == -1, "deceleration proxy saturates");
    check(decodeShiftEdges(true,true) == 0, "ambiguous dual shift produces no direction");

    DriverInput gas; gas.throttle = 1; gas.automatic = false;
    reset(s,c);
    run(s,600,gas,c);
    check(s.speed > 8 && s.speed < 70, "car accelerates to finite speed with engine limits");
    check(std::abs(s.position.x) < 0.001f && std::abs(s.yaw) < 0.001f, "straight input preserves symmetry");
    check(s.gear == 1, "manual transmission never auto-shifts");
    DriverInput shift = gas; shift.shiftUp = true;
    run(s,60,shift,c);
    check(s.gear == 2, "holding shift causes exactly one rising-edge shift");
    shift.shiftUp = false; run(s,1,shift,c);
    shift.shiftUp = true; run(s,1,shift,c);
    check(s.gear == 3, "second press shifts after release");
    for (int i=0; i<12; ++i) {
        shift.shiftUp = false; run(s,20,shift,c);
        shift.shiftUp = true; run(s,20,shift,c);
    }
    check(s.gear == c.gearCount, "gear respects configured vehicle maximum");

    DriverInput braking; braking.brake = 1; braking.automatic = false;
    run(s,600,braking,c);
    check(s.speed < 0.03f && std::hypot(s.velocity.x,s.velocity.z) < 0.03f, "sustained brake stops vehicle");
    run(s,120,braking,c);
    check(s.speed >= 0 && s.velocity.z >= -0.001f, "braking cannot accelerate backwards");

    VehicleState left,right;
    reset(left,c); reset(right,c);
    left.velocity.z = right.velocity.z = 18.0f;
    left.speed = right.speed = 18.0f;
    DriverInput leftInput; leftInput.steer = -0.25f; leftInput.throttle = 0.35f;
    DriverInput rightInput = leftInput; rightInput.steer = 0.25f;
    run(left,120,leftInput,c); run(right,120,rightInput,c);
    check(right.yaw > 0 && left.yaw < 0, "positive input turns right in x/z convention");
    check(near(left.position.x,-right.position.x,0.001f), "left/right contact dynamics are symmetric");
    check(near(left.position.z,right.position.z,0.001f), "left/right travel is symmetric");
    check(std::isfinite(right.slip) && right.slip > 0, "slip is continuous contact output");

    // A contact/yaw sign error often looks correct briefly, then causes a
    // neutral car to spin faster. Test recovery across parking and road speeds.
    for (float initialSpeed : {0.0f,1.0f,10.0f,22.2f,40.0f}) {
        reset(s,c);
        s.velocity = {2.0f,0,initialSpeed}; s.speed = initialSpeed;
        s.yawRate = 0.4f; s.yawFrameVelocity = 0.4f*physicsDt;
        DriverInput neutral;
        run(s,240,neutral,c);
        check(std::abs(s.lateralSpeed) < 0.01f, "neutral contact recovers lateral disturbance");
        check(std::abs(s.yawRate) < 0.001f, "neutral contact damps yaw disturbance");
        check(std::hypot(s.velocity.x,s.velocity.z) <= std::hypot(2.0f,initialSpeed),
              "neutral contact does not generate translational energy");
        if (initialSpeed <= 1.0f) check(s.speed < 0.0001f, "rolling resistance reaches rest without residual creep");
    }

    reset(s,c); s.speed = 40.0f; s.velocity.z = 40.0f;
    DriverInput hardCorner; hardCorner.steer = 1.0f;
    run(s,600,hardCorner,c);
    check(std::isfinite(s.yawRate) && std::abs(s.yawRate) < 6.0f, "saturated full-lock coast remains numerically bounded");
    check(std::hypot(s.velocity.x,s.velocity.z) < 20.0f, "saturated corner contact dissipates initial energy");

    VehicleState first,second;
    reset(first,c); reset(second,c);
    for (int i=0; i<3600; ++i) {
        DriverInput in;
        in.throttle = i % 600 < 420 ? 0.8f : 0.1f;
        in.brake = i % 600 >= 480 ? 0.6f : 0;
        in.steer = std::sin(i * 0.01f) * 0.36f;
        step(first,in,c,openRoad()); step(second,in,c,openRoad());
        check(std::isfinite(first.position.x) && std::isfinite(first.yaw), "long replay stays finite");
    }
    check(first.position.x == second.position.x && first.position.z == second.position.z
          && first.yaw == second.yaw && first.rpm == second.rpm && first.gear == second.gear,
          "identical 60 Hz input replay is deterministic in the same build");
    check(first.tick == 3600, "one step advances exactly one simulation tick");

    RoadContact wall; wall.halfWidth = 5.0f; wall.lateralOffset = 4.20f;
    reset(s,c,{4.2f,0,0}); s.velocity = {8,0,18}; s.speed = 18;
    const float before = dot(s.velocity,s.velocity);
    DriverInput coast; coast.automatic = false;
    step(s,coast,c,wall);
    check(s.wallContact && s.wallImpactSpeed > 0, "wall detects outbound contact");
    check(s.position.x <= wall.halfWidth-c.contactHalfWidth+0.001f, "wall resolves penetration");
    check(dot(s.velocity,s.velocity) <= before, "wall response does not add kinetic energy");
    check(s.wallImpactSpeed < 20, "wall impact telemetry remains in velocity units");

    reset(s,c);
    DriverInput invalid;
    invalid.steer = std::numeric_limits<float>::quiet_NaN();
    invalid.throttle = 100;
    invalid.brake = -100;
    step(s,invalid,c,openRoad());
    check(s.steering == 0 && s.throttle == 1 && s.brake == 0, "host inputs are finite and bounded");
    std::cout << "PASS: " << checks << " physics checks. Original force-law parity remains unmeasured.\n";
}
