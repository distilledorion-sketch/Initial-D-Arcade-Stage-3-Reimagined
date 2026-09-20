#pragma once
#include "original_battle_profile.h"
#include "original_data.h"
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace idas3::original {
//159720 profile-derived solver fields. Retains the caller's condition, path
// progress and rival policy; those are race-owner inputs, not tuning defaults.
void applyOriginalProfilePhysicsSelection(OriginalPhysicsSelection&,const OriginalBattleProfile&);
struct OriginalBasicTuningStep {std::array<std::uint32_t,5> words{};};
struct OriginalOptionalTuningStep {std::array<std::uint32_t,6> words{};};
struct OriginalPerformanceTuningStep {std::array<std::uint32_t,3> words{};};
struct OriginalTuningPackage {std::uint32_t sourceAddress=0;std::vector<OriginalBasicTuningStep> steps;};
struct OriginalCarTuningData {
    std::array<std::uint32_t,8> sourceRow{};
    std::vector<OriginalTuningPackage> packages;
    std::vector<OriginalOptionalTuningStep> optional;
    std::vector<std::uint32_t> colors;
    std::vector<OriginalPerformanceTuningStep> performance;
};
struct OriginalTuningData {
    std::array<OriginalCarTuningData,35> cars;
    // Exact original EUC-JP/JIS bytes, not retranslated tuning descriptions.
    std::map<std::uint32_t,std::string> descriptions;
    static OriginalTuningData load(const std::filesystem::path& gameRoot);
    const OriginalCarTuningData& car(std::uint32_t carId)const;
};
enum class OriginalTuningChildKind {none,basic,performance,optionalPart};
struct OriginalTuningResultSetup {
    OriginalTuningChildKind kind=OriginalTuningChildKind::none;
    std::uint32_t threshold=0x7fffffffu;
    bool notice=false;
    std::uint32_t sourceOwnerKind=0;
};
//06FCA4..07042A. Called once after result points are committed. Preserves the
// original random candidate selection, card flags, cooldown and stage repair.
OriginalTuningResultSetup prepareOriginalResultTuning(OriginalBattleProfile&,const OriginalTuningData&,std::uint32_t& rngSeed);
struct OriginalTuningMutation {
    bool profileChanged=false;
    std::uint32_t pointsSpent=0;
    // Original ACar methods are a presentation boundary; the saved appearance
    // bytes remain authoritative for the next race's car assembly.
    struct CarCall {std::uint32_t address=0,argument5=0,argument6=0;};
    std::vector<CarCall> carCalls;
};
// Exact profile mutations from result-child commands1,2,3,4,5,6. Other commands
// have no profile mutation here and belong to the child presentation protocol.
OriginalTuningMutation applyOriginalTuningCommand(OriginalBattleProfile&,const OriginalTuningData&,std::uint32_t command);
}
