#include "original_ffb.h"

#include <algorithm>

namespace idas3ffb {
namespace {

std::array<std::uint8_t, 4u> packet{};
std::uint8_t packetIndex = 0u;
std::int32_t calibrationPosition = 8192;
std::uint8_t maximumSpring = 0x7fu;

std::atomic<ReplySink> replySink{nullptr};

// Even revision means settled, odd means a writer is mid-update. Readers retry
// rather than sample a mix of two commands.
std::atomic<std::uint64_t> revision{0u};
std::atomic<std::uint64_t> rumbleSerial{0u};
std::atomic<bool> active{false};
std::atomic<bool> calibrating{false};
// Board default is 0.8 drive power, which is (12 >> 0) / 15 in command 3 terms.
std::atomic<std::uint8_t> powerStep{12u};
std::atomic<std::int32_t> torqueRaw{0};
std::atomic<std::uint8_t> springRaw{0u};
std::atomic<std::uint8_t> damperStrengthRaw{0u};
std::atomic<std::uint8_t> damperParameterRaw{0u};
std::atomic<std::uint8_t> rumbleAmplitudeRaw{0u};
std::atomic<std::uint8_t> rumbleFrequencyHalfHz{0u};
std::atomic<std::uint16_t> wheelPosition{8192u};

void beginPublish() { revision.fetch_add(1u, std::memory_order_acq_rel); }
void endPublish() { revision.fetch_add(1u, std::memory_order_release); }

void sendReply(std::uint16_t position) {
    const ReplySink sink = replySink.load(std::memory_order_acquire);
    if (sink == nullptr) return;
    const std::uint8_t first = 0x90u;
    const std::uint8_t second = static_cast<std::uint8_t>((position >> 7u) & 0x7fu);
    const std::uint8_t third = static_cast<std::uint8_t>(position & 0x7fu);
    sink(first);
    sink(second);
    sink(third);
    sink(static_cast<std::uint8_t>((first ^ second ^ third) & 0x7fu));
}

}  // namespace

void setReplySink(ReplySink sink) {
    replySink.store(sink, std::memory_order_release);
}

void reset() {
    packet = {};
    packetIndex = 0u;
    calibrationPosition = 8192;
    maximumSpring = 0x7fu;
    beginPublish();
    rumbleSerial.store(0u, std::memory_order_relaxed);
    active.store(false, std::memory_order_relaxed);
    calibrating.store(false, std::memory_order_relaxed);
    powerStep.store(12u, std::memory_order_relaxed);
    torqueRaw.store(0, std::memory_order_relaxed);
    springRaw.store(0u, std::memory_order_relaxed);
    damperStrengthRaw.store(0u, std::memory_order_relaxed);
    damperParameterRaw.store(0u, std::memory_order_relaxed);
    rumbleAmplitudeRaw.store(0u, std::memory_order_relaxed);
    rumbleFrequencyHalfHz.store(0u, std::memory_order_relaxed);
    wheelPosition.store(8192u, std::memory_order_relaxed);
    endPublish();
}

void setWheelPosition(std::uint16_t cabinetSteering) {
    // 0..65535 JVS axis to 16383..0 encoder: the MIDI encoder direction is
    // opposite the JVS steering axis on the standard Initial D cabinet.
    wheelPosition.store(static_cast<std::uint16_t>(
        16383u - (static_cast<std::uint32_t>(cabinetSteering) * 16383u / 65535u)),
        std::memory_order_release);
}

void receiveByte(std::uint8_t byte) {
    // The board integrates torque into its own position estimate while it hunts
    // for centre, because the real axis is meaningless until calibration ends.
    if (calibrating.load(std::memory_order_relaxed)) {
        calibrationPosition = std::clamp<std::int32_t>(
            calibrationPosition + torqueRaw.load(std::memory_order_relaxed),
            0, 16383);
    }
    if ((byte & 0x80u) != 0u) packetIndex = 0u;
    packet[packetIndex] = byte;
    if (packetIndex == 3u &&
        static_cast<std::uint8_t>((packet[0] ^ packet[1] ^ packet[2]) & 0x7fu) ==
            packet[3]) {
        const std::uint8_t command = static_cast<std::uint8_t>(packet[0] & 0x7fu);
        const std::uint8_t first = packet[1];
        const std::uint8_t second = packet[2];
        // Status and reply-mode traffic is frequent and changes no effect.
        // Publishing a revision for it would make readers retry for nothing.
        const bool publishes =
            command == CommandEnable || command == CommandPower ||
            command == CommandTorque ||
            (command == CommandRumble && active.load(std::memory_order_relaxed)) ||
            command == CommandDamper || command == CommandSpring ||
            command == CommandReset;
        if (publishes) beginPublish();
        switch (command) {
        case CommandEnable:
            if (second == 0u) {
                active.store(false, std::memory_order_relaxed);
                calibrating.store(false, std::memory_order_relaxed);
            } else if (second == 1u) {
                active.store(true, std::memory_order_relaxed);
            }
            break;
        case CommandForceLimit:
            maximumSpring = first;
            break;
        case CommandPower:
            powerStep.store(static_cast<std::uint8_t>(first >> 3u),
                            std::memory_order_relaxed);
            break;
        case CommandTorque:
            torqueRaw.store(
                static_cast<std::int32_t>(
                    (static_cast<std::uint16_t>(first) << 7u) | second) - 0x80,
                std::memory_order_relaxed);
            break;
        case CommandRumble:
            if (active.load(std::memory_order_relaxed)) {
                rumbleFrequencyHalfHz.store(first, std::memory_order_relaxed);
                rumbleAmplitudeRaw.store(second, std::memory_order_relaxed);
                rumbleSerial.fetch_add(1u, std::memory_order_relaxed);
            }
            break;
        case CommandDamper:
            damperStrengthRaw.store(first, std::memory_order_relaxed);
            damperParameterRaw.store(second, std::memory_order_relaxed);
            break;
        case CommandSpring:
            springRaw.store(std::min<std::uint8_t>(first, maximumSpring),
                            std::memory_order_relaxed);
            break;
        case CommandReset:
            calibrating.store(true, std::memory_order_relaxed);
            calibrationPosition = 8192;
            break;
        default:
            break;
        }
        if (publishes) endPublish();

        const bool calibration = calibrating.load(std::memory_order_acquire);
        sendReply(calibration
            ? static_cast<std::uint16_t>(calibrationPosition)
            : wheelPosition.load(std::memory_order_acquire));
    }
    packetIndex = static_cast<std::uint8_t>((packetIndex + 1u) % packet.size());
}

bool submit(std::uint8_t command, std::uint8_t first, std::uint8_t second) {
    // Every parameter byte on this bus is seven-bit; a high bit would be read
    // as the start of a new packet and silently corrupt the stream.
    if ((command & 0x80u) != 0u || (first & 0x80u) != 0u ||
        (second & 0x80u) != 0u)
        return false;
    const std::uint8_t status = static_cast<std::uint8_t>(command | 0x80u);
    receiveByte(status);
    receiveByte(first);
    receiveByte(second);
    receiveByte(static_cast<std::uint8_t>((status ^ first ^ second) & 0x7fu));
    return true;
}

Snapshot snapshot() {
    Snapshot result{};
    for (unsigned attempt = 0u; attempt < 8u; ++attempt) {
        const std::uint64_t before = revision.load(std::memory_order_acquire);
        if ((before & 1u) != 0u) continue;
        const float power = std::clamp(
            static_cast<float>(powerStep.load(std::memory_order_relaxed)) / 15.0f,
            0.0f, 1.0f);
        result.revision = before;
        result.rumbleSerial = rumbleSerial.load(std::memory_order_relaxed);
        result.active = active.load(std::memory_order_relaxed);
        result.calibrating = calibrating.load(std::memory_order_relaxed);
        result.power = power;
        result.torque = std::clamp(
            static_cast<float>(torqueRaw.load(std::memory_order_relaxed)) /
                128.0f * power, -1.0f, 1.0f);
        result.spring = std::clamp(
            static_cast<float>(springRaw.load(std::memory_order_relaxed)) /
                127.0f * power, 0.0f, 1.0f);
        result.damperStrength = std::clamp(
            static_cast<float>(damperStrengthRaw.load(std::memory_order_relaxed)) /
                127.0f * power, 0.0f, 1.0f);
        result.damperParameter = std::clamp(
            static_cast<float>(damperParameterRaw.load(std::memory_order_relaxed)) /
                127.0f, 0.0f, 1.0f);
        const int amplitude = std::max<int>(0,
            static_cast<int>(rumbleAmplitudeRaw.load(std::memory_order_relaxed)) - 1);
        result.rumbleIntensity = std::clamp(
            static_cast<float>(amplitude) / 80.0f * power, 0.0f, 1.0f);
        result.rumbleFrequencyHz = static_cast<float>(
            rumbleFrequencyHalfHz.load(std::memory_order_relaxed)) / 2.0f;
        const std::uint64_t after = revision.load(std::memory_order_acquire);
        if (before == after && (after & 1u) == 0u) {
            result.revision = after;
            result.coherent = true;
            return result;
        }
    }
    result.coherent = false;
    return result;
}

}  // namespace idas3ffb
