#pragma once
#include "original_battle_profile.h"

namespace idas3 {
// One use is consumed when a card is accepted, not when each race finishes.
// Continue/rematch retain that accepted card, including its final (zero-left) use.
inline constexpr unsigned renewedCardPlays=50;
inline bool cardRenewalDue(const original::OriginalBattleProfile& profile){
    return profile.u(1140)==0;
}
inline bool consumeCardPlay(original::OriginalBattleProfile& profile){
    const auto remaining=profile.u(1140);
    if(!remaining)return false;
    profile.setu(1140,remaining-1);
    return true;
}
}
