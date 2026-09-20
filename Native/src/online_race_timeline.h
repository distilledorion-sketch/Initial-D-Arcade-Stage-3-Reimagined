#pragma once
#include "online_race_simulation.h"
#include <functional>
#include <span>

namespace idas3::original {
struct OnlineInputRecord {
    std::uint64_t race=0,frame=0;
    unsigned slot=0;
    OriginalVehicleInputs input;
};
struct OnlineTimelineMetrics {
    std::uint64_t rollbacks=0,replayedFrames=0,rejectedBatches=0,stalls=0,confirmedEffects=0;
    unsigned maxDepth=0;
    double maxReplayMs=0,maxCorrection=0;
};
// Host accepts slot1 input, decides the canonical two-slot input history, and
// publishes it. Client predicts its own slot1 immediately but only confirms
// history echoed by the host. No client position/finish claim is authoritative.
class OnlineRaceTimeline {
public:
    static constexpr std::size_t capacity=64;
    using ConfirmedOutput=std::function<void(const OnlineRaceFrame&,std::uint64_t digest)>;
    OnlineRaceTimeline(OnlineRaceSimulation& simulation,std::uint64_t race,bool host,ConfirmedOutput output={});
    bool localInput(const OriginalVehicleInputs& input);
    bool receive(std::span<const OnlineInputRecord> records); // caller is the authenticated opposite role
    void reconcile();
    bool advance();
    std::vector<OnlineInputRecord> outgoing(std::uint64_t from,std::size_t maximumFrames=32)const;
    std::uint64_t frame()const{return next_;}
    std::uint64_t confirmed()const{return confirmed_;}
    std::uint64_t confirmedDigest()const{return confirmedDigest_;}
    std::uint64_t digestAfter(std::uint64_t frame)const;
    std::size_t memoryBytes()const;
    const OnlineTimelineMetrics& metrics()const{return metrics_;}
private:
    static constexpr std::uint64_t absent=~std::uint64_t(0);
    struct Entry {
        std::uint64_t frame=absent,digest=0;
        std::array<bool,2> known{},authoritative{};
        std::array<OriginalVehicleInputs,2> received{},used{};
        OnlineRaceSimulation::Checkpoint before;
        OnlineRaceFrame effects;
    };
    OnlineRaceSimulation& simulation_;
    std::uint64_t race_,next_=0,confirmed_=0,dirty_=absent,confirmedDigest_=0;
    bool host_;
    ConfirmedOutput output_;
    std::array<Entry,capacity> entries_;
    std::array<OriginalVehicleInputs,2> previousConfirmed_;
    OnlineTimelineMetrics metrics_;
    Entry& entry(std::uint64_t frame);
    void simulate(std::uint64_t frame);
    void confirm();
};
}
