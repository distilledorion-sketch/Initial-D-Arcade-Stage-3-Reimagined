#include "local_save_slots.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace idas3;

int main(int argc, char** argv) try {
    if (argc != 2) throw std::invalid_argument("a writable working directory is required");
    const std::filesystem::path root = std::filesystem::path(argv[1]) / "save-slot-tests";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    unsigned cases = 0;

    LocalSaveSlots slots(root);
    if (slots.used() != 0) throw std::runtime_error("A fresh directory reported saved files");
    for (unsigned i = 0; i < LocalSaveSlots::count; ++i)
        if (slots.at(i).used) throw std::runtime_error("A fresh slot reported itself used");
    ++cases;

    // Each file keeps its own profile directory, which is what stops progress
    // and parts leaking between files.
    for (unsigned i = 0; i < LocalSaveSlots::count; ++i)
        for (unsigned j = i + 1; j < LocalSaveSlots::count; ++j)
            if (slots.profileDirectory(i) == slots.profileDirectory(j))
                throw std::runtime_error("Two save files share one profile directory");
    ++cases;

    auto profile = original::makeOriginalFreshBattleProfile();
    profile.setu(16, 12);
    profile.setu(76, 3);
    profile.setu(44, 30); profile.setu(48, 31); profile.setu(52, 32);
    if (!slots.adopt(1, profile)) throw std::runtime_error("A finished setup did not record its file");
    if (!slots.addPlayTime(1, 4000)) throw std::runtime_error("Play time was not recorded");
    if (!slots.addPlayTime(1, 271)) throw std::runtime_error("Play time did not accumulate");
    ++cases;

    if (!slots.addWin(1) || !slots.addWin(1)) throw std::runtime_error("A win was not recorded");
    if (slots.at(1).wins != 2) throw std::runtime_error("Wins did not accumulate");
    if (slots.at(0).wins || slots.at(2).wins) throw std::runtime_error("A win reached another file");
    ++cases;

    const auto check = [](const LocalSaveSlots& store) {
        const auto& one = store.at(1);
        if (!one.used || one.car != 12 || one.nameLength != 3) throw std::runtime_error("Slot contents differ");
        if (one.nameGlyphs[0] != 30 || one.nameGlyphs[2] != 32) throw std::runtime_error("Name glyphs differ");
        if (one.playedSeconds != 4271) throw std::runtime_error("Play time differs");
        if (one.wins != 2) throw std::runtime_error("Wins differ");
        if (one.lastPlayed.size() != 10) throw std::runtime_error("Last played date is not yyyy/mm/dd");
        if (store.at(0).used || store.at(2).used) throw std::runtime_error("Writing one file disturbed another");
    };
    check(slots);
    ++cases;

    // The manifest has to survive a restart, which is the whole point of it.
    LocalSaveSlots reopened(root);
    check(reopened);
    if (reopened.used() != 1) throw std::runtime_error("Reopening lost the used count");
    ++cases;

    // A truncated or corrupt manifest must read as an empty file rather than
    // as a half-populated one.
    {
        std::ofstream out(reopened.path(3), std::ios::trunc);
        out << "idas3-save-slot-v1 9 100\n";
    }
    LocalSaveSlots damaged(root);
    if (damaged.at(3).used) throw std::runtime_error("A truncated manifest was accepted");
    if (!damaged.at(1).used) throw std::runtime_error("A truncated file lost an intact neighbour");
    ++cases;

    {
        std::ofstream out(damaged.path(4), std::ios::trunc);
        out << "something-else 1 2 3 1 0 0 0 0 0 2026/01/01\n";
    }
    LocalSaveSlots foreign(root);
    if (foreign.at(4).used) throw std::runtime_error("A foreign manifest header was accepted");
    ++cases;

    // A car outside the roster is refused rather than clamped silently.
    {
        std::ofstream out(foreign.path(2), std::ios::trunc);
        out << "idas3-save-slot-v1 99 10 0 0 220 220 220 220 220 2026/01/01\n";
    }
    LocalSaveSlots bad(root);
    if (bad.at(2).used) throw std::runtime_error("An out-of-range car was accepted");
    ++cases;

    std::cout << "PASS " << cases << " save file cases: independence, adoption, accumulation, "
              << "wins, persistence across a restart, and truncated/foreign/out-of-range manifests.\n";
    std::filesystem::remove_all(root);
} catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
}
