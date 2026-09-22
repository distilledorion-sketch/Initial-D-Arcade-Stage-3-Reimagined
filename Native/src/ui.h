#pragma once
#include "course.h"
#include "physics.h"
#include "race.h"
#include "original_hud.h"
#include "original_results.h"
#include "original_battle_hud.h"
#include "original_battle_names.h"
#include "host_platform.h"
#if defined(IDAS3_PORTABLE_SCENE)
#include "menu_font.h"
#endif
#include <map>
#include <string>
#include <cstdint>

namespace idas3 {
class Frontend;
// Presentation only: an online peer is not a Legend/Bunta battle owner.
struct OnlineBattleHudState {
    bool active=false;
    std::uint32_t playerCar=0,rivalCar=0;
    std::string playerName,rivalName;
    float advantage=0,rivalPositionFraction=0;
    std::int32_t frame=0;
};
struct HudBattlePresentation {
    bool online=false,mirrorEnabled=false;
    float signedAdvantage=0;
    std::uint32_t profileMode=0,game2dCommands=0,portraitCommands=0,playerGlyphs=0,rivalGlyphs=0;
    std::uint32_t localMapMarkers=0,opponentMapMarkers=0,sectionCount=0,sectionCapacity=0,elapsed6000=0;
    std::array<std::uint32_t,4> cumulativeSections{},renderedSectionDurations{};
    std::string playerName,rivalName,playerCarCode,rivalCarCode;
};
struct UiState {
    bool menu=true,paused=false,wet=false,night=false,automatic=true,debug=false,gamepad=false,originalHandling=false;
    bool originalWeatherScenery=true,snow=false;
    bool showControls=false;
    bool suppressPauseOverlay=false;
    bool multiplayer=false;
    OnlineBattleHudState onlineBattleHud;
    bool timeExtended=false,extendedCountdown=false;
    bool rearView=false;
    std::int32_t displayedRemaining6000=0;
    bool useDisplayedRemaining=false;
    bool battle=false,battleWon=false;
    std::uint32_t battleEnemy=0,battleProfileMode=0,battleRivalCar=0;
    std::int32_t battleHudFrame=0;
    unsigned hudIntroFrame=240; // Source race-owner age; replay/diagnostic default is settled.
    float battleAdvantage=0,battleRivalPositionFraction=0;
    int carProfile=0;
    const Course* course=nullptr;
    const VehicleState* car=nullptr;
    // The opponent, for the course map indicator; null outside a battle.
    const VehicleState* rival=nullptr;
    const RaceClock* race=nullptr;
    // The race-end announcement holds the road view before the results.
    OriginalHudState::FinishBanner finishBanner=OriginalHudState::FinishBanner::none;
    const OriginalResultsState* results=nullptr;
    const OriginalBattleResultsState* battleResults=nullptr;
    double bestTime=0;
    float progress=0,fps=0;
    std::string message,musicName;
    Frontend* frontend=nullptr;
};
class Hud {
public:
    ~Hud();
    void loadOriginal(const std::filesystem::path& root);
    void resize(int w,int h);
    void setMapSize(int size){mapSize_=size>=0&&size<=2?size:0;}
    void setMapZoom(int zoom){mapZoom_=zoom>=0&&zoom<=2?zoom:2;}
    const std::uint32_t* paint(const UiState& s);
    const std::uint32_t* paintResult(const OriginalBattleResultsState&,
        std::span<const std::uint32_t> tuningOverlay,bool paused,bool showControls,bool suppressPauseOverlay=false);
    // Read-only, for the development HUD chunk dump that identifies an
    // element which is not ported yet.
    const OriginalRaceHud& originalBank()const{return originalHud;}
    // The surface key the Unity UI capture identifies this overlay by.
    const std::uint32_t* overlayPixels()const{return pixels;}
    // Submissions from the most recent actual paint, never a diagnostic redraw.
    const HudBattlePresentation& lastBattlePresentation()const{return battlePresentation_;}
private:
    friend struct CourseMapTestAccess;
#if defined(IDAS3_PORTABLE_SCENE)
    std::vector<std::uint32_t> scenePixels_;
    MenuFont sceneFont_;
#else
    HDC dc=nullptr;HBITMAP bitmap=nullptr;HGDIOBJ originalBitmap=nullptr;
#endif
    std::uint32_t* pixels=nullptr;int width=0,height=0;
    // Desktop panels use a uniform logical canvas. Original sprite painters
    // retain their own source layout and edge anchors in the actual viewport.
    float sx=1,sy=1,offsetX=0,offsetY=0;
#if !defined(IDAS3_PORTABLE_SCENE)
    std::map<int,HFONT> fonts;
#endif
    OriginalRaceHud originalHud;
    OriginalTimeAttackResults originalResults;
    OriginalBattleResults originalBattleResults;
    OriginalBattleHudAssets originalBattleHud;
    OriginalBattleNames originalBattleNames;
    OriginalBattleHudAnimation battleAnimation,battleFrameAnimation;
    HudBattlePresentation battlePresentation_;
    struct PortraitAssets {NativeModel model;NativeTextureBank textures;};
    std::map<std::string,PortraitAssets> portraits;
    std::int32_t lastBattleFrame=-1;
    std::uint32_t lastBattleEnemy=0xffffffff;
    std::uint32_t lastBattleProfileMode=0xffffffff;
    bool originalHudReady=false;
    void rect(float x,float y,float w,float h,COLORREF c);
    void line(float x,float y,float xx,float yy,COLORREF c,int thick=1);
    void text(float x,float y,const std::string& text,int size,COLORREF c,bool bold=false);
    void label(float x,float y,const std::string& text,int size,COLORREF c,bool bold=false);
    // The source draws a window of the course around the player rather than
    // the whole route: the view turns with the car so the road ahead is always
    // up, the player sits fixed inside the frame, and the opponent appears
    // only while inside that window.
    void map(const Course& course,float x,float y,float w,float h,
        const VehicleState& player,const VehicleState* rival);
    // The whole authored route, for the course-selection panel.
    void routePreview(const Course& course,float x,float y,float w,float h);
    // The map field is see-through in the cabinet artwork, so it writes its
    // own alpha into the overlay rather than going through the solid fill.
    // Device-space map primitives. Unity collects UI triangles rather than
    // reading the pixel buffer, so these carry their own alpha through the
    // same helpers the rest of the HUD uses.
    void mapFill(float x,float y,float w,float h,COLORREF color,unsigned alpha);
    void mapStroke(float x0,float y0,float x1,float y1,COLORREF color,float thickness);
    // The desktop path forces alpha onto everything GDI drew, which would
    // flatten the map's see-through field. The map owns its own alpha, so it
    // records its frame and that pass leaves it alone.
    int mapSize_=0,mapZoom_=2;
    int mapX0_=0,mapY0_=0,mapX1_=0,mapY1_=0;
};
}
