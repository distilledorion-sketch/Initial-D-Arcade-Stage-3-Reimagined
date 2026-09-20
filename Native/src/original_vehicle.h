#pragma once
#include "original_controls.h"
#include "original_dynamics.h"
#include "original_transmission.h"

namespace idas3::original {

struct OriginalVehicleState {
    OriginalDriveState drive;
    OriginalLossState loss;
    OriginalTailState tail;
    idas3::OriginalTransmissionState transmission;
    idas3::OriginalTransmissionGlobals transmissionGlobals;
    idas3::OriginalControls controls;
};

// Selected original table cells and live global tuning, assembled by the
// caller from the verified game data. No synthesized vehicle tuning defaults.
struct OriginalVehicleParameters {
    OriginalFrameParameters frame;
    OriginalAngularParameters angular;
    OriginalSteeringMemoryParameters steeringMemory;
    OriginalLossParameters loss;
    idas3::OriginalTransmissionParameters transmission;
    idas3::OriginalTransmissionProfile profile;
    OriginalRoadParameters road;
    idas3::OriginalInputConstants inputConstants=idas3::verifiedGds0033InputConstants();
};
struct OriginalVehicleInputs {
    idas3::RawAnalog16 analog{};
    idas3::OriginalInputCalibration calibration{};
    std::uint32_t suppressRawThrottle0C2F4BC8=0;
    std::uint8_t pressedByte=0;
    bool automaticMode=false,gearEnabled=false;
    std::int32_t elapsedFrames0C900E84=0;
};
struct OriginalVehicleStepResult {
    float frameCoefficient=0,motionScalar=0;
    // Original142520 request; the platform's original feedback-service policy
    // must consume this. It is not another force input or an arbitrary shake.
    OriginalAngularResult feedback;
};

// One original CEC0 invocation at its original frame cadence, pure typed C++.
// This is NOT the enclosing157AE0/158200/15B0A0 contact-and-surface pipeline.
// Caller must provide its state and selected original road/tuning data.
OriginalVehicleStepResult stepOriginalVehicle(OriginalVehicleState& state,
    const OriginalVehicleInputs& inputs,const OriginalVehicleParameters& parameters,
    OriginalMath math);

} // namespace idas3::original
