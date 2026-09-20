#pragma once
#include <cstdint>
#include <span>
namespace idas3 {
struct NativeImage;
struct UnityUiVertex{float x,y,u,v;std::uint32_t argb,offsetArgb;};
// Neutral pixel-coordinate UI triangles. Flags:1 original material,2 behind3D,
//4 outer additive layer. TSP/PCW retain original sampling/color/blend semantics.
struct UnityUiDraw{std::uint32_t first,count,texture,tsp,pcw,flags;float opacity;std::uint32_t reserved;float clipLeft,clipTop,clipRight,clipBottom;};
struct UnityUiFrame{std::uint32_t size,width,height,drawCount,vertexCount,textureCount,unresolvedSurfaces,reserved;std::uint64_t revision;};
struct UnityUiTextureInfo{std::uint32_t size,width,height,bytes;};
bool unityUiEnabled();
// Command-only HUD pixels and reusable frame storage. The exact diagnostic
// argument -idas3-scene-perf-baseline restores the prior allocation/clear work.
bool unityUiFrameReuseEnabled();
// Call before destroying/replacing a source image. Existing exported copies
// and cached draw-list texture IDs remain valid; only the source lookup expires.
void unityUiForgetTexture(const NativeImage& image);
bool unityUiTriangle(const std::uint32_t* target,int width,int height,const NativeImage& image,
    UnityUiVertex a,UnityUiVertex b,UnityUiVertex c,float opacity,std::uint32_t tsp,bool original,std::uint32_t pcw);
void unityUiClear(const std::uint32_t* target,int width,int height,std::uint32_t argb=0);
void unityUiCopy(const std::uint32_t* destination,const std::uint32_t* source,int width,int height,
    float scaleX=1,float scaleY=1,float offsetX=0,float offsetY=0,bool append=false,bool additive=false);
void unityUiCopyRegion(const std::uint32_t* target,int width,int height,float left,float top,float right,float bottom,float x,float y,float w,float h);
bool unityUiCopyImage(const std::uint32_t* destination,int width,int height,const NativeImage& image,float x,float y,float w,float h,float opacity);
void unityUiSolid(const std::uint32_t* target,int width,int height,float x,float y,float w,float h,std::uint32_t argb);
void unityUiLine(const std::uint32_t* target,int width,int height,float x,float y,float xx,float yy,float thickness,std::uint32_t argb);
void unityUiText(const std::uint32_t* target,int width,int height,void* hdc,float x,float y,const wchar_t* text,int length,std::uint32_t argb);
// Renderer calls this in its original overlay submission order. The source
//surface dimensions differ from the viewport for originalCanvas passes.
void unityUiSubmit(const std::uint32_t* pixels,int sourceWidth,int sourceHeight,bool behind,bool additive,bool originalCanvas);
}
#if defined(_WIN32)
#define IDAS3_UI_EXPORT extern "C" __declspec(dllexport)
#else
#define IDAS3_UI_EXPORT extern "C" __attribute__((visibility("default")))
#endif
IDAS3_UI_EXPORT void Idas3UiEnable(int enabled);
IDAS3_UI_EXPORT void Idas3UiBeginFrame(int width,int height);
IDAS3_UI_EXPORT int Idas3UiGetFrame(idas3::UnityUiFrame* frame);
IDAS3_UI_EXPORT int Idas3UiCopyDraws(idas3::UnityUiDraw* destination,int capacity);
IDAS3_UI_EXPORT int Idas3UiCopyVertices(idas3::UnityUiVertex* destination,int capacity);
IDAS3_UI_EXPORT int Idas3UiGetTextureInfo(unsigned texture,idas3::UnityUiTextureInfo* info);
IDAS3_UI_EXPORT int Idas3UiCopyTextureRGBA(unsigned texture,void* destination,int capacity);
