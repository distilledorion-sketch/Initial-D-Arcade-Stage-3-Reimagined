#include "original_legend_visit.h"
#include "unity_ui_capture.h"
#include <algorithm>

namespace idas3::original {
namespace {
using C=OriginalLegendReturnCommand;
// The caller still owns these; the visit forwards them untouched.
bool callerOwned(C command){
    switch(command){
    case C::MusicRequest:case C::MusicFade:case C::Cue:case C::SoundSet:
    case C::StreamVolume:case C::StreamStart:case C::StreamPlay:case C::StreamStop:
    case C::StreamFade:case C::StartCourseClear:case C::StepCourseClear:
    case C::CourseClearFade:case C::FinishCourseClear:case C::DrawCredit:
    case C::ContinueAccepted:case C::Finish:
        return true;
    default:return false;
    }
}
}

std::string OriginalLegendVisit::playerName(const OriginalBattleProfile& profile)const{
    // The source copies the player record's name into the dialogue object; the
    // same characters are the name-entry glyphs the profile already stores.
    return originalRivalDialogPlayerName(profile);
}

void OriginalLegendVisit::begin(OriginalBattleProfile& profile,const Setup& setup){
    setup_=setup;
    state_={};
    pending_.clear();
    selected_=0;dialogVisible_=false;overlayVisible_=false;movieRunning_=false;movieFade_=0;
    OriginalLegendReturnSetup returnSetup;
    returnSetup.resultStatus80=setup.resultStatus;
    returnSetup.continue92=setup.canContinue;
    returnSetup.action96=0;
    frame_=initializeOriginalLegendReturn(state_,profile,returnSetup);
    active_=true;
    apply(profile,frame_);
}

void OriginalLegendVisit::advance(OriginalBattleProfile& profile,const Input& in){
    if(!active_||state_.finished)return;
    // The dialogue's own Main runs before the owner dispatch reads its state.
    if(dialogVisible_)scene_.step(profile);
    if(in.previous&&selected_)selected_=0;
    if(in.next&&!selected_)selected_=1;
    OriginalLegendReturnInput input;
    input.dialogReady=dialogVisible_&&scene_.ready();
    input.dialogClosed=!dialogVisible_||scene_.closed();
    input.debugSkip=in.skip;
    input.confirm=in.confirm;
    input.selectedIndex=selected_;
    input.continuationEnabled=setup_.continuationEnabled;
    input.freePlay=setup_.freePlay;
    input.creditReady=false;
    input.coinEvent=0;
    input.nextDialogPageExists=originalRivalDialogKindAvailable(scene_.state().kind+1);
    frame_=advanceOriginalLegendReturn(state_,profile,input);
    timerTicks_=profile.u(1176);
    apply(profile,frame_);
}

void OriginalLegendVisit::apply(OriginalBattleProfile& profile,const OriginalLegendReturnFrame& frame){
    overlayVisible_=false;
    for(const auto& event:frame.events){
        switch(event.command){
        case C::ConfigureDialog:{
            OriginalRivalDialogSetup dialog;
            dialog.enemy=event.a;dialog.kind=event.b;
            dialog.weather=setup_.weather;dialog.playerCar=setup_.playerCar;dialog.cheer=setup_.cheer;
            dialog.playerName=playerName(profile);
            scene_.begin(profile,dialog);
            dialogVisible_=true;
            break;}
        case C::AdvanceDialog:
            if(dialogVisible_)advanceOriginalRivalDialogPage(scene_.state(),scene_.data(),profile,
                std::int32_t(event.a));
            break;
        case C::CloseDialog:
            if(dialogVisible_)closeOriginalRivalDialog(scene_.state());
            break;
        case C::SkipDialog:
            if(dialogVisible_)skipOriginalRivalDialog(scene_.state());
            break;
        case C::DestroyCommon:
            dialogVisible_=false;
            break;
        case C::StartCourseClear:{
            if(event.a>8)throw std::runtime_error("Invalid conquered course");
            const auto name="conquer0"+std::to_string(event.a);
            const auto folder=root_/"data/original_assets/conquer"/name;
            conquerModel_=NativeModel::load(folder/(name+".idasmesh"));
            // Source depth puts the translucent shadow behind the lettering;
            // the Unity UI submission path has no per-batch depth buffer.
            for(auto& chunk:conquerModel_.chunks){
                const auto depth=[](const NativeModelBatch& b){float z=0;for(const auto& v:b.vertices)z+=v.position.z;return b.vertices.empty()?0.f:z/float(b.vertices.size());};
                std::stable_sort(chunk.batches.begin(),chunk.batches.end(),[&](const auto& a,const auto& b){return depth(a)<depth(b);});
            }
            // The UI cache keys source images by allocation; discard the old
            // lookup before a different course bank can reuse that address.
            for(unsigned i=0;i<conquerTextures_.size();++i)unityUiForgetTexture(conquerTextures_.at(i));
            conquerTextures_=NativeTextureBank::load(folder/"textures/textures.idastex");
            conquerAnimation_.begin(event.a);movieRunning_=true;movieFade_=0;
            pending_.push_back(event);
            break;}
        case C::StepCourseClear:{
            const auto& animation=conquerAnimation_.step();
            for(unsigned i=0;i<animation.cueCount;++i)pending_.push_back({C::Cue,animation.cues[i],1});
            break;}
        case C::CourseClearFade:
            movieFade_=event.a>255?255:event.a;
            pending_.push_back(event);
            break;
        case C::FinishCourseClear:
            movieRunning_=false;movieFade_=0;
            pending_.push_back(event);
            break;
        case C::DrawContinue:
            selected_=event.a;
            overlayVisible_=true;
            overlay_=state_.won?OriginalLegendChoiceKind::ContinueAfterWin
                :OriginalLegendChoiceKind::ContinueAfterLoss;
            break;
        case C::DrawNextRival:
            selected_=event.a;
            overlayVisible_=true;
            // 0F12A0 uses 0F0F20 in phases105/106; 0F2240 uses 0F1EC0
            // in phases207/208. Confirmation must not change the question.
            overlay_=state_.won?OriginalLegendChoiceKind::Challenge
                :OriginalLegendChoiceKind::Rematch;
            break;
        default:
            if(callerOwned(event.command))pending_.push_back(event);
            break;
        }
    }
    if(state_.finished)active_=false;
}

void OriginalLegendVisit::paint(std::span<std::uint32_t> target,int width,int height)const{
    if(!active_&&!state_.finished)return;
    if(movieRunning_){
        const float fit=std::min(float(width)/640.f,float(height)/480.f);
        SpritePlacement placement;placement.invertY=true;placement.authoredHeight=0;
        placement.scale=100.f*fit;placement.offsetX=(width-640.f*fit)*.5f;placement.offsetY=(height-480.f*fit)*.5f;
        const auto& animation=conquerAnimation_.presentation;
        // Source depth puts the trailing text copies behind the primary word.
        // Submit back-to-front because Unity's overlay has no depth buffer.
        compositeOriginalMenuChunk(target,width,height,conquerTextures_,conquerModel_.chunks.at(0),placement);
        for(int chunk=8;chunk>=1;--chunk)for(unsigned i=1;i<animation.count;++i){
            const auto& draw=animation.draws[i];if(draw.chunk!=unsigned(chunk))continue;
            auto translated=placement;translated.offsetX+=draw.x*100.f*fit;
            compositeOriginalMenuChunk(target,width,height,conquerTextures_,conquerModel_.chunks.at(chunk),translated);
        }
        return;
    }
    if(dialogVisible_)scene_.paint(target,width,height);
    if(overlayVisible_)scene_.paintChoice(target,width,height,overlay_,selected_,timerTicks_);
}
}
