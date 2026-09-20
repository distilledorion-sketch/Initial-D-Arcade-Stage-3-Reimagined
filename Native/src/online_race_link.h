#pragma once
#include "online_race_timeline.h"

namespace idas3::original {
// Transport-independent, bounded input/acknowledgement protocol. Only the
// authenticated room peer may supply packets. Wire data contains controls,
// never client-authored positions, collision impulses, or finish claims.
class OnlineRaceLink {
public:
    OnlineRaceLink(OnlineRaceSimulation& simulation,std::uint64_t race,bool host,OnlineRaceTimeline::ConfirmedOutput output={});
    std::vector<std::uint8_t> packet()const;
    void receive(std::span<const std::uint8_t> packet);
    bool step(const OriginalVehicleInputs& local);
    void reconcile();
    const OnlineRaceTimeline& timeline()const{return timeline_;}
    std::uint64_t verifiedPeerFrames()const{return verified_;}
private:
    const std::uint64_t race_;
    OnlineRaceTimeline timeline_;
    std::uint64_t peerConfirmed_=0,peerDigest_=0,verified_=0;
    void verify();
};
}
