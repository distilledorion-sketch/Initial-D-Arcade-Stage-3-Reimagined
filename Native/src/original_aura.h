#pragma once
#include "native_assets.h"

namespace idas3::original {
struct OriginalAuraStyle {
    bool visible=false;
    unsigned palette=0;
    float growth=0;
};
//17B0A0: battle level and consecutive wins, independently of winning percent.
OriginalAuraStyle originalAuraStyle(std::uint32_t level,std::uint32_t streak,bool opponent=false);
//17B7E0: original60Hz owner tick selects one of30 authored color frames.
unsigned originalAuraColorFrame(std::uint32_t frame);

class OriginalAura {
public:
    void load(const std::filesystem::path& projectRoot);
    void configure(std::uint32_t level,std::uint32_t streak,bool opponent=false);
    // Does not advance time. Repeated renders of one simulation frame are
    // idempotent. Position is the original actor position, before body offsets.
    void update(std::uint32_t frame,unsigned carId,Vec3 carPosition,Vec3 cameraEye);
    bool visible()const{return loaded_&&style_.visible&&validPose_;}
    const NativeModel& model()const{return model_;}
    const NativeAssembly& assembly()const{return assembly_;}
    const OriginalAuraStyle& style()const{return style_;}
    unsigned colorFrame()const{return colorFrame_;}
    //Diagnostic only: highest-alpha authored color of the displayed frame.
    std::uint32_t colorArgb()const{return colorArgb_;}
private:
    NativeModel authored_,model_;
    NativeAssembly assembly_;
    std::array<std::vector<std::uint32_t>,10> colors_;
    std::vector<std::uint32_t> colorIndices_;
    std::array<float,35> carHalfLengths_{};
    OriginalAuraStyle style_;
    unsigned colorFrame_=~0u,publishedPalette_=~0u;
    std::uint32_t colorArgb_=0;
    bool loaded_=false,validPose_=false,opponent_=false;
};
} // namespace idas3::original
