#include "original_battle_result_animation.h"
#include <bit>
#include <stdexcept>

namespace idas3::original {
namespace {
std::int32_t signedWord(std::uint32_t n){return std::bit_cast<std::int32_t>(n);}
std::uint32_t divideSigned(std::uint32_t n,std::int32_t divisor){return std::uint32_t(std::int64_t(signedWord(n))/divisor);}
constexpr std::uint32_t cap=999999999;
// Canonical table0C2F4E28: reveal30, count60, hold300.
constexpr std::uint32_t fadeFrames=30,countFrames=60,holdFrames=300;
}
OriginalBattleResultCommit initializeOriginalBattleResultAnimation(OriginalBattleResultAnimationState& s,const OriginalBattleResultAnimationSetup& in){
    s={};s.delta=in.deduction?0u-in.earnedMagnitude:in.earnedMagnitude;
    //06FC48..06FCA4: subtract signed earned amount for the display's starting
    // total, and independently commit the capped final total to profile+72.
    s.startingBalance=in.balanceBeforeCap-s.delta;s.displayedBalance=s.startingBalance;
    s.committedBalance=in.balanceBeforeCap>cap-1?cap:in.balanceBeforeCap;
    s.upgradeThreshold=in.upgradeThreshold;s.upgradeNotice=in.upgradeNotice;s.upgradeChildPresent=in.upgradeChildPresent;
    return {s.committedBalance,signedWord(s.delta)};
}
OriginalBattleResultAnimationFrame advanceOriginalBattleResultAnimation(OriginalBattleResultAnimationState& s,OriginalBattleResultAnimationInput in){
    OriginalBattleResultAnimationFrame out;
    if(s.complete){out.displayedBalance=s.displayedBalance;out.fadeAlpha=255;out.sourcePhase=4;out.finished=true;return out;}
    if(s.phase>4)throw std::invalid_argument("Invalid original battle result phase");
    const auto transition=[&](std::uint32_t phase){s.phase=phase;s.frame=0xffffffff;};
    const auto cue=[&](std::uint8_t id){out.cueIds.at(out.cueCount++)=id;};
    //0709E0..070E42. Source arithmetic intentionally keeps32-bit products,
    // including overflow, signed division and the unsigned upper cap.
    switch(s.phase){
    case 0:{const auto numerator=s.frame*255u;out.fadeAlpha=255u-std::uint32_t(std::int32_t(float(signedWord(numerator))/float(fadeFrames)));if(s.frame==fadeFrames)transition(1);break;}
    case 1:{s.displayedBalance=s.startingBalance+divideSigned(s.delta*s.frame,countFrames);if(s.displayedBalance>cap)s.displayedBalance=cap;
        if(s.displayedBalance>=s.upgradeThreshold)s.highlightBalance=true;
        if(s.frame==divideSigned(s.frame,3)*3u)cue(5);
        if(s.upgradeNotice&&s.frame==30)cue(14);
        if(s.frame==countFrames)transition(2);break;}
    case 2:s.displayedBalance=s.committedBalance;if(s.frame==holdFrames){if(s.upgradeChildPresent){transition(3);s.showUpgradeChild=true;}else transition(4);}break;
    case 3:if(!s.upgradeChildPresent||in.upgradeChildFinished)transition(4);break;
    case 4:{const auto frame=signedWord(s.frame)>std::int32_t(fadeFrames)?fadeFrames:s.frame;
        out.fadeAlpha=std::uint32_t(std::int32_t(float(signedWord(frame*255u))/float(fadeFrames)));
        if(s.frame==fadeFrames+3){s.complete=true;transition(0);}break;}
    }
    //0EDFE0's balance presentation is sampled by the owner during this frame.
    out.displayedBalance=s.displayedBalance;out.highlightBalance=s.highlightBalance;out.upgradeNotice=s.upgradeNotice;
    out.showUpgradeChild=s.showUpgradeChild;out.sourcePhase=s.phase;out.finished=s.complete;
    if(!s.showUpgradeChild&&s.highlightBalance){++s.balanceBlinkFrame;out.balanceVisible=signedWord(s.balanceBlinkFrame)-signedWord(divideSigned(s.balanceBlinkFrame,30)*30u)<=19;}
    //070ECE..070F2A: input follows drawing. A confirm during fade/count jumps
    // to the final balance with60holdticks left; a second confirm skips hold.
    if(in.confirmEdge&&!s.complete){
        if(signedWord(s.phase)<=1){s.displayedBalance=s.committedBalance;if(s.displayedBalance>=s.upgradeThreshold)s.highlightBalance=true;s.phase=2;s.frame=holdFrames-60;}
        else if(s.phase==2)s.frame=holdFrames-1;
    }
    ++s.frame;
    return out;
}
}
