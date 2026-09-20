#include "original_vehicle.h"
#include <bit>

namespace idas3::original {
OriginalVehicleStepResult stepOriginalVehicle(OriginalVehicleState& s,
        const OriginalVehicleInputs& in,const OriginalVehicleParameters& p,OriginalMath math) {
    OriginalVehicleStepResult out;
    out.frameCoefficient=prepareOriginalFrame(s.drive,p.frame);
    s.controls=idas3::conditionOriginalInputs(in.analog,in.calibration,
        {in.suppressRawThrottle0C2F4BC8,s.drive.u(0x13C)},p.inputConstants,
        std::bit_cast<std::uint32_t>(s.transmission.filtered18));
    s.drive.setf(0x1C8,s.controls.steeringAlias);
    s.transmissionGlobals.coupledSnapshot98ac=std::bit_cast<float>(s.controls.passthroughBits);
    OriginalPreparedControls workingControls{s.controls.steering,s.controls.throttle,s.controls.brake};
    const auto preparation=prepareOriginalDriveState(s.drive,workingControls,out.frameCoefficient,p.road,math);
    s.controls.throttle=workingControls.throttle;
    if(preparation.setServiceFlag0C91FB40)s.tail.statistics0C91FB0C[13]=1;

    out.motionScalar=computeOriginalMotionScale(s.drive,p.angular);
    auto memoryParameters=p.steeringMemory;
    memoryParameters.shiftDownPressed=(in.pressedByte&0x10u)!=0;
    updateOriginalSteeringMemory(s.drive,memoryParameters);
    out.feedback=updateOriginalAngular(s.drive,p.angular,math,out.motionScalar);
    auto lossParameters=p.loss;
    lossParameters.normalizedBrake0CAA98A0=s.controls.brake;
    updateOriginalLongitudinalLoss(s.drive,s.loss,lossParameters);

    idas3::OriginalTransmissionDrive drive{
        s.drive.f(0x080),s.drive.u(0x12C),s.drive.u(0x130),
        s.drive.f(0x220),s.drive.f(0x224),s.drive.f(0x228),
        s.drive.f(0x238),s.drive.f(0x240),s.drive.f(0x248),s.drive.u(0x400)};
    s.transmissionGlobals.loss9880=s.loss.speedLoss0CAA9880;
    s.transmissionGlobals.previousGear988c=s.tail.previousGear0CAA988C;
    s.transmissionGlobals.throttle9898=s.controls.throttle;
    const idas3::OriginalTransmissionInputs transmissionInputs{
        in.pressedByte,in.automaticMode,in.gearEnabled,out.frameCoefficient};
    const auto sine=[](float argument,void* context){return static_cast<OriginalMath*>(context)->sinF32(argument);};
    idas3::stepOriginalTransmission(s.transmission,drive,s.transmissionGlobals,
        transmissionInputs,p.transmission,p.profile,sine,&math);
    s.drive.setf(0x220,drive.field220);s.drive.setf(0x238,drive.velocity238);
    s.drive.setf(0x240,drive.delta240);s.drive.setu(0x400,drive.field400);
    s.loss.speedLoss0CAA9880=s.transmissionGlobals.loss9880;
    finishOriginalDriveState(s.drive,s.loss,s.tail,
        {s.transmission.gear00,s.transmission.filtered18,s.transmission.delta24,
         s.transmissionGlobals.coupledSnapshot98ac,in.gearEnabled,in.elapsedFrames0C900E84},math);
    s.transmissionGlobals.loss9880=s.loss.speedLoss0CAA9880;
    s.transmissionGlobals.previousGear988c=s.tail.previousGear0CAA988C;
    return out;
}
} // namespace idas3::original
