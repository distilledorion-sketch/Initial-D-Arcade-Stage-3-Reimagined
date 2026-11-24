#pragma once
#include "native_assets.h"

namespace idas3 {
void applyTextureSkin(const std::filesystem::path& bank, std::span<NativeImage> images);
}
