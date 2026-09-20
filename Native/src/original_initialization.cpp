#include "original_initialization.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace idas3::original {
OriginalInitializationResult initializeOriginalVehicle(OriginalVehicleState& s,
        OriginalVehicleParameters& p,OriginalInitializationSideState& side,
        const OriginalInitializationInputs& in) {
    if(in.throttleHistoryCount0C285098>s.tail.throttleHistory0CAA99E0.size())
        throw std::invalid_argument("Original throttle history exceeds its decoded storage");
    p.angular.global0C900E40=p.angular.global0C8FF380=p.angular.global0C900EC0=
        p.angular.global0C900E54=1.0f;
    p.steeringMemory.global0C900E4C=p.steeringMemory.global0C90094C=
        p.steeringMemory.global0C900EB0=p.steeringMemory.global0C900E30=
        p.steeringMemory.global0C900EF8=1.0f;
    side.contactMultipliers.fill(1.0f);
    side.otherGlobals.fill(0);
    side.actorPosition=in.position;
    side.actorFlags50=(side.actorFlags50&~63u)|(in.vehicleIndex0C901654&63u);
    for(std::size_t i=0;i<3;++i){s.drive.setf(i*4,in.position[i]);s.drive.setf(0xC+i*4,in.angles[i]);}
    // Writes are deliberately enumerated. Clearing the object would destroy
    // contact/material state that this original initializer leaves untouched.
    constexpr std::array zeroOffsets{
        0x024,0x028,0x02C,0x030,0x034,0x038,0x078,0x094,0x098,0x09C,0x0A0,
        0x0D8,0x0DC,0x0E0,0x0E4,0x0EC,0x0F4,0x118,0x140,0x144,
        0x174,0x178,0x17C,0x180,0x184,0x188,0x18C,0x190,0x198,0x19C,
        0x1A8,0x1AC,0x1B0,0x1B4,0x218,0x224,0x228,0x22C,0x230,0x234,
        0x238,0x23C,0x244,0x248,0x258,0x25C,0x260,0x264,0x274,0x278,
        0x27C,0x284,0x288,0x290,0x294,0x298,0x29C,0x2A0,0x2A4,
        0x378,0x37C,0x380,0x384,0x388,0x38C,0x390,0x394,0x398,0x39C,0x3A0,0x3A4,
        0x3E4,0x3E8,0x3EC,0x3F0,0x3F4,0x3F8,0x3FC,0x400,0x404,0x408,0x40C};
    for(const auto offset:zeroOffsets)s.drive.setu(offset,0);
    for(const auto offset:{0xC4,0xC8,0xCC,0xD0})s.drive.setf(offset,in.position[1]);
    for(const auto offset:{0x108,0x10C,0x110})s.drive.setf(offset,in.angles[1]);
    s.drive.setu(0x114,in.vehicleType0C284EF4);
    s.drive.setu(0x1C0,in.throttleHistoryCount0C285098);
    side.randomSeed0C37C778=side.randomSeed0C37C778*1103515245u+12345u;
    const auto randomValue=(side.randomSeed0C37C778>>16)&0x7FFFu;
    s.drive.setf(0x21C,std::fma(float(randomValue),std::bit_cast<float>(0x3C23D70Au),
        std::bit_cast<float>(0x3A83126Fu)));
    s.drive.setu(0x434,in.mode0C9015FC!=0);
    s.drive.setu(0x438,in.mode0C9015C0!=0);
    s.transmission.gear00=1;s.transmission.target14=0;
    s.transmission.filtered18=0;s.transmission.tach1c=0;
    s.transmissionGlobals.phase9870=0;s.transmissionGlobals.loss9880=0;
    s.transmissionGlobals.shiftDifference98a8=0;s.transmissionGlobals.coupling98d0=0;
    s.transmissionGlobals.downCounter9cfc=0;s.transmissionGlobals.flag91fb4c=0;
    s.controls.throttleAlias=0;
    s.loss.speedLoss0CAA9880=s.loss.persistentPenalty0CAA9884=0;
    s.tail.previousSpeed0CAA9874=0;s.tail.speedDelta0CAA9878=0;
    s.tail.counter0CAA9CE4=0;s.tail.counter0CAA9CE8=0;
    s.tail.history0CAA98E0.fill(0);
    std::fill_n(s.tail.throttleHistory0CAA99E0.begin(),in.throttleHistoryCount0C285098,0.0f);
    s.tail.steeringHistory0CAA9BE0.fill(0);
    s.tail.statistics0C91FB0C[13]=0;
    p.steeringMemory.mask0CAA9CF0=in.modeMask0C283E08;
    p.frame.global0CAA987C=0;
    return {};
}
} // namespace idas3::original
