#include "original_race_start.h"
#include <stdexcept>

namespace idas3::original {
void OriginalRaceStart::reset(std::uint32_t mode){
    if(mode!=0&&mode!=2)throw std::invalid_argument("Original local race start supports numeric modes0/2");
    remaining_=240;mode_=mode;started_=false;
}
OriginalRaceStartFrame OriginalRaceStart::step(){
    OriginalRaceStartFrame out;
    if(remaining_){
        --remaining_;
        if(remaining_){
            out.countdownDigit=std::int32_t(remaining_/60);
            out.cue2=remaining_%60==59;
            out.requestService1=remaining_==70;
            if(remaining_==60){started_=true;out.go=true;out.cue3=true;}
        }
    }
    out.countdownRemaining=remaining_;
    out.gearEnabled=out.runRules=started_;
    return out;
}
void warmupOriginalRaceSession(OriginalDrivingSession& session,
    const OriginalVehicleInputs& incoming,std::uint32_t platformFrame,
    std::uint8_t digital,const std::function<void(const OriginalDrivingStepEffects&)>& publish){
    auto inputs=incoming;inputs.gearEnabled=false;
    for(unsigned i=0;i<60;++i){
        session.setPlatformFrame(platformFrame,digital);
        const auto effects=session.tick(inputs);
        if(publish)publish(effects);
    }
    // No outer platform tick occurs in062100's loop. tick()'s ordinary host
    // convenience increment is not allowed to escape the initialization loop.
    session.setPlatformFrame(platformFrame,digital);
}
} // namespace idas3::original
