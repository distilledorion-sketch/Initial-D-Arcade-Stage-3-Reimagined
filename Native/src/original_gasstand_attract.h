#pragma once
#include "native_assets.h"
#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>
#include <vector>

namespace idas3::original {
// TITLE3 child11. This is the source's layered2D gas-station conversation,
// not a free-camera3D scene. Step once per original60Hz Main call.
struct OriginalGasstandState {
    std::uint32_t phase=0,elapsed=0,script=0,message=0,speaker=0;
    std::int32_t fade=15;
    bool fadeEnabled=true,brands=false,bubble=false,ranking=false,completed=false;
    float brandX=6.4f,bubbleX=0,bubbleY=0;
    std::uint32_t backgroundPhase=0,displayedBackgroundPhase=0;
};
struct OriginalGasstandDraw {
    enum class Bank { gasstand,etc,alphabet } bank=Bank::gasstand;
    unsigned index=0;
    // Polygon offset relative to bank's originaldefault position, model units;
    // alphabet positions are original640x480 top-left pixels.
    Vec3 position{};
    float scaleX=1,scaleY=1;
    std::uint32_t color=0xffffffff;
};
void resetOriginalGasstand(OriginalGasstandState&,unsigned script=0);
unsigned nextOriginalGasstandScript(unsigned previous,bool skipBattle=false,bool skipCustomization=false);
void stepOriginalGasstand(OriginalGasstandState&);
std::uint32_t originalGasstandFadeArgb(const OriginalGasstandState&);
std::vector<OriginalGasstandDraw> originalGasstandDraws(const OriginalGasstandState&);
float originalAlphabetTextWidth(std::string_view text);

class OriginalGasstandAttract {
public:
    static bool available(const std::filesystem::path& root);
    void load(const std::filesystem::path& root);
    void reset(unsigned script=0){resetOriginalGasstand(state_,script);}
    void step(){stepOriginalGasstand(state_);}
    const OriginalGasstandState& state()const{return state_;}
    bool completed()const{return state_.completed;}
    // Paint to caller's640x480 sourcecanvas; larger destinations use one final
    // fit. Fade is separate so caller applies it after complete composition.
    void paint(std::span<std::uint32_t> argb,int width,int height)const;
    // Independent general-compositor path for CPU raster regression tests.
    void paintReference(std::span<std::uint32_t> argb,int width,int height)const;
    // Native free-play prompts reuse the recovered alphabet, not host fonts.
    void paintText(std::span<std::uint32_t>,int width,int height,std::string_view,
        float x,float y,float scale=1,std::uint32_t color=0xffffffff)const;
    // Extend only the backdrop into widescreen margins after fitting the
    // unmodified 4:3 composition. Uses original tile57 and its live phase.
    void extendBackdrop(std::span<std::uint32_t>,int width,int height)const;
private:
    OriginalGasstandState state_;
    NativeModel gasstand_,etc_;
    NativeTextureBank gasstandTextures_,etcTextures_,alphabet_;
    std::vector<NativeImage> preparedAlphabet_;
    void paintImpl(std::span<std::uint32_t>,int,int,bool)const;
};
}
