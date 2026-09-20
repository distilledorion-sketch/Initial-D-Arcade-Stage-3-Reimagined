#pragma once
#include "original_vehicle.h"

namespace idas3::original {
struct OriginalHostControls {
    // Native coordinates: positive steering turns right; pedals in[0,1].
    float steering=0,throttle=0,brake=0;
    bool shiftDown=false,shiftUp=false;
};
struct OriginalHostInputState {std::uint8_t previousShiftButtons=0;};
enum class ControllerResponse { FlycastGamepad=0, Previous=1, FlycastWheel=2 };
// Host-only response, before the calibrated cabinet boundary. The Flycast
// profiles reproduce v2.5 JVS axis ranges with default game calibration;
// gamepad uses a 10% radial dead zone, wheel uses no dead zone.
OriginalHostControls controllerHostControls(std::int16_t steeringAxis,
    std::int16_t pairedAxis,std::uint8_t throttle,std::uint8_t brake,
    ControllerResponse response,float steeringDeadzone=-1.f);
// Virtual calibrated cabinet boundary. This supplies the original solver's
// exact8-bit ADC representation without requiring cabinet hardware/settings.
// Host keyboard/stick filtering belongs before this boundary. It does not
// alter the original solver or claim a particular physical wheel calibration.
OriginalVehicleInputs adaptOriginalHostInput(OriginalHostInputState& state,
    const OriginalHostControls& controls,bool automatic,bool gearEnabled,
    std::int32_t elapsedFrames);
}
