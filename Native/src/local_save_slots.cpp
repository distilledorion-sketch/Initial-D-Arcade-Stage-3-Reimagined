#include "local_save_slots.h"
#include <chrono>
#include <fstream>
#include <sstream>
#include <stdexcept>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace idas3 {
namespace {
constexpr const char* header = "idas3-save-slot-v1";

bool plainSaveEntry(const std::filesystem::path& entry) {
    std::error_code ec;
    const auto status = std::filesystem::symlink_status(entry, ec);
    if (ec || (!std::filesystem::is_directory(status) && !std::filesystem::is_regular_file(status)))
        return false;
#if defined(_WIN32)
    // Junctions and other reparse points are not all reported as symlinks.
    const auto attributes = GetFileAttributesW(entry.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
        return false;
#endif
    return true;
}

bool plainSaveTree(const std::filesystem::path& directory) {
    if (!plainSaveEntry(directory)) return false;
    std::error_code ec;
    if (!std::filesystem::is_directory(directory, ec) || ec) return false;
    std::filesystem::recursive_directory_iterator current(directory, ec), end;
    if (ec) return false;
    for (; current != end; current.increment(ec)) {
        if (ec || !plainSaveEntry(current->path())) return false;
    }
    return !ec;
}

bool preventLegacyImport(const std::filesystem::path& root) {
    const auto marker = root / ".no-legacy-import";
    std::error_code ec;
    const auto status = std::filesystem::symlink_status(marker, ec);
    if (status.type() != std::filesystem::file_type::not_found)
        return !ec && std::filesystem::is_regular_file(status) && plainSaveEntry(marker);
    std::ofstream out(marker, std::ios::trunc);
    if (!out) return false;
    out << "A save was explicitly deleted. Do not import legacy drivers again.\n";
    out.close();
    return !out.fail();
}
}

LocalSaveSlots::LocalSaveSlots(std::filesystem::path directory) : directory_(std::move(directory)) {
    reload();
}

std::filesystem::path LocalSaveSlots::path(unsigned slot) const {
    if (slot >= count) throw std::out_of_range("Save slot");
    return directory_ / ("slot_" + std::to_string(slot + 1)) / "save.txt";
}

std::filesystem::path LocalSaveSlots::profileDirectory(unsigned slot) const {
    if (slot >= count) throw std::out_of_range("Save slot");
    return directory_ / ("slot_" + std::to_string(slot + 1)) / "driver_profiles_v1";
}

const LocalSaveSlots::Slot& LocalSaveSlots::at(unsigned slot) const {
    if (slot >= count) throw std::out_of_range("Save slot");
    return slots_[slot];
}

unsigned LocalSaveSlots::used() const {
    unsigned n = 0;
    for (const auto& slot : slots_) if (slot.used) ++n;
    return n;
}

void LocalSaveSlots::reload() {
    slots_ = {};
    if (directory_.empty()) return;
    for (unsigned slot = 0; slot < count; ++slot) {
        std::ifstream in(path(slot));
        if (!in) continue;
        std::string tag;
        Slot value;
        unsigned length = 0;
        if (!(in >> tag) || tag != header) continue;
        if (!(in >> value.car >> value.playedSeconds >> value.wins >> length)) continue;
        if (value.car > 34 || length > 5) continue;
        value.nameLength = length;
        bool ok = true;
        for (unsigned i = 0; i < 5; ++i) if (!(in >> value.nameGlyphs[i])) ok = false;
        if (!ok) continue;
        // The date is optional and may be absent on a file that was made but
        // never played; anything else on the line is ignored.
        in >> value.lastPlayed;
        if (value.lastPlayed == "-") value.lastPlayed.clear();
        value.used = true;
        slots_[slot] = value;
    }
}

bool LocalSaveSlots::write(unsigned slot, const Slot& value) {
    if (slot >= count) throw std::out_of_range("Save slot");
    if (directory_.empty()) return false;
    const auto file = path(slot);
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    // Write beside the file and rename, so an interrupted save cannot leave a
    // half-written slot behind.
    const auto temporary = file.parent_path() / "save.tmp";
    {
        std::ofstream out(temporary, std::ios::trunc);
        if (!out) return false;
        out << header << ' ' << value.car << ' ' << value.playedSeconds << ' '
            << value.wins << ' ' << value.nameLength;
        for (const auto glyph : value.nameGlyphs) out << ' ' << glyph;
        out << ' ' << (value.lastPlayed.empty() ? std::string("-") : value.lastPlayed) << '\n';
        if (!out) return false;
    }
    std::filesystem::rename(temporary, file, ec);
    if (ec) {
        std::filesystem::remove(temporary, ec);
        return false;
    }
    slots_[slot] = value;
    slots_[slot].used = true;
    return true;
}

bool LocalSaveSlots::erase(unsigned slot) {
    if (slot >= count || directory_.empty()) return false;
    std::error_code ec;
    // Anchor all deletion paths to one resolved save root, then address only
    // the fixed slot child. Never traverse links inside that child.
    const auto root = std::filesystem::canonical(directory_, ec);
    if (ec || !std::filesystem::is_directory(root, ec) || ec) return false;
    const auto selected = root / ("slot_" + std::to_string(slot + 1));
    if (selected.parent_path() != root) return false;
    const auto status = std::filesystem::symlink_status(selected, ec);
    if (status.type() == std::filesystem::file_type::not_found) {
        if (!preventLegacyImport(root)) return false;
        slots_[slot] = {};
        return true;
    }
    if (ec || !plainSaveTree(selected)) return false;

    std::filesystem::path staging;
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    for (unsigned attempt = 0; attempt < 16; ++attempt) {
        const auto candidate = root / (".deleted-slot-" + std::to_string(slot + 1) + "-" +
            std::to_string(stamp) + "-" + std::to_string(attempt));
        ec.clear();
        if (std::filesystem::create_directory(candidate, ec)) {
            staging = candidate;
            break;
        }
        if (ec) return false;
    }
    if (staging.empty()) return false;
    if (!preventLegacyImport(root)) {
        std::filesystem::remove(staging, ec);
        return false;
    }

    // Detaching the whole directory is the commit. In particular, a failed
    // rename leaves every car, record, setup marker and manifest untouched.
    std::filesystem::rename(selected, staging / "save", ec);
    if (ec) {
        std::filesystem::remove(staging, ec);
        return false;
    }
    slots_[slot] = {};
    // A locked file can delay physical cleanup. Keep any leftovers under the
    // non-slot name rather than restoring a partly deleted active driver.
    if (staging.parent_path() == root && plainSaveTree(staging))
        std::filesystem::remove_all(staging, ec);
    return true;
}

bool LocalSaveSlots::adopt(unsigned slot, const original::OriginalBattleProfile& profile) {
    if (slot >= count) throw std::out_of_range("Save slot");
    auto value = slots_[slot];
    value.car = profile.u(16);
    const auto length = profile.u(76);
    value.nameLength = length <= 5 ? length : 0;
    for (unsigned i = 0; i < 5; ++i) value.nameGlyphs[i] = i < value.nameLength ? profile.u(44 + 4 * i) : 220;
    if (value.lastPlayed.empty()) value.lastPlayed = today();
    return write(slot, value);
}

bool LocalSaveSlots::addPlayTime(unsigned slot, std::uint64_t seconds) {
    if (slot >= count) throw std::out_of_range("Save slot");
    if (!seconds) return true;
    auto value = slots_[slot];
    value.playedSeconds += seconds;
    value.lastPlayed = today();
    return write(slot, value);
}

bool LocalSaveSlots::addWin(unsigned slot) {
    if (slot >= count) throw std::out_of_range("Save slot");
    auto value = slots_[slot];
    ++value.wins;
    value.lastPlayed = today();
    return write(slot, value);
}

std::string LocalSaveSlots::today() {
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm parts{};
#if defined(_WIN32)
    localtime_s(&parts, &now);
#else
    localtime_r(&now, &parts);
#endif
    std::ostringstream text;
    text << (parts.tm_year + 1900) << '/'
         << (parts.tm_mon + 1 < 10 ? "0" : "") << (parts.tm_mon + 1) << '/'
         << (parts.tm_mday < 10 ? "0" : "") << parts.tm_mday;
    return text.str();
}
}
