#include "original_host_input.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace idas3::original {
OriginalHostControls controllerHostControls(std::int16_t steeringAxis,
    std::int16_t pairedAxis,std::uint8_t throttle,std::uint8_t brake,
    ControllerResponse response,float steeringDeadzone){
    if(!std::isfinite(steeringDeadzone)||(steeringDeadzone!=-1.f&&(steeringDeadzone<0||steeringDeadzone>.3f)))
        throw std::invalid_argument("Steering deadzone must be0..0.3 or the profile default");
    const float deadzone=steeringDeadzone<0?(response==ControllerResponse::Previous?.13f:
        response==ControllerResponse::FlycastGamepad?.1f:0.f):steeringDeadzone;
    if(response==ControllerResponse::Previous){
        float axis=steeringAxis/32767.f;
        axis=std::abs(axis)<deadzone?0:std::copysign((std::abs(axis)-deadzone)/(1.f-deadzone),axis);
        return {std::clamp(axis,-1.f,1.f),throttle/255.f,brake/255.f};
    }
    if(response!=ControllerResponse::FlycastGamepad&&response!=ControllerResponse::FlycastWheel)
        throw std::invalid_argument("Unknown controller response");
    int axis=steeringAxis;
    if(response==ControllerResponse::FlycastGamepad||deadzone>0){
        const float magnitude=std::abs(steeringAxis/32768.f);
        const float other=response==ControllerResponse::FlycastGamepad?pairedAxis/32768.f:0.f;
        const float radiusSquared=magnitude*magnitude+other*other;
        if(radiusSquared==0||radiusSquared<deadzone*deadzone)axis=0;
        else{
            const float projectedDeadZone=magnitude*deadzone/std::sqrt(radiusSquared);
            axis=int(std::round((magnitude-projectedDeadZone)/(1.f-projectedDeadZone)*32768.f));
            if(steeringAxis<0)axis=-axis;
            axis=std::clamp(axis,-32768,32767);
        }
    }
    // JVS covers the whole unsigned ADC range, rather than pre-scaling the
    // controller into the game's narrower calibrated active range. Signed
    // low-byte wire compensation has already been reversed by the guest.
    const int steerByte=std::min(axis+32768,0xff7f)>>8;
    const auto pedal=[](std::uint8_t value){
        // Preserve the host's 8-bit trigger resolution. Flycast receives a
        // 15-bit half axis; quantize to it before its JVS conversion.
        int analog=int(std::lround(value*(32767.f/255.f)))*2;
        if(analog>=0x8000&&analog<0x8100)analog=0x8100;
        return std::clamp(((std::min(analog,0xff7f)>>8)-64)/107.f,0.f,1.f);
    };
    return {std::clamp((steerByte-128)/80.f,-1.f,1.f),pedal(throttle),pedal(brake)};
}
OriginalVehicleInputs adaptOriginalHostInput(OriginalHostInputState& state,
    const OriginalHostControls& controls,bool automatic,bool gearEnabled,
    std::int32_t elapsedFrames){
    if(!std::isfinite(controls.steering)||!std::isfinite(controls.throttle)||!std::isfinite(controls.brake))
        throw std::invalid_argument("Native driving controls must be finite");
    OriginalVehicleInputs out;
    // Physical right is positive ADC displacement; original conditioning
    // negates it into the original steering axis. With the original RH chase
    // projection, that negative original steering turns right on screen.
    const int steering=128+int(std::lround(std::clamp(controls.steering,-1.f,1.f)*80.f));
    // Original159AE0 defaults are{128,32,32}; inverse pedal conditioning
    // therefore starts at raw high-byte64, then spans107 original steps.
    const auto pedal=[](float value){return 64+int(std::lround(std::clamp(value,0.f,1.f)*107.f));};
    out.analog={std::uint16_t(steering<<8),std::uint16_t(pedal(controls.throttle)<<8),std::uint16_t(pedal(controls.brake)<<8)};
    out.calibration={128,32,32};
    const auto held=std::uint8_t((controls.shiftDown?0x10u:0u)|(controls.shiftUp?0x20u:0u));
    out.pressedByte=held&std::uint8_t(~state.previousShiftButtons);
    state.previousShiftButtons=held;
    out.automaticMode=automatic;out.gearEnabled=gearEnabled;
    out.elapsedFrames0C900E84=elapsedFrames;
    return out;
}
}
