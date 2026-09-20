#pragma once
#include <array>
#include <cstdint>
#include <span>
#include "original_battle_profile.h"

namespace idas3::original {
// 031860 seeds both course and model tables from2EF2AC+12+16*scene.
// Values are original6000/sec. They are shared by directions/weather/car IDs.
std::uint32_t originalDefaultTimeAttackRecord6000(unsigned condition);
// Authentic records partition by scene, route, weather. Transmission and night
// are stored metadata and do not partition these ranking tables.
struct OriginalRecordPartition {
    unsigned scene{},route{},weather{};
    std::uint32_t courseOffset(unsigned rank)const; //032680, relative to backup base
    std::uint32_t modelOffset(unsigned originalCarId)const; //0326C0
    std::uint32_t personalIndex()const; //134820, index into active-profile31CA4C
};
OriginalRecordPartition originalRecordPartition(unsigned condition,bool wet);
//032000: rank10 means outside the10 stored entries. Equal times follow the
// existing entry; no empty-slot semantics are invented (original defaults exist).
unsigned originalTimeAttackRank(const std::array<std::uint32_t,10>& times,std::uint32_t candidate);
bool originalModelRecordImproved(std::uint32_t existing,std::uint32_t candidate); //032060
bool originalPersonalRecordImproved(std::uint32_t existing,std::uint32_t candidate); //07E46A..07E48E
// The selected driver's original card record. 31CA4C/31CADC/31CB90 are
// profile offsets176/320/500; global leaderboard rows cannot identify it.
struct OriginalPersonalTimeAttackRecord {
    std::uint32_t ticks6000=0,night=0;
    std::array<std::uint32_t,3> intermediate6000{};
};
OriginalPersonalTimeAttackRecord originalPersonalTimeAttackRecord(const OriginalBattleProfile&,OriginalRecordPartition);
// ARegistTA07E83A..07E960: requires an accepted/new card flag, improves
// strictly (or first time), then stores total, day/night and intermediates.
bool registerOriginalPersonalTimeAttackRecord(OriginalBattleProfile&,OriginalRecordPartition,
    std::uint32_t ticks6000,std::uint32_t night,std::span<const std::uint32_t> intermediate6000);
}
