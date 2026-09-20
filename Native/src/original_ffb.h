#pragma once

#include <array>
#include <atomic>
#include <cstdint>

// Initial D's force-feedback board hangs off the AICA MIDI port. The cabinet
// game writes four-byte packets (command|0x80, p1, p2, seven-bit XOR checksum)
// and the board replies with the 14-bit wheel encoder position, centre 8192.
//
// This module reproduces that board's decode and scaling exactly, so the remake
// drives a wheel with the cabinet's own effect model rather than an invented
// one: a signed constant torque, a centring spring capped by a separately set
// limit, a viscous damper expressed as a strength/pole pair, and a one-shot
// sine rumble -- all scaled by a global drive power.
//
// Behaviour is the oracle's (Flycast core/hw/naomi/midiffb.cpp) command for
// command. The remake has no original binary emitting the protocol, so
// original_ffb_owner.* generates the commands from race state; keeping the
// command layer as the interface means a captured cabinet stream can replace
// that generator without touching anything below it.
//
// This is strictly an OUTPUT path. Nothing here may feed back into dynamics.
namespace idas3ffb {

// Commands the board understands. Values are the wire command, i.e. the low
// seven bits of the first packet byte.
enum Command : std::uint8_t {
    CommandEnable        = 0x00u,  // p2: 0 off, 1 on
    CommandForceLimit    = 0x01u,  // p1 caps CommandSpring
    CommandUnknownEnable = 0x02u,  // follows 1 or 6; no decoded effect
    CommandPower         = 0x03u,  // p1 >> 3 over 15
    CommandTorque        = 0x04u,  // ((p1 << 7) | p2) - 0x80
    CommandRumble        = 0x05u,  // p1 half-Hz, p2 amplitude
    CommandDamper        = 0x06u,  // p1 strength, p2 pole
    CommandSpring        = 0x0Bu,  // min(p1, limit)
    CommandReset         = 0x7Fu,  // enters calibration
};

// A coherent view of the board state. Never assembled from two different
// commands: see snapshot().
struct Snapshot {
    std::uint64_t revision = 0u;
    std::uint64_t rumbleSerial = 0u;   // changes only on a fresh rumble event
    bool coherent = false;
    bool active = false;
    bool calibrating = false;
    float power = 0.8f;                // board default before any command 3
    float torque = 0.0f;               // [-1, 1], positive is one wheel side
    float spring = 0.0f;               // [0, 1]
    float damperStrength = 0.0f;       // [0, 1]
    float damperParameter = 0.0f;      // [0, 1], the transfer-function pole
    float rumbleIntensity = 0.0f;      // [0, 1]
    float rumbleFrequencyHz = 0.0f;
};

// Optional sink for the board's reply, for a future captured-stream path. The
// remake does not run an AICA, so by default nothing consumes replies.
using ReplySink = void (*)(std::uint8_t byte);

void setReplySink(ReplySink sink);
void reset();

// Steering position for the reply, as the cabinet's 0..65535 JVS axis. The MIDI
// encoder runs OPPOSITE the JVS axis on this cabinet, so this inverts.
void setWheelPosition(std::uint16_t cabinetSteering);

// Feed one wire byte. Packets are validated by checksum before they apply.
void receiveByte(std::uint8_t byte);

// Encode and feed a command, so the generator exercises the same checksum and
// decode path the wire would. Returns false if the parameters are out of the
// seven-bit range the protocol allows.
bool submit(std::uint8_t command, std::uint8_t first, std::uint8_t second);

// Lock-free read. coherent is false when the writer was mid-update after
// several attempts; the caller must keep its previous state and retry rather
// than apply a torn mix of two commands.
Snapshot snapshot();

}  // namespace idas3ffb
