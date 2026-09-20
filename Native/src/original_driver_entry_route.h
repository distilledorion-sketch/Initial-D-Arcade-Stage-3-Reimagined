#pragma once
#include "original_battle_profile.h"
#include <cstdint>

namespace idas3::original {
// These are source state operations, not a test for whether a host save exists.
//102CEA..102CF0: the accepted new-card request when0B0AE0 returns zero.
void requestOriginalDriverSetup(OriginalBattleProfile&);
//080D06..080D1E and sibling result/card owners. Only acts when mask2 is set.
void finishOriginalDriverSetupFlag(OriginalBattleProfile&);
bool originalDriverSetupRequested(const OriginalBattleProfile&);
bool originalDriverNameImportsExisting(const OriginalBattleProfile&);
struct OriginalCardAcceptanceRoutes {
    std::uint32_t ordinary76{},renewal80{},converted84{},integralColor88{};
};
//0FEC40..0FED00 card acceptance. Caller explicitly owns the hardware/card-use
// boundary; do not run this on every native save load (it consumes a card use).
std::uint32_t acceptOriginalDriverCard(OriginalBattleProfile&,
    const OriginalCardAcceptanceRoutes&);
}
