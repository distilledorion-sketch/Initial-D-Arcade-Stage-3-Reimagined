#include "original_car_menu_transition.h"
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
std::uint32_t truncateUnsigned(float n){ //222300
    if(2147483648.f>n)return truncateSigned(n);
    const float shifted=n+(-2147483648.f);
    return 0x80000000u+truncateSigned(shifted);
}
}
void initializeOriginalCarMenuTransition(OriginalCarMenuTransition& s){
    s.frame452=0;s.phase456=0;s.fade464=7;s.fadeRange468=7;
    s.overlayEnabled472=1;s.frame476=0;s.timedOut672=0;
}
OriginalCarMenuTransitionEvents tickOriginalCarMenuTransition(
        OriginalCarMenuTransition& s,const OriginalCarMenuTransitionInput& input){
    OriginalCarMenuTransitionEvents out;
    switch(s.phase456){
    case 0:
        --s.fade464;
        if(signedWord(s.fade464)<0){s.fade464=0;s.overlayEnabled472=0;++s.phase456;}
        break;
    case 1:
        s.sharedCountdown1176=signedWord(s.sharedCountdown1176)>0?s.sharedCountdown1176-1:0;
        if(!s.sharedCountdown1176)s.timedOut672=1;
        // Source first applies the caller-owned local-car/color selection.
        if(input.confirmPressed||s.timedOut672){
            out.selectionCommitted=true;++s.phase456;s.frame452=0;
        }else if(input.cancelPressed&&!(s.profileFlags1180&8)){
            out.cancelAccepted=true;s.phase456=5;s.overlayEnabled472=1;s.fadeRange468=15;
        }
        break;
    case 2:
        if(signedWord(s.frame452)>120){++s.phase456;s.frame452=0;}
        break;
    case 3:
        ++s.frame452; // This increment is additional to the common tail.
        if(signedWord(s.frame452)>50){++s.phase456;s.overlayEnabled472=1;s.fadeRange468=15;}
        break;
    case 4:
        ++s.fade464;
        if(signedWord(s.fade464)>15){
            s.fade464=15;s.parentEvent64=32;out.showroomCommitRequested=true;out.parentRequested=true;
        }
        break;
    case 5:
        ++s.fade464;
        if(signedWord(s.fade464)>15){
            s.fade464=15;s.parentEvent64=(s.previousScreen76<<16)|4u;out.parentRequested=true;
        }
        break;
    default:break;
    }
    ++s.frame476;++s.frame452;
    if(s.phase456==2){
        float phase=float(signedWord(s.frame452))/36.f;
        if(0.f>phase)phase=0.f;if(phase>1.f)phase=1.f;
        out.confirmationPhaseWritten=true;out.confirmationPhase=phase;
    }
    return out;
}
std::uint32_t originalCarMenuFadeArgb(const OriginalCarMenuTransition& s){
    const float fraction=float(signedWord(s.fade464))/float(signedWord(s.fadeRange468));
    const float scaled=fraction*255.f;
    return truncateUnsigned(scaled)<<24;
}
}
