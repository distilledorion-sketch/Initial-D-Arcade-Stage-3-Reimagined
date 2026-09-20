#include "menu_font.h"
#include "unity_ui_capture.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace idas3 {
namespace {
std::uint32_t word(const std::vector<unsigned char>& bytes, std::size_t at) {
    if (at + 4 > bytes.size()) throw std::runtime_error("Truncated menu font");
    return unsigned(bytes[at]) | (unsigned(bytes[at + 1]) << 8) |
           (unsigned(bytes[at + 2]) << 16) | (unsigned(bytes[at + 3]) << 24);
}
float real(const std::vector<unsigned char>& bytes, std::size_t at) {
    const auto value = word(bytes, at);
    float out = 0;
    std::memcpy(&out, &value, 4);
    return out;
}
}

MenuFont MenuFont::load(const std::filesystem::path& root) {
    MenuFont font;
    std::ifstream in(root / "data/native_assets/menu_font/font.bin", std::ios::binary);
    if (!in) return font;
    const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)), {});
    if (bytes.size() < 24 || std::memcmp(bytes.data(), "IDMF", 4) || word(bytes, 4) != 1)
        throw std::runtime_error("Unrecognised menu font asset");
    const auto width = word(bytes, 8), height = word(bytes, 12);
    font.baked_ = float(word(bytes, 16));
    const auto count = word(bytes, 20);
    if (!width || !height || width > 8192 || height > 8192 || count > 512 || font.baked_ <= 0)
        throw std::runtime_error("Invalid menu font header");
    std::size_t at = 24;
    for (unsigned i = 0; i < count; ++i, at += 24) {
        const auto code = word(bytes, at);
        Glyph glyph{real(bytes, at + 4), real(bytes, at + 8), real(bytes, at + 12),
                    real(bytes, at + 16), real(bytes, at + 20), true};
        if (code < font.glyphs_.size()) font.glyphs_[code] = glyph;
    }
    const std::size_t pixels = std::size_t(width) * height;
    if (bytes.size() != at + pixels * 4) throw std::runtime_error("Menu font pixel extent mismatch");
    font.atlas_.width = width;
    font.atlas_.height = height;
    font.atlas_.argb.resize(pixels);
    for (std::size_t i = 0; i < pixels; ++i) font.atlas_.argb[i] = word(bytes, at + i * 4);
    return font;
}

float MenuFont::width(std::string_view text, float pixelHeight) const {
    if (!ready()) return 0;
    const float scale = pixelHeight / baked_;
    float total = 0;
    for (const unsigned char c : text) {
        const auto& glyph = glyphs_[c < glyphs_.size() ? c : ' '];
        total += (glyph.present ? glyph.advance : glyphs_[' '].advance) * scale;
    }
    return total;
}

void MenuFont::blit(std::span<std::uint32_t> target, int targetWidth, int targetHeight,
                    const Glyph& glyph, float x, float y, float scale, std::uint32_t argb) const {
    const int w = int(std::lround(glyph.w * scale)), h = int(std::lround(glyph.h * scale));
    if (w <= 0 || h <= 0) return;
    // Unity's UI is a triangle list, not a pixel buffer: raw writes are
    // invisible there. The atlas never changes, so it registers once and each
    // glyph is one textured quad tinted by its vertex colour.
    if (unityUiEnabled()) {
        const float left = float(std::lround(x)), top = float(std::lround(y));
        const float u0 = glyph.x / float(atlas_.width), v0 = glyph.y / float(atlas_.height);
        const float u1 = (glyph.x + glyph.w) / float(atlas_.width);
        const float v1 = (glyph.y + glyph.h) / float(atlas_.height);
        const UnityUiVertex a{left, top, u0, v0, argb, 0}, b{left + float(w), top, u1, v0, argb, 0},
                            c{left, top + float(h), u0, v1, argb, 0},
                            d{left + float(w), top + float(h), u1, v1, argb, 0};
        // The flat-fill mode unityUiSolid uses ignores the texture and paints the
        // whole quad, which turns every glyph into a block. This is the sprite
        // compositor's own textured mode: sample the atlas, modulate by the
        // vertex colour, blend on alpha.
        constexpr std::uint32_t tsp = (1u << 15) | (1u << 16) | (1u << 20) | (3u << 6);
        unityUiTriangle(target.data(), targetWidth, targetHeight, atlas_, a, b, c, 1, tsp, false, 0x0a);
        unityUiTriangle(target.data(), targetWidth, targetHeight, atlas_, c, b, d, 1, tsp, false, 0x0a);
        return;
    }
    const unsigned tint[3]{(argb >> 16) & 255, (argb >> 8) & 255, argb & 255};
    const unsigned opacity = (argb >> 24) & 255;
    // The atlas is baked at 48px and the screen wants 10..30, so every
    // destination pixel averages the source rectangle it covers. Point
    // sampling at those ratios drops whole strokes -- it was turning the
    // small dates' zeros into Ds.
    const float step = 1.f / scale;
    for (int row = 0; row < h; ++row) {
        const int destinationY = int(std::lround(y)) + row;
        if (destinationY < 0 || destinationY >= targetHeight) continue;
        const float y0 = glyph.y + float(row) * step, y1 = y0 + step;
        const auto firstRow = std::size_t(std::max(0.f, std::floor(y0)));
        const auto lastRow = std::size_t(std::max(y0 + 1.f, std::ceil(y1)));
        for (int column = 0; column < w; ++column) {
            const int destinationX = int(std::lround(x)) + column;
            if (destinationX < 0 || destinationX >= targetWidth) continue;
            const float x0 = glyph.x + float(column) * step, x1 = x0 + step;
            const auto firstColumn = std::size_t(std::max(0.f, std::floor(x0)));
            const auto lastColumn = std::size_t(std::max(x0 + 1.f, std::ceil(x1)));
            unsigned total = 0, taken = 0;
            for (auto sy = firstRow; sy < lastRow && sy < atlas_.height; ++sy)
                for (auto sx = firstColumn; sx < lastColumn && sx < atlas_.width; ++sx, ++taken)
                    total += (atlas_.argb[sy * atlas_.width + sx] >> 24) & 255;
            if (!taken) continue;
            const unsigned alpha = total / taken * opacity / 255;
            if (!alpha) continue;
            auto& destination = target[std::size_t(destinationY) * targetWidth + destinationX];
            const auto mix = [&](unsigned shift, unsigned value) {
                const unsigned under = (destination >> shift) & 255;
                return ((value * alpha + under * (255 - alpha)) / 255) << shift;
            };
            destination = 0xff000000u | mix(16, tint[0]) | mix(8, tint[1]) | mix(0, tint[2]);
        }
    }
}

void MenuFont::paint(std::span<std::uint32_t> target, int targetWidth, int targetHeight,
                     std::string_view text, float x, float y, float pixelHeight,
                     std::uint32_t argb, std::uint32_t outline, float outlineWidth) const {
    if (!ready() || targetWidth <= 0 || targetHeight <= 0) return;
    const float scale = pixelHeight / baked_;
    float pen = x;
    for (const unsigned char c : text) {
        const auto& glyph = glyphs_[c < glyphs_.size() ? c : ' '];
        if (!glyph.present) { pen += glyphs_[' '].advance * scale; continue; }
        if (outlineWidth > 0) {
            const int reach = std::max(1, int(std::lround(outlineWidth)));
            // Filling the whole ring costs one quad per offset under the host,
            // which is thousands for a screen of captions. Eight offsets at the
            // ring's edge read the same at these sizes.
            if (unityUiEnabled()) {
                const float r = float(reach);
                for (const auto& step : {std::pair{-r, -r}, {0.f, -r}, {r, -r}, {-r, 0.f},
                                         {r, 0.f}, {-r, r}, {0.f, r}, {r, r}})
                    blit(target, targetWidth, targetHeight, glyph,
                         pen + step.first, y + step.second, scale, outline);
            } else {
                for (int dy = -reach; dy <= reach; ++dy)
                    for (int dx = -reach; dx <= reach; ++dx)
                        if (dx || dy)
                            blit(target, targetWidth, targetHeight, glyph,
                                 pen + float(dx), y + float(dy), scale, outline);
            }
        }
        blit(target, targetWidth, targetHeight, glyph, pen, y, scale, argb);
        pen += glyph.advance * scale;
    }
}
}
