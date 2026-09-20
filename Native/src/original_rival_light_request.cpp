#include "original_rival_light_request.h"
#include <bit>

namespace idas3::original {
OriginalRivalLightRequest advanceOriginalRivalLightRequest(
    OriginalRivalLightState& state,const OriginalRivalLightInputs& input){
    if(input.numericRaceMode==2||input.numericRaceMode==3)
        return OriginalRivalLightRequest::Hold;
    if(input.numericRaceMode==1)
        return (input.selectedActorFlags80&0x10000u)?OriginalRivalLightRequest::Off:OriginalRivalLightRequest::On;
    if(input.profileMode!=0||input.enemy!=29)
        return OriginalRivalLightRequest::Hold;
    constexpr float lower=std::bit_cast<float>(0xc099999au);
    if(input.signedAdvantage100>=lower&&input.signedAdvantage100<20.f)
        ++state.frames1732;
    else state.frames1732=0;
    return std::bit_cast<std::int32_t>(state.frames1732)>240
        &&input.playerProgress1460>1439&&input.playerProgress1460<=2249
        ?OriginalRivalLightRequest::Off:OriginalRivalLightRequest::On;
}
}
