#pragma once
#include "native_assets.h"
#include "original_matrix.h"
#include "original_battle_profile.h"

namespace idas3 {
class OriginalNumberPlate {
public:
    NativeModel model;
    NativeTextureBank textures;
    // Ordinary fresh-player digits; an original saved profile can supply its own.
    static OriginalNumberPlate load(const std::filesystem::path& root,unsigned carId,unsigned factoryColor=0);
    static OriginalNumberPlate loadRival(const std::filesystem::path& root,unsigned carId,unsigned enemyId);
    void setDigits(const std::array<std::uint8_t,5>& digits);
    // Actual player062FA0 ->0631A8 ->057280: five uint32 name IDs,
    // zero beyond signed profile length76. Ranking byte sentinels do not apply.
    static std::array<std::uint8_t,5> playerDigits(const original::OriginalBattleProfile& profile);
    void setPlayerProfile(const original::OriginalBattleProfile& profile){setDigits(playerDigits(profile));}
    const NativeAssembly& assembly()const{return assembly_;}
    static constexpr std::array<std::uint8_t,5> freshDigits(){return {2,2,9,3,6};}
private:
    static OriginalNumberPlate loadAppearance(const std::filesystem::path& root,unsigned carId,const std::filesystem::path& placement,const std::array<std::uint8_t,5>& digits);
    NativeAssembly assembly_;
};
}
