#pragma once
#include "original_driving_session.h"
#include <array>
#include <span>

namespace idas3::original {
// Isolated physics prototype, NOT enabled by the shipping multiplayer session.
// Inputs have already passed the host edge adapter: prediction holds analog
// controls, but never repeats a one-frame manual shift press.
struct RollbackInput {
    std::uint64_t race=0,frame=0;
    OriginalVehicleInputs controls;
};
struct RollbackMetrics {
    std::uint64_t rollbacks=0,replayedFrames=0,predictedFrames=0,stalls=0,rejectedPackets=0;
    unsigned deepestReplay=0;
    double maxReplayMs=0,maxCorrection=0,maxYawCorrection=0;
};
class ExperimentalRollback {
public:
    static constexpr std::size_t capacity=64;
    ExperimentalRollback(OriginalDrivingSession& session,std::uint64_t race,OriginalVehicleInputs neutral);
    // Poll a whole received batch, then reconcile once (not once per packet).
    // Invalid/conflicting/out-of-window batches fail without modifying history.
    bool receive(std::span<const RollbackInput> batch);
    void reconcile();
    bool advance();
    std::uint64_t frame() const{return next_;}
    std::uint64_t confirmed() const{return confirmed_;}
    std::uint64_t digestAfter(std::uint64_t frame) const;
    const RollbackMetrics& metrics() const{return metrics_;}
    std::size_t memoryBytes() const;
private:
    static constexpr std::uint64_t absent=~std::uint64_t(0);
    struct Slot {
        std::uint64_t frame=absent,digest=0;
        bool actual=false;
        OriginalVehicleInputs received{},used{};
        OriginalDrivingSession::Checkpoint before;
    };
    OriginalDrivingSession& session_;
    const std::uint64_t race_;
    OriginalVehicleInputs neutral_,previousConfirmed_;
    std::array<Slot,capacity> slots_;
    std::uint64_t next_=0,confirmed_=0,dirty_=absent;
    RollbackMetrics metrics_;
    Slot& slot(std::uint64_t frame);
    void simulate(std::uint64_t frame);
    void confirm();
};
}
