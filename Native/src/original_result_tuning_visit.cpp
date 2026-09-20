#include "original_result_tuning_visit.h"
#include <bit>
#include <stdexcept>

namespace idas3::original {
OriginalResultTuningVisitBegin beginOriginalResultTuningVisit(OriginalResultTuningVisit& s,
    OriginalBattleProfile& p,const OriginalTuningData& data,std::uint32_t& rng,
    const OriginalBattleResultAnimationSetup& setup,std::vector<std::uint8_t>* fullTuneOffers){
    const auto before=p.words;
    s={};
    auto numeric=setup;
    // The result producer has already calculated its award/deduction. This
    // store is the source cap/commit, never an additional addition of points.
    const auto commit=initializeOriginalBattleResultAnimation(s.animation,numeric);
    p.setu(72,commit.balance);
    if(!fullTuneOffers||(p.u(1180)&0xc00)!=0xc00)s.tuning=prepareOriginalResultTuning(p,data,rng);
    // Explicit Gameplay action only: retain the mandatory source children,
    // then offer each uninstalled, affordable optional part once. No random
    // repeat/cooldown or extra race is needed to finish this visit.
    if(fullTuneOffers&&(p.u(1180)&0xc00)==0xc00){
        s.tuning={};const auto& optional=data.car(p.u(16)).optional;
        fullTuneOffers->resize(optional.size(),0);
        for(unsigned i=0;i<optional.size();++i){const auto& row=optional[i].words;
            if((*fullTuneOffers)[i]||p.byte(156+row[1])==row[0]||p.u(72)<row[3])continue;
            (*fullTuneOffers)[i]=1;p.setByte(154,std::uint8_t(i));p.setByte(155,0);p.setu(1176,879);
            s.tuning.kind=OriginalTuningChildKind::optionalPart;s.tuning.sourceOwnerKind=2;break;
        }
    }
    s.basicCompletionPresented=(p.u(1180)&0x400)!=0; //06FE3A owner+300
    if(s.tuning.kind!=OriginalTuningChildKind::none)
        s.child=beginOriginalTuningChild(p,data,s.tuning.kind);
    s.animation.upgradeChildPresent=s.tuning.kind!=OriginalTuningChildKind::none;
    s.animation.upgradeThreshold=s.tuning.threshold;
    s.animation.upgradeNotice=s.tuning.notice;
    if(fullTuneOffers){
        s.animation.phase=3;s.animation.frame=0;s.animation.displayedBalance=commit.balance;
        s.animation.showUpgradeChild=s.animation.upgradeChildPresent;
    }
    s.initialized=true;
    return {commit,before!=p.words,p.u(0)!=1||(p.u(1180)&0x20000)!=0};
}

OriginalResultTuningVisitFrame advanceOriginalResultTuningVisit(OriginalResultTuningVisit& s,
    OriginalBattleProfile& p,const OriginalTuningData& data,OriginalResultTuningVisitInput input){
    if(!s.initialized)throw std::logic_error("Original result tuning visit not initialized");
    OriginalResultTuningVisitFrame out;
    out.sourceOwnerFrame=s.animation.frame;
    if(s.animation.complete){out.result=advanceOriginalBattleResultAnimation(s.animation);return out;}
    const auto before=p.words;
    const auto event=[&](OriginalTuningPreviewEvent::Kind kind){out.previewEvents.push_back({kind,{}});};
    const auto cue=[&](std::uint32_t id){out.cueIds.push_back(id);};
    //070B60: a child is advanced only if the frame ENTERED phase3. The frame
    // that changes phase2 to3 draws the initialized child without updating it.
    if(s.animation.phase==3&&s.animation.upgradeChildPresent){
        out.childAdvanced=true;
        out.child=advanceOriginalTuningChild(s.child,p,data,{input.selectionAxis,input.confirmEdge});
        out.cueIds=out.child.directCueIds;
        const auto command=out.child.command;
        if(command==11){
            if(!s.previewStarted){s.previewStarted=true;event(OriginalTuningPreviewEvent::Kind::startPreview);}
            if(!s.basicCompletionPresented&&(p.u(1180)&0x400)){
                s.basicCompletionPresented=true;event(OriginalTuningPreviewEvent::Kind::focusZero);
            }
            cue(11);
        }else if(command==12||command==13)cue(command);
        else if(command>=1&&command<=6){
            if(command<=3)cue(6);
            out.mutation=applyOriginalTuningCommand(p,data,command);
            for(const auto& call:out.mutation.carCalls)
                out.previewEvents.push_back({OriginalTuningPreviewEvent::Kind::carCall,call});
            if(command==1||command==3)s.previewRemovalFrames=60;
        }else if(command==10){
            const auto frame=std::bit_cast<std::int32_t>(out.sourceOwnerFrame);
            if(frame==std::int32_t(std::int64_t(frame)/3)*3)cue(5);
        }
    }
    // The existing numeric owner retains its exact post-draw confirm policy.
    // Source commands7/8/9 deliberately do nothing in the parent jump table.
    out.result=advanceOriginalBattleResultAnimation(s.animation,{input.confirmEdge,out.child.command==14});
    for(unsigned i=0;i<out.result.cueCount;++i)cue(out.result.cueIds[i]);
    //070E2A..070E40: the same update that arms60 immediately decrements it.
    if(s.previewRemovalFrames&&--s.previewRemovalFrames==0){
        event(OriginalTuningPreviewEvent::Kind::resetFocus);s.previewStarted=false;
    }
    out.profileChanged=before!=p.words;
    return out;
}
}
