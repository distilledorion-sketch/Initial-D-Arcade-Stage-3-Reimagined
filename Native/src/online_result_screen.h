#pragma once
#include "original_battle_result_animation.h"
#include <algorithm>
#include <cstdint>

namespace idas3 {
// Local presentation after an authenticated result. This owner never changes
// race rules, saves, awards or the peer handshake.
struct OnlineResultScreen {
    int page=-1,decision=0,selected=0; // finish, points, Continue; decision 1 Yes / 2 No
    unsigned frame=0,countdown=879,confirmFrames=0,finishFade=0;
    bool confirming=false,blocked=false,peerLeft=false,peerReady=false,recordFailed=false;
    bool confirmPending=false,cancelPending=false;
    int direction=0;
    original::OriginalBattleResultAnimationState points;
    original::OriginalBattleResultAnimationFrame pointsFrame;

    void begin(unsigned earned,unsigned balance){
        *this={};page=0;
        original::initializeOriginalBattleResultAnimation(points,{earned,balance,false});
        pointsFrame.displayedBalance=points.displayedBalance;pointsFrame.fadeAlpha=255;
    }
    void input(bool confirm,bool cancel,int axis,unsigned flags){
        blocked=(flags&1)!=0;peerLeft=(flags&2)!=0;peerReady=(flags&4)!=0;
        recordFailed=(flags&8)!=0;
        direction=blocked?0:std::clamp(axis,-1,1);
        if(blocked){confirmPending=cancelPending=false;return;}
        confirmPending|=confirm;cancelPending|=cancel;
    }
    // One source 60 Hz presentation tick. The existing HResult owner supplies
    // its exact fade, count-up, cues and skip policy; AContinue uses its original
    // 879-tick timer and 41-tick confirmation dwell.
    unsigned advance(bool finishAudioDone){
        if(page<0||blocked)return 0;
        const bool confirm=confirmPending,cancel=cancelPending;
        confirmPending=cancelPending=false;
        ++frame;
        if(page==0){
            if(confirm||cancel)finishFade=15;
            else if(frame>=300&&finishAudioDone&&finishFade<15)++finishFade;
            if(finishFade==15){page=1;frame=0;}
        }else if(page==1){
            pointsFrame=original::advanceOriginalBattleResultAnimation(points,{confirm||cancel,false});
            if(pointsFrame.finished){page=2;frame=0;}
        }else if(decision==1){
            // A driver who already chose Yes can still leave while waiting.
            if(cancel){selected=1;decision=2;return 3;}
        }else if(!decision){
            if(confirming){if(++confirmFrames>40)decision=selected?2:1;}
            else {
                if(countdown)--countdown;
                const int previous=selected;
                if(direction)selected=direction<0?0:1;
                if(cancel||(peerLeft&&confirm)||!countdown)selected=1;
                if(confirm||cancel||!countdown){confirming=true;confirmFrames=0;return 3;}
                if(previous!=selected)return 2;
            }
        }
        return 0;
    }
};
}
