#pragma once
#include "original_hud.h"

namespace idas3 {
struct OriginalBattleResultsState {
    std::uint32_t resultStatus=0,profileMode=0,totalTicks6000=0;
    std::array<std::uint32_t,4> sectionTimes6000{};
    unsigned sectionCount=0,sectionCapacity=4;
    float signedAdvantage=0;
    // Participation, victory/finish bonus, advantage/record bonus, earned,
    // capped balance. profileMode1 selects original iResult2DTA0EEDA0/0EEFC0.
    std::array<std::uint32_t,5> points{};
    bool deduction=false,circuitLayout=false;
    //0EDFE0 state+64 selects alternate TOTAL POINTS digits and unit. The
    // owner advances the original30-frame blink once per60Hz update; painting
    // only consumes that frame's visibility, including repeated/pause paints.
    bool balanceHighlighted=false,balanceVisible=true;
    std::uint32_t frame60=0;
};
// Source timing stores intermediate checkpoints separately from the finish.
void completeOriginalResultSections(OriginalBattleResultsState&);
class OriginalBattleResults {
public:
    static OriginalBattleResults load(const std::filesystem::path& gameRoot);
    std::vector<OriginalHudDraw> drawList(const OriginalBattleResultsState& state)const;
    void paint(std::span<std::uint32_t> argb,int width,int height,const OriginalBattleResultsState& state,bool straightAlphaOverlay=false)const;
private:
    NativeModel model_;
    NativeTextureBank textures_;
};
// Original0C0CEB20 normal-layout Time Attack record display. The race owner
// supplies the frozen cumulative timestamps; this component does not change
// records, infer a best result, or advance the card/ranking/retry lifecycle.
struct OriginalResultsState {
    unsigned carId=0,condition=6;
    std::uint32_t totalTicks6000=0;
    std::int32_t remainingTicks6000=0;
    std::array<std::uint32_t,4> sectionTimes6000{};
    unsigned sectionCount=0,sectionCapacity=4;
    // Original display state+48/+52/+56: TOTAL/MODEL/BEST clocks.
    // +60==1 enables MODEL; a zero BEST value displays dashes.
    std::array<std::uint32_t,3> bestTimes6000{};
    bool modelBestAvailable=false;
    bool suppliedRecordTargets=false; // Empty supplied course/model targets are absent, not factory records.
    static constexpr std::uint32_t newRecord=0x08000000;
    static constexpr std::uint32_t courseRecord=0x10000000;
    static constexpr std::uint32_t modelRecord=0x20000000;
    static constexpr std::uint32_t personalBest=0x40000000;
    // Preserve original independent bits and course/model/personal priority.
    std::uint32_t recordFlags=0;
    bool edgeAnchored=false;
    bool livePanel=false;
    float slide208=0,slide212=0; // Live +D0 labels / +D4 backing strips.
    // Post-finish record announcement owns the screen; ordinary race clocks
    // and record comparison panels must not remain behind its two captions.
    bool announcementOnly=false;
    std::array<std::int32_t,2> differences6000{};
    std::array<bool,2> differenceAvailable{};
};
struct OriginalResultsDraw {
    enum class Bank { race,timeAttack } bank=Bank::race;
    std::uint32_t index=0;
    original::OriginalMatrix matrix{};
};
class OriginalTimeAttackResults {
public:
    static OriginalTimeAttackResults load(const std::filesystem::path& gameRoot);
    // Supplemental original commands; frozen left-side clocks use the already
    // verified OriginalRaceHud component when paint() is called.
    std::vector<OriginalResultsDraw> drawList(const OriginalResultsState& state)const;
    void paint(std::span<std::uint32_t> argb,int width,int height,const OriginalResultsState& state)const;
private:
    OriginalRaceHud hud_;
    NativeModel main_,timeAttack_;
    NativeTextureBank mainTextures_,timeAttackTextures_;
};
}
