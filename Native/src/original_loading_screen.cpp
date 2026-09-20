#include "original_loading_screen.h"
#include <algorithm>
#include <stdexcept>
#include <string>

namespace idas3 {
namespace {
constexpr const char* directory = "data/original_assets/loading";
// All source paintings, including PAL8 art decoded with the original program palettes.
constexpr std::uint32_t artworkNumbers[]{0,1,2,3,4,5,6,7,8,9,10,11,12};

std::filesystem::path bankPath(const std::filesystem::path& root, const char* name) {
    return root / directory / name;
}
}

bool OriginalLoadingScreen::available(const std::filesystem::path& root) {
    std::error_code ec;
    return std::filesystem::exists(bankPath(root, "loadcmn") / "loadcmn.idasmesh", ec) &&
           std::filesystem::exists(bankPath(root, "loadwait") / "loadwait.idasmesh", ec);
}

void OriginalLoadingScreen::load(const std::filesystem::path& root) {
    if (loaded_) return;
    const auto bank = [&](const char* name) {
        const auto folder = bankPath(root, name);
        return Bank{NativeModel::load(folder / (std::string(name) + ".idasmesh")),
                    NativeTextureBank::load(folder / "textures" / "textures.idastex")};
    };
    common_ = bank("loadcmn");
    wait_ = bank("loadwait");
    artwork_.clear();
    for (const auto number : artworkNumbers) {
        const std::string name = "load" + (number < 10 ? std::string("0") : std::string()) +
                                 std::to_string(number);
        std::error_code ec;
        if (!std::filesystem::exists(bankPath(root, name.c_str()) / "textures" / "textures.idastex", ec))
            continue;
        artwork_.push_back({bank(name.c_str()), number});
    }
    if (artwork_.empty()) throw std::runtime_error("No loading artwork could be decoded");
    loaded_ = true;
}

void OriginalLoadingScreen::begin(std::uint32_t sequence) {
    chosen_ = artwork_.empty() ? 0 : sequence % std::uint32_t(artwork_.size());
}

void OriginalLoadingScreen::paintBank(std::span<std::uint32_t> target, int width, int height,
                                      const Bank& bank, int onlyChunk) const {
    // Authored at a hundred units per pixel with y upward, fitted to whatever
    // canvas the caller paints on.
    const float fit = std::min(float(width) / 640.f, float(height) / 480.f);
    SpritePlacement placement;
    placement.invertY = true;
    placement.authoredHeight = 0;
    placement.scale = unitsPerPixel * fit;
    placement.offsetX = (float(width) - 640.f * fit) * .5f;
    placement.offsetY = (float(height) - 480.f * fit) * .5f;
    for (std::size_t index = 0; index < bank.model.chunks.size(); ++index) {
        if (onlyChunk >= 0 && std::size_t(onlyChunk) != index) continue;
        compositeOriginalMenuChunk(target, width, height, bank.textures,
                                   bank.model.chunks[index], placement);
    }
}

void OriginalLoadingScreen::paint(std::span<std::uint32_t> target, int width, int height) const {
    if (!loaded_) throw std::logic_error("Loading screen painted before it was loaded");
    if (width <= 0 || height <= 0 || target.size() != std::size_t(width) * height)
        throw std::runtime_error("Invalid loading screen destination");
    const auto& showing = artwork_.at(chosen_);
    paintBank(target, width, height, showing.bank, -1);
    paintBank(target, width, height, common_, int(showing.number));
    // Keep the original artwork and caption without the separate green
    // PLEASE WAIT overlay.
}
}
