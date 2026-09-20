#pragma once
#include "native_assets.h"
#include <array>
#include <filesystem>
#include <span>
#include <vector>

namespace idas3::original {
struct OriginalDemoOverlayCue {
    std::uint32_t chunk=0,start=0,fadeInEnd=0,fadeOutStart=0,end=0;
    std::array<float,3> from{},to{},scaleFrom{},scaleTo{};
};
struct OriginalDemoFadeCue {
    std::uint32_t rgb=0,start=0,fadeInEnd=0,fadeOutStart=0,end=0;
    float depth=0;
};
struct OriginalDemoOverlayDraw {
    std::uint32_t row=0,chunk=0,alpha=255;
    Vec3 translation; // Original matrix translation, including (-3.2,+2.4,-.15).
    float scaleX=1,scaleY=1;
    bool materialOverride=false;
};
struct OriginalDemoFadeDraw {std::uint32_t argb=0;float depth=0;};
class OriginalDemoOverlays {
public:
    void load(const std::filesystem::path& root);
    // Stateless: frame is owner+0x4a4 BEFORE Main's final increment.
    void paint(std::span<std::uint32_t>,int width,int height,unsigned frame)const;
    // Transparent HUD carrier: avoids applying source alpha twice when the
    // finished overlay is blended over the3D scene by Renderer.
    void paintOverlay(std::span<std::uint32_t>,int width,int height,unsigned frame)const;
    // After fitting the original640x480 canvas, extend only full-screen fades
    // into the remaining area. The fitted source canvas stays byte-identical.
    void extendBackdrop(std::span<std::uint32_t>,int width,int height,unsigned frame)const;
    std::vector<OriginalDemoOverlayDraw> draws(unsigned frame)const;
    std::vector<OriginalDemoFadeDraw> fades(unsigned frame)const;
    const std::vector<OriginalDemoOverlayCue>& cues()const{return cues_;}
    const std::vector<OriginalDemoFadeCue>& fadeCues()const{return fades_;}
private:
    void paintImpl(std::span<std::uint32_t>,int,int,unsigned,bool straightAlphaOverlay)const;
    NativeModel model_;
    NativeTextureBank textures_;
    std::vector<OriginalDemoOverlayCue> cues_;
    std::vector<OriginalDemoFadeCue> fades_;
};
}
