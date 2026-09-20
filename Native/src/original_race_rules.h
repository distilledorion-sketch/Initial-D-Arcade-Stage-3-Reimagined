#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace idas3::original {
struct OriginalRaceRuleRow {
    std::int32_t startIndex{},goalIndex{};
    std::array<std::int32_t,6> lapIndices{},extensionIndices{};
    std::array<std::int32_t,4> sectionIndices{};
};
// Numeric mode3 selects the third authored row; other modes use direction0/1.
std::uint32_t originalRaceRuleRowIndex(std::uint32_t condition,std::uint32_t raceMode);
const OriginalRaceRuleRow& originalRaceRuleRow(std::uint32_t row);
std::int32_t originalTimeAttackInitialSeconds(std::uint32_t condition,std::uint32_t difficulty);
const std::array<std::int32_t,6>& originalTimeAttackBonusSeconds(std::uint32_t condition);
std::int32_t originalLegendInitialSeconds(std::uint32_t enemy,std::uint32_t difficulty);
const std::array<std::int32_t,6>& originalLegendBonusSeconds(std::uint32_t enemy);

struct OriginalPathCoordinate {std::int32_t index{};float fraction{};};
// 061460: shortest signed, periodic integer displacement; fraction is copied.
void advanceOriginalRaceProgress(OriginalPathCoordinate& accumulated,
    OriginalPathCoordinate previous,OriginalPathCoordinate current,std::int32_t period);
struct OriginalRaceTimer {std::int32_t step{};std::uint32_t value{},status{};};
void advanceOriginalRaceTimer(OriginalRaceTimer& timer); // 067ED? / 068028
struct OriginalRaceTimeRecord {
    std::array<std::uint32_t,4> lapTimes{};
    std::uint32_t lapCount{};
    std::array<std::uint32_t,4> sectionTimes{};
    std::uint32_t sectionCount{},finishTime{0xffffffffu};
};
bool appendOriginalLapTime(OriginalRaceTimeRecord&,std::uint32_t time);
bool appendOriginalSectionTime(OriginalRaceTimeRecord&,std::uint32_t time);
void recordOriginalFinishTime(OriginalRaceTimeRecord&,std::uint32_t time);

using OriginalRacePoint=std::array<float,3>;
struct OriginalRaceGate {OriginalRacePoint center{},normal{};};
// 061780, after its two course virtuals: original finite F32/FMAC order,
// absolute plane distances, truncation to timer units; no sign-crossing guard.
std::uint32_t originalRaceCrossingTime(const OriginalRaceGate& gate,
    OriginalRacePoint previous,OriginalRacePoint current,
    std::uint32_t previousTime,std::uint32_t currentTime);

struct OriginalTimeAttackSetup {
    std::uint32_t condition{};
    // Explicit cabinet setting. Default2 is a native configuration choice,
    // not a claim that every original cabinet used that setting.
    std::uint32_t difficultyCode{2};
    // Original profile+32/race+684; exactly1 adds7seconds to the start budget.
    std::uint32_t profileField32{};
};
struct OriginalLegendRaceSetup {
    std::uint32_t condition{},enemy{},difficultyCode{2};
    // Profile byte116+enemy. Its HIGH nibble, not weather, adds7seconds.
    std::uint8_t rivalClearFlags{};
};
struct OriginalBattleResult {
    std::uint32_t outcomeCode{1}; // original race+1644:0=win,1=lose
    bool rivalFinishedLatch{}; // race+1573; freezes result after rival goal
};
bool originalPlayerAhead(OriginalPathCoordinate player,OriginalPathCoordinate rival);
// Full local branch068A60 with numeric mode0. A tie loses. Rival finish
// latches the result but does not itself end the player's race.
void updateOriginalBattleResult(OriginalBattleResult&,OriginalPathCoordinate player,
    OriginalPathCoordinate rival,std::int32_t goalIndex);
enum class OriginalRacePhase {Ready,Running,Finished,TimeUp};
struct OriginalRaceRuleState {
    OriginalRacePhase phase{OriginalRacePhase::Ready};
    OriginalPathCoordinate progress{},previousCoordinate{};
    OriginalRacePoint previousPosition{};
    OriginalRaceTimer elapsed{100,0,0},remaining{-100,0,0};
    OriginalRaceTimeRecord times{};
    std::uint32_t previousSampleTime{},lapCounter{},sectionCounter{},extensionCounter{};
    bool automaticBrake{},timeUpFlag{};
    OriginalPathCoordinate rivalProgress{},previousRivalCoordinate{};
    OriginalBattleResult battleResult{};
};
struct OriginalRaceEvents {
    bool lap{},section{},timeExtension{},finished{},timeUp{};
    std::int32_t secondsAdded{};
};
// The independent predicates of local067A80, before handler side effects.
OriginalRaceEvents originalRaceTriggers(const OriginalRaceRuleRow& row,
    std::int32_t progress,std::uint32_t lapCounter,std::uint32_t sectionCounter,
    std::uint32_t extensionCounter);

// Native owner of the recovered local numeric-mode0/2 race rules. XYZ->path
// projection (096200), countdown presentation, result menus/card writes and
// automatic-brake application remain explicit caller-owned boundaries.
class OriginalRaceRules {
public:
    // root is InitialDRemake root. Original files live under data/courses.
    // Coordinates are in race direction: reverse index i addresses source
    // point N-1-i. The period is N-1, including on point-to-point courses.
    void reset(const std::filesystem::path& root,OriginalTimeAttackSetup setup,
        OriginalPathCoordinate initialCoordinate,OriginalRacePoint position);
    void resetLegend(const std::filesystem::path& root,OriginalLegendRaceSetup setup,
        OriginalPathCoordinate initialCoordinate,OriginalRacePoint position);
    // Live Bunta has common numeric race0 with profile2: ordinary battle
    // result/actor behavior, but the profile override selects TA timers.
    void resetBunta(const std::filesystem::path& root,OriginalTimeAttackSetup setup,
        OriginalPathCoordinate initialCoordinate,OriginalRacePoint position);
    void resetImported(std::vector<OriginalRacePoint> center,std::vector<OriginalRacePoint> left,
        std::vector<OriginalRacePoint> right,bool reverse,const OriginalRaceRuleRow& row,
        std::int32_t initialSeconds,std::array<std::int32_t,6> bonuses,OriginalRacePoint position);
    void start();
    // Host retire action: enter the existing timeout result without a finish.
    bool retire();
    // Called once per original60Hz Main update. vehicleStopped is exactly
    // original actor+80 bit14, not a guessed speed threshold.
    OriginalRaceEvents tick(OriginalPathCoordinate coordinate,OriginalRacePoint position,
        bool vehicleStopped=false);
    OriginalRaceEvents tickBattle(OriginalPathCoordinate coordinate,OriginalRacePoint position,
        OriginalPathCoordinate rivalCoordinate,bool vehicleStopped=false);
    const OriginalRaceRuleState& state()const{return state_;}
    // Trusted, same-race numerical checkpoint only; never a packet decoder.
    void restoreNumericalState(const OriginalRaceRuleState& state){state_=state;}
    const OriginalRaceRuleRow& rules()const{return row_;}
    std::int32_t pathPeriod()const{return period_;}
    OriginalRaceGate gate(std::int32_t relativeIndex)const;
    std::uint32_t displayedElapsed()const;
private:
    OriginalRaceEvents tickFrame(OriginalPathCoordinate,OriginalRacePoint,bool,
        const OriginalPathCoordinate* rivalCoordinate);
    OriginalTimeAttackSetup setup_{};
    bool legend_{},battle_{},imported_{};
    std::array<std::int32_t,6> importedBonuses_{};
    std::uint32_t enemy_{};
    OriginalRaceRuleRow row_{};
    OriginalRaceRuleState state_{};
    std::int32_t period_{};
    std::vector<OriginalRacePoint> center_,left_,right_;
};
} // namespace idas3::original
