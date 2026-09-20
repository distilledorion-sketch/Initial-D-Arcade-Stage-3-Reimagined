#include "local_save_slots.h"
#include <chrono>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace idas3 {
namespace {
constexpr const char* header = "idas3-save-slot-v1";
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
