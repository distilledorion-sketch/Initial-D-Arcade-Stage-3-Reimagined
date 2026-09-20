#pragma once
#include "original_race_rules.h"

namespace idas3::original {
// Authoring-path object095E80/096200. These are the unswapped source edge
// streams, not the canonicalized renderer boundaries or short physics PATH.
class OriginalRacePath {
public:
    OriginalRacePath()=default;
    OriginalRacePath(std::vector<OriginalRacePoint> center,
        std::vector<OriginalRacePoint> sourceLeft,std::vector<OriginalRacePoint> sourceRight,
        bool reverse);
    static OriginalRacePath load(const std::filesystem::path& root,std::uint32_t condition);
    std::int32_t period()const{return static_cast<std::int32_t>(center_.size())-1;}
    bool reverse()const{return reverse_;}
    // Returns whether a cell was found; on failure the original preserves both
    // input words. fullSearch is original byte+32 (constructor defaultfalse;
    // 09B0A0 enables it). Valid previous indices are0..period-1. Forward-only
    // index-1 is also accepted for an explicitly requested full scan.
    bool project(OriginalRacePoint position,OriginalPathCoordinate& coordinate,
        bool fullSearch=false)const;
private:
    struct Plane {OriginalRacePoint anchor{},normal{};};
    struct Cell {std::array<Plane,4> planes;};
    std::vector<OriginalRacePoint> center_;
    std::vector<Cell> cells_;
    bool reverse_{};
    bool contains(std::int32_t sourceIndex,OriginalRacePoint position,
        std::array<float,4>& distances)const;
};

// Player portion of0680C0: two alternating XYZ/coordinate/timestamp records,
// copied prior search state,061460 progress, and the published coordinate.
// It intentionally owns no timer, checkpoint handlers or external card state.
struct OriginalRacePathHistory {
    std::array<OriginalRacePoint,2> positions{};
    std::array<OriginalPathCoordinate,2> coordinates{};
    std::array<std::uint32_t,2> sampleTimes{};
    std::uint32_t activeIndex{};
    OriginalPathCoordinate progress{},publishedCoordinate{};
};
bool advanceOriginalRacePathHistory(const OriginalRacePath& path,
    OriginalRacePathHistory& history,OriginalRacePoint currentPosition,
    std::uint32_t currentTime,bool fullSearch=false);
} // namespace idas3::original
