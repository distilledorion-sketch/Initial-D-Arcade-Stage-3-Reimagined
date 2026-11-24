#pragma once
#include <cstdint>
namespace idas3::original {
// Source16CD20 chooses from authored IDs0..8,10,11. Visit08FE00 uses
// fixed12 for original profile mode0/enemy29; TA07C0A0 always selects.
// Legend and HBunta both register Visit08FE00 after their pre-race dialogue.
// The shared word is0C37C778. Rendering is not an owner.
enum class OriginalLoadingOwner { TimeAttack, Visit };
inline std::uint32_t selectOriginalLoadingArtwork(OriginalLoadingOwner owner,
    std::uint32_t profileMode,std::uint32_t enemy,std::uint32_t& seed) {
    if(owner==OriginalLoadingOwner::Visit&&profileMode==0&&enemy==29)return 12;
    seed=seed*1103515245u+12345u;
    const auto value=((seed>>16)&32767u)%11u;
    return value+(value>8u?1u:0u);
}
}
