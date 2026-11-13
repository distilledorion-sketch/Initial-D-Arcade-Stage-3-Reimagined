#pragma once
#include "original_battle_profile.h"
#include <filesystem>

namespace idas3 {
// Local persistence revision. Deliberately independent of the service epoch
// and per-course handling revisions: Season 2 resets every Time Attack board.
inline constexpr unsigned timeAttackSeason=2;
inline void clearPreviousSeasonTimeAttack(original::OriginalBattleProfile& profile){
    // Original card totals, day/night metadata, and three section times for
    // all 36 course/direction/weather partitions. The gaps are other stats.
    for(unsigned index=0;index<36;++index){
        profile.setu(176+4*index,0);profile.setu(320+4*index,0);
        for(unsigned section=0;section<3;++section)profile.setu(500+4*(index*3+section),0);
    }
}
struct TimeAttackSeasonMigration {
    bool complete=true;
    unsigned profilesMigrated=0,recordFilesMigrated=0;
};
// Retry-safe, narrowly scoped startup migration. Every persisted file records
// its own season; no global marker can incorrectly complete a partial reset.
// Does not touch general replay archives, settings, identity, or save metadata.
TimeAttackSeasonMigration migrateTimeAttackSeason(const std::filesystem::path& userdata);
}
