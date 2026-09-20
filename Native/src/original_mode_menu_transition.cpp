#include "original_mode_menu_transition.h"
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
bool buntaEligible(const OriginalModeMenuTransition& s){return signedWord(s.profilePoints72)>3999&&(s.profileFlags1180&3)!=0;}
}
void initializeOriginalModeMenuTransition(OriginalModeMenuTransition& s){
    s.drawFrame444=0;s.fade448=15;s.phase468=0;s.selected564=0;s.overlayEnabled456=1;
    s.pointsWarning520=s.pointsWarningAge524=s.cardWarning528=s.cardWarningAge532=0;
    s.committed460=0;s.timedOut572=0;s.sharedCountdown1176=1279;
}
OriginalModeMenuTransitionEvents tickOriginalModeMenuTransition(
        OriginalModeMenuTransition& s,const OriginalModeMenuTransitionInput& input){
    OriginalModeMenuTransitionEvents out;bool accepted=input.confirmPressed;
    //125E20 runs before dispatch, including during fade/confirmation. It
    // checks the old selected index, before09C580 returns this tick's index.
    if(s.selected564==2&&accepted){
        if(signedWord(s.profilePoints72)<=3999){
            out.pointsRejected=true;s.pointsWarning520=1;s.pointsWarningAge524=0;accepted=false;
        }else if(!(s.profileFlags1180&3)){
            out.cardRejected=true;s.cardWarning528=1;s.cardWarningAge532=0;accepted=false;
        }
    }
    if(s.pointsWarning520){const auto previous=s.pointsWarningAge524++;if(signedWord(previous)>120)s.pointsWarning520=0;}
    if(s.cardWarning528){const auto previous=s.cardWarningAge532++;if(signedWord(previous)>120)s.cardWarning528=0;}
    switch(s.phase468){
    case 0:
        --s.fade448;
        if(signedWord(s.fade448)<0){s.fade448=0;s.overlayEnabled456=0;++s.phase468;out.readyRequested=true;}
        break;
    case 1:
        s.sharedCountdown1176=signedWord(s.sharedCountdown1176)>0?s.sharedCountdown1176-1:0;
        if(!s.sharedCountdown1176)s.timedOut572=1;
        if(s.selected564!=input.selectedIndex){s.pointsWarning520=0;s.selected564=input.selectedIndex;out.selectionChanged=true;}
        if(accepted||s.timedOut572){
            if(s.timedOut572&&s.selected564==2&&!buntaEligible(s))s.selected564=1;
            s.confirmationFrame452=0;s.committed460=1;s.profileMode0=s.selected564;
            s.profileByte1191=2;++s.phase468;out.selectionCommitted=true;
        }
        break;
    case 2:
        ++s.confirmationFrame452;
        if(signedWord(s.confirmationFrame452)>120){s.overlayEnabled456=1;++s.phase468;}
        break;
    case 3:
        ++s.fade448;
        if(signedWord(s.fade448)>15){s.fade448=15;s.parentEvent64=8;out.parentRequested=true;}
        break;
    default:break;
    }
    out.timerDisplayValue=signedWord(s.sharedCountdown1176)/80;
    if(s.phase468==2){
        out.confirmationPhaseWritten=true;float phase=float(signedWord(s.confirmationFrame452))/120.f;
        if(0.f>phase)phase=0.f;if(phase>1.f)phase=1.f;out.confirmationPhase=phase;
    }
    if(input.hiddenExitRequested){s.parentEvent64=8;out.parentRequested=true;}
    return out;
}
std::uint32_t originalModeMenuFadeArgb(const OriginalModeMenuTransition& s){
    const float fraction=float(signedWord(s.fade448))/15.f;
    const float scaled=fraction*255.f;return truncateUnsigned(scaled)<<24;
}
void advanceOriginalModeMenuDraw(OriginalModeMenuTransition& s){++s.drawFrame444;}
std::array<int,2> originalModeWarningChunks(const OriginalModeMenuTransition& s){
    return {s.pointsWarning520==1?0:s.pointsWarning520==2?1:-1,s.cardWarning528==1?0:-1};
}
}
