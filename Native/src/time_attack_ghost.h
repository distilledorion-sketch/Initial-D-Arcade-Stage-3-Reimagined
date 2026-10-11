#pragma once
#include "race.h"
#include <filesystem>

namespace idas3 {
// One best completed run per save/course/direction/weather. Independent of
// optional replay archives and community uploads; contains no simulation owner.
class TimeAttackGhost {
public:
    Replay replay;
    unsigned car=0;
    bool load(const std::filesystem::path& path);
    enum class Saved { Unchanged, Replaced, Failed };
    static Saved saveBest(const std::filesystem::path& path,const Replay& run,unsigned car);
    ReplayFrame sample(double tick) const;
    static std::filesystem::path path(const std::filesystem::path& profiles,unsigned course,bool reverse,bool wet);
    static std::filesystem::path telemetryPath(const std::filesystem::path& saves,const std::string& runKey,unsigned course);
    // Removes only known pre-reset Odawara best-ghost names across the legacy
    // profile directory and five save slots. Optional replay archives are separate.
    // Refuses linked paths/non-file entries and leaves current revision-2 ghosts.
    static bool removeRetiredOdawaraGhosts(const std::filesystem::path& saves);
};
}
