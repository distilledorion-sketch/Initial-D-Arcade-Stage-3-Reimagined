#include "original_frontend_transition.h"
#include <bit>
#include <cmath>

namespace idas3::original {
namespace {
std::int32_t signedWord(std::uint32_t n){return std::bit_cast<std::int32_t>(n);}
std::uint32_t truncateSigned(float n){
    if(std::isnan(n)||n<=-2147483648.f)return 0x80000000u;
    if(n>=2147483648.f)return 0x7fffffffu;
    return std::uint32_t(std::int32_t(n));
}
std::uint32_t truncateUnsigned(float n){ //222300, not a saturating clamp.
    if(2147483648.f>n)return truncateSigned(n);
    const float shifted=n+(-2147483648.f);
    return 0x80000000u+truncateSigned(shifted);
}
}
OriginalTitleTransitionEvents tickOriginalTitleTransition(OriginalTitleTransition& state,
        std::uint32_t cabinetMode){
    const auto frame=state.frame84;std::uint32_t alpha=0;
    if(signedWord(frame)>870){
        const float remaining=float(signedWord(900u-frame));
        const float fraction=remaining/30.f;
        const float scaled=fraction*255.f;
        alpha=255u-truncateSigned(scaled);
    }
    ++state.frame84;return {alpha<<24,frame==900, cabinetMode==4};
}
void initializeOriginalMakerTransition(OriginalMakerTransition& state){
    state.frame444=0;state.phase448=0;state.fade456=7;
    state.overlayEnabled460=1;state.timedOut484=0;
}
OriginalMakerTransitionEvents tickOriginalMakerTransition(OriginalMakerTransition& s,
        const OriginalMakerTransitionInput& input){
    OriginalMakerTransitionEvents out;
    switch(s.phase448){
    case 0:
        --s.fade456;
        if(signedWord(s.fade456)<0){s.fade456=0;s.overlayEnabled460=0;++s.phase448;}
        break;
    case 1:
        s.sharedCountdown1176=signedWord(s.sharedCountdown1176)>0?s.sharedCountdown1176-1:0;
        if(!s.sharedCountdown1176)s.timedOut484=1;
        if(s.selected440!=input.selectedIndex){s.selected440=input.selectedIndex;s.frame444=0;out.selectionChanged=true;}
        if(input.confirmPressed||s.timedOut484){
            ++s.phase448;s.overlayEnabled460=1;s.frame444=0;out.selectionCommitted=true;
            out.resetSelectedCar=input.profileMaker40!=s.selected440;
        }
        break;
    case 2:
        if(signedWord(s.frame444)>120){++s.phase448;s.frame444=0;}
        break;
    case 3:
        ++s.fade456;
        if(signedWord(s.fade456)>7){s.fade456=7;s.frame444=0;++s.phase448;}
        break;
    case 4:
        if(signedWord(s.frame444)>3){
            if(signedWord(s.sharedCountdown1176)<=879){s.profileFlags1180|=8;s.sharedCountdown1176=879;}
            s.parentEvent64=32;out.parentRequested=true;
        }
        break;
    default:break;
    }
    if(s.phase448==2){
        float phase=float(signedWord(s.frame444))/36.f;
        if(0.f>phase)phase=0.f;if(phase>1.f)phase=1.f;
        out.confirmationPhaseWritten=true;out.confirmationPhase=phase;
    }
    ++s.frame444;return out;
}
std::uint32_t originalMakerFadeArgb(const OriginalMakerTransition& s){
    const float fraction=float(signedWord(s.fade456))/7.f;
    const float scaled=fraction*255.f;
    return truncateUnsigned(scaled)<<24;
}
OriginalFadeOverlayEvents applyOriginalSimpleFadeOverlay(OriginalFadeOverlay& state,std::uint32_t argb){
    if(!state.resourceAvailable)return {};
    state.diffuse0=argb;state.diffuse1=argb;return {true,(argb&0xff000000u)!=0};
}
}
