#pragma once
#include "original_race_rules.h"
#include <span>
#include <string_view>

namespace idas3::original {
// Original099DA0 centreline-distance table and099360 coordinate interpolation.
// Uses the authoring path in source order; reverse is applied during lookup.
class OriginalBattleMetrics {
public:
    OriginalBattleMetrics()=default;
    OriginalBattleMetrics(std::span<const OriginalRacePoint> sourceCenter,bool reverse);
    static OriginalBattleMetrics load(const std::filesystem::path& root,std::uint32_t condition);
    float distance(OriginalPathCoordinate accumulated)const;
    float advantage(OriginalPathCoordinate player,OriginalPathCoordinate rival)const;
    //0657EC/06587C: raw current coordinate / total path length for HUD92/96.
    float positionFraction(OriginalPathCoordinate current)const;
    float totalLength()const;
    std::span<const float> cumulativeDistances()const{return cumulative_;}
private:
    std::vector<float> cumulative_;
    bool reverse_{};
};
//0C9F20: profile0 selects the31-entry2FBB24 table, profile2 forcesBunta.
// Other profile modes do not have a portrait in this constructor.
std::string_view originalBattlePortraitBank(std::uint32_t enemy,std::uint32_t profileMode);
}
