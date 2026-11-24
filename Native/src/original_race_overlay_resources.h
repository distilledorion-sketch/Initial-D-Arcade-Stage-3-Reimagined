#pragma once
#include <cstdint>
namespace idas3::original {
// Resource selection by canonical16B640. Presentation position, language and
// option timing remain separate concerns; this owner does not infer them.
enum class OriginalModeLevelUpArtwork { None, Bunta, Versus };
enum class OriginalRaceBannerArtwork { Interrupt02, Versus };
struct OriginalRaceOverlayResources {
    bool driverLevelUp{};
    OriginalModeLevelUpArtwork modeLevelUp{};
    OriginalRaceBannerArtwork banner{};
};
inline OriginalRaceOverlayResources prepareOriginalRaceOverlayResources(
    std::uint32_t profileMode,std::uint32_t& sharedRandom){
    OriginalRaceOverlayResources result;
    result.driverLevelUp=profileMode!=1;
    result.modeLevelUp=profileMode==2?OriginalModeLevelUpArtwork::Bunta:
        profileMode==3?OriginalModeLevelUpArtwork::Versus:OriginalModeLevelUpArtwork::None;
    result.banner=profileMode==3?OriginalRaceBannerArtwork::Versus:OriginalRaceBannerArtwork::Interrupt02;
    if(result.banner==OriginalRaceBannerArtwork::Interrupt02){
        //16B9EE calls rand before loading the fixed interrupt02 resources.
        // Its return is discarded; the shared word still advances here.
        sharedRandom=sharedRandom*1103515245u+12345u;
    }
    return result;
}
}
