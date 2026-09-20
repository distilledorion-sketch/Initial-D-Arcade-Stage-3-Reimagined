#pragma once
#include <cstdint>

namespace idas3 {
// ATimeExtension: constructor 05AEE0 receives 120 at 06872A;
// 05B0E0 decrements the lifetime and requests message priority 2.
// Timer refill: 065642..065698 adds 6000 per tick, cue 1 every fifth tick.
struct OriginalRaceFeedback {
    unsigned extensionTicks=0,refillFrame=0;
    std::int32_t displayedRemaining=0;
    bool refilling=false;
    void reset(std::int32_t remaining){*this={};displayedRemaining=remaining;}
    void extend(std::int32_t before){extensionTicks=120;displayedRemaining=before;refillFrame=0;refilling=true;}
    bool tick(std::int32_t remaining){
        if(extensionTicks)--extensionTicks;
        if(!refilling){displayedRemaining=remaining;return false;}
        const bool cue=refillFrame%5==0;++refillFrame;
        const auto next=std::int64_t(displayedRemaining)+6000;
        if(next>=remaining){displayedRemaining=remaining;refilling=false;}
        else displayedRemaining=std::int32_t(next);
        return cue;
    }
};
}
