#include "original_ffb.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

// Pins the cabinet force-feedback decode and scaling to the board's behaviour.
// Every expected value here comes from the protocol itself, not from this
// implementation: the oracle is Flycast core/hw/naomi/midiffb.cpp, whose
// recorded Initial D v3 traffic is reproduced below.
//
// No original image and no hardware are required; this is pure decode.
int main() try {
    unsigned checks = 0;
    auto check = [&](bool ok, const char* why) {
        ++checks;
        if (!ok) throw std::runtime_error(why);
    };
    auto near = [](float a, float b) { return std::fabs(a - b) < 1e-4f; };

    idas3ffb::reset();
    idas3ffb::Snapshot s = idas3ffb::snapshot();
    check(s.coherent, "a settled board must read coherently");
    check(!s.active && !s.calibrating, "the board starts inactive");
    check(near(s.power, 0.8f), "board default drive power is 0.8");
    check(s.torque == 0.0f && s.spring == 0.0f, "no force before any command");

    // A parameter byte with the high bit set would be read as a new packet.
    check(!idas3ffb::submit(idas3ffb::CommandTorque, 0x80u, 0u),
          "a parameter above 0x7f must be refused");
    check(!idas3ffb::submit(0x80u, 0u, 0u), "a command above 0x7f must be refused");

    // Initial D v3 init, exactly as recorded by the oracle: force limit 30 40,
    // the unknown enable 7f 54, then phase alignment.
    check(idas3ffb::submit(idas3ffb::CommandForceLimit, 0x30u, 0x40u), "limit");
    check(idas3ffb::submit(idas3ffb::CommandUnknownEnable, 0x7fu, 0x54u), "enable");

    // Command 3: power = (p1 >> 3) / 15. 0x78 >> 3 == 15, so full power.
    check(idas3ffb::submit(idas3ffb::CommandPower, 0x78u, 0x04u), "power");
    check(near(idas3ffb::snapshot().power, 1.0f), "0x78 is full drive power");

    // Command 4: raw = ((p1 << 7) | p2) - 0x80, force = raw / 128 * power.
    // 0x80 is neutral; 0x00 is full one way; 0x17f is full the other.
    check(idas3ffb::submit(idas3ffb::CommandTorque, 1u, 0u), "torque neutral");
    check(near(idas3ffb::snapshot().torque, 0.0f), "0x080 is neutral torque");
    check(idas3ffb::submit(idas3ffb::CommandTorque, 0u, 0u), "torque min");
    check(near(idas3ffb::snapshot().torque, -1.0f), "0x000 is full negative torque");
    check(idas3ffb::submit(idas3ffb::CommandTorque, 2u, 0x7fu), "torque max");
    check(near(idas3ffb::snapshot().torque, 1.0f), "0x17f is full positive torque");

    // The board is not active yet, so spring/damper values still decode and
    // scale: activation gates the host effect, not the decode.
    check(idas3ffb::submit(idas3ffb::CommandSpring, 0x7fu, 0u), "spring");
    check(near(idas3ffb::snapshot().spring, 0x30 / 127.0f),
          "spring is capped by the command 1 force limit, not by 0x7f");

    // Command 6 is a ratio: strength p1/127 scaled by power, pole p2/127 raw.
    check(idas3ffb::submit(idas3ffb::CommandDamper, 0x02u, 0x2cu), "damper");
    s = idas3ffb::snapshot();
    check(near(s.damperStrength, 2 / 127.0f), "damper strength is p1/127 * power");
    check(near(s.damperParameter, 0x2c / 127.0f), "damper pole is p2/127, unscaled");

    // Rumble only latches while the board is active, and bumps its serial so a
    // one-shot is never replayed by a reacquire.
    const std::uint64_t quiet = idas3ffb::snapshot().rumbleSerial;
    check(idas3ffb::submit(idas3ffb::CommandRumble, 0x0au, 0x20u), "rumble inactive");
    check(idas3ffb::snapshot().rumbleSerial == quiet,
          "an inactive board must ignore rumble");

    check(idas3ffb::submit(idas3ffb::CommandEnable, 0u, 1u), "enable on");
    check(idas3ffb::snapshot().active, "command 0 with p2=1 activates");
    check(idas3ffb::submit(idas3ffb::CommandRumble, 0x0au, 0x20u), "rumble active");
    s = idas3ffb::snapshot();
    check(s.rumbleSerial == quiet + 1u, "an active rumble bumps the serial once");
    check(near(s.rumbleFrequencyHz, 5.0f), "p1 is frequency in half Hz");
    check(near(s.rumbleIntensity, (0x20 - 1) / 80.0f * s.power),
          "intensity is (p2-1)/80 scaled by power");

    // Command 0 with p2=0 stops everything and leaves calibration.
    check(idas3ffb::submit(idas3ffb::CommandEnable, 0u, 0u), "enable off");
    check(!idas3ffb::snapshot().active, "command 0 with p2=0 deactivates");

    // Reset enters calibration, where the board integrates torque into its own
    // position estimate and ignores the real axis.
    check(idas3ffb::submit(idas3ffb::CommandReset, 0u, 0u), "reset");
    check(idas3ffb::snapshot().calibrating, "command 0x7f begins calibration");

    // Replies: 0x90, position high seven bits, low seven bits, checksum.
    std::vector<std::uint8_t> reply;
    static std::vector<std::uint8_t>* sink = &reply;
    idas3ffb::setReplySink([](std::uint8_t byte) { sink->push_back(byte); });
    idas3ffb::submit(idas3ffb::CommandEnable, 0u, 0u);
    idas3ffb::setWheelPosition(0u);
    reply.clear();
    idas3ffb::submit(idas3ffb::CommandTorque, 1u, 0u);
    check(reply.size() == 4u, "one command produces one four-byte reply");
    check(reply[0] == 0x90u, "reply status byte is 0x90");
    const unsigned position = (unsigned(reply[1]) << 7) | reply[2];
    check(position == 16383u,
          "the MIDI encoder runs opposite the JVS axis: axis 0 is encoder 16383");
    check(reply[3] == ((reply[0] ^ reply[1] ^ reply[2]) & 0x7fu), "reply checksum");
    idas3ffb::setWheelPosition(65535u);
    reply.clear();
    idas3ffb::submit(idas3ffb::CommandTorque, 1u, 0u);
    check(((unsigned(reply[1]) << 7) | reply[2]) == 0u, "axis 65535 is encoder 0");
    idas3ffb::setReplySink(nullptr);

    // A packet whose checksum does not match must change nothing.
    idas3ffb::reset();
    idas3ffb::submit(idas3ffb::CommandPower, 0x78u, 0x04u);
    const float before = idas3ffb::snapshot().power;
    idas3ffb::receiveByte(static_cast<std::uint8_t>(idas3ffb::CommandPower | 0x80u));
    idas3ffb::receiveByte(0x08u);
    idas3ffb::receiveByte(0x00u);
    idas3ffb::receiveByte(0x7fu);  // deliberately wrong
    check(near(idas3ffb::snapshot().power, before),
          "a bad checksum must not apply the packet");

    std::cout << "original_ffb: " << checks << " checks passed\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << "original_ffb FAILED: " << error.what() << '\n';
    return 1;
}
