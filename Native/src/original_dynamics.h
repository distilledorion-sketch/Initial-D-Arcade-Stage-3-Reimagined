#pragma once
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

namespace idas3::original {

// Typed storage for ONE original drive object, not a guest address space or CPU.
// Offsets retain conservative names until every physical meaning is proven.
struct OriginalDriveState {
    static constexpr std::size_t byteSize = 0x500;
    std::array<std::uint32_t, byteSize / 4> words{};
    float f(std::size_t offset) const { return std::bit_cast<float>(words.at(offset / 4)); }
    void setf(std::size_t offset, float value) { words.at(offset / 4) = std::bit_cast<std::uint32_t>(value); }
    std::uint32_t u(std::size_t offset) const { return words.at(offset / 4); }
    void setu(std::size_t offset, std::uint32_t value) { words.at(offset / 4) = value; }
};

struct OriginalFrameParameters {
    // FULL conditionCode*140 + vehicleIndex*4; unlike angular rows, no /2.
    float table0C28451C = 0;
    float table0C28866C = 0; // selected by global0C9015F0*4
    float global0CAA987C = 0;
    std::uint32_t global0C9015E4 = 0;
    std::uint32_t global0C9015D4 = 0;
    float global0C901650 = 0;
};

// CEC0..CF84 frame parameter producer. Returns live fr15 for later speed
// normalization/transmission, publishes +250 and clears +214/+218. Controls
// acquired in that interval are independently handled by original_controls.
float prepareOriginalFrame(OriginalDriveState& drive,const OriginalFrameParameters& parameters);

// Every value is an actual selected original table cell or live original global.
// No default tuning values are supplied. Table index is
// truncTowardZero(conditionCode / 2)*140 + vehicleIndex*4 for the tables below.
struct OriginalAngularParameters {
    float carRecord0C283F18_08 = 0;
    float carRecord0C283F18_0C = 0;
    float table0C285124 = 0;
    float global0C900E40 = 0;
    float table0C285AFC = 0;
    float global0C900EC0 = 0;
    float table0C285610 = 0;
    float global0C8FF380 = 0;
    float table0C287398 = 0;
    float table0C285FE8 = 0;
    float global0C900E54 = 0;
};

// Supply the validated original math implementation. A std::sin/cos adapter is
// useful for local characterization but does not establish SH-4 bit parity.
struct OriginalMath {
    float (*sinF32)(float) = nullptr;
    float (*cosF32)(float) = nullptr;
    // 0C1F6B80 uses SH-4 FIPR, whose rounding must be separately validated.
    float (*dot3F32)(const std::array<float,3>&,const std::array<float,3>&) = nullptr;
};

// Finite normal-FPSCR FIPR contract verified in the local Flycast primary
// interpreter: ordered double products/sum, one final F32 rounding. This is
// separate from physical SH-4 silicon certification and exceptional NaN modes.
float originalFiprDot3(const std::array<float,3>& a,const std::array<float,3>& b);

struct OriginalPreparedControls { float steering=0,throttle=0,brake=0; };
struct OriginalRoadParameters {
    std::uint32_t condition0C9015CC=0;
    std::uint32_t mode0C9015E0=0;
    // Actual pointer0C901728 data, not a substituted rendering centerline.
    // Original accesses inclusive lastIndex before aliasing it back to index0.
    std::span<const std::array<float,3>> path0C901728;
    std::uint32_t lastIndex0C283E88=0;
};
struct OriginalPreparationResult { bool setServiceFlag0C91FB40=false; };

// [0C15D124,0C15D922): nonlinear controls, normalized speed, original contact
// projection/mode state and bounded 128-point path search. Existing contact
// vectors and flags must be produced by the original upstream stages.
// The throttle global is conditionally zeroed and returned through controls.
OriginalPreparationResult prepareOriginalDriveState(OriginalDriveState& drive,
    OriginalPreparedControls& controls,float frameCoefficientFr15,
    const OriginalRoadParameters& road,OriginalMath math);

struct OriginalSteeringMemoryParameters {
    float table0C2864D4 = 0;
    float global0C90094C = 0;
    float table0C286EAC = 0;
    float global0C900E30 = 0;
    float table0C287884 = 0;
    float global0C900EF8 = 0;
    float table0C2869C0 = 0;
    float global0C900E4C = 0;
    float table0C287D70 = 0;
    float global0C900EB0 = 0;
    std::uint32_t mask0C900EBC = 0;
    std::uint32_t mask0CAA9CF0 = 0;
    bool shiftDownPressed = false;
};

// Original shift/brake/lift/sign-change contributions, memory transfer and
// limits at 0C15D9EA..0C15DEA8. Call after retaining the motion scalar and before
// updateOriginalAngular. Table entries/globals come from the exact live case.
void updateOriginalSteeringMemory(OriginalDriveState& drive,
    const OriginalSteeringMemoryParameters& parameters);

struct OriginalAngularResult {
    float motionScale = 0;          // original live fr6, built at 0C15D91C..D9E6
    float steeringCorrection = 0;   // old +284 / 55 times the original state max
    float primaryContribution = 0;  // +D8, before retention of +DC
    float directionError = 0;       // +274
    // Original side-effect call at 0C15E32E is exposed, not silently discarded.
    std::uint32_t feedbackArgument = 0;
    float feedbackStrength = 0;
    float feedbackSpeed = 0; //1596E0 drive+238 *3.6, before transmission
};

// Call at the original 0C15D922 boundary. Do not recompute this after later
// parameter stages may have modified its input fields.
float computeOriginalMotionScale(const OriginalDriveState& drive,
    const OriginalAngularParameters& parameters);

// Instruction-derived lift of original scalar angular stages [0C15DEA8,0C15E370).
// Caller must supply the retained motion scalar and state after the
// preceding original parameter/contact stages. It does NOT derive those fields
// with an invented tire model and is not yet a standalone full-car solver.
OriginalAngularResult updateOriginalAngular(
    OriginalDriveState& drive,
    const OriginalAngularParameters& parameters,
    OriginalMath math,
    float motionScaleFromD9E6);

struct OriginalLossState {
    float speedLoss0CAA9880 = 0;
    float persistentPenalty0CAA9884 = 0;
};

struct OriginalLossParameters {
    std::uint32_t condition0C9015CC = 0;
    float normalizedBrake0CAA98A0 = 0;
    // Vehicle-indexed pairs: cap at base+8*index, growth at base+8*index+4.
    float cap0C284F80 = 0;
    float growth0C284F84 = 0;
};

// Original accumulation and persistent-penalty decay, [0C15E370,0C15E4E6).
// The later transmission stage owns subtraction of speedLoss from +238.
void updateOriginalLongitudinalLoss(
    OriginalDriveState& drive,
    OriginalLossState& loss,
    const OriginalLossParameters& parameters);

struct OriginalTailState {
    std::array<float,64> history0CAA98E0{};
    std::array<float,128> throttleHistory0CAA99E0{};
    std::array<float,64> steeringHistory0CAA9BE0{};
    std::uint32_t counter0CAA9CE4=0,counter0CAA9CE8=0,counter0CAA9CEC=0;
    float mean0CAA9CE0=0;
    float previousSpeed0CAA9874=0,speedDelta0CAA9878=0;
    float filteredDelta0CAA98B0=0;
    std::uint32_t lastNonzeroGear0CAA9888=0,previousGear0CAA988C=0;
    // Preserve unwritten statistics/service fields; this is an output record.
    std::array<std::uint32_t,16> statistics0C91FB0C{};
};
struct OriginalTailInputs {
    std::uint32_t gear=0;
    float transmissionFiltered18=0,transmissionDelta24=0;
    float priorFiltered0CAA98AC=0;
    bool gearEnabled=false; // bit15 of object*[0C900954]+0x50
    std::int32_t elapsedFrames0C900E84=0;
};

// [0C15EA42, original CEC0 return), including called 0C15ECE0. Preserves all
// history rings, throttle average, exact position updates and statistics.
// Upstream contact offsets +258/+25C/+260/+264 are required, not synthesized.
void finishOriginalDriveState(OriginalDriveState& drive,OriginalLossState& loss,
    OriginalTailState& tail,const OriginalTailInputs& inputs,OriginalMath math);

} // namespace idas3::original
