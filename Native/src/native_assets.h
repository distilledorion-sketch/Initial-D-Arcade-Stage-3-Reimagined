#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>
#include "math_types.h"

namespace idas3 {
// Decoded, ordinary native assets. No guest addresses, BIOS or GPU commands.
struct NativeImage {
    std::uint32_t width=0,height=0;
    // Straight alpha 0xAARRGGBB; same byte order as the Windows HUD surface.
    std::vector<std::uint32_t> argb;
};
class NativeTextureBank {
public:
    static NativeTextureBank load(const std::filesystem::path& path);
    const NativeImage& at(std::uint32_t index)const;
    std::size_t size()const{return images.size();}
private:
    std::vector<NativeImage> images;
};
struct OriginalSpriteVertex {
    float x=0,y=0,z=0,u=0,v=0;
    std::uint32_t color=0,descriptor=0;
};
struct OriginalSprite {
    std::uint32_t texture=0,tagA=0,tagB=0;
    std::array<OriginalSpriteVertex,4> vertices;
};
class NativeSpriteBank {
public:
    // Reads the original RIP's simple data records directly, without emulation.
    static NativeSpriteBank load(const std::filesystem::path& table,const std::filesystem::path& payload);
    std::vector<OriginalSprite> sprites;
};
struct NativeModelVertex {
    std::uint32_t header=0;
    Vec3 position,normal;
    float u=0,v=0;
    std::uint32_t color0=0,color1=0;
};
struct NativeModelBatch {
    std::uint32_t sourceOffset=0;
    std::array<std::uint32_t,8> ich{};
    std::array<std::uint32_t,16> material{};
    std::vector<NativeModelVertex> vertices;
    std::vector<std::uint32_t> indices;
};
struct NativeModelChunk {
    std::uint32_t index=0,sourceOffset=0,sourceSize=0;
    std::array<std::uint32_t,24> header{};
    std::vector<NativeModelBatch> batches;
};
class NativeModel {
public:
    static NativeModel load(const std::filesystem::path& path);
    std::vector<NativeModelChunk> chunks;
};
struct NativeModelInstance {
    std::uint32_t chunk=0;
    // Row-major matrix, column vectors, translation at 3/7/11.
    std::array<float,16> transform{};
    // Original 1F6AF0 resets the view rotation for course lamp quads.
    // Added by the optional source-captured scene lamp sidecar.
    bool billboard=false;
};
class NativeAssembly {
public:
    static NativeAssembly load(const std::filesystem::path& path,std::size_t chunkCount);
    std::vector<NativeModelInstance> instances;
};
// Draws inserted into an immutable source assembly at an original instance
// boundary. Replacement is used only for an already captured animated prop.
struct NativeAssemblyInsertion {
    std::size_t before=0;
    NativeAssembly assembly;
    std::size_t replaceCount=0;
};
struct SpritePlacement {
    float scale=1,offsetX=0,offsetY=0;
    // Original RIP coordinate system is an explicit caller choice.
    bool invertY=false;
    float authoredHeight=480;
    float opacity=1;
    // Original 1B7F80/1B8240 UI color-buffer path for VUR (0x4A) batches.
    // This is a draw-state override; the imported source bytes stay intact.
    bool defaultOriginalUiColors=false;
    // Opt-in carrier for transparent HUD overlays later composited with
    // SRC_ALPHA. Existing opaque source-reference raster behavior is default.
    bool straightAlphaOverlay=false;
    // Menu-chunk asset measurements need pixels even when Unity capture is on.
    // Do not export temporary measurement surfaces or alter global capture state.
    bool softwareOnly=false;
};
// CPU triangle compositor shared by native menus and deterministic UI previews.
// The original strip order is (0,1,2),(2,1,3). UVs, colors and authored positions
// are retained. Blending is straight-alpha source-over with alpha preserved.
void compositeOriginalSprite(std::span<std::uint32_t> target,int width,int height,
    const NativeImage& image,const OriginalSprite& sprite,const SpritePlacement& placement);
void compositeImage(std::span<std::uint32_t> target,int width,int height,
    const NativeImage& image,float x,float y,float drawWidth,float drawHeight,float opacity=1);
// Orthographic original menu geometry. Caller supplies the recovered screen
// transform and selected chunk. Texture wrap/clamp/mirror uses original TSP.
  void compositeOriginalMenuChunk(std::span<std::uint32_t> target,int width,int height,
      const NativeTextureBank& textures,const NativeModelChunk& chunk,const SpritePlacement& placement,
      std::span<const Vec3> transformedPositions={});
}
