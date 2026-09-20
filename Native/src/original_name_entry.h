#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace idas3::original {
struct OriginalNameGlyph {std::array<std::uint8_t,2> bytes{};};
struct OriginalNameKeyboardRecord {std::array<std::uint32_t,6> words{};};
struct OriginalNameEntryTables {
    std::array<OriginalNameGlyph,221> glyphs;
    std::array<OriginalNameKeyboardRecord,173> keyboard;
    std::vector<std::pair<std::string,std::string>> substitutions;
    static OriginalNameEntryTables load(const std::filesystem::path& root);
    std::string encodedName(const std::array<std::uint32_t,5>& glyphIds,unsigned length)const;
};
struct OriginalNameEntrySelector {
    std::uint32_t selected=0,phase=0,minimum=0,maximum=52;
    bool wrap=true;
    float progress=0,velocity=0,deltaX=0,deltaY=0;
};
struct OriginalNameEntryState {
    float cursorX452=0,cursorY456=0;
    std::uint32_t selected460=0,blinkArgb468=0xffffffff,blinkPhase472=0,blinkFrame476=0;
    std::array<std::uint32_t,5> glyphIds480{220,220,220,220,220},keyboardIds484{};
    std::uint32_t length528=0,frame572=0,fade576=15,fadeEnabled584=0,phase588=0,hold592=0;
    std::uint8_t activeOverlay596=1,timedOut604=0;
    std::uint32_t page600=2,sharedCountdown1176=4879,profileFlags1180=0,parentEvent64=0;
    std::uint8_t profileKind1192=0;
    // Caller-owned selected-card record state returned by16D680(0). The native
    // saved-driver boundary defaults to10; no card hardware is simulated here.
    std::int32_t selectedCardState96=10;
    std::uint32_t previousScreen76=0x0801,alternateScreen80=0x0802;
    OriginalNameEntrySelector selector;
};
struct OriginalNameEntryInput {
    float steering=0; // Original0D4340 steering axis, normalized-1..1.
    bool confirmPressed=false,backPressed=false;
    int rowJump=0; // Original digital row controls:-1 or+1.
};
struct OriginalNameEntryEvents {
    std::vector<unsigned> cueIds;
    bool nameCommitted=false,parentRequested=false,substituted=false,requestCardState10=false;
};
// Initialize126C20's controller fields. ProfileKind2 imports the supplied name;
// other source entry kinds start empty. The shared timer is reset to4879.
void initializeOriginalNameEntry(OriginalNameEntryState& state,
    const std::array<std::uint32_t,5>& existingGlyphIds={},unsigned existingLength=0);
OriginalNameEntryEvents tickOriginalNameEntry(OriginalNameEntryState&,
    const OriginalNameEntryInput&,const OriginalNameEntryTables&);
std::uint32_t originalNameEntryFadeArgb(const OriginalNameEntryState&);
// Source selector positions initialized by126C20; input and painter share them.
std::array<float,2> originalNameEntryTilePosition(unsigned tile);
}
