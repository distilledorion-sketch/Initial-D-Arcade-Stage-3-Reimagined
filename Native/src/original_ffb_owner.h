#pragma once

#include "original_ffb.h"

// The cabinet's board takes commands, not forces. original_ffb.cpp is the board;
// this is the half of the pair that decides what to send it, which in the arcade
// lives in the SH-4 binary at 0C15A5E0 and reaches the wire through the encoder
// at 0C15AC80 and the sixteen one-command wrappers at 0C159B40..0C159F00.
//
// Taken from the original and not to be "improved":
//   * Of the sixteen wrappers only eleven are ever called. Commands 0x07, 0x0B,
//     0x0D and 0x70 have NO caller anywhere in the image -- so the cabinet never
//     sends a spring command, and all self-centring is carried by the constant
//     force below. Do not add a spring here; it would double the centring.
//   * The torque code is 14 bits centred on 0x80 and is dominated by
//     -(int)(200 * wheelPosition * grip): a centring spring expressed as a
//     constant force (0C15A790..0C15A7AA).
//   * A signed road term of at most +-40 is added to it (0C15A626..0C15A688).
//   * It is clamped to +-limit, where limit = carLimit[car] + strengthLimitA +
//     strengthLimitB, twice clamped to [1,127] (0C15A7A0..0C15A83E).
//   * It is ALSO clamped to a window that opens as frames/2 from the centre, so
//     the wheel eases into full authority over the first 256 frames of a race
//     rather than snapping (0C15A606, 0C15A856..0C15A86C).
//   * The damper is always sent with strength 2 and only its pole varies, as
//     (int)(damperRate[a] * grip * grip * damperScale[b]) clamped to [1,120]
//     (0C15A87E..0C15A8E2).
//   * The rumble is a fixed 63 half-Hz -- 31.5 Hz -- sine whose amplitude alone
//     changes, and it is silenced with (0,0) rather than left running
//     (0C15AA9A..0C15AAB8).
//   * Every command is sent only when its value changes.
//
// This remake's own, and marked REMAKE MAPPING where it appears: which race
// quantity stands in for the original's vehicle fields at 0C900F00+0x1CC
// (wheel position), +0x27C (the grip factor, used as 1 - x), +0x3C4/+0x3C8 and
// +0x3B4/+0x3B8 (the road term), and what drives the rumble amplitude.
namespace idas3ffb {

// What the remake knows about the car this frame. All fields are finite or the
// update is refused, because a NaN reaching the board would latch a force.
struct OwnerInput {
    bool driving = false;        // false releases the wheel, as command 0 p2=0 does
    int carIndex = 0;            // 0..34, selects the cabinet's per-car force limit
    float speedKmh = 0.0f;
    float steering = 0.0f;       // [-1,1] where the wheel actually is
    float headingError = 0.0f;   // radians of slip; the front tyres unload as it grows
    float wallLateral = 0.0f;    // [-1,1] signed push out of a barrier
    float impact = 0.0f;         // [0,1] one-shot collision severity
    bool wallContact = false;
    float strength = 1.0f;       // [0,1], quantised to the cabinet's eleven dial steps
    bool invert = false;         // wheels whose positive torque turns the other way
};

// The cabinet's own tuning tables, read out of the image. carLimit is indexed by
// the car; the other four by the two eleven-position operator dials that the
// player's strength setting stands in for here.
extern const int carLimit[35];          // 0C31FE04
extern const int strengthLimitA[11];    // 0C31FD54, indexed by *0C9015D8
extern const int strengthLimitB[11];    // 0C31FDD8, indexed by *0C9015EC
extern const float damperRate[11];      // 0C31FD80, indexed by *0C9015D8
extern const float damperScale[11];     // 0C31FDAC, indexed by *0C9015EC

class Owner {
public:
    void reset();
    // dt is only used to age the rumble one-shot; the ramp counts frames, as
    // the original's does.
    void update(const OwnerInput& input, float dt);

    // What the owner last decided to send, for tests and for the options screen.
    int lastTorqueCode() const { return lastTorque; }
    int lastLimit() const { return limit; }
    int lastPole() const { return lastDamperPole; }

private:
    bool started = false;
    int frames = 0;              // 0CAA974C: saturates at 256
    int limit = 0;
    int lastTorque = -1;         // 0CAA9754, initialised to the centre
    int lastDamperPole = -1;     // 0CAA9758
    int lastPower = -1;
    int lastRumble = -1;         // 0CAA975C, the on/off latch
    float rumbleHold = 0.0f;
};

}  // namespace idas3ffb
