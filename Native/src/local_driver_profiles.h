#pragma once
#include "original_battle_profile.h"
#include <filesystem>

namespace idas3 {
// Native replacement for card storage. Each selected car has one local driver
// profile; original gameplay fields are preserved without running card I/O.
class LocalDriverProfiles {
public:
    enum class Origin { Fresh, Saved, Backup, Unreadable };
    struct Loaded {original::OriginalBattleProfile profile;Origin origin=Origin::Fresh;};
    explicit LocalDriverProfiles(std::filesystem::path directory):directory_(std::move(directory)){}
    Loaded load(unsigned car)const;
    bool save(unsigned car,const original::OriginalBattleProfile& profile)const;
    std::filesystem::path path(unsigned car)const;
private:
    std::filesystem::path directory_;
};
}
