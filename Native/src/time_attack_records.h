#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace idas3 {
struct TimeAttackEntry {
    unsigned condition{},weather{},car{};
    std::uint32_t ticks6000{};
    // Native rows keep their actual driver and original ranking metadata.
    // 221 is the source name terminator. Legacy CSV rows stay unnamed.
    std::array<std::uint8_t,5> nameGlyphs{221,221,221,221,221};
    bool manual=false,night=false;
    std::array<std::uint32_t,3> intermediate6000{};
    bool metadataUnknown=false; // Shared historical rows only; not a save field.
};
struct TimeAttackBest {
    std::uint32_t course{},model{};
};
// Native local-profile storage. Original record selectors share course/direction
// and weather; transmission and time of day do not partition these records.
class TimeAttackRecords {
public:
    bool load(const std::filesystem::path& file);
    bool save(const std::filesystem::path& file)const;
    TimeAttackBest best(unsigned condition,unsigned weather,unsigned car)const;
    TimeAttackEntry personalBest(unsigned condition,unsigned weather,unsigned car)const;
    void record(TimeAttackEntry entry);
    const std::vector<TimeAttackEntry>& entries()const{return entries_;}
private:
    std::vector<TimeAttackEntry> entries_;
};
}
