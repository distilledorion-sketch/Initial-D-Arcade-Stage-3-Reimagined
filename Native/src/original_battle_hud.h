#pragma once
#include "original_hud.h"

namespace idas3 {
struct OriginalBattleHudState {
    std::uint32_t flags104=0;
    float validity96=0,signedAdvantage100=0;
    std::uint32_t profileMode0C31C99C=0;
    bool alternateLayout96=false;
    float slide208=0,slide212=0;
    std::int32_t frame204=0;
};
struct OriginalBattleHudAnimation {
    // This persistent draw counter advances only along the source negative
    // advantage branch after the40-frame gate; it is not a host timer.
    std::uint32_t negativeBlink0CA9B548=0;
};
struct OriginalBattleHudDraw {
    enum class Kind { baseHud,beginProjection6,game2d,portrait228,playerName232,opponentName236,endProjection };
    Kind kind=Kind::game2d;
    OriginalHudDraw draw;
};
// Complete0CAE20 orchestration, with base0C96A0, bank projection, portrait
// bank228 and name objects232/236 retained as explicit submission boundaries.
// Call once per original HUD draw; paintGame2d does not advance animation.
// The base matrix is the caller's matrix after base HUD/projection selection.
std::vector<OriginalBattleHudDraw> drawOriginalBattleHud(const OriginalBattleHudState& state,
    OriginalBattleHudAnimation& animation,const original::OriginalMatrix& base=original::originalIdentityMatrix());
class OriginalBattleHudAssets {
public:
    static OriginalBattleHudAssets load(const std::filesystem::path& nativeRoot);
    // Paint only verified game2d submissions. The caller binds the explicit
    // base/portrait/name commands to their own original assets/services.
    void paintGame2d(std::span<std::uint32_t> argb,int width,int height,
        std::span<const OriginalBattleHudDraw> commands,bool edgeAnchored=false)const;
private:
    NativeModel model_;
    NativeTextureBank textures_;
};
}
