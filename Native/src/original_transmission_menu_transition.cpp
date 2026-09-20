#include "original_transmission_menu_transition.h"
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
std::uint32_t truncateUnsigned(float n){
    if(2147483648.f>n)return truncateSigned(n);
    const float shifted=n+(-2147483648.f);
    return 0x80000000u+truncateSigned(shifted);
}
}
void initializeOriginalTransmissionMenuTransition(OriginalTransmissionMenuTransition& s){
    s.frame452=0;s.exitHold456=0;s.phase476=0;s.fade460=15;
    s.overlayEnabled464=1;s.committed468=0;s.timedOut516=0;
    s.selected512=s.profileTransmission68;
}
OriginalTransmissionMenuTransitionEvents tickOriginalTransmissionMenuTransition(
        OriginalTransmissionMenuTransition& s,const OriginalTransmissionMenuTransitionInput& input){
    OriginalTransmissionMenuTransitionEvents out;
    switch(s.phase476){
    case 0:
        --s.fade460;
        if(signedWord(s.fade460)<0){s.fade460=0;s.overlayEnabled464=0;++s.phase476;}
        break;
    case 1:
        s.sharedCountdown1176=signedWord(s.sharedCountdown1176)>0?s.sharedCountdown1176-1:0;
        if(!s.sharedCountdown1176)s.timedOut516=1;
        if(s.selected512!=input.selectedIndex){s.selected512=input.selectedIndex;out.selectionChanged=true;}
        if(input.confirmPressed||s.timedOut516){
            s.committed468=1;s.profileTransmission68=s.selected512;
            out.selectionCommitted=true;++s.phase476;s.frame452=0;
        }
        break;
    case 2:
        if(signedWord(s.frame452)>120){++s.phase476;s.frame452=0;s.overlayEnabled464=1;}
        break;
    case 3:
        ++s.fade460;
        if(signedWord(s.fade460)>15){
            s.fade460=15;const auto previousHold=s.exitHold456++;
            if(signedWord(previousHold)>3){
                out.parentRequested=true;
                if(!(s.profileFlags1180&2))s.parentEvent64=8;
                else if(s.profileCar16!=29)s.parentEvent64|=1;
                else if(s.profileByte1192==1)s.parentEvent64=(s.alternateScreen80<<16)|4u;
                else{
                    s.profileByte152=0;out.profileByte152Cleared=true;
                    s.parentEvent64=(s.previousScreen76<<16)|4u;
                }
            }
        }
        break;
    default:break;
    }
    ++s.frame452;
    if(s.phase476==2){
        float phase=float(signedWord(s.frame452))/36.f;
        if(0.f>phase)phase=0.f;if(phase>1.f)phase=1.f;
        out.confirmationPhaseWritten=true;out.confirmationPhase=phase;
    }
    return out;
}
std::uint32_t originalTransmissionMenuFadeArgb(const OriginalTransmissionMenuTransition& s){
    const float fraction=float(signedWord(s.fade460))/15.f;
    const float scaled=fraction*255.f;
    return truncateUnsigned(scaled)<<24;
}
}
