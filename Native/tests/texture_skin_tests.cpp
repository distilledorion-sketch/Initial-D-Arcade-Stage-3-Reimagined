#include "texture_skins.h"
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
using idas3::NativeImage;

int main(int argc, char** argv) try {
    if (argc != 2) throw std::runtime_error("Supply a private test directory.");
    const auto root = fs::absolute(argv[1]) / std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto assets = root / "InitialDUnity_Data/StreamingAssets/IDAS3/data/original_assets";
    const NativeImage stock{2, 1, {0x26345678u, 0xfdffffffu}};
    // Two RGBA pixels. Their encoded alpha deliberately differs from the stock alpha.
    const unsigned char png[]{
        0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0,0,0,0x0d,0x49,0x48,0x44,0x52,0,0,0,2,0,0,0,1,
        8,6,0,0,0,0xf4,0x22,0x7f,0x8a,0,0,0,0x11,0x49,0x44,0x41,0x54,0x78,0x9c,0x63,0x60,
        0x5e,0xfe,0xe7,0xff,0xff,6,0x16,9,0,0x15,0x31,4,0x41,0x0b,0x17,0xf9,0x2c,0,0,0,0,
        0x49,0x45,0x4e,0x44,0xae,0x42,0x60,0x82
    };
    auto check = [](bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); };
    for (const auto& [bank, folder] : {std::pair{"cars/toyota_ae86t/textures", "Cars/toyota_ae86t"},
            {"hud/game2d/textures", "HUD/game2d"}, {"menus/game2d/textures", "HUD/game2d_sprites"},
            {"navigation/ez", "HUD/navigation/ez"}, {"hud/faces/driver/textures", "HUD/faces/driver"}}) {
        auto source = assets / bank / "textures.idastex";
        auto path = root / "Mods" / folder / "texture_001.png";
        fs::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(png), sizeof(png));
        std::array images{stock, stock};
        idas3::applyTextureSkin(source, images);
        check(images[0].argb == stock.argb, "Unselected texture changed");
        const bool car = fs::path(bank).begin()->string() == "cars";
        const std::vector<std::uint32_t> expected = car ? std::vector<std::uint32_t>{0x2603a7fcu, 0xfdff8004u}
                                                       : std::vector<std::uint32_t>{0xff03a7fcu, 0x18ff8004u};
        check(images[1].hasTextureSkin && images[1].argb == expected, "Skin RGB or alpha mismatch");
        images[1] = {1, 2, stock.argb};
        idas3::applyTextureSkin(source, images);
        check(images[1].argb == stock.argb, "Wrong-size PNG was applied");
        std::ofstream(path, std::ios::binary | std::ios::trunc) << "incomplete";
        images[1] = stock;
        idas3::applyTextureSkin(source, images);
        check(images[1].argb == stock.argb, "Damaged PNG changed stock texture");
        fs::remove(path);
        idas3::applyTextureSkin(source, images);
        check(images[1].argb == stock.argb, "Removing skin did not restore stock colors");
    }
    std::cout << "Car, meter and sprite skins passed RGB, alpha, size, damage and removal checks.\n";
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
