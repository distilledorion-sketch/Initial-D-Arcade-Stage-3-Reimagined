#pragma once
#include "native_assets.h"

namespace idas3 {
// 0C19DA74..0C19DAAA inverts the current view, clears its camera X/Z,
// inverts again, and draws bank+0x158 chunk0. For a rigid view this is
// equivalent to translating the authored background by camera X/Z only.
// Original inverse rounding is separately measured; no world-unit rescale.
inline NativeModelInstance originalAkinaBackgroundInstance(Vec3 cameraWorld) {
    NativeModelInstance instance;
    instance.chunk=0;
    instance.transform={1,0,0,cameraWorld.x,0,1,0,0,0,0,1,cameraWorld.z,0,0,0,1};
    return instance;
}
inline constexpr const char* originalAkinaBackgroundModel=
    "data/original_models/courses/k_df/background/df_etc_f.idasmesh";
inline constexpr const char* originalAkinaBackgroundTextures=
    "data/original_assets/courses/k_df/background/textures/textures.idastex";
}
