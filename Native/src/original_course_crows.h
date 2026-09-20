#pragma once
#include "native_assets.h"
#include "original_matrix.h"

namespace idas3 {
struct OriginalCourseCrowState {
    std::uint32_t flightFrame=0;
    std::array<std::uint32_t,19> animationFrames{};
    bool initialized=false;
};
//041D20/042440: nineteen original1F9E60 random values, then flight cursor0.
//The caller supplies the shared source seed boundary; this component never
//chooses a private seed or changes the driving session's random stream.
void resetOriginalCourseCrows(OriginalCourseCrowState&,std::uint32_t& sourceSeed);
//Recover the unique nineteen preceding RNG steps ending at the supplied
//player-initialization boundary. This is a native boundary reconstruction,
//not a claim that the original boot/menu random history has been replayed.
//The input is by value: neither helper nor owner mutates the driving stream.
std::uint32_t originalCourseCrowPrecedingSeed(std::uint32_t drivingEntrySeed);
void resetOriginalCourseCrowsBeforeDrivingSeed(OriginalCourseCrowState&,std::uint32_t drivingEntrySeed);
//0424C0: one call per original course-owner update, independently of drawing.
void advanceOriginalCourseCrows(OriginalCourseCrowState&);
//042520's complete world transform before nineteen group translations.
original::OriginalMatrix originalCourseCrowFlightMatrix(
    const std::array<float,3>& position,const std::array<float,3>& next,
    const original::OriginalFscaTable&);

class OriginalCourseCrows {
public:
    //Original042700 chooses the other Myogi constructor in night or rain.
    static bool availableFor(unsigned courseIndex,bool night,bool wet){return courseIndex==0&&!night&&!wet;}
    static OriginalCourseCrows load(const std::filesystem::path& root);
    void reset(std::uint32_t& sourceSeed);
    void resetBeforeDrivingSeed(std::uint32_t drivingEntrySeed);
    void advance();
    const NativeAssembly& assembly()const{return assembly_;}
    const OriginalCourseCrowState& state()const{return state_;}
    NativeModel model;
    NativeTextureBank textures;
private:
    std::array<std::array<float,3>,19> group_{};
    std::vector<original::OriginalMatrix> flight_;
    OriginalCourseCrowState state_{};
    NativeAssembly assembly_;
    void updateAssembly();
};
}
