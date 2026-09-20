#pragma once
#include "native_assets.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>

namespace idas3 {
// A plain variable-width bitmap face for native menu screens, baked by
// tools/make_menu_font.py. The recovered cabinet alphabet is a display face
// with no lowercase and no punctuation to speak of, which does not suit a list
// of save files; this carries its own metrics so the caller places and tints.
class MenuFont {
public:
    static MenuFont load(const std::filesystem::path& root);
    bool ready() const { return !glyphs_.empty() && atlas_.width > 0; }
    float width(std::string_view text, float pixelHeight) const;
    // Draws at pixelHeight, with an optional dark outline behind it the way the
    // source's own captions are outlined.
    void paint(std::span<std::uint32_t> target, int targetWidth, int targetHeight,
               std::string_view text, float x, float y, float pixelHeight,
               std::uint32_t argb, std::uint32_t outline = 0, float outlineWidth = 0) const;

private:
    struct Glyph { float x = 0, y = 0, w = 0, h = 0, advance = 0; bool present = false; };
    NativeImage atlas_;
    std::array<Glyph, 128> glyphs_{};
    float baked_ = 48;
    void blit(std::span<std::uint32_t> target, int targetWidth, int targetHeight,
              const Glyph& glyph, float x, float y, float scale, std::uint32_t argb) const;
};
}
