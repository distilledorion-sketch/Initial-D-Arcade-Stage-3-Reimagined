#pragma once
#include "original_battle_profile.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

namespace idas3 {
// Five independent save files. A file holds one car and its own driver profile
// store, so progress, parts and balance never cross between files: the whole
// per-car profile directory is scoped to the slot rather than shared.
class LocalSaveSlots {
public:
    static constexpr unsigned count = 5;
    struct Slot {
        bool used = false;
        unsigned car = 0;
        // Length and glyph identities exactly as the source profile keeps them,
        // so the file screen can render the name with the game's own alphabet.
        unsigned nameLength = 0;
        std::array<std::uint32_t, 5> nameGlyphs{220, 220, 220, 220, 220};
        std::uint64_t playedSeconds = 0;
        unsigned wins = 0;
        // yyyy/mm/dd, or empty when the file has never been played.
        std::string lastPlayed;
        bool operator==(const Slot&) const = default;
    };
    explicit LocalSaveSlots(std::filesystem::path directory = {});
    void reload();
    const Slot& at(unsigned slot) const;
    unsigned used() const;
    // Where this file's per-car driver profiles and setup markers live.
    std::filesystem::path profileDirectory(unsigned slot) const;
    std::filesystem::path path(unsigned slot) const;
    bool write(unsigned slot, const Slot& value);
    // Records what a finished driver setup chose, leaving play time alone.
    bool adopt(unsigned slot, const original::OriginalBattleProfile& profile);
    // Adds elapsed play time and refreshes the last-played date.
    bool addPlayTime(unsigned slot, std::uint64_t seconds);
    // One more battle won on this file. Time attack has no opponent and
    // never counts here.
    bool addWin(unsigned slot);
    static std::string today();

private:
    std::filesystem::path directory_;
    std::array<Slot, count> slots_{};
};
}
