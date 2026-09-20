#include "original_controls.h"
#include <bit>
#include <limits>

namespace idas3 {
namespace {
static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559,
              "Original controls require IEEE binary32");
float asFloat(std::uint32_t bits) { return std::bit_cast<float>(bits); }
float negateBits(float value) { return asFloat(std::bit_cast<std::uint32_t>(value)^0x80000000u); }
float absoluteBits(float value) { return asFloat(std::bit_cast<std::uint32_t>(value)&0x7fffffffu); }
std::int32_t signedWord(std::uint32_t word) { return std::bit_cast<std::int32_t>(word); }

// Deliberately use the original comparisons, rather than std::clamp, to retain
// ordered-comparison behavior for NaN and the sign of zero.
float clampPedal(float value) {
    if(0.0f>value) return 0.0f;
    if(value>1.0f) return 1.0f;
    return value;
}
}

OriginalControls conditionOriginalInputs(
    RawAnalog16 raw, OriginalInputCalibration calibration, OriginalInputMode mode,
    const OriginalInputConstants& constants, std::uint32_t passthroughBits) {
    OriginalControls out;
    const float negativeSign=asFloat(constants.negativeSignBits);
    const float steeringDivisor=asFloat(constants.steeringDivisorBits);
    const float pedalDivisor=asFloat(constants.pedalDivisorBits);
    const float steeringScale=asFloat(constants.steeringScaleBits);

    // Original loads sign-extend MEM16, mask to 16 bits, then shift right 8.
    // Taking the upper byte of an explicit uint16 word gives the same value.
    out.steeringHighByte=std::uint32_t(raw.steering)>>8;
    out.throttleHighByte=std::uint32_t(raw.throttle)>>8;
    out.brakeHighByte=std::uint32_t(raw.brake)>>8;
    out.throttleHighByteAfterMode=mode.suppressRawThrottleWord?0u:out.throttleHighByte;

    // 0C15CF88..0C15CFB4: preserve unsigned wrap followed by signed tests and
    // FLOAT FPUL's signed conversion. Each add/sub corresponds to an opcode.
    std::uint32_t steeringWord=out.steeringHighByte;
    steeringWord+=static_cast<std::uint32_t>(static_cast<std::int32_t>(constants.steeringIntegerAddend));
    steeringWord-=calibration.steeringWord;
    std::uint32_t throttleWord=out.throttleHighByteAfterMode;
    throttleWord+=32u;
    throttleWord-=calibration.throttleWord;
    std::uint32_t brakeWord=out.brakeHighByte;
    brakeWord+=32u;
    steeringWord-=128u;
    brakeWord-=calibration.brakeWord;
    throttleWord-=64u;
    brakeWord-=64u;
    out.steeringInteger=signedWord(steeringWord);
    out.throttleIntegerBeforeFloor=signedWord(throttleWord);
    out.brakeIntegerBeforeFloor=signedWord(brakeWord);
    if(out.throttleIntegerBeforeFloor<0) throttleWord=0;
    if(out.brakeIntegerBeforeFloor<0) brakeWord=0;
    out.throttleInteger=signedWord(throttleWord);
    out.brakeInteger=signedWord(brakeWord);

    // 0C15CFB4..0C15CFD6: select sign before dividing and saturate magnitude.
    const float steeringInteger=static_cast<float>(out.steeringInteger);
    float signedMagnitude=1.0f;
    if(0.0f>steeringInteger) signedMagnitude=negativeSign;
    float magnitude=steeringInteger;
    magnitude/=steeringDivisor;
    magnitude=absoluteBits(magnitude);
    if(1.0f>magnitude) signedMagnitude*=magnitude;
    out.steeringBeforeScale=signedMagnitude;

    // 0C15CFD6..0C15D004: retain the comparison and second division performed
    // by each original pedal branch. No extra lower clamp occurs until publish.
    const float throttleInteger=static_cast<float>(out.throttleInteger);
    float throttleRatio=throttleInteger;
    throttleRatio/=pedalDivisor;
    float throttleValue=throttleInteger;
    if(1.0f>throttleRatio) throttleValue/=pedalDivisor;
    else throttleValue=1.0f;
    const float brakeInteger=static_cast<float>(out.brakeInteger);
    float brakeRatio=brakeInteger;
    brakeRatio/=pedalDivisor;
    float brakeValue=1.0f;
    if(1.0f>brakeRatio) { brakeValue=brakeInteger;brakeValue/=pedalDivisor; }

    // 0C15D004..0C15D03C: optional scale precedes original-coordinate FNEG.
    if(mode.scaleSteeringWord) signedMagnitude*=steeringScale;
    float steeringValue=negateBits(signedMagnitude);
    out.steeringAlias=steeringValue;
    out.throttleAlias=throttleValue;
    out.brakeBeforeClamp=brakeValue;
    out.passthroughBits=passthroughBits;

    // 0C15D03C..L_0C15D124: only the three global outputs are overwritten by
    // clamps. The drive steering alias and throttle alias retain earlier bits.
    if(negativeSign>steeringValue) steeringValue=negativeSign;
    else if(steeringValue>1.0f) steeringValue=1.0f;
    out.steering=steeringValue;
    out.throttle=clampPedal(throttleValue);
    out.brake=clampPedal(brakeValue);
    return out;
}
}
