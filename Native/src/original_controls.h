#pragma once
#include <cstdint>

namespace idas3 {

// Pure original-coordinate boundary from FUN_0C15CEC0. This has no dependency
// on DriverInput or the renderer's steering convention. It does not smooth,
// invert a host controller, choose calibration defaults, or advance physics.
struct RawAnalog16 {
    std::uint16_t steering, throttle, brake;
};

// Preserve the full memory words: SH-4 integer arithmetic wraps at 32 bits.
// These values must come from original calibration/state, not a host deadzone.
struct OriginalInputCalibration {
    std::uint32_t steeringWord, throttleWord, brakeWord;
};

struct OriginalInputMode {
    std::uint32_t suppressRawThrottleWord; // [0C2F4BC8] != 0
    std::uint32_t scaleSteeringWord;       // [drive state +013C] != 0
};

struct OriginalInputConstants {
    std::int16_t steeringIntegerAddend; // signed16 literal at 0C15D0A6
    std::uint32_t negativeSignBits;     // float32 literal at 0C15D0F0
    std::uint32_t steeringDivisorBits;  // float32 literal at 0C15D0F4
    std::uint32_t pedalDivisorBits;     // float32 literal at 0C15D0F8
    std::uint32_t steeringScaleBits;    // float32 literal at 0C15D0FC
};

// Extracted from the user's verified 4,194,304-byte program loaded at 0C020000.
// SHA-256 efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335.
// The values are exact literal bits; no inferred tuning defaults are included.
constexpr OriginalInputConstants verifiedGds0033InputConstants() {
    return {128,0xbf800000u,0x42a00000u,0x42d60000u,0x3f4ccccdu};
}

struct OriginalControls {
    std::uint32_t steeringHighByte=0, throttleHighByte=0, brakeHighByte=0;
    std::uint32_t throttleHighByteAfterMode=0;
    std::int32_t steeringInteger=0;
    std::int32_t throttleIntegerBeforeFloor=0, brakeIntegerBeforeFloor=0;
    std::int32_t throttleInteger=0, brakeInteger=0;
    float steeringBeforeScale=0;
    // The drive-state alias is written BEFORE final global clamping.
    float steeringAlias=0;          // [0C900F00 +01C8]
    float steering=0;               // [0CAA9894], clamped
    float throttleAlias=0;          // [0CAA989C], before final global clamp
    float throttle=0;               // [0CAA9898], clamped
    float brakeBeforeClamp=0;
    float brake=0;                  // [0CAA98A0], clamped
    // Unrelated raw word copied by this block; no float reinterpretation.
    std::uint32_t passthroughBits=0; // [0C900EA0] -> [0CAA98AC]
};

// Exact operation/branch order for raw acquisition through the block ending
// at L_0C15D124. Requires IEEE float32 round-to-nearest, no fast-math or FMA
// contraction. Caller adapts original steering coordinates exactly once.
OriginalControls conditionOriginalInputs(
    RawAnalog16 raw,
    OriginalInputCalibration calibration,
    OriginalInputMode mode,
    const OriginalInputConstants& constants,
    std::uint32_t passthroughBits);

}
