#pragma once
#include <cstdint>

namespace idas3::original {
//202860 snapshots the retained SRAM counter and BIOS RTC baseline.2050A0
//publishes203440 every16 outer iterations, retrying next time if GD is busy.
//This is the platform counter used by AMiddleSeq's srand, not the RNG itself.
class OriginalPlatformClock {
public:
    void begin(std::uint32_t retained,std::uint32_t bootRtc){
        current_=base_=retained;bootRtc_=bootRtc;counter_=0;
    }
    bool tick(std::uint32_t rtc,bool disabled=false,bool busy=false){
        if(disabled)return false;
        ++counter_;
        if(counter_&15u)return false;
        if(busy){counter_=15;return false;}
        return publish(rtc);
    }
    //Exact batching of the ordinary ready/ enabled branch. Host suspension
    //must not require iterating millions of empty platform frames on resume.
    bool advanceReadyTicks(std::uint64_t ticks,std::uint32_t lastPublicationRtc){
        const bool publishDue=ticks>=16u-(counter_&15u);
        counter_+=std::uint32_t(ticks);
        return publishDue&&publish(lastPublicationRtc);
    }
    std::uint32_t current()const{return current_;}
    std::uint32_t base()const{return base_;}
    std::uint32_t counter()const{return counter_;}
private:
    bool publish(std::uint32_t rtc){
        const auto value=rtc-bootRtc_+base_;
        const bool changed=value!=current_;current_=value;return changed;
    }
    std::uint32_t current_=0,base_=0,bootRtc_=0,counter_=0;
};
}
