#include "original_ffb_owner.h"

#include <algorithm>
#include <cmath>

namespace idas3ffb {

// ---- the cabinet's own tables, read out of the SH-4 image ----
// 0C31FE04, thirty-five entries, one per car, indexed by *0C901654. The same
// index selects a 24-byte engine record at 0C270EB0 whose first float is the
// redline (8000..10000 rpm), which is how the index was identified.
const int carLimit[35] = {
    90, 90, 90, 94, 95, 93, 95, 98, 97, 90, 91, 91, 92, 90, 94, 93, 97, 95,
    96, 98, 90, 91, 98, 98, 97, 90, 90, 98, 98, 91, 88, 96, 92, 89, 99,
};
// 0C31FD54 and 0C31FDD8, the two eleven-position operator dials. Their top
// entries repeat, which is the cabinet saying the dial saturates.
const int strengthLimitA[11] = {-35, -28, -21, -14, -7, 0, 7, 14, 21, 28, 35};
const int strengthLimitB[11] = {-40, -30, -20, -10, 0, 10, 20, 30, 40, 70, 70};
// 0C31FD80 and 0C31FDAC, the same two dials again, for the damper.
const float damperRate[11] = {10.f, 10.f, 15.f, 20.f, 25.f, 35.f, 40.f, 45.f, 50.f, 55.f, 60.f};
const float damperScale[11] = {.2f, .4f, .6f, .8f, 1.f, 1.2f, 1.4f, 1.6f, 1.8f, 2.6f, 2.6f};

namespace {

constexpr int kRampCap = 256;        // 0C15A60C: the counter saturates at 0xFF+1
constexpr int kCentre = 0x80;        // command 4 subtracts this
constexpr int kLimitCeiling = 127;   // 0C15A7CC..0C15A7D2 and again at 0C15A83A
constexpr int kRoadTerm = 40;        // 0C15A648/0C15A676: the road term's own bound
constexpr float kCentringGain = 200.0f;  // 0C15A794..0C15A79A: 100, then doubled
constexpr std::uint8_t kRumbleHalfHz = 63u;   // 0C15AA98: 31.5 Hz, never varied
constexpr int kDamperStrength = 2;   // 0C15A8DC: the only strength the game sends
// The board's power before any command 3 is 0.8, i.e. step 12 of 15. The arcade
// leaves the drive at full and does its scaling with the dials above, so power
// is set once and the player's setting moves the dials instead.
constexpr int kPowerStep = 15;

bool finite(float v) { return std::isfinite(v); }

// FTRC truncates toward zero; std::lround would not, and the difference shows
// up as a whole board unit near the centre.
int truncate(float v) { return static_cast<int>(v); }

int dial(float strength) {
    return std::clamp(static_cast<int>(std::lround(std::clamp(strength, 0.0f, 1.0f) * 10.0f)), 0, 10);
}

}  // namespace

void Owner::reset() {
    started = false;
    frames = 0;
    limit = 0;
    lastTorque = lastDamperPole = lastPower = lastRumble = -1;
    rumbleHold = 0.0f;
}

void Owner::update(const OwnerInput& input, float dt) {
    if (!finite(input.speedKmh) || !finite(input.steering) ||
        !finite(input.headingError) || !finite(input.wallLateral) ||
        !finite(input.impact) || !finite(input.strength) || !finite(dt)) {
        if (started) submit(CommandEnable, 0u, 0u);
        reset();
        return;
    }
    if (!input.driving || input.strength <= 0.0f) {
        // Command 0 with p2 = 0 is how the cabinet lets go of the wheel between
        // races; it is not the same as sending zero torque, which would hold.
        if (started) submit(CommandEnable, 0u, 0u);
        reset();
        return;
    }
    if (!started) {
        // The cabinet's own ready-to-drive state, from step 240 of the boot
        // script at 0C159FA0: force limit, the undecoded command 2, then enable.
        //
        // What is deliberately NOT replayed from that script: step 0's command
        // 0x7F, which puts the board into calibration and on real hardware
        // sweeps the wheel to find its stops -- a DirectInput wheel has already
        // done that and would only fight us -- and the 421..539 self-test that
        // servos the wheel from the cabinet's own ADC.
        submit(CommandForceLimit, 48u, 64u);   // 0C15A0C0
        submit(CommandUnknownEnable, 127u, 84u);  // 0C15A0C6
        submit(CommandEnable, 0u, 1u);         // 0C15A0CE
        started = true;
        frames = 0;
    }
    if (frames <= kRampCap) ++frames;

    const int index = dial(input.strength);
    const int car = std::clamp(input.carIndex, 0, 34);

    // ---- power (command 3): p1 >> 3 over 15, so only sixteen steps exist. ----
    if (kPowerStep != lastPower) {
        submit(CommandPower, static_cast<std::uint8_t>(kPowerStep << 3), 0u);
        lastPower = kPowerStep;
    }

    // REMAKE MAPPING for the original's vehicle field +0x27C, which the owner
    // only ever uses as (1 - x) and which scales both the centring force and the
    // damper. Slip is what makes an arcade wheel go light, so slip is what stands
    // in for it here.
    const float grip = std::clamp(1.0f - std::fabs(input.headingError) / 0.30f, 0.0f, 1.0f);

    // ---- the force limit ----
    // This is NOT command 1. The arcade sends command 1 exactly once, at boot,
    // as {0x01,48,64} (0C15A0C0 and again at 0C15A120); the limit computed here
    // is only ever a local clamp on the torque code (0C15A7A0..0C15A83E), and
    // command 1 caps the spring, which this game never sends.
    limit = std::clamp(carLimit[car] + strengthLimitA[index] + strengthLimitB[index], 1, kLimitCeiling);
    // The original then trims it by int(int(limit*0.9) * (f(+0x24C)+1) * 0.5).
    // That field's neutral value is exactly -1.0f, which makes the whole term
    // vanish, and the remake has nothing that stands in for it, so the trim is
    // deliberately left at its neutral rather than guessed at.

    // ---- torque (command 4) ----
    // The dominant term is the cabinet's own: a centring force proportional to
    // where the wheel is, faded out as grip is lost. REMAKE MAPPING only in that
    // `steering` stands in for the original's wheel position at +0x1CC.
    const float wheel = std::clamp(input.steering, -1.0f, 1.0f);
    int value = kCentre - truncate(kCentringGain * wheel * grip);

    // The signed road term, bounded exactly as the original bounds its own.
    // REMAKE MAPPING: the original differences two vehicle fields; here a barrier
    // supplies the push, and nothing else does yet.
    float road = 0.0f;
    if (input.wallContact) road = std::clamp(input.wallLateral, -1.0f, 1.0f);
    value += std::clamp(truncate(road * float(kRoadTerm)), -kRoadTerm, kRoadTerm);

    if (input.invert) value = kCentre - (value - kCentre);
    value = std::clamp(value, kCentre - limit, kCentre + limit);
    // The soft start. Until 254 frames have passed this is the binding clamp.
    const int window = frames / 2;
    value = std::clamp(value, kCentre - window, kCentre + window);
    value = std::clamp(value, 0, 0x3fff);
    if (value != lastTorque) {
        submit(CommandTorque, static_cast<std::uint8_t>((value >> 7) & 0x7f),
               static_cast<std::uint8_t>(value & 0x7f));
        lastTorque = value;
    }

    // ---- damper (command 6): strength is always 2; the pole carries the feel. ----
    const int pole = std::clamp(
        truncate(damperRate[index] * grip * grip * damperScale[index]), 1, 120);
    if (pole != lastDamperPole) {
        submit(CommandDamper, static_cast<std::uint8_t>(kDamperStrength),
               static_cast<std::uint8_t>(pole));
        lastDamperPole = pole;
    }

    // ---- rumble (command 5): fixed 31.5 Hz, amplitude only, latched off. ----
    // REMAKE MAPPING for the amplitude and its decay. The original accumulates
    // seven terms from vehicle fields around +0x258..+0x264 and latches the
    // result; the hold here is what turns a one-frame impact value into an event
    // a driver can feel.
    if (input.impact > 0.0f) rumbleHold = std::max(rumbleHold, std::clamp(input.impact, 0.0f, 1.0f));
    else if (dt > 0.0f) rumbleHold = std::max(0.0f, rumbleHold - dt * 4.0f);
    // Amplitude 1 is silence in the board (it subtracts one), so an audible
    // rumble starts at 2 and 81 is already full scale.
    const int amplitude = rumbleHold > 0.0f
        ? std::clamp(1 + static_cast<int>(std::lround(rumbleHold * 80.0f)), 2, 81)
        : 0;
    if (amplitude != lastRumble) {
        if (amplitude) submit(CommandRumble, kRumbleHalfHz, static_cast<std::uint8_t>(amplitude));
        else submit(CommandRumble, 0u, 0u);
        lastRumble = amplitude;
    }
}

}  // namespace idas3ffb
