#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace idas3 {

// Semantic lift of GDS-0033 0C15E52E..0C15EA42. This is not wired into the
// experimental model. Validate it against the instruction oracle before use.
// Names containing offsets deliberately preserve unresolved original meaning.
struct OriginalTransmissionState {
    std::uint32_t gear00=0, snapshot04=0, snapshot08=0, rangeFlag0c=0;
    std::uint32_t untouched10=0;
    float target14=0, filtered18=0, tach1c=0, normalized20=0, delta24=0;
};
static_assert(sizeof(OriginalTransmissionState)==0x28);
static_assert(offsetof(OriginalTransmissionState,filtered18)==0x18);
static_assert(offsetof(OriginalTransmissionState,tach1c)==0x1c);
static_assert(offsetof(OriginalTransmissionState,delta24)==0x24);

struct OriginalTransmissionProfile {
    // Entire 88-byte record at 0C28825C + profileIndex*88, retained as bits.
    std::array<std::uint32_t,22> words{};
    float atByteOffset(std::size_t offset) const;
};

struct OriginalPowertrainRow {
    // Six original words at 0C28830C + rowIndex*24.
    std::uint32_t profileIndex=0, maximumGear=0;
    float base08=0, lower0c=0, upper10=0, divisor14=0;
};

struct OriginalTransmissionParameters {
    std::uint32_t profileIndex=0, maximumGear=0;
    float workingBase=0; // fr14; ordinary row +08 minus 500, override unchanged
    float lower=0, upper=0, divisor=0; // fr8, fr6, fr7
};

struct OriginalTransmissionDrive {
    float field080=0;
    std::uint32_t field12c=0, field130=0;
    float field220=0, field224=0, field228=0;
    float velocity238=0, delta240=0, selection248=0;
    std::uint32_t field400=0;
};

struct OriginalTransmissionGlobals {
    float loss9880=0;                  // 0CAA9880, mutable
    std::uint32_t previousGear988c=0;   // 0CAA988C, read-only in this stage
    float throttle9898=0;              // 0CAA9898, read-only
    float shiftDifference98a8=0;       // 0CAA98A8
    float coupledSnapshot98ac=0;       // 0CAA98AC
    float coupling98d0=0;              // 0CAA98D0
    std::uint32_t phase9870=0;          // 0CAA9870, signed conversion semantics
    std::uint32_t downCounter9cfc=0;    // 0CAA9CFC
    std::uint32_t flag91fb4c=0;         // 0C91FB4C
};

struct OriginalTransmissionInputs {
    std::uint8_t pressedByte=0;         // raw newly-pressed byte at 0C92ED40
    bool automaticMode=false;          // 0C9015C4 != 0; also controls down hold
    bool gearEnabled=true;             // bit15 of *[0C900954]+0x50
    float coefficientFr15=0;           // earlier solver output, not a fixed ratio
};

// Exact original sine boundary. Called only on the tach overshoot branch.
// A null callback fails closed if that branch is reached; std::sin is not
// silently substituted for the unvalidated original helper's behavior.
using OriginalTransmissionSine = float (*)(float argument, void* context);

OriginalTransmissionProfile decodeOriginalTransmissionProfile(std::span<const std::byte> record);
OriginalPowertrainRow decodeOriginalPowertrainRow(std::span<const std::byte> record);
OriginalTransmissionParameters selectOriginalTransmissionParameters(
    const OriginalPowertrainRow& ordinary,
    const OriginalPowertrainRow& overrideRow,
    bool overrideMode);

// Independently closed E52E..E6F6 block; contains no FMAC or math-service call.
void decideOriginalTransmissionGear(
    OriginalTransmissionState& state,
    OriginalTransmissionDrive& drive,
    OriginalTransmissionGlobals& globals,
    const OriginalTransmissionInputs& inputs,
    const OriginalTransmissionParameters& parameters,
    const OriginalTransmissionProfile& profile);

void stepOriginalTransmission(
    OriginalTransmissionState& state,
    OriginalTransmissionDrive& drive,
    OriginalTransmissionGlobals& globals,
    const OriginalTransmissionInputs& inputs,
    const OriginalTransmissionParameters& parameters,
    const OriginalTransmissionProfile& profile,
    OriginalTransmissionSine sine=nullptr,
    void* sineContext=nullptr);

} // namespace idas3
