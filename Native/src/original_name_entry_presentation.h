#pragma once
#include "native_assets.h"
#include "original_name_entry.h"
#include "original_showroom.h"
#include <array>
#include <span>
#include <vector>

namespace idas3 {
struct OriginalNameEntryDraw {
    unsigned bank=0,chunk=0;
    float x=0,y=0,z=0,scale=1;
    // -1 retains authored UVs/colors. Glyph UVs follow1B1820's atlas addressing.
    int glyph=-1,pulseAlpha=-1;
};
// Native presentation of1263E0's V3 children1B1300/1B1CA0.
// advance is an original60Hz draw tick; paint/drawList are read-only, including
// repeated rendering while paused. No operating-system font substitutes.
class OriginalNameEntryPresentation {
public:
    static OriginalNameEntryPresentation load(const std::filesystem::path& root);
    void reset();
    void advance(const original::OriginalNameEntryState& state);
    std::vector<OriginalNameEntryDraw> drawList(const original::OriginalNameEntryState& state)const;
    std::vector<OriginalNameEntryDraw> timerDraws(std::uint32_t sharedCountdown)const;
    // bank0 is the legacy select wrapper's polygon bank, not a V3 bank index.
    std::vector<OriginalNameEntryDraw> legacyDraws(const original::OriginalNameEntryState& state)const;
    NativeModelChunk materialize(const OriginalNameEntryDraw& draw)const;
    void paintBackground(std::span<std::uint32_t> canvas,int width,int height)const;
    // Original depth order: background, car, name backing, slots glow,
    // foreground, cursor glow, legacy label, full-screen source fade. All calls are read-only.
    void paintNameBacking(std::span<std::uint32_t> canvas,int width,int height)const;
    void paint(std::span<std::uint32_t> canvas,int width,int height,
        const original::OriginalNameEntryState& state,std::uint32_t sharedCountdown)const;
    // Clear canvas to zero first. RGB is already weighted by source alpha;
    // composite using ONE + ONE, never SRC_ALPHA again. Ignore carrier alpha.
    void paintGlow(std::span<std::uint32_t> canvas,int width,int height,
        const original::OriginalNameEntryState& state)const;
    void paintCursor(std::span<std::uint32_t> canvas,int width,int height,
        const original::OriginalNameEntryState& state)const;
    void paintLegacy(std::span<std::uint32_t> canvas,int width,int height,
        const original::OriginalNameEntryState& state)const;
    OriginalShowroomFrame showroomFrame(unsigned car)const{return showroom_.transmissionFrame(car);}
    std::array<float,2> cursor()const{return cursor_;}
    float pulsePhase()const{return pulse_;}
    std::uint64_t frames()const{return frames_;}
private:
    void paintDraws(std::span<std::uint32_t> canvas,int width,int height,
        std::vector<OriginalNameEntryDraw> draws,bool additive)const;
    std::array<NativeModel,4> models_;
    std::array<NativeTextureBank,4> textures_;
    std::array<std::array<float,3>,63> positions_{};
    std::array<float,2> previousInput_{.4f,-1.08f},previousOutput_{.4f,-1.08f},cursor_{.4f,-1.08f};
    float pulse_=1;
    unsigned alpha_=255;
    std::uint64_t frames_=0;
    OriginalShowroom showroom_;
};
}
