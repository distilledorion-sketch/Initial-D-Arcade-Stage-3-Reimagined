#include "original_race_rules.h"
#include <bit>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace idas3::original {
namespace {
#include "original_race_rules_data.inc"
#include "original_battle_rules_data.inc"
std::int32_t signed32(std::uint32_t x){return std::bit_cast<std::int32_t>(x);}
std::uint32_t ftrc(float x){
    if(std::isnan(x)||x<=-2147483648.0f)return 0x80000000u;
    if(x>=2147483648.0f)return 0x7fffffffu;
    return std::uint32_t(std::int32_t(x));
}
OriginalRacePoint normalized(OriginalRacePoint p){
    double length=double(p[0])*double(p[0]);length+=double(p[1])*double(p[1]);length+=double(p[2])*double(p[2]);length+=0.0;
    const float scale=1.0f/std::sqrt(float(length));
    for(float& x:p)x*=scale;
    return p;
}
std::vector<OriginalRacePoint> readPath(const std::filesystem::path& path){
    std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("Original race path unavailable: "+path.string());
    const auto size=f.tellg();if(size<32||size>12000008)throw std::runtime_error("Invalid original race path size");
    f.seekg(0);std::uint32_t count{},components{};f.read(reinterpret_cast<char*>(&count),4);f.read(reinterpret_cast<char*>(&components),4);
    if(count<2||components!=3||size!=std::streamoff(8+std::size_t(count)*12))throw std::runtime_error("Invalid original race path header");
    std::vector<OriginalRacePoint> p(count);f.read(reinterpret_cast<char*>(p.data()),std::streamsize(count)*12);
    if(!f)throw std::runtime_error("Truncated original race path");
    for(auto v:p)for(float x:v)if(!std::isfinite(x))throw std::runtime_error("Non-finite original race path");
    return p;
}
void validate(OriginalPathCoordinate c,std::int32_t period){
    if(c.index<0||c.index>=period||!std::isfinite(c.fraction)||c.fraction<0||c.fraction>1)
        throw std::invalid_argument("Race coordinate must be an oriented authoring segment and fraction0..1");
}
}
std::uint32_t originalRaceRuleRowIndex(std::uint32_t condition,std::uint32_t mode){
    if(condition>=18||mode>3)throw std::invalid_argument("Invalid original race selection");
    return (condition/2)*3+(mode==3?2:condition%2);
}
const OriginalRaceRuleRow& originalRaceRuleRow(std::uint32_t row){return raceRows[row>26?0:row];}
std::int32_t originalTimeAttackInitialSeconds(std::uint32_t condition,std::uint32_t difficulty){return timeAttackInitialSeconds[difficulty>4?0:difficulty][condition>17?0:condition];}
const std::array<std::int32_t,6>& originalTimeAttackBonusSeconds(std::uint32_t condition){return timeAttackBonusSeconds[condition>17?0:condition];}
std::int32_t originalLegendInitialSeconds(std::uint32_t enemy,std::uint32_t difficulty){return legendInitialSeconds[difficulty>4?0:difficulty][enemy>30?0:enemy];}
const std::array<std::int32_t,6>& originalLegendBonusSeconds(std::uint32_t enemy){return legendBonusSeconds[enemy>30?0:enemy];}
bool originalPlayerAhead(OriginalPathCoordinate player,OriginalPathCoordinate rival){return player.index>rival.index||(player.index==rival.index&&player.fraction>rival.fraction);}
void updateOriginalBattleResult(OriginalBattleResult& result,OriginalPathCoordinate player,OriginalPathCoordinate rival,std::int32_t goal){
    if(result.rivalFinishedLatch)return;
    result.outcomeCode=originalPlayerAhead(player,rival)?0u:1u;
    if(rival.index>=goal)result.rivalFinishedLatch=true;
}
void advanceOriginalRaceProgress(OriginalPathCoordinate& accumulated,OriginalPathCoordinate previous,OriginalPathCoordinate current,std::int32_t period){
    if(period<=0)throw std::invalid_argument("Original race path period must be positive");
    // All arithmetic before signed remainder is original wrapping32-bit math.
    const auto value=signed32(std::uint32_t(current.index)-std::uint32_t(previous.index)+std::uint32_t(period));
    const auto delta=value%period;
    const auto change=delta<period/2?delta:delta-period;
    accumulated.index=signed32(std::uint32_t(accumulated.index)+std::uint32_t(change));
    accumulated.fraction=current.fraction;
}
void advanceOriginalRaceTimer(OriginalRaceTimer& timer){
    const auto previous=timer.value;timer.value+=ftrc(float(timer.step));
    if(timer.step>0){if(signed32(previous)>signed32(timer.value))timer.status|=1;}
    else if(signed32(timer.value)>signed32(previous))timer.status|=2;
}
bool appendOriginalLapTime(OriginalRaceTimeRecord& r,std::uint32_t t){if(r.lapCount>=4)return false;r.lapTimes[r.lapCount++]=t&~3u;return true;}
bool appendOriginalSectionTime(OriginalRaceTimeRecord& r,std::uint32_t t){if(r.sectionCount>=4)return false;r.sectionTimes[r.sectionCount++]=t&~3u;return true;}
void recordOriginalFinishTime(OriginalRaceTimeRecord& r,std::uint32_t t){r.finishTime=t&~3u;}
OriginalRaceEvents originalRaceTriggers(const OriginalRaceRuleRow& row,std::int32_t progress,std::uint32_t lapCounter,std::uint32_t sectionCounter,std::uint32_t extensionCounter){
    OriginalRaceEvents result;
    if(sectionCounter<4){const auto target=row.sectionIndices[sectionCounter];result.section=target!=-1&&progress>=target;}
    if(lapCounter<6){const auto target=row.lapIndices[lapCounter];result.lap=target!=-1&&progress>=target;}
    if(extensionCounter<6){const auto target=row.extensionIndices[extensionCounter];result.timeExtension=target!=-1&&progress>=target;}
    result.finished=progress>=row.goalIndex;return result;
}
std::uint32_t originalRaceCrossingTime(const OriginalRaceGate& gate,OriginalRacePoint previous,OriginalRacePoint current,std::uint32_t previousTime,std::uint32_t currentTime){
    const auto normal=normalized(gate.normal); // 06182A, after099540 normalization.
    auto distance=[&](OriginalRacePoint p){
        for(std::size_t i=0;i<3;++i)p[i]-=gate.center[i];
        float value=p[1]*normal[1];value=std::fma(p[0],normal[0],value);value=std::fma(p[2],normal[2],value);
        if(value<0.0f)value=-value;return value;
    };
    const float before=distance(previous),after=distance(current),sum=before+after;
    const float fraction=sum==0.0f?0.0f:before/sum;
    const float delta=float(signed32(currentTime-previousTime))*fraction;
    return previousTime+ftrc(delta);
}
void OriginalRaceRules::reset(const std::filesystem::path& root,OriginalTimeAttackSetup setup,OriginalPathCoordinate initial,OriginalRacePoint position){
    if(setup.condition>=18||setup.difficultyCode>4)throw std::invalid_argument("Original time attack requires condition0..17 and difficulty0..4");
    constexpr std::array<const char*,9> folders{"k_ez","s_nm","h_hd","k_df","s_vh","s_uh","n_sy","k_tu","k_df"};
    const auto base=root/"data"/"courses";const std::string prefix=folders[setup.condition/2];
    auto center=readPath(base/(prefix+"_path.bin")),left=readPath(base/(prefix+"_path_l.bin")),right=readPath(base/(prefix+"_path_r.bin"));
    if(center.size()!=left.size()||center.size()!=right.size())throw std::runtime_error("Original race path count mismatch");
    const auto period=std::int32_t(center.size()-1);validate(initial,period);
    for(float x:position)if(!std::isfinite(x))throw std::invalid_argument("Non-finite race position");
    setup_=setup;legend_=false;battle_=false;imported_=false;enemy_=0;row_=originalRaceRuleRow(originalRaceRuleRowIndex(setup.condition,2));period_=period;
    center_=std::move(center);left_=std::move(left);right_=std::move(right);state_={};
    // 0671A0 seeds the previous path coordinate at the authored start and
    // accumulated progress0. Caller projection is first consumed on Main tick.
    state_.previousCoordinate={row_.startIndex,0};state_.previousPosition=position;
    state_.previousRivalCoordinate=state_.previousCoordinate;
    state_.remaining.value=std::uint32_t(originalTimeAttackInitialSeconds(setup.condition,setup.difficultyCode)*6000+(setup.profileField32==1?42000:0));
}
void OriginalRaceRules::resetLegend(const std::filesystem::path& root,OriginalLegendRaceSetup setup,OriginalPathCoordinate initial,OriginalRacePoint position){
    if(setup.enemy>30)throw std::invalid_argument("Original Legend rival must be0..30");
    reset(root,{setup.condition,setup.difficultyCode,0},initial,position);
    legend_=true;battle_=true;enemy_=setup.enemy;
    const auto ticks=std::uint32_t(originalLegendInitialSeconds(setup.enemy,setup.difficultyCode))*6000u;
    state_.remaining.value=ticks+((signed32(ticks)>0&&(setup.rivalClearFlags&0xf0)!=0)?42000u:0u);
}
void OriginalRaceRules::resetBunta(const std::filesystem::path& root,OriginalTimeAttackSetup setup,OriginalPathCoordinate initial,OriginalRacePoint position){
    reset(root,setup,initial,position);
    battle_=true;
    // Mode0 and2 both select the directional row. Their distinction here is
    // battle progress/result handling; profile2 retains the TA timer columns.
    row_=originalRaceRuleRow(originalRaceRuleRowIndex(setup.condition,0));
}
void OriginalRaceRules::resetImported(std::vector<OriginalRacePoint> center,std::vector<OriginalRacePoint> left,
    std::vector<OriginalRacePoint> right,bool reverse,const OriginalRaceRuleRow& row,
    std::int32_t initialSeconds,std::array<std::int32_t,6> bonuses,OriginalRacePoint position){
    if(center.size()<21||center.size()!=left.size()||left.size()!=right.size()||initialSeconds<=0||initialSeconds>999)
        throw std::invalid_argument("Invalid imported race data");
    const int period=int(center.size())-1;
    if(row.startIndex<0||row.goalIndex<=0||row.startIndex+row.goalIndex>=period)throw std::invalid_argument("Imported race gates outside path");
    for(const auto* stream:{&center,&left,&right})for(auto p:*stream)for(float f:p)if(!std::isfinite(f))throw std::invalid_argument("Invalid imported path point");
    for(auto bonus:bonuses)if(bonus<0||bonus>999)throw std::invalid_argument("Invalid imported time extension");
    setup_={unsigned(reverse),2,0};legend_=battle_=false;imported_=true;importedBonuses_=bonuses;enemy_=0;
    row_=row;period_=period;center_=std::move(center);left_=std::move(left);right_=std::move(right);state_={};
    state_.previousCoordinate={row.startIndex,0};state_.previousPosition=position;state_.remaining.value=unsigned(initialSeconds)*6000;
}
void OriginalRaceRules::start(){if(period_<=0)throw std::logic_error("Race rules are not loaded");if(state_.phase==OriginalRacePhase::Ready)state_.phase=OriginalRacePhase::Running;}
bool OriginalRaceRules::retire(){
    if(period_<=0||(state_.phase!=OriginalRacePhase::Ready&&state_.phase!=OriginalRacePhase::Running))return false;
    state_.phase=OriginalRacePhase::TimeUp;state_.remaining.value=0;
    state_.automaticBrake=true;state_.timeUpFlag=true;state_.battleResult.outcomeCode=1;
    return true;
}
OriginalRaceGate OriginalRaceRules::gate(std::int32_t relativeIndex)const{
    if(period_<=0)throw std::logic_error("Race rules are not loaded");
    auto index=(row_.startIndex+relativeIndex)%period_;if(index<0)index+=period_;
    const bool reverse=(setup_.condition&1)!=0;if(reverse)index=period_-index;
    const auto& a=reverse?right_[index]:left_[index];const auto& b=reverse?left_[index]:right_[index];
    auto across=normalized({b[0]-a[0],0.0f,b[2]-a[2]});
    return {center_[index],{across[2],0.0f,-across[0]}};
}
OriginalRaceEvents OriginalRaceRules::tick(OriginalPathCoordinate coordinate,OriginalRacePoint position,bool vehicleStopped){
    if(battle_)throw std::logic_error("Battle requires tickBattle with original rival coordinates");
    return tickFrame(coordinate,position,vehicleStopped,nullptr);
}
OriginalRaceEvents OriginalRaceRules::tickBattle(OriginalPathCoordinate coordinate,OriginalRacePoint position,OriginalPathCoordinate rival,bool vehicleStopped){
    if(!battle_)throw std::logic_error("tickBattle requires resetLegend or resetBunta");
    return tickFrame(coordinate,position,vehicleStopped,&rival);
}
OriginalRaceEvents OriginalRaceRules::tickFrame(OriginalPathCoordinate coordinate,OriginalRacePoint position,bool vehicleStopped,const OriginalPathCoordinate* rival){
    OriginalRaceEvents events;if(state_.phase!=OriginalRacePhase::Running)return events;
    validate(coordinate,period_);for(float x:position)if(!std::isfinite(x))throw std::invalid_argument("Non-finite race position");
    if(rival)validate(*rival,period_);
    advanceOriginalRaceProgress(state_.progress,state_.previousCoordinate,coordinate,period_);
    if(rival){advanceOriginalRaceProgress(state_.rivalProgress,state_.previousRivalCoordinate,*rival,period_);state_.previousRivalCoordinate=*rival;}
    const auto crossing=[&](std::int32_t relative){return originalRaceCrossingTime(gate(relative),state_.previousPosition,position,state_.previousSampleTime,state_.elapsed.value);};
    // 067A80 processes at most one from each distinct list per frame, in this order.
    events=originalRaceTriggers(row_,state_.progress.index,state_.lapCounter,state_.sectionCounter,state_.extensionCounter);
    if(events.section){const auto target=row_.sectionIndices[state_.sectionCounter];appendOriginalSectionTime(state_.times,crossing(target));++state_.sectionCounter;}
    if(events.lap){const auto target=row_.lapIndices[state_.lapCounter];appendOriginalLapTime(state_.times,crossing(target));++state_.lapCounter;}
    if(events.timeExtension){
        events.secondsAdded=(imported_?importedBonuses_:legend_?originalLegendBonusSeconds(enemy_):originalTimeAttackBonusSeconds(setup_.condition))[state_.extensionCounter];
        state_.remaining.value+=std::uint32_t(events.secondsAdded)*6000u;++state_.extensionCounter;
    }
    if(events.finished){recordOriginalFinishTime(state_.times,crossing(row_.goalIndex));state_.phase=OriginalRacePhase::Finished;}
    if(rival)updateOriginalBattleResult(state_.battleResult,state_.progress,state_.rivalProgress,row_.goalIndex);
    // Local067D00: a nonpositive countdown requests original automatic brake;
    // the transition waits for actorbit14 (or the <=-5sec fallback).
    if(signed32(state_.remaining.value)<=0){
        state_.automaticBrake=true;
        if(vehicleStopped||signed32(state_.remaining.value)<=-30000){state_.timeUpFlag=true;if(!events.finished){state_.phase=OriginalRacePhase::TimeUp;events.timeUp=true;}}
    }else {state_.automaticBrake=false;advanceOriginalRaceTimer(state_.remaining);}
    state_.previousCoordinate=coordinate;state_.previousPosition=position;state_.previousSampleTime=state_.elapsed.value;
    advanceOriginalRaceTimer(state_.elapsed);return events;
}
std::uint32_t OriginalRaceRules::displayedElapsed()const{return state_.phase==OriginalRacePhase::Finished?state_.times.finishTime:state_.elapsed.value;}
} // namespace idas3::original
