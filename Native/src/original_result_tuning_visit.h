#pragma once
#include "original_battle_result_animation.h"
#include "original_tuning_child.h"

namespace idas3::original {
// Native owner of the common HResult lifecycle06FC48/0709E0. It does not award
// race points: balanceBeforeCap is the source result producer's final balance.
struct OriginalResultTuningVisit {
    OriginalBattleResultAnimationState animation;
    OriginalTuningResultSetup tuning;
    OriginalTuningChild child;
    bool initialized=false,previewStarted=false,basicCompletionPresented=false;
    std::uint32_t previewRemovalFrames=0;
};
struct OriginalResultTuningVisitBegin {
    OriginalBattleResultCommit balanceCommit;
    bool profileChanged=false;
    //07042A..070468: set1/PACK21, request music2, service current manager.
    bool requestResultSoundSet=false;
};
struct OriginalResultTuningVisitInput {
    bool confirmEdge=false;
    float selectionAxis=0.5f;
};
struct OriginalTuningPreviewEvent {
    enum class Kind {startPreview,focusZero,carCall,resetFocus};
    Kind kind=Kind::startPreview;
    OriginalTuningMutation::CarCall call{};
};
struct OriginalResultTuningVisitFrame {
    OriginalBattleResultAnimationFrame result;
    OriginalTuningChildFrame child;
    OriginalTuningMutation mutation;
    std::uint32_t sourceOwnerFrame=0;
    bool childAdvanced=false,profileChanged=false;
    // Ordered source events. Apply preview events before drawing the car;
    // source078680 advances its pose after that draw, once per owner tick.
    std::vector<std::uint32_t> cueIds;
    std::vector<OriginalTuningPreviewEvent> previewEvents;
};
// Invoke once for each new result owner, using the existing shared game RNG.
// Commits the capped balance, then performs source random candidate selection,
// card/cooldown gating and child construction. No synthetic card flags are set.
OriginalResultTuningVisitBegin beginOriginalResultTuningVisit(
    OriginalResultTuningVisit&,OriginalBattleProfile&,const OriginalTuningData&,
    std::uint32_t& sharedRandomSeed,const OriginalBattleResultAnimationSetup&,
    std::vector<std::uint8_t>* fullTuneOffers=nullptr);
// Complete owner update. Child commands1..6 are dispatched here exactly once;
// callers must not dispatch frame.child.command again. Command14 starts the
// source exit fade. Completed visits are inert, including profile and events.
OriginalResultTuningVisitFrame advanceOriginalResultTuningVisit(
    OriginalResultTuningVisit&,OriginalBattleProfile&,const OriginalTuningData&,
    OriginalResultTuningVisitInput={});
}
