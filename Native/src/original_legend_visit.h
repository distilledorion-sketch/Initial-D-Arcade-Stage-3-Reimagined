#pragma once
#include "original_legend_return.h"
#include "original_conquer_animation.h"
#include "original_rival_dialog_scene.h"
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace idas3::original {
// Drives the source post-result Legend owner: the scripted rival dialogue, the
// continue and next-rival choices, and the destination it hands back. One
// advance() is one original 60 Hz owner update.
class OriginalLegendVisit {
public:
    struct Setup {
        std::uint32_t resultStatus=0;   // 0 win, 1 loss, 2 time up; already recorded
        std::uint32_t playerCar=0,weather=0,cheer=0;
        bool continuationEnabled=true,freePlay=true;
        // The retained A_VISIT parent byte: a continue is available. Free play
        // always grants one, so accepting the prompt carries on into the game.
        bool canContinue=true;
    };
    // skip is the start button the dialogue advertises on screen.
    struct Input { bool confirm=false,previous=false,next=false,skip=false; };

    static bool available(const std::filesystem::path& root){
        return OriginalRivalDialogScene::available(root);
    }
    void load(const std::filesystem::path& root){scene_.load(root);root_=root;}
    bool loaded()const{return scene_.loaded();}
    void begin(OriginalBattleProfile&,const Setup&);
    bool active()const{return active_;}
    void advance(OriginalBattleProfile&,const Input&);
    bool finished()const{return state_.finished;}
    // True while a choice prompt is on screen and the player's input matters.
    bool choiceVisible()const{return overlayVisible_;}
    OriginalLegendChoiceKind choiceKind()const{return overlay_;}
    std::uint32_t selectedIndex()const{return selected_;}
    std::uint32_t countdownTicks()const{return timerTicks_;}
    const OriginalRivalDialogState& dialogueState()const{return scene_.state();}
    OriginalLegendReturnDestination destination()const{return frame_.destination;}
    // True when this win cleared its course for the first time, which is
    // what routes the owner through the course-clear movie.
    bool courseCleared()const{return state_.courseClear528;}
    // Original course-clear artwork follows the existing 301-frame owner,
    // audio events and closing fade.
    bool courseClearRunning()const{return movieRunning_;}
    std::uint32_t courseClearFadeArgb()const{return movieFade_<<24;}
    void paint(std::span<std::uint32_t> target,int width,int height)const;
    std::uint32_t fadeArgb()const{return dialogVisible_?scene_.fadeArgb():0u;}
    // Events the caller still owns: music, cues and the course-clear movie.
    std::vector<OriginalLegendReturnEvent> takeEvents(){return std::move(pending_);}
private:
    OriginalRivalDialogScene scene_;
    std::filesystem::path root_;
    NativeModel conquerModel_;
    OriginalConquerAnimation conquerAnimation_;
    NativeTextureBank conquerTextures_;
    OriginalLegendReturnState state_{};
    OriginalLegendReturnFrame frame_{};
    Setup setup_{};
    std::vector<OriginalLegendReturnEvent> pending_;
    std::uint32_t selected_=0,timerTicks_=879;
    bool active_=false,dialogVisible_=false,overlayVisible_=false,movieRunning_=false;
    std::uint32_t movieFade_=0;
    OriginalLegendChoiceKind overlay_{};
    void apply(OriginalBattleProfile&,const OriginalLegendReturnFrame&);
    std::string playerName(const OriginalBattleProfile&)const;
};
}
