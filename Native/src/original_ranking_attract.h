#pragma once
#include "original_matrix.h"
#include <cstdint>
#include <array>
#include <vector>

namespace idas3::original {
// Child12's page/controller tail02F5F4..02F784. Rankings, car resource
// lifetime and the earlier slide phases remain explicit caller responsibilities.
struct OriginalRankingPageState {
    std::uint32_t courseIndex=0,persistedCourseIndex=0,conditionIndex=0,wet=0;
    std::int32_t pageTicks=1200,watchdogTicks=10800;
    std::uint32_t detailMode=0,detailPage=0,cooldown=90;
    bool refreshCar=false,completed=false;
};
struct OriginalRankingPageInput {
    bool detailPressed=false,nextConditionPressed=false;
};
struct OriginalRankingPageEvents { bool clearLeaderboard=false; };
OriginalRankingPageState initialOriginalRankingPage(unsigned persistedCourseIndex);
OriginalRankingPageEvents stepOriginalRankingPage(OriginalRankingPageState&,OriginalRankingPageInput={});
unsigned originalRankingCourse(const OriginalRankingPageState&);

// Earlier Main02F1C0 resource phases. Step this BEFORE stepOriginalRankingPage:
// page input requests are consumed by the resource phase on the following tick.
// Resource readiness/allocation are host boundaries; all delay/slide arithmetic
// and original appearance command ordering are retained here.
struct OriginalRankingCarSource {
    std::uint32_t car=0,packedAppearance=0;
    std::array<std::uint8_t,5> name{221,221,221,221,221};
};
struct OriginalRankingResourceState {
    std::uint32_t phase=6,loadDelay=0,slideTicks=0;
    std::uint32_t car=0,packedAppearance=0,yawUnits=0xffffe000;
    bool hasCar=false;
};
struct OriginalRankingCarCommand {
    std::uint32_t address=0,argument=0;
    bool operator==(const OriginalRankingCarCommand&)const=default;
};
struct OriginalRankingResourceEvents {
    bool destroyCar=false,createCar=false,configureCar=false,drawCar=false;
    float slideX=0;
    std::uint32_t drawYawUnits=0;
    std::array<std::uint8_t,5> plateDigits{};
    std::vector<OriginalRankingCarCommand> carCommands;
};
OriginalRankingResourceEvents stepOriginalRankingResources(OriginalRankingResourceState&,
    OriginalRankingPageState&,const OriginalRankingCarSource&,std::uint32_t carLoadStatus=1);
//057380 ->057280: derive the original plate from the five encoded name bytes.
std::array<std::uint8_t,5> originalRankingPlateDigits(const std::array<std::uint8_t,5>&);

struct OriginalRankingScene {
    // All four matrices are relative to the original enclosing scene matrix;
    // they are not a replacement camera projection. Column-major XF order.
    OriginalMatrix background,shadow,car,reflection;
    std::uint32_t backgroundChunk=0,shadowChunk=3;
    std::uint32_t nextYawUnits=0;
    //02F9C6 selects ACar+224 draw-layer mask0, then restores2. This is
    // layer visibility (027A1E/027E8E), not a color or opacity override.
    // Retained API name for existing callers; consume source assembly layers.
    bool reflectionRequiresMaterialMode0=true;
};
OriginalRankingScene originalRankingScene(unsigned car,std::uint32_t yawUnits,
    float slideX,const OriginalFscaTable&);
// Native scene/controller components exist, but the full record/glyph layer
// is deliberately gated until implemented. Do not enter child12 from this flag.
inline constexpr bool originalRankingPresentationComplete=false;
}
