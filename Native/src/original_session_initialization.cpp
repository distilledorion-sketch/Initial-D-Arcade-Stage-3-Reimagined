#include "original_session_initialization.h"
#include <algorithm>

namespace idas3::original {
OriginalSessionDefaults verifiedOriginalSessionDefaults() {
    // Exact37 words at270B08 in the canonical4MiB image, SHA256
    // efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335.
    // The differential test checks every word against the independent image.
    return {{
        0,0,0,0,0,0x0C92DFC4,0x0C9032CC,0x0C92DCA0,
        0,0x0C91155C,0x0C92DE34,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0x0C91FBA0,0x0C911A0C,0x0C90345C
    }};
}

OriginalInitializationResult initializeOriginalSession(OriginalVehicleState& vehicle,
        OriginalVehicleParameters& parameters,OriginalInitializationSideState& side,
        OriginalActorState& actor,OriginalRoadContactState& road,
        OriginalSessionInitializationState& session,const OriginalInitializationInputs& inputs,
        const OriginalSessionDefaults& defaults) {
    session.activeActorIdentity0C900954=0x0C9008A4;
    actor.setu(0x54,0);
    session.elapsedFrames0C900E84=0;
    session.steeringMask0C900EBC=0;
    for(std::size_t i=0;i<3;++i)side.actorPosition[i]=actor.f(i*4);
    side.actorFlags50=actor.u(0x50);
    const auto result=initializeOriginalVehicle(vehicle,parameters,side,inputs);
    for(std::size_t i=0;i<3;++i)actor.setf(i*4,side.actorPosition[i]);
    actor.setu(0x50,side.actorFlags50);
    for(const auto offset:{0x2AC,0x2B8,0x2C4,0x2D0})vehicle.drive.setf(offset,inputs.position[1]);
    for(const auto offset:{0x338,0x33C,0x340,0x344,0x13C,0x12C,0x130,0x134,0x138})
        vehicle.drive.setu(offset,0);
    for(std::size_t corner=0;corner<4;++corner){
        clearOriginalCollisionQuery(road.surfaces0CAA9518[corner]);
        clearOriginalCollisionQuery(road.sweeps0CAA9618[corner]);
    }
    std::copy_n(defaults.words0C270B08.begin(),16,vehicle.tail.statistics0C91FB0C.begin());
    vehicle.transmissionGlobals.flag91fb4c=defaults.words0C270B08[16];
    std::copy_n(defaults.words0C270B08.begin()+17,20,session.statisticsRemainder0C91FB50.begin());
    side.otherGlobals[17]=vehicle.tail.statistics0C91FB0C[13]; //91FB40 shared alias
    session.previousFlag0CAA94C4=0;
    session.state0C31FD44=0;
    return result;
}
} // namespace idas3::original
