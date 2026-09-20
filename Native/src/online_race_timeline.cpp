#include "online_race_timeline.h"
#include "original_host_input.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>

namespace idas3::original {
namespace {
bool equal(const OriginalVehicleInputs& a,const OriginalVehicleInputs& b){
    return a.analog.steering==b.analog.steering&&a.analog.throttle==b.analog.throttle&&a.analog.brake==b.analog.brake&&
        a.calibration.steeringWord==b.calibration.steeringWord&&a.calibration.throttleWord==b.calibration.throttleWord&&
        a.calibration.brakeWord==b.calibration.brakeWord&&a.pressedByte==b.pressedByte&&a.suppressRawThrottle0C2F4BC8==b.suppressRawThrottle0C2F4BC8;
    // Transmission mode and GO enable are owned by race setup/countdown.
}
bool valid(const OriginalVehicleInputs& input){
    // The native host adapter produces this fixed calibrated ADC boundary.
    return input.calibration.steeringWord==128&&input.calibration.throttleWord==32&&input.calibration.brakeWord==32&&
        !input.suppressRawThrottle0C2F4BC8&&(input.pressedByte&~0x30u)==0;
}
}
OnlineRaceTimeline::OnlineRaceTimeline(OnlineRaceSimulation& simulation,std::uint64_t race,bool host,ConfirmedOutput output)
    :simulation_(simulation),race_(race),host_(host),output_(std::move(output)){
    if(!race||simulation.frame()!=0)throw std::invalid_argument("Timeline requires a fresh synchronized race");
    for(unsigned slot=0;slot<2;++slot){OriginalHostInputState hostInput;previousConfirmed_[slot]=adaptOriginalHostInput(hostInput,{},simulation.setup().automatic[slot],false,0);}
}
OnlineRaceTimeline::Entry& OnlineRaceTimeline::entry(std::uint64_t frame){
    auto& value=entries_[frame%capacity];if(value.frame!=frame){value=Entry{};value.frame=frame;}return value;
}
bool OnlineRaceTimeline::localInput(const OriginalVehicleInputs& input){
    if(!valid(input)||next_-confirmed_>=capacity)return false;
    const unsigned slot=host_?0:1;auto& e=entry(next_);
    if(e.known[slot]&&!equal(e.received[slot],input))return false;
    e.known[slot]=true;e.authoritative[slot]=host_;e.received[slot]=input;return true;
}
bool OnlineRaceTimeline::receive(std::span<const OnlineInputRecord> records){
    auto reject=[&](){++metrics_.rejectedBatches;return false;};
    if(records.size()>capacity*2)return reject();
    const auto oldest=next_>capacity?next_-capacity:0;
    for(std::size_t i=0;i<records.size();++i){const auto& r=records[i];
        if(r.race!=race_||r.slot>1||(host_&&r.slot!=1)||r.frame>=confirmed_+capacity||!valid(r.input))return reject();
        if(r.frame<oldest)continue;
        const auto& e=entries_[r.frame%capacity];
        if(e.frame==r.frame&&e.known[r.slot]&&!equal(e.received[r.slot],r.input))return reject();
        for(std::size_t j=0;j<i;++j)if(records[j].frame==r.frame&&records[j].slot==r.slot&&!equal(records[j].input,r.input))return reject();
    }
    for(const auto& r:records){
        if(r.frame<oldest||r.frame<confirmed_)continue;
        auto& e=entry(r.frame);
        if(!e.known[r.slot]&&r.frame<next_&&!equal(e.used[r.slot],r.input))dirty_=std::min(dirty_,r.frame);
        e.known[r.slot]=e.authoritative[r.slot]=true;e.received[r.slot]=r.input;
    }
    return true;
}
void OnlineRaceTimeline::simulate(std::uint64_t frame){
    auto& e=entry(frame);e.before=simulation_.checkpoint();
    for(unsigned slot=0;slot<2;++slot){
        if(e.known[slot])e.used[slot]=e.received[slot];
        else {e.used[slot]=frame==confirmed_?previousConfirmed_[slot]:entries_[(frame-1)%capacity].used[slot];e.used[slot].pressedByte=0;}
    }
    e.effects=simulation_.step(e.used);e.digest=simulation_.digest();
}
void OnlineRaceTimeline::confirm(){
    while(confirmed_<next_){auto& e=entries_[confirmed_%capacity];
        if(e.frame!=confirmed_||!e.authoritative[0]||!e.authoritative[1])break;
        previousConfirmed_=e.used;
        // Confirm exactly once. Replayed speculative frames never emit here.
        confirmedDigest_=e.digest;++confirmed_;++metrics_.confirmedEffects;if(output_)output_(e.effects,e.digest);
    }
}
void OnlineRaceTimeline::reconcile(){
    if(dirty_!=absent){
        const auto start=std::chrono::steady_clock::now();
        std::array<OriginalActorState,2> before{simulation_.car(0).actor(),simulation_.car(1).actor()};
        simulation_.restore(entries_[dirty_%capacity].before);
        const auto depth=unsigned(next_-dirty_);
        for(auto f=dirty_;f<next_;++f)simulate(f);
        ++metrics_.rollbacks;metrics_.replayedFrames+=depth;metrics_.maxDepth=std::max(metrics_.maxDepth,depth);
        for(unsigned slot=0;slot<2;++slot){double squared=0;for(unsigned offset:{0u,4u,8u}){double delta=before[slot].f(offset)-simulation_.car(slot).actor().f(offset);squared+=delta*delta;}
            metrics_.maxCorrection=std::max(metrics_.maxCorrection,std::sqrt(squared));}
        metrics_.maxReplayMs=std::max(metrics_.maxReplayMs,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());dirty_=absent;
    }
    confirm();
}
bool OnlineRaceTimeline::advance(){
    reconcile();if(next_-confirmed_>=capacity){++metrics_.stalls;return false;}
    if(!entries_[next_%capacity].known[host_?0:1]||entries_[next_%capacity].frame!=next_)throw std::logic_error("Local input must be sampled before advancing");
    simulate(next_++);confirm();return true;
}
std::vector<OnlineInputRecord> OnlineRaceTimeline::outgoing(std::uint64_t from,std::size_t maximumFrames)const {
    if(maximumFrames>capacity||from>next_||(next_>capacity&&from<next_-capacity))throw std::out_of_range("Input acknowledgement fell outside retransmit history");
    std::vector<OnlineInputRecord> result;
    for(auto f=from;f<std::min(next_,from+maximumFrames);++f){const auto& e=entries_[f%capacity];
        for(unsigned slot=0;slot<2;++slot)if(e.frame==f&&(host_?e.authoritative[slot]:slot==1&&e.known[slot]))result.push_back({race_,f,slot,e.received[slot]});}
    return result;
}
std::uint64_t OnlineRaceTimeline::digestAfter(std::uint64_t frame)const {
    const auto& e=entries_[frame%capacity];if(e.frame!=frame||frame>=next_)throw std::out_of_range("Race digest is outside history");return e.digest;
}
std::size_t OnlineRaceTimeline::memoryBytes()const {
    std::size_t bytes=sizeof(*this);for(const auto& e:entries_)bytes+=e.before.memoryBytes();return bytes;
}
}
