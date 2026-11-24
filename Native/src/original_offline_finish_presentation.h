#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>

namespace idas3::original {
// Ordinary local ARace Finish05C280/05C4A0, followed by Exit05D120.
// Numeric0 is both live Legend and Bunta; numeric2 is ordinary Time Attack.
// Challenger+64C, linked1 and the legacy/special numeric3 owner are distinct.
// Input has already passed the continuously updated accepted-source edge bank.
// Qualification of records, outer solver, camera, audio I/O and parent close /
// result dispatch are explicit callers. No host/render/audio clock is used.
struct OriginalOfflineFinishPresentation {
    enum class Phase { Inactive, AwaitOutcome, Hold, Exit, Done };
    Phase phase=Phase::Inactive;
    unsigned kind=2,counter=0,exitCounter=0,selectedHudCode=0,visibleHudCode=0;
    std::uint32_t selectedRecord=0,visibleRecord=0;
    std::array<std::uint32_t,3> records{};
    unsigned recordCount=0;
    bool goal=false,lost=false,recordQualified=false,cardAccepted=false,blank=false;
    void begin(unsigned mode,bool reachedGoal,bool loss,std::uint32_t recordFlags,
               bool acceptedCard,unsigned priorHudCode=0,std::uint32_t priorRecord=0){
        if(mode!=0&&mode!=2)throw std::invalid_argument("Ordinary local Finish requires numeric0/2");
        *this={};phase=Phase::AwaitOutcome;kind=mode;goal=reachedGoal;lost=loss;cardAccepted=acceptedCard;
        selectedHudCode=visibleHudCode=priorHudCode;selectedRecord=visibleRecord=priorRecord;
        if(mode==2&&goal){
            // Readiness inserts personal, model, course; Main pops last first.
            for(auto flag:{0x40000000u,0x20000000u,0x10000000u})
                if(recordFlags&flag)records[recordCount++]=flag;
            recordQualified=recordCount!=0;
        }
    }
    bool active()const{return phase!=Phase::Inactive;}
    bool done()const{return phase==Phase::Done;}
    bool drawsViews()const{return phase==Phase::AwaitOutcome||phase==Phase::Hold;}
    bool runsSolver()const{return active()&&!done();}
    // Returns the stream-stop request. It never waits for stream completion.
    bool advance(bool startPressed){
        if(!runsSolver())return false;
        //0654E0 prepares the HUD before dispatching this child. Values0..2
        //are consumed warning codes; Finish selection appears one tick later.
        visibleHudCode=selectedHudCode;visibleRecord=selectedRecord;
        if(selectedHudCode<=2)selectedHudCode=0;
        if(phase==Phase::AwaitOutcome){phase=Phase::Hold;return false;}
        if(phase==Phase::Hold){
            if(!goal)selectedHudCode=6;
            else if(kind==0)selectedHudCode=counter<120?3u:lost?5u:4u;
            else if(counter<120||!recordQualified)selectedHudCode=3;
            else{
                if(counter%120==0&&recordCount)selectedRecord=records[--recordCount];
                //31CE38 is active-profile1180, the accepted/new-card bits.
                if(cardAccepted)selectedHudCode=7;
            }
            const bool exit=counter>=120&&(counter>600||startPressed);
            if(exit)phase=Phase::Exit;
            ++counter;return exit;
        }
        //Next dispatch initializes Exit and hardware-blanks, then executes its
        //Main. Sixth Main marks completion; parent End unblanks that same tick.
        blank=true;if(exitCounter==0)exitCounter=1;
        if(exitCounter==6){phase=Phase::Done;blank=false;}
        ++exitCounter;return false;
    }
};
struct OriginalCabinetStartEdge {
    bool previous=false;
    bool accept(bool held){const bool edge=held&&!previous;previous=held;return edge;}
};
}
