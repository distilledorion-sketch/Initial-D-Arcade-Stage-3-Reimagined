#pragma once
#include "native_assets.h"
#include "original_battle_profile.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace idas3 {
// The banner the source shows over the two cars on the start line: the VS mark,
// the course, which race of the set this is, and the conditions it runs under.
// Multiplayer can replace that metadata with each player's battle record.
//
// `model/start2d` authors every piece at one hundred units per pixel with y
// running upward, the same as the loading screen, so nothing here is placed by
// eye in the source asset. Compact metadata uses the recovered source camera
// projection; imported title art extends its baseline and word spacing.
// Names and the animated VS mark retain their recovered source placements.
//
// Camera/showcase ownership is separate. tick() is the recovered 60Hz
// iStart2D owner; its terminal frame remains until the caller removes it.
struct OriginalVsBannerSetup {
    // Myogi, Usui, Akagi, Akina, Happogahara, Irohazaka, Shomaru,
    // Tsuchisaka, Akina Snow (the game's course order).
    std::uint32_t course = 0;
    // 0 uphill, 1 downhill, 2 outbound, 3 inbound, 4 clockwise,
    // 5 counter-clockwise, 6 reverse. Which of the seven a course shows for
    // each of its two directions is selected by source table0C2FC994. Callers
    // supply the semantic direction; drawDirection can suppress the word.
    std::uint32_t direction = 0;
    bool drawDirection = true;
    bool night = false, wet = false, snow = false;
    // 1..5 draws RACE and that digit. 0 draws FINAL, and extra draws EXTRA.
    std::uint32_t race = 1;
    bool extra = false;
    // The bank's full-screen chunk. Drawn on its own the banner wants it; laid
    // over the two cars on the start line it must not cover them, so the race
    // clears this and keeps only the words and the mark.
    bool drawBackdrop = true;
    original::OriginalBattleProfile profile{};
    std::uint32_t enemy = 0;
    bool showVersus = true;
    // Online display names use the same original glyph bank. Unsupported
    // codepoints become '?' and long names fit inside the source name area.
    std::string localNameUtf8, opponentNameUtf8;
    // Multiplayer record rows replace the host course metadata. Index0 is
    // the local/left car, index1 its opponent; values are completed races.
    bool showBattleRecords = false;
    std::array<std::uint32_t,2> battles{}, wins{};
    std::string customCourseName;
    bool compactHeader=false;
};
struct OriginalVsMetadataPlacement {
    int chunk=0;
    float left=0,top=0,width=0,height=0;
};
struct OriginalVsBattleRecordPlacement {
    float left=0,top=0,width=0,height=0,opacity=0;
};
class OriginalVsBanner {
public:
    static constexpr float unitsPerPixel = 100.f;
    static bool available(const std::filesystem::path& root);
    void load(const std::filesystem::path& root);
    bool loaded() const { return loaded_; }
    void begin(const OriginalVsBannerSetup&);
    void tick();
    std::uint32_t sourceTick() const { return sourceTick_; }
    static constexpr std::uint32_t animationSettleTicks = 152;
    bool animationSettled() const { return sourceTick_ >= animationSettleTicks; }
    int activeVsChunk() const { return setup_.showVersus ? vsChunk_ : -1; }
    const std::string& displayName(unsigned side) const;
    std::string profileDisplayName(const original::OriginalBattleProfile&) const;
    static const char* rivalDisplayName(unsigned enemy);
    const std::string& displayBattleRecord(unsigned side) const;
    // Logical640x480 bounds and the exact name-motion opacity used to paint.
    OriginalVsBattleRecordPlacement battleRecordPlacement(unsigned side) const;
    void paint(std::span<std::uint32_t> target, int width, int height) const;
    // Asset inventory retained for existing tooling. The recovered 14B700
    // owner actually selects only group 41..70, at one chunk per source tick.
    std::vector<int> unsequencedZoomChunks() const;
    // Static word selection, with legacy119 representing the VS asset. paint
    // substitutes activeVsChunk() and its recovered (2.89,-2.14) placement.
    std::vector<int> chunks() const;
    // Logical640x480 bounds used by paint(). Compact original-course titles
    // and labels follow the source projection; legacy noncompact is a host layout.
    std::vector<OriginalVsMetadataPlacement> metadataPlacements() const;
    // Imported art shares the visible height/anchor of the source title. Its
    // aspect ratio stays intact instead of squeezing every name to one width.
    OriginalVsMetadataPlacement importedTitlePlacement() const;

private:
    NativeModel model_;
    std::vector<std::array<float,4>> chunkBounds_;
    std::vector<float> chunkDepths_;
    std::array<std::array<float,4>,9> titleInkBounds_{};
    NativeTextureBank textures_;
    NativeTextureBank nameFont_;
    NativeTextureBank recordFont_,importedTitles_;
    std::array<std::uint16_t,9216> nameIndices_{};
    std::array<std::uint32_t,9216> nameUnicode_{};
    std::array<std::array<std::uint8_t,2>,221> profileGlyphs_{};
    std::array<std::array<std::array<std::uint8_t,16>,2>,32> rivalNames_{};
    std::array<std::uint8_t,16> defaultName_{};
    std::array<std::array<float,3>,80> nameMotion_{};
    std::array<std::vector<std::uint16_t>,3> names_;
    std::array<std::string,2> displayNames_;
    std::array<std::string,2> displayBattleRecords_;
    std::array<float,2> nameX_{}, nameY_{}, nameAlpha_{};
    std::uint32_t sourceTick_=0, phaseTick_=0, vsCounter_=0;
    std::uint32_t playerMotion_=0, opponentMotion_=0;
    int phase_=0,vsChunk_=-1;
    OriginalVsBannerSetup setup_;
    bool loaded_ = false;
    void paintChunk(std::span<std::uint32_t> target, int width, int height, int chunk) const;
    OriginalVsMetadataPlacement metadataPlacement(int chunk) const;
    OriginalVsMetadataPlacement projectedMetadataPlacement(int chunk) const;
    OriginalVsMetadataPlacement sourceTitleInkPlacement() const;
    unsigned headerCourse() const;
    void paintNames(std::span<std::uint32_t> target, int width, int height) const;
    void paintBattleRecords(std::span<std::uint32_t> target,int width,int height) const;
    std::vector<std::uint16_t> encodeSource(std::span<const std::uint8_t>) const;
    std::vector<std::uint16_t> encodeUtf8(const std::string&) const;
    std::string decodeName(const std::vector<std::uint16_t>&) const;
};
}
