#pragma once
#include "original_ics_player.h"
#include <filesystem>
#include <memory>
#include <span>

namespace idas3 {
struct OriginalOneShotStatistics {
    std::uint64_t cues=0,notes=0,chokes=0,stolen=0,dropped=0,frames=0;
    unsigned activeVoices=0,peakVoices=0;
};
// ARM1F8C priority-list selection, in oldest-first source list order.
// Returns an index, or -1 when all old sounds have stronger priority.
int originalOneShotSteal(std::span<const std::uint8_t> priorities,unsigned incoming);
// Original A9 sequences and PCM voice parameters, rendered at44100Hz.
// This owns the original32-voice SFX partition. Arbitration against the other
// users of the original64 physical slots remains a caller integration boundary.
class OriginalOneShotPlayback {
public:
    explicit OriginalOneShotPlayback(const std::filesystem::path& root);
    ~OriginalOneShotPlayback();
    OriginalOneShotPlayback(const OriginalOneShotPlayback&)=delete;
    OriginalOneShotPlayback& operator=(const OriginalOneShotPlayback&)=delete;
    void reset();
    // Actual bank numbers20,21,22,24,25; cue is the authored A9 track number.
    void play(unsigned bankNumber,unsigned cue);
    void stopBank(unsigned bankNumber);
    void setBankVolume(unsigned bankNumber,std::uint8_t level);
    void clearDspSends();
    OriginalIcsMixFrame renderFrame();
    OriginalOneShotStatistics statistics()const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
