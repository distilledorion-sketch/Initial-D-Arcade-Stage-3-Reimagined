#pragma once
#include <stdint.h>
#pragma pack(push,8)
typedef struct Idas3SceneVertex { float position[3],normal[3],color[4],uv[2],offset[4]; } Idas3SceneVertex;
typedef struct Idas3SceneRange {
    uint32_t first,count,texture,tsp,pcw,isp,gmp,flags,gloss,lightScope,viewMask,sourceIndex;
    // flags:1 original,2 emissive,4 billboard,8 showroom light override,
    //16 course geometry,32 ordinary-car source face culling. Neither marker
    // changes lightScope or the authored ISP culling bits.
    float lightDirection[4];
} Idas3SceneRange;
typedef struct Idas3SceneTexture { uint32_t width,height; uint64_t pixelCount; const uint32_t* argb; } Idas3SceneTexture;
typedef struct Idas3SceneOverlay {
    const uint32_t* surface; uint32_t width,height,flags,reserved;
    // flags:1 behind scene,2 additive RGB,4 source640x480 fitted canvas.
    // Surface identifies the UI agent's captured drawlist. Never a game frame.
} Idas3SceneOverlay;
typedef struct Idas3SceneCamera {
    float eye[3],target[3],up[3],verticalFov,aspect,nearClip,farClip,viewport[4];
    // bit0 left-handed; bit1 a panel aperture. Viewport is in native top-left
    // pixel coordinates. A fixed main viewport is widened on a wider display
    // so it reveals more world; a panel aperture is a hole in a menu screen
    // and must keep its rectangle instead.
    uint32_t leftHanded;
} Idas3SceneCamera;
typedef struct Idas3SceneFrame {
    uint32_t size,version; uint64_t frameGeneration,textureGeneration;
    uint32_t width,height,vertexCount,rangeCount,textureCount,overlayCount,viewCount,screenFadeArgb;
    const Idas3SceneVertex* vertices; const Idas3SceneRange* ranges;
    const Idas3SceneTexture* textures; const Idas3SceneOverlay* overlays;
    const float* frameConstants; //2*92floats: original per-view368byte Frame.
    const uint32_t* lightConstants; //2views*4scopes*156words; scope0none/1course/2player/3opponent.
    const uint32_t* fogConstants; //136words: colors/density +128 packed table uints.
    Idas3SceneCamera cameras[2];
} Idas3SceneFrame;
#pragma pack(pop)
#ifdef __cplusplus
#include "native_assets.h"
#include <array>
#include <span>
#include <vector>
namespace idas3 {
class Renderer;class Mesh;struct OverlayPass;struct OriginalShowroomLighting;struct OriginalRearViewFrame;
class UnitySceneCapture {
public:
    void loadTextures(const NativeTextureBank&,bool append);
    void capture(const Renderer&,const Mesh&,Vec3 eye,Vec3 target,bool night,bool wet,const uint32_t* overlay,bool behind,
        const OriginalShowroomLighting*,const OriginalRearViewFrame*,std::span<const OverlayPass>);
    const Idas3SceneFrame& frame()const{return frame_;}
    const std::uint64_t* geometryIds()const{return geometryIds_.data();}
private:
    Idas3SceneFrame frame_{sizeof(Idas3SceneFrame),1};
    std::vector<Idas3SceneVertex> vertices_;
    std::vector<Idas3SceneRange> ranges_;
    std::vector<std::uint64_t> geometryIds_;
    std::vector<NativeImage> images_;
    std::vector<Idas3SceneTexture> textures_;
    std::vector<Idas3SceneOverlay> overlays_;
    std::array<float,184> frameConstants_{};
    std::array<uint32_t,1248> lightConstants_{};
    std::array<uint32_t,136> fogConstants_{};
};
}
static_assert(sizeof(Idas3SceneVertex)==64&&sizeof(Idas3SceneRange)==64);
static_assert(sizeof(Idas3SceneCamera)==72&&sizeof(Idas3SceneFrame)==256);
#endif
