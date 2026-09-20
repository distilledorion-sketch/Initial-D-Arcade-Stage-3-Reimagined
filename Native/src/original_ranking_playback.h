#pragma once
#include "original_ranking_attract.h"
#include "original_ranking_board.h"

namespace idas3::original {
// Native owner for child12's source controller. Frontend alone steps this at
// 60 Hz; presentation reads it without advancing time or modifying records.
class OriginalRankingPlayback {
public:
    OriginalRankingPageState page;
    OriginalRankingResourceState resources;
    OriginalRankingResourceEvents events;
    std::uint32_t boardFrame=0;
    std::uint64_t ticks=0,resourceRevision=0;
    std::array<std::uint8_t,5> plateDigits{};
    std::uint32_t drawLayerMask=3;
    void reset(unsigned persistedCourseIndex=0);
    void step(const OriginalRankingRecords&,OriginalRankingPageInput={},std::uint32_t carLoadStatus=1);
    bool completed()const{return page.completed;}
private:
    std::uint32_t nextDrawLayerMask_=3;
};
}
