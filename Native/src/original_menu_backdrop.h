#pragma once
#include "native_assets.h"

namespace idas3::original {
// Build the complete staggered logo pattern from the installed arcade artwork.
NativeImage buildMenuLogoBackdrop(const NativeTextureBank&,const NativeModelChunk&);
void paintMenuLogoBackdrop(std::span<std::uint32_t>,int width,int height,const NativeImage&,
                          std::uint64_t frame,bool marginsOnly=false);
}
