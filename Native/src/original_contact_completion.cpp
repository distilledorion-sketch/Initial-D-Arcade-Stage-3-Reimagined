#include "original_contact_completion.h"
#include <stdexcept>

namespace idas3::original {
OriginalContactCompletionEffects finishOriginalContactFrame(OriginalDriveState& d,
    const idas3::OriginalTransmissionState& transmission,
    OriginalContactCompletionState& state,const OriginalContactCompletionInputs& inputs,
    const OriginalContactEngineOutput& engineOutput){
    OriginalContactCompletionEffects effects;
    effects.gear=transmission.gear00;
    effects.engineValue=transmission.tach1c>0.f?transmission.tach1c:0.f;
    effects.throttle=d.f(0x1B8);
    if(engineOutput)engineOutput(effects,state.randomSeed0C37C778);
    if(state.previousFlag0CAA94C4==0&&d.u(0x148)==1)d.setu(0x14C,10);
    auto timer=d.u(0x14C);
    if(std::bit_cast<std::int32_t>(timer)>0)--timer;
    if(std::bit_cast<std::int32_t>(timer)<0)timer=0;
    d.setu(0x14C,timer);
    state.cues0C900E5C[2]=0;state.cues0C900E5C[3]=0;
    if(timer==9){
        effects.requestCue4=true;
        const auto count=d.u(0x3F8)+1u;
        d.setu(0x3F8,count);
        if(std::bit_cast<std::int32_t>(count)<=98){
            if(state.positionCursor>=state.impactPositions.size()||state.frameCursor>=state.impactFrames.size())
                throw std::out_of_range("Original contact record cursor outside native backing");
            state.impactPositions[state.positionCursor++]={d.u(0),d.u(4),d.u(8)};
            state.impactFrames[state.frameCursor++]=inputs.frame0C92DE30;
        }
        if(d.f(0x1CC)>0.f)state.cues0C900E5C[2]=1;
        else state.cues0C900E5C[3]=1;
    }
    state.previousFlag0CAA94C4=d.u(0x148);
    state.cues0C900E5C[4]=((d.u(0x164)&15u)==4u||(d.u(0x168)&15u)==4u)?1u:0u;
    ++state.elapsedFrames0C900E84;
    state.randomSeed0C37C778=state.randomSeed0C37C778*1103515245u+12345u;
    const auto random=(state.randomSeed0C37C778>>16)&0x7fffu;
    state.steeringMask0C900EBC+=random&3u;
    if((inputs.digitalByte0C92ED00&0x80u)!=0){
        for(std::size_t i=0;i<6;++i)state.snapshot0CAA9718[i]=d.u(i*4);
        state.snapshot0CAA9718[6]=d.u(0x1B8);
        state.snapshot0CAA9718[7]=d.u(0x1C4);
    }
    return effects;
}
}
