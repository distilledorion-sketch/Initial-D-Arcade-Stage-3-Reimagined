#include "experimental_rollback.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>

namespace idas3::original {
namespace {
bool equal(const OriginalVehicleInputs& a,const OriginalVehicleInputs& b){
    return a.analog.steering==b.analog.steering&&a.analog.throttle==b.analog.throttle&&a.analog.brake==b.analog.brake
        &&a.calibration.steeringWord==b.calibration.steeringWord&&a.calibration.throttleWord==b.calibration.throttleWord
        &&a.calibration.brakeWord==b.calibration.brakeWord&&a.suppressRawThrottle0C2F4BC8==b.suppressRawThrottle0C2F4BC8
        &&a.pressedByte==b.pressedByte&&a.automaticMode==b.automaticMode&&a.gearEnabled==b.gearEnabled;
    // elapsedFrames is owned/replaced by OriginalDrivingSession::tick.
}
}
ExperimentalRollback::ExperimentalRollback(OriginalDrivingSession& session,std::uint64_t race,OriginalVehicleInputs neutral)
    :session_(session),race_(race),neutral_(neutral){
    if(!race||!session.rollbackSafe())throw std::invalid_argument("Rollback requires a race ID and a side-effect-free numerical session");
    neutral_.pressedByte=0;
    previousConfirmed_=neutral_;
}
ExperimentalRollback::Slot& ExperimentalRollback::slot(std::uint64_t frame){
    auto& s=slots_[frame%capacity];
    if(s.frame!=frame){s=Slot{};s.frame=frame;}
    return s;
}
bool ExperimentalRollback::receive(std::span<const RollbackInput> batch){
    auto reject=[&](){++metrics_.rejectedPackets;return false;};
    if(batch.size()>capacity)return reject();
    // Keep the active [oldest-unconfirmed, newest-future] interval <=64.
    const auto floor=next_>capacity?next_-capacity:0;
    for(std::size_t i=0;i<batch.size();++i){
        const auto& p=batch[i];
        if(p.race!=race_||p.frame>=confirmed_+capacity)return reject();
        if(p.frame<floor)continue; // a harmless very old retransmission
        const auto& s=slots_[p.frame%capacity];
        if(s.frame==p.frame&&s.actual&&!equal(s.received,p.controls))return reject();
        for(std::size_t j=0;j<i;++j)if(batch[j].frame==p.frame&&!equal(batch[j].controls,p.controls))return reject();
    }
    for(const auto& p:batch){
        if(p.frame<floor)continue;
        // Confirmed old inputs need no slot allocation; their ring slot may
        // already be used by a future input and must not be overwritten.
        if(p.frame<confirmed_)continue;
        auto& s=slot(p.frame);
        if(!s.actual&&p.frame<next_&&!equal(s.used,p.controls))dirty_=std::min(dirty_,p.frame);
        s.received=p.controls;s.actual=true;
    }
    return true;
}
void ExperimentalRollback::simulate(std::uint64_t frame){
    auto& s=slot(frame);
    s.before=session_.checkpoint();
    if(s.actual)s.used=s.received;
    else {
        s.used=frame==confirmed_?previousConfirmed_:frame?slots_[(frame-1)%capacity].used:neutral_;
        s.used.pressedByte=0;
        ++metrics_.predictedFrames;
    }
    session_.tick(s.used); // returned audio/FFB/telemetry effects intentionally discarded
    s.digest=session_.rollbackDigest();
}
void ExperimentalRollback::confirm(){
    while(confirmed_<next_){const auto& s=slots_[confirmed_%capacity];if(s.frame!=confirmed_||!s.actual)break;previousConfirmed_=s.used;++confirmed_;}
}
void ExperimentalRollback::reconcile(){
    if(dirty_!=absent){
        const auto start=std::chrono::steady_clock::now();
        const auto before=session_.actor();
        session_.restore(slots_[dirty_%capacity].before);
        const auto depth=unsigned(next_-dirty_);
        for(auto frame=dirty_;frame<next_;++frame)simulate(frame);
        ++metrics_.rollbacks;metrics_.replayedFrames+=depth;
        metrics_.deepestReplay=std::max(metrics_.deepestReplay,depth);
        double distance=0;for(auto offset:{0u,4u,8u}){const double delta=before.f(offset)-session_.actor().f(offset);distance+=delta*delta;}
        metrics_.maxCorrection=std::max(metrics_.maxCorrection,std::sqrt(distance));
        metrics_.maxYawCorrection=std::max(metrics_.maxYawCorrection,std::abs(std::remainder(double(before.f(28)-session_.actor().f(28)),6.283185307179586)));
        metrics_.maxReplayMs=std::max(metrics_.maxReplayMs,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
        dirty_=absent;
    }
    confirm();
}
bool ExperimentalRollback::advance(){
    reconcile();
    // Fail closed on prolonged loss. Do not discard unconfirmed states and
    // silently pretend an unrecoverable divergence was a successful rollback.
    if(next_-confirmed_>=capacity){++metrics_.stalls;return false;}
    simulate(next_++);confirm();return true;
}
std::uint64_t ExperimentalRollback::digestAfter(std::uint64_t frame) const {
    const auto& s=slots_[frame%capacity];
    if(s.frame!=frame||frame>=next_)throw std::out_of_range("Rollback digest no longer in history");
    return s.digest;
}
std::size_t ExperimentalRollback::memoryBytes() const {
    std::size_t bytes=sizeof(*this);for(const auto& s:slots_)bytes+=s.before.memoryBytes();return bytes;
}
}
