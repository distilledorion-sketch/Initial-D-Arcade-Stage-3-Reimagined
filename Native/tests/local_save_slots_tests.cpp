#include "local_save_slots.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
using namespace idas3;

int main(int argc, char** argv) try {
    if (argc != 2) throw std::invalid_argument("a writable working directory is required");
    const auto working = std::filesystem::absolute(argv[1]).lexically_normal();
    const std::filesystem::path root = working / "save-slot-tests";
    if (root.parent_path() != working || root.filename() != "save-slot-tests")
        throw std::invalid_argument("the fixture must stay inside its working directory");
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
        std::filesystem::create_directories(reopened.path(3).parent_path());
        std::ofstream out(reopened.path(3), std::ios::trunc);
        out << "idas3-save-slot-v1 9 100\n";
    }
    LocalSaveSlots damaged(root);
    if (damaged.at(3).used) throw std::runtime_error("A truncated manifest was accepted");
    if (!damaged.at(1).used) throw std::runtime_error("A truncated file lost an intact neighbour");
    ++cases;

    {
        std::filesystem::create_directories(damaged.path(4).parent_path());
        std::ofstream out(damaged.path(4), std::ios::trunc);
        out << "something-else 1 2 3 1 0 0 0 0 0 2026/01/01\n";
    }
    LocalSaveSlots foreign(root);
    if (foreign.at(4).used) throw std::runtime_error("A foreign manifest header was accepted");
    ++cases;

    // A car outside the roster is refused rather than clamped silently.
    {
        std::filesystem::create_directories(foreign.path(2).parent_path());
        std::ofstream out(foreign.path(2), std::ios::trunc);
        out << "idas3-save-slot-v1 99 10 0 0 220 220 220 220 220 2026/01/01\n";
    }
    LocalSaveSlots bad(root);
    if (bad.at(2).used) throw std::runtime_error("An out-of-range car was accepted");
    ++cases;

    const auto putFile = [](const std::filesystem::path& file, const std::string& text) {
        std::filesystem::create_directories(file.parent_path());
        std::ofstream out(file, std::ios::binary);
        out << text;
        if (!out) throw std::runtime_error("Could not seed deletion fixture");
    };
    const auto readFile = [](const std::filesystem::path& file) {
        std::ifstream in(file, std::ios::binary);
        if (!in) throw std::runtime_error("Deletion lost an unrelated file");
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    };

    // Erasing a slot owns every car's tune, setup and records, including
    // backups, while the next driver's same car is a different save.
    auto neighbour = profile;
    neighbour.setu(16, 8);
    if (!bad.adopt(0, neighbour)) throw std::runtime_error("Could not seed the neighbouring save");
    const auto neighbourManifest = readFile(bad.path(0));
    const auto neighbourCar = bad.profileDirectory(0) / "car_12.profile";
    putFile(neighbourCar, "neighbour tune");
    putFile(bad.profileDirectory(1) / "car_12.profile", "selected tune");
    putFile(bad.profileDirectory(1) / "car_12.profile.previous", "selected backup");
    putFile(bad.profileDirectory(1) / "car_08.profile", "second car tune");
    putFile(bad.profileDirectory(1) / "driver_setup.txt", "selected setup");
    putFile(bad.profileDirectory(1) / "records" / "personal.txt", "selected records");
    const auto beforeInvalid = bad.at(1);
    if (bad.erase(LocalSaveSlots::count) || bad.at(1) != beforeInvalid)
        throw std::runtime_error("Invalid deletion changed a save");
    ++cases;

    if (!bad.erase(1)) throw std::runtime_error("Could not erase the selected save");
    if (bad.at(1).used || std::filesystem::exists(bad.path(1).parent_path()))
        throw std::runtime_error("Deleting a save left its active directory or state behind");
    if (!bad.at(0).used || readFile(bad.path(0)) != neighbourManifest ||
        readFile(neighbourCar) != "neighbour tune")
        throw std::runtime_error("Deleting one save modified its neighbour");
    if (!std::filesystem::is_regular_file(root / ".no-legacy-import"))
        throw std::runtime_error("Deleting a save did not prevent legacy re-import");
    LocalSaveSlots afterErase(root);
    if (afterErase.at(1).used || !afterErase.at(0).used || afterErase.used() != 1)
        throw std::runtime_error("Deletion did not persist across restart");
    for (const auto& entry : std::filesystem::directory_iterator(root))
        if (entry.path().filename().string().starts_with(".deleted-slot-"))
            throw std::runtime_error("Normal deletion failed to clean its staging directory");
    ++cases;

    // Failure to persist the no-import marker must keep the live save whole.
    std::filesystem::remove(root / ".no-legacy-import");
    std::filesystem::create_directory(root / ".no-legacy-import");
    const auto beforeFailure = afterErase.at(0);
    if (afterErase.erase(0) || afterErase.at(0) != beforeFailure ||
        readFile(afterErase.path(0)) != neighbourManifest || readFile(neighbourCar) != "neighbour tune")
        throw std::runtime_error("A failed deletion damaged or emptied the active save");
    std::filesystem::remove(root / ".no-legacy-import");
    ++cases;

#if defined(_WIN32)
    // An open directory handle without delete sharing blocks the commit.
    // This must fail without removing any of the selected driver's files.
    const auto held = CreateFileW(afterErase.path(0).parent_path().c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (held == INVALID_HANDLE_VALUE) throw std::runtime_error("Could not hold the deletion fixture open");
    const bool erasedWhileHeld = afterErase.erase(0);
    CloseHandle(held);
    if (erasedWhileHeld || afterErase.at(0) != beforeFailure ||
        readFile(afterErase.path(0)) != neighbourManifest || readFile(neighbourCar) != "neighbour tune")
        throw std::runtime_error("A failed directory rename damaged the active save");
    ++cases;
#endif

    // Linked slot trees must never delete their external target. Symlink
    // creation may require OS privileges, so keep the other checks portable.
    const auto linkedRoot = root / "linked-store";
    const auto outside = root / "outside-slot";
    std::filesystem::create_directories(linkedRoot);
    putFile(outside / "save.txt", neighbourManifest);
    putFile(outside / "keep.txt", "outside data");
    std::error_code linkError;
    std::filesystem::create_directory_symlink(outside, linkedRoot / "slot_1", linkError);
    if (!linkError) {
        LocalSaveSlots linked(linkedRoot);
        if (!linked.at(0).used || linked.erase(0) || !linked.at(0).used ||
            readFile(outside / "keep.txt") != "outside data")
            throw std::runtime_error("Deleting a linked slot touched its target");
        std::filesystem::remove(linkedRoot / "slot_1");
        ++cases;

        std::filesystem::create_directory_symlink(outside, afterErase.profileDirectory(0) / "linked-records", linkError);
        if (linkError) throw std::runtime_error("Could not seed a nested save link");
        if (afterErase.erase(0) || !afterErase.at(0).used || readFile(neighbourCar) != "neighbour tune" ||
            readFile(outside / "keep.txt") != "outside data")
            throw std::runtime_error("Deleting a nested link damaged either save");
        std::filesystem::remove(afterErase.profileDirectory(0) / "linked-records");
        ++cases;
    } else {
        std::cout << "NOTE symlink deletion checks skipped: " << linkError.message() << '\n';
    }

    // Deleting the last driver leaves the import tombstone after a restart.
    if (!afterErase.erase(0)) throw std::runtime_error("Could not erase the last driver");
    LocalSaveSlots emptyAgain(root);
    if (emptyAgain.used() || !std::filesystem::is_regular_file(root / ".no-legacy-import"))
        throw std::runtime_error("The last deletion did not retain its no-import marker");
    LocalSaveSlots unconfigured;
    if (unconfigured.erase(0)) throw std::runtime_error("An unconfigured store accepted deletion");
    ++cases;

    std::cout << "PASS " << cases << " save file cases: independence, adoption, accumulation, "
              << "wins, persistence, manifest validation, bounded deletion and failure recovery.\n";
    std::filesystem::remove_all(root);
} catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
}
