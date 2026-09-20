#pragma once
#include "original_battle_profile.h"
#include "original_rival_dialog_data.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace idas3::original {
// Native rival-dialogue scene: the source script interpreter 0F6D40, its text
// engine (the 0C5420 bank family), the 0F69E0 step machine and the 0FAD60
// outer phase machine, producing an ordinary per-frame draw list. No guest
// memory, display lists or GPU commands; the recovered layout is evaluated
// directly in native floating point.

// One placed glyph, exactly as 0C6A00/0C68E0 append it, in source screen
// pixels on the original 640x480 canvas.
struct OriginalDialogGlyph { std::uint16_t texture{}; float x{},y{},z{}; };

// Source text-line object: 0C60C0 builds the page, 0C5EE0 the speaker label.
struct OriginalDialogLine {
    float cell=32,linePitch=24,scale=1;
    float cursorX=0,cursorY=0,startX=0,startY=0,lineStart=0;
    std::vector<OriginalDialogGlyph> glyphs;
    std::uint32_t color=0;   // +84, zero until the first C command
    float zOffset=0.00001f;
    // Reveal is the source page timer (0C6FE0). The page itself is drawn
    // whole: 0F6880 draws through vtable slot 3, which raises the drawn count
    // to the full glyph count for the draw and restores it afterwards.
    std::uint32_t revealed=0,interval=1,tick=0;
    std::array<std::uint32_t,64> waitGlyph{},waitFrames{};
    std::uint32_t waitHead=0,waitCount=0;
};

enum class OriginalRivalDialogBank : std::uint8_t { Portrait,Background,Scene,Alphabet,Namekana };
struct OriginalRivalDialogDraw {
    OriginalRivalDialogBank bank{};
    std::uint32_t chunk{};
    // Source 1F69D0 scale then 1F6AC0 translate over an identity matrix.
    float scaleX=1,scaleY=1,scaleZ=1,x=0,y=0,z=0;
    // Only glyph draws carry a colour: 0C66E0 writes the line colour into the
    // part's diffuse words. Scene parts keep their own imported material.
    std::uint32_t color=0xffffffffu;
    bool tinted=false;
};

struct OriginalRivalDialogSetup {
    std::uint32_t enemy=0,kind=0,weather=0,playerCar=0;
    // 0F8B00 stores 1F9E60(1000)%7 at 31CE45 for the spectator draw. Supplied
    // by the caller's shared RNG, never invented here.
    std::uint32_t cheer=0;
    // Player name in original JIS pairs, as 0F8780 copies it to dialog+440.
    std::string playerName;
    // Separate0F88C0 configuration: character31, authored per-course Bunta
    // scripts and rival_bunta_challenge artwork. Existing Legend defaults stay.
    bool buntaChallenge=false;
    std::uint32_t buntaCourse=0;
};

struct OriginalRivalDialogState {
    // Outer object (0F8340/0F8780/0FAD60).
    std::uint32_t character=0,kind=0,course=0,night=0,weather=0,variant=0;
    std::uint32_t phase=0;                   // +420
    std::int32_t fade=20;                    // +408
    bool fadeEnabled=true;                   // +412
    bool skip=false;                         // +508
    bool nameVisible=false;                  // +428
    float nameSlide=2.0f,nameSlideRate=0.2f; // +464/+468
    bool completed=false,closed=false;
    // Background element list (0F4660 list, 0F4B40 select, 0F4BC0 apply).
    std::uint32_t picture=0,requestedPicture=0,backgroundWait=0;
    std::uint32_t portraitChunk=0,backgroundChunk=0,spectators=0;
    // Script object (0F5F40).
    std::uint32_t cursor=0,scriptPhase=0,speaker=0,playerCar=0;
    std::uint32_t frame68=0,frame72=0;
    bool endFlag=false;                      // +80
    std::uint32_t revealCounter=0,revealThreshold=0;  // +376/+380
    bool request2=false,request4=false;       // bytes +600/+601
    std::array<std::uint32_t,21> flags{};     // +384..+464
    std::array<std::uint32_t,10> slotFlags{}; // +332
    std::int32_t textAnchorX=0;               // 31ABD8
    // Scene selection bytes at 31CE45..31CE48; 31CE45 is the loader's random.
    std::uint32_t cheer=0,sceneByte46=0,sceneByte47=0,sceneByte48=0;
    OriginalDialogLine text,name;
    std::string playerName;
    bool initialized=false;
    bool buntaChallenge=false;
};

// Once per scene. Runs the 0F8160 prescan, whose result is the opening page
// picture, then selects and applies that background element.
void resetOriginalRivalDialog(OriginalRivalDialogState&,const OriginalRivalDialogData&,
    const OriginalBattleProfile&,const OriginalRivalDialogSetup&);
// Exactly one source owner update (0FAD60). Returns the 0F69E0 status.
std::uint32_t stepOriginalRivalDialog(OriginalRivalDialogState&,const OriginalRivalDialogData&,
    const OriginalBattleProfile&);
// 0FAA40 -> 0FA5E0: rebuild the script for another authored page of this rival.
void advanceOriginalRivalDialogPage(OriginalRivalDialogState&,const OriginalRivalDialogData&,
    const OriginalBattleProfile&,std::int32_t delta);
inline bool originalRivalDialogReady(const OriginalRivalDialogState& s){return s.phase==2;}
void closeOriginalRivalDialog(OriginalRivalDialogState&);   // 0FAAA0
void skipOriginalRivalDialog(OriginalRivalDialogState&);    // 0FAD20
// Portrait, background, page text, speaker label, player-name plate and the
// window frame. The ten randomised spectator sprites of 0FB240 are not
// ported yet and are absent from this list.
std::vector<OriginalRivalDialogDraw> originalRivalDialogDraws(const OriginalRivalDialogState&);
std::uint32_t originalRivalDialogFadeArgb(const OriginalRivalDialogState&);
// The player's name in original JIS pairs, from the profile's name-entry
// glyphs. Empty when the profile carries no entered name.
std::string originalRivalDialogPlayerName(const OriginalBattleProfile&);
// Original 1FA280 rational tangent of a 16-bit angle, in source float order.
float originalDialogTangent(std::uint16_t angle);
// Imported bank folders for the selected scene, as the source paths resolve.
std::string originalRivalDialogPortraitBank(const OriginalRivalDialogData&,std::uint32_t enemy);
std::string originalRivalDialogBackgroundBank(const OriginalRivalDialogData&,std::uint32_t enemy,
    std::uint32_t weather);
}
