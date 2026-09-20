#pragma once
#include "original_initialization.h"
#include <filesystem>
#include <vector>

namespace idas3::original {
// Original course order: Myogi, Usui, Akagi, Akina, Happogahara,
// Irohazaka, Shomaru, Tsuchisaka, Akina Snow; condition=2*course+direction.
// Selector is the explicit original042700 r6 value, normally condition&1.
std::filesystem::path originalCollisionFile(std::uint32_t conditionCode,std::uint32_t selector);
struct OriginalPhysicsSelection {
    //9015E0 is the profile's enemyID (+24), retained under this legacy field
    // name. It is separate from the selected car and byte164 upgrade level.
    std::uint32_t vehicleIndex=0,conditionCode=0,vehicleMode0C9015E0=0;
    std::uint32_t upgradeIndex0C9015F0=0,overrideMode0C9015F4=0;
    std::uint32_t mode0C9015FC=0,mode0C9015C0=0;
    // mode0C9015C0 is retained for source compatibility; the data factory
    // derives the original snow flag from conditionCode>15 as159720 does.
    std::uint32_t progressEnabled0C9015E4=0,progressMode0C9015D4=0;
    float progress0C901650=0;
};
enum class OriginalWeather : std::uint32_t { Dry=0, Wet=1 };
// Source profile+32 is explicitly labelled weatherID; zero is DRY and one
// is RAIN.159720 copies it to9015FC, then15EE00 writes drive434.
void selectOriginalWeather(OriginalPhysicsSelection& selection,OriginalWeather weather);
// A fresh, untuned profile entering solo numeric-mode2 Time Attack.134A60 /
//1348A0 establish the profile bytes;133A60 selects the car;159720 derives
// the solver globals. This does not substitute for a loaded/tuned card.
OriginalPhysicsSelection makeOriginalFreshTimeAttackSelection(
    std::uint32_t vehicleIndex,std::uint32_t conditionCode,OriginalWeather weather);
struct OriginalPhysicsPath {
    std::uint32_t conditionCode=0,inclusiveLastIndex=0;
    std::vector<std::array<float,3>> points;
};

// Compact identified data tables, not an executable image or address space.
class OriginalPhysicsData {
public:
    static OriginalPhysicsData load(const std::filesystem::path& tablesFile);
    OriginalPhysicsPath loadPath(const std::filesystem::path& dataRoot,std::uint32_t conditionCode) const;
    OriginalVehicleParameters parameters(const OriginalPhysicsSelection& selection,
        const OriginalPhysicsPath& path) const;
    OriginalInitializationInputs initialization(const OriginalPhysicsSelection& selection,
        std::array<float,3> position,std::array<float,3> angles) const;
    // Expose actual car dimensions/other record words for the contact lift.
    std::array<std::uint32_t,11> carRecord(std::uint32_t vehicleIndex) const;
private:
    struct Section {std::uint32_t address=0;std::vector<std::byte> bytes;};
    std::vector<Section> sections_;
    std::span<const std::byte> bytes(std::uint32_t address,std::size_t size) const;
    std::uint32_t word(std::uint32_t address) const;
    float scalar(std::uint32_t address) const;
};
} // namespace idas3::original
