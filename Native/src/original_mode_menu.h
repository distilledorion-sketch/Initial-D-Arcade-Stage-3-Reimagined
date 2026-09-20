#pragma once
#include "native_assets.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

namespace idas3::original {
enum class OriginalGameMode : std::uint32_t { LegendOfTheStreets=0,TimeAttack=1,BuntaChallenge=2 };
struct OriginalModeMenuState {
    OriginalGameMode selected=OriginalGameMode::LegendOfTheStreets;
    float confirmationPhase=0;                  // Original display+436.
    std::uint32_t selectedFrames=0;              // +440, resets on change.
    std::array<std::int32_t,3> focusFrames{};     // +444, capped at20.
    std::array<float,3> focusValues{};            // +456, original .05*n.
};
// Original1934A0/1934E0/193740. Caller supplies60Hz frames and the original
// confirmation frame/120; this component does not invent a mode launch policy.
void selectOriginalGameMode(OriginalModeMenuState& state,OriginalGameMode mode);
void setOriginalModeConfirmation(OriginalModeMenuState& state,float phase);
void stepOriginalModeMenu(OriginalModeMenuState& state);
struct OriginalModeMenuDraw {
    std::uint32_t bank=19,chunk=0;
    float x=0,y=0,scale=1;                       // Original model coordinates.
    std::uint32_t color=0xffffffffu;
    bool replaceVertexColors=false;
    bool replaceMaterialDiffuse=false;
};
// Exact table2A1478 and193500/193740 artwork calls. The common component's
// recursive1B8FE0 children remain an explicit external composition boundary.
std::vector<OriginalModeMenuDraw> originalModeMenuDraws(const OriginalModeMenuState& state);
class OriginalModeMenu {
public:
    void load(const std::filesystem::path& root);
    const std::vector<std::uint32_t>& paint(int width,int height,const OriginalModeMenuState& state,
        std::array<int,2> warningChunks={-1,-1});
    // Geometry/sample cache only, capped at32MiB; no per-animation-frame images.
    std::size_t preparedRasterBytes()const;
private:
    struct RasterCache;
    std::shared_ptr<RasterCache> raster;
    NativeModel common,mode;
    NativeTextureBank commonTextures,modeTextures;
    std::array<NativeModel,2> warnings;
    std::array<NativeTextureBank,2> warningTextures;
    std::vector<std::uint32_t> pixels;
    std::vector<std::uint32_t> originalCanvas;
    std::vector<std::uint32_t> previousKey;
    void renderOriginalCanvas(const OriginalModeMenuState& state,std::array<int,2> warningChunks);
};
}
