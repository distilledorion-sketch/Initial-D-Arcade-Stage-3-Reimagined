#include "texture_skins.h"
#ifdef _WIN32
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <cstdio>

namespace {
namespace fs = std::filesystem;
using Microsoft::WRL::ComPtr;

fs::path skinFolder(const fs::path& bank) {
    if (bank.filename() != "textures.idastex") return {};
    const auto parent = bank.parent_path();
    const bool navigation = parent.parent_path().filename() == "navigation";
    if (!navigation && parent.filename() != "textures") return {};
    const auto group = navigation ? parent : parent.parent_path();
    const auto owner = group.parent_path();
    const bool portrait = owner.filename() == "faces" && owner.parent_path().filename() == "hud";
    const auto category = portrait ? owner.parent_path() : owner;
    const auto assets = category.parent_path();
    const auto data = assets.parent_path();
    const auto cache = data.parent_path();
    const auto streaming = cache.parent_path();
    const auto player = streaming.parent_path();
    if (assets.filename() != "original_assets" || data.filename() != "data" || cache.filename() != "IDAS3" ||
        streaming.filename() != "StreamingAssets" || player.filename() != "InitialDUnity_Data") return {};
    const auto mods = player.parent_path() / "Mods";
    if (navigation) return mods / "HUD/navigation" / group.filename();
    if (portrait) return mods / "HUD/faces" / group.filename();
    if (category.filename() == "cars") return mods / "Cars" / group.filename();
    if (category.filename() == "hud") return mods / "HUD" / group.filename();
    if (category.filename() == "menus" && group.filename() == "game2d") return mods / "HUD/game2d_sprites";
    return {};
}

bool readColors(IWICImagingFactory* factory, const fs::path& path, idas3::NativeImage& image, bool keepOriginalAlpha) {
    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
            WICDecodeMetadataCacheOnDemand, &decoder))) return false;
    GUID format{};
    ComPtr<IWICBitmapFrameDecode> frame;
    UINT width = 0, height = 0;
    if (FAILED(decoder->GetContainerFormat(&format)) || format != GUID_ContainerFormatPng ||
        FAILED(decoder->GetFrame(0, &frame)) || FAILED(frame->GetSize(&width, &height)) ||
        width != image.width || height != image.height) return false;
    ComPtr<IWICFormatConverter> converter;
    if (FAILED(factory->CreateFormatConverter(&converter)) || FAILED(converter->Initialize(frame.Get(),
            GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom))) return false;
    std::vector<std::uint32_t> colors(image.argb.size());
    if (FAILED(converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(colors.size() * 4),
            reinterpret_cast<BYTE*>(colors.data())))) return false;
    if (keepOriginalAlpha) {
        for (std::size_t i = 0; i < colors.size(); ++i)
            colors[i] = (image.argb[i] & 0xff000000u) | (colors[i] & 0x00ffffffu);
    }
    image.argb.swap(colors);
    image.hasTextureSkin = true;
    return true;
}
}
#endif

void idas3::applyTextureSkin(const std::filesystem::path& bank, std::span<NativeImage> images) {
#ifdef _WIN32
    const auto folder = skinFolder(bank);
    std::error_code error;
    if (folder.empty() || !fs::is_directory(folder, error)) return;
    const bool carSkin = folder.parent_path().filename() == "Cars";
    const HRESULT apartment = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(apartment) && apartment != RPC_E_CHANGED_MODE) return;
    struct ApartmentScope {
        bool release;
        ~ApartmentScope() { if (release) CoUninitialize(); }
    } scope{SUCCEEDED(apartment)};
    ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) return;
    for (std::size_t i = 0; i < images.size(); ++i) {
        char filename[32];
        std::snprintf(filename, sizeof(filename), "texture_%03zu.png", i);
        const auto replacement = folder / filename;
        if (!fs::is_regular_file(replacement, error)) continue;
        if (fs::file_size(replacement, error) > 24u * 1024u * 1024u || error ||
            !readColors(factory.Get(), replacement, images[i], carSkin))
            std::fwprintf(stderr, L"Skipped skin texture %ls (expected a %u x %u PNG).\n",
                          replacement.c_str(), images[i].width, images[i].height);
    }
#endif
}
