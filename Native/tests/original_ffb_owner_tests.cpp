#include "original_ffb_owner.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
std::size_t checks = 0;
void require(bool value, const char* what) {
    ++checks;
    if (!value) throw std::runtime_error(std::string("failed: ") + what);
}
void near(float a, float b, float tolerance, const char* what) {
    ++checks;
    if (!(std::fabs(a - b) <= tolerance)) {
        char text[256];
        std::snprintf(text, sizeof(text), "failed: %s (%.6f vs %.6f)", what, double(a), double(b));
        throw std::runtime_error(text);
    }
}

idas3ffb::OwnerInput driving(float steer, float strength = 1.0f, int car = 0) {
    idas3ffb::OwnerInput input{};
    input.driving = true;
    input.carIndex = car;
    input.speedKmh = 120.0f;
    input.steering = steer;
    input.strength = strength;
    return input;
}

// The ramp only opens with frames, so most assertions need the wheel to have
// been driving for a while first. 512 frames clears the 254-frame soft start.
void run(idas3ffb::Owner& owner, const idas3ffb::OwnerInput& input, int frames) {
    for (int i = 0; i < frames; ++i) owner.update(input, 1.0f / 60.0f);
}
void restart(idas3ffb::Owner& owner) { idas3ffb::reset(); owner.reset(); }
}  // namespace

int main() try {
    using namespace idas3ffb;
    Owner owner;

    // ---- the board starts released, and an idle owner leaves it that way ----
    restart(owner);
    owner.update({}, 1.0f / 60.0f);
    require(!snapshot().active, "an owner that is not driving never enables the board");

    // ---- the soft start: the window opens by one unit every two frames ----
    restart(owner);
    const auto hard = driving(1.0f);
    owner.update(hard, 1.0f / 60.0f);
    auto board = snapshot();
    require(board.coherent, "the first frame publishes a coherent board state");
    require(board.active, "driving enables the board");
    require(std::fabs(board.torque) <= 0.5f / 128.0f + 1e-6f,
            "frame one cannot reach more than half a unit of torque");
    float previous = std::fabs(board.torque);
    for (int frame = 2; frame <= 64; ++frame) {
        owner.update(hard, 1.0f / 60.0f);
        const float now = std::fabs(snapshot().torque);
        require(now >= previous - 1e-6f, "the soft start never goes backwards");
        require(now <= float(frame) / 2.0f / 128.0f + 1e-6f, "the soft start respects its window");
        previous = now;
    }

    // ---- self-centring: the constant force opposes where the wheel is ----
    restart(owner); run(owner, driving(1.0f), 512);
    const float right = snapshot().torque;
    restart(owner); run(owner, driving(-1.0f), 512);
    const float left = snapshot().torque;
    require(right < 0.0f, "a wheel turned right is pushed back left");
    require(left > 0.0f, "a wheel turned left is pushed back right");
    near(std::fabs(right), std::fabs(left), 1.5f / 128.0f, "the two sides match to a quantisation step");
    // Every torque the board reports is an exact multiple of 1/128, because the
    // wire carries an integer and nothing below the board divides it.
    const float steps = right * 128.0f;
    near(steps, std::round(steps), 1e-3f, "torque lands on the board's own 1/128 grid");
    restart(owner); run(owner, driving(0.0f), 512);
    near(snapshot().torque, 0.0f, 1e-6f, "a centred wheel is left alone");
    // Inversion mirrors the whole command, not just its sign near the centre.
    restart(owner); auto flipped = driving(1.0f); flipped.invert = true; run(owner, flipped, 512);
    near(snapshot().torque, -right, 1.5f / 128.0f, "inversion mirrors the board command");

    // ---- the force limit is the cabinet's own per-car and per-dial sum ----
    for (int car : {0, 7, 30, 34}) {
        for (int step : {0, 5, 10}) {
            restart(owner);
            const float strength = float(step) / 10.0f;
            run(owner, driving(1.0f, strength <= 0.0f ? 0.05f : strength, car), 512);
            const int dial = std::clamp(int(std::lround((strength <= 0.f ? 0.05f : strength) * 10.f)), 0, 10);
            const int expected = std::clamp(carLimit[car] + strengthLimitA[dial] + strengthLimitB[dial], 1, 127);
            require(owner.lastLimit() == expected, "the limit is carLimit + both operator dials");
            // Full lock asks for far more than the limit allows, so the torque
            // must sit exactly on the limit.
            near(std::fabs(snapshot().torque), float(expected) / 128.0f, 1e-6f,
                 "saturated torque is exactly the limit over 128");
        }
    }
    // The lightest and heaviest cars really do differ -- at the middle dial.
    restart(owner); run(owner, driving(1.0f, 0.5f, 30), 512);
    const int lightest = owner.lastLimit();
    restart(owner); run(owner, driving(1.0f, 0.5f, 34), 512);
    require(owner.lastLimit() > lightest, "car 34 is heavier on the wheel than car 30");
    // At the top dial they do not: +35 and +70 push every car past the board's
    // own ceiling of 127, which is why the arcade's two dials both repeat their
    // last entry. Turning the strength all the way up flattens the cars.
    restart(owner); run(owner, driving(1.0f, 1.0f, 30), 512);
    const int loud30 = owner.lastLimit();
    restart(owner); run(owner, driving(1.0f, 1.0f, 34), 512);
    require(loud30 == 127 && owner.lastLimit() == 127, "the top dial saturates every car at the board ceiling");

    // ---- the cabinet never sends a spring command ----
    restart(owner); run(owner, driving(0.7f), 512);
    require(snapshot().spring == 0.0f, "command 0x0B has no caller in the arcade and must not be invented");

    // ---- the damper: strength fixed at 2, pole = rate * grip^2 * scale ----
    restart(owner); run(owner, driving(0.0f, 0.5f), 512);
    board = snapshot();
    near(board.damperStrength, 2.0f / 127.0f, 1e-6f, "damper strength is the cabinet's 2");
    require(owner.lastPole() == 42, "a gripping car at the middle dial gets the cabinet's pole of 42");
    // Slip is what fades the damper, and it fades as the square.
    auto slipping = driving(0.0f, 0.5f);
    slipping.headingError = 0.15f;   // half of the 0.30 rad slip scale, so grip = 0.5
    restart(owner); run(owner, slipping, 512);
    require(owner.lastPole() == 10, "half grip quarters the pole, as grip enters squared");
    auto sideways = driving(0.0f, 0.5f);
    sideways.headingError = 1.5f;
    restart(owner); run(owner, sideways, 512);
    require(owner.lastPole() == 1, "a car that is fully sideways has the lightest pole the board allows");
    // Every dial position must stay inside the board's own [1,120].
    for (int step = 0; step <= 10; ++step) {
        restart(owner); run(owner, driving(0.0f, float(step) / 10.0f <= 0.f ? 0.05f : float(step) / 10.0f), 512);
        require(owner.lastPole() >= 1 && owner.lastPole() <= 120, "the pole never leaves [1,120]");
    }

    // ---- slip also lightens the centring force ----
    restart(owner); run(owner, driving(0.5f, 0.5f), 512);
    const float gripping = std::fabs(snapshot().torque);
    auto lightened = driving(0.5f, 0.5f);
    lightened.headingError = 0.2f;
    restart(owner); run(owner, lightened, 512);
    require(std::fabs(snapshot().torque) < gripping, "the wheel goes light as the front tyres let go");

    // ---- the rumble is a fixed 31.5 Hz sine, amplitude only ----
    restart(owner);
    auto impact = driving(0.0f);
    impact.impact = 1.0f;
    impact.wallContact = true;
    run(owner, impact, 10);
    board = snapshot();
    near(board.rumbleFrequencyHz, 31.5f, 1e-6f, "63 half-hertz is the only rumble rate the game sends");
    require(board.rumbleIntensity > 0.0f, "an impact is felt");
    run(owner, driving(0.0f), 120);
    require(snapshot().rumbleIntensity == 0.0f, "the rumble is switched off, not faded to nearly nothing");

    // ---- a barrier pushes the wheel, bounded by the cabinet's own +-40 ----
    restart(owner);
    auto wall = driving(0.0f);
    wall.wallContact = true;
    wall.wallLateral = 1.0f;
    run(owner, wall, 512);
    const float pushed = snapshot().torque;
    require(pushed > 0.0f, "a barrier on one side pushes the wheel the other way");
    near(pushed, 40.0f / 128.0f, 1e-6f, "the road term is bounded at the cabinet's 40");
    wall.wallLateral = -1.0f;
    restart(owner); run(owner, wall, 512);
    near(snapshot().torque, -40.0f / 128.0f, 1e-6f, "and the same the other way");

    // ---- the drive power is the board's, not the player's slider ----
    restart(owner); run(owner, driving(0.5f, 0.2f), 512);
    const float lowPower = snapshot().power;
    restart(owner); run(owner, driving(0.5f, 1.0f), 512);
    near(snapshot().power, lowPower, 1e-6f,
         "the player's strength moves the operator dials, not the board's drive power");
    near(snapshot().power, 1.0f, 1e-6f, "the arcade leaves the drive at full");

    // ---- letting go ----
    restart(owner); run(owner, driving(1.0f), 300);
    require(snapshot().active, "still driving");
    owner.update({}, 1.0f / 60.0f);
    require(!snapshot().active, "leaving the race releases the wheel");
    restart(owner); run(owner, driving(1.0f), 300);
    auto broken = driving(1.0f);
    broken.headingError = std::nanf("");
    owner.update(broken, 1.0f / 60.0f);
    require(!snapshot().active, "a NaN releases the wheel instead of holding the last force");

    std::cout << "PASS cabinet owner: soft start over 254 frames, self-centring constant force on the "
                 "board's 1/128 grid, per-car and per-dial force limit from 0C31FE04/0C31FD54/0C31FDD8, "
                 "no spring command (the arcade has no caller for 0x0B), damper strength 2 with a pole "
                 "of rate*grip^2*scale from 0C31FD80/0C31FDAC, a +-40 road term, 31.5 Hz rumble switched "
                 "off rather than faded, and release on idle and on NaN. " << checks
              << " checks. Board decode is original_ffb's; only the race-quantity stand-ins for vehicle "
                 "fields +0x1CC, +0x27C, the +-40 pair and the rumble accumulator are the remake's own.\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
