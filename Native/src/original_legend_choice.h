#pragma once
#include <cstdint>
#include <vector>

namespace idas3::original {
// The original owners choose the painter by race outcome, not confirmation:
// loss uses 0F0C00 (continue) and 0F0F20 (rematch); win uses 0F1BC0
// (continue) and 0F1EC0 (the next rival's challenge). Each painter serves both
// its choice and confirmation phases. Keep the old numeric names as aliases
// for callers built against the first port. Positions are original pixels.
enum class OriginalLegendChoiceKind : std::uint8_t {
    Continue,ContinueConfirm,NextRival,NextRivalConfirm,
    ContinueAfterLoss=Continue,ContinueAfterWin=ContinueConfirm,
    Rematch=NextRival,Challenge=NextRivalConfirm
};
enum class OriginalLegendChoiceBank : std::uint8_t { Continue,Prompt };
struct OriginalLegendChoiceDraw {
    OriginalLegendChoiceBank bank{};
    std::uint32_t chunk{};
    float x=0,y=0,z=0;             // original pixels, and the source layer depth
    // Every layer's own matrix scale carries the depth compensation, which the
    // projection divides back out, so this is what the source scales a layer by
    // on top of that. The option on the near row is drawn at 0.8.
    float scale=1.f;
    std::uint32_t color=0xffffffffu;
    // The layer diffuse word the source material carries, kept for identity;
    // the painter draws the imported chunk and does not override it.
    bool tinted=false;
};
// selected is the source owner's +248 selection; timerTicks is the continue
// countdown the source reads through 134680.
std::vector<OriginalLegendChoiceDraw> originalLegendChoiceDraws(OriginalLegendChoiceKind kind,
    std::uint32_t selected,std::uint32_t timerTicks);
}
