#pragma once
#include "native_assets.h"
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace idas3 {
// The screen shown between a chosen course and the race: one of the authored
// paintings, the INITIAL D VERSION 3 mark over it and PLEASE WAIT. The layout
// is the source's own -- `model/loading` authors its quads at one hundred units
// per pixel with y running upward, so nothing here is placed by eye.
//
// All thirteen paintings use their original palettes; artworkCount reports
// the installed pool and begin selects one painting for the whole load.
class OriginalLoadingScreen {
public:
    static constexpr float unitsPerPixel = 100.f;
    static bool available(const std::filesystem::path& root);
    void load(const std::filesystem::path& root);
    bool loaded() const { return loaded_; }
    std::size_t artworkCount() const { return artwork_.size(); }
    // Chooses the painting for one showing. The source picks per race and logs
    // it as "Load BG Number". Cycle the installed pool without repeating a
    // painting until every available painting has been used.
    void begin(std::uint32_t sequence);
    std::uint32_t artwork() const { return artwork_.empty() ? 0 : artwork_[chosen_].number; }
    void paint(std::span<std::uint32_t> target, int width, int height) const;

private:
    struct Bank { NativeModel model; NativeTextureBank textures; };
    // Each painting keeps the number it has in the source set, because the
    // common bank authors one logo placement per painting and they are matched
    // by that number: loadcmn chunk 10 is where the logo sits over load10.
    struct Artwork { Bank bank; std::uint32_t number = 0; };
    std::vector<Artwork> artwork_;
    Bank common_, wait_;
    std::uint32_t chosen_ = 0;
    bool loaded_ = false;
    // onlyChunk selects one authored placement; -1 paints the whole bank.
    void paintBank(std::span<std::uint32_t> target, int width, int height,
                   const Bank&, int onlyChunk) const;
};
}
