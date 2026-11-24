#pragma once
#include <cstdint>

namespace idas3::original {
// Finish child05C280/05C4A0 and linked exit05D120/05D180/05D200.
// The network adapter supplies readiness/outcome; no transport, reward,
// driving simulation or audio-duration decision belongs to this owner.
struct OriginalLinkedFinishPresentation {
    enum class Phase { AwaitOutcome, Hold, Exit, Done };
    Phase phase=Phase::AwaitOutcome;
    unsigned counter=0,exitCounter=0,selectedHudCode=0,visibleHudCode=0;
    bool goal=false,lost=false,blank=false;
    void begin(bool reachedGoal,bool loss,unsigned priorHudCode=0){
        *this={};goal=reachedGoal;lost=loss;selectedHudCode=visibleHudCode=priorHudCode;
    }
    bool done()const{return phase==Phase::Done;}
    void advance(bool outcomeReady){
        if(done())return;
        // Outer0654E0 prepares the HUD from prior+630 BEFORE child dispatch.
        visibleHudCode=selectedHudCode;
        if(selectedHudCode<=2)selectedHudCode=0; //066110..11C consumes warning.
        if(phase==Phase::AwaitOutcome){
            //05D084: the successful outcome call sets ready+88 only. Its
            // initial zero counter is first processed on the next update.
            if(outcomeReady)phase=Phase::Hold;
            return;
        }
        if(phase==Phase::Hold){
            //05C5A0: linked goal-loser is immediate; winner and timeout use
            // the120-update boundary. Manual cabinet input is ignored.
            selectedHudCode=goal?(lost?5u:counter<120?3u:4u):(counter<120?6u:lost?5u:4u);
            if(counter>600)phase=Phase::Exit;
            ++counter;
            return;
        }
        // The next parent update initializes child4 and sets VO_CONTROL's
        // blank_video bit. Its Main maps zero toone and exits when it reads6.
        blank=true;
        if(exitCounter==0)exitCounter=1;
        if(exitCounter==6){phase=Phase::Done;blank=false;} // End unblanks.
        ++exitCounter;
    }
};
}
