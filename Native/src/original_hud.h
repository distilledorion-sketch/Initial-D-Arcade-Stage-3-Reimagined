#pragma once
#include "native_assets.h"
#include "original_matrix.h"
#include <span>

namespace idas3 {
// Original065032..0650D8, table2F4E48. Optional bytes are the source
// globals31CA38+8 and31CA34; defaults select each car's stock dial.
unsigned originalTachTypeForCar(unsigned carId,std::uint8_t tuningByte8=0,std::uint8_t tuneSelectionByte0=0);
// Host display boundary: keep telemetry within the selected authored dial.
// The original draw routine and the transmission's RPM remain unchanged.
float originalTachDisplayRpm(float rpm,unsigned tachType);
struct OriginalHudState {
    float speedKmh=0,rpm=0;
    int gear=1;
    bool automatic=false;
    // Original display state+20:0=8000,1=9000,2=10000,3=12000 full scale.
    // Caller supplies the selected car's original value; no car-ID guess here.
    unsigned tachType=0;
    std::uint32_t elapsedTicks6000=0;
    // Opt in to the settled original normal-layout TIME / SECTION TIME panel.
    // Inputs are original6000/sec cumulative checkpoints, not section deltas.
    // 065DF0..065E32 subtracts the preceding checkpoint for each visible row.
    bool timePanel=false;
    bool extendedCountdown=false; // Imported courses may start above the original 99-second cap.
    std::int32_t remainingTicks6000=0;
    std::array<std::uint32_t,4> sectionTimes6000{};
    unsigned sectionCount=0;
    unsigned sectionCapacity=4;
    std::uint32_t finishTicks6000=0xffffffff;
    std::uint32_t flags=0x7004; // total time / tach / gear / speed
    bool alternateLayout=false;
    // Native widescreen presentation: translate the intact time/instrument
    // groups toward screen edges, retaining the original uniform scale.
    // The default keeps the recovered centered4:3 layout for comparisons.
    bool edgeAnchored=false;
    bool timeExtended=false;
    // The race-end announcement, drawn from game2d chunks 182 to 185.
    // Each chunk carries its own authored screen quad, all four sharing
    // one position, so they need no placement of their own.
    enum class FinishBanner : std::uint8_t { none,finish,win,lose,timeUp } finishBanner=FinishBanner::none;
};
struct OriginalHudDraw {
    enum class Kind { polygon,sprite } kind=Kind::polygon;
    std::uint32_t index=0,textureOverride=0xffffffff;
    original::OriginalMatrix matrix{};
};
class OriginalRaceHud {
public:
    static OriginalRaceHud load(const std::filesystem::path& gameRoot);
    std::vector<OriginalHudDraw> drawList(const OriginalHudState& state)const;
    // Composites onto an existing native ARGB overlay; does not clear it.
    // 640x480 original display area is fitted uniformly and centered.
    void paint(std::span<std::uint32_t> argb,int width,int height,const OriginalHudState& state)const;
    // Original game2d chunks0/1/2 are1/2/3; chunk3 isGO (texture37).
    // Source countdown age drives the original scale filter and GO fade.
    void paintStartSignal(std::span<std::uint32_t> argb,int width,int height,int digit,unsigned elapsedTicks=1)const;
    // Development diagnostic only: composite one bank chunk on its own,
    // centred by its authored bounds, so an unported HUD element can be
    // identified by eye instead of guessed at. Never used by the game.
    void paintChunk(std::span<std::uint32_t> argb,int width,int height,unsigned chunk)const;
    // Same diagnostic, but at the chunk's OWN authored screen position instead
    // of centred. Original HUD polygons carry their placement in their vertices
    // at 100 units per pixel, so this is where the arcade actually puts it.
    void paintAuthoredChunk(std::span<std::uint32_t> argb,int width,int height,unsigned chunk)const;
    // Places one bank chunk centred on a device position at a given pixel
    // height. The course map draws the source's own indicator spheres, game2d
    // chunks 93 and 94, through this.
    void paintChunkAt(std::span<std::uint32_t> argb,int width,int height,unsigned chunk,
        float centerX,float centerY,float pixelHeight)const;
    unsigned chunkCount()const{return unsigned(model_.chunks.size());}
private:
    NativeModel model_;
    std::size_t maxChunkVertices_=0;
    std::array<Vec3,4> startSignalCenters_{};
    NativeTextureBank modelTextures_,spriteTextures_;
    NativeSpriteBank sprites_;
    original::OriginalFscaTable trig_;
};
}
