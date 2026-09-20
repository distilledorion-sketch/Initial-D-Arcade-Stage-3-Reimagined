#pragma once
#include "native_assets.h"
#include "original_legend_choice.h"
#include "original_rival_dialog.h"
#include <filesystem>
#include <span>
#include <string>

namespace idas3::original {
// Imported artwork for one rival dialogue scene plus a CPU painter for the
// draw list the component produces. Banks are ordinary decoded assets; the
// placement is the recovered source layout, not an approximation.
class OriginalRivalDialogScene {
public:
    static bool available(const std::filesystem::path& root);
    // Loads the dialogue records and the banks every scene shares.
    void load(const std::filesystem::path& root);
    bool loaded()const{return loaded_;}
    // Loads this rival's portrait and background banks and resets the scene.
    void begin(const OriginalBattleProfile&,const OriginalRivalDialogSetup&);
    std::uint32_t step(const OriginalBattleProfile& profile){
        return stepOriginalRivalDialog(state_,data_,profile);
    }
    const OriginalRivalDialogState& state()const{return state_;}
    OriginalRivalDialogState& state(){return state_;}
    const OriginalRivalDialogData& data()const{return data_;}
    bool ready()const{return originalRivalDialogReady(state_);}
    bool closed()const{return state_.closed;}
    std::uint32_t fadeArgb()const{return originalRivalDialogFadeArgb(state_);}
    // Paints the current draw list onto a caller canvas, fitting the original
    // 640x480 composition. The owner applies the fade afterwards.
    void paint(std::span<std::uint32_t> target,int width,int height)const;
    // Paints a Legend return choice overlay over whatever is already drawn.
    void paintChoice(std::span<std::uint32_t> target,int width,int height,
        OriginalLegendChoiceKind,std::uint32_t selected,std::uint32_t timerTicks)const;
private:
    struct Bank { NativeModel model; NativeTextureBank textures; };
    std::filesystem::path root_;
    OriginalRivalDialogData data_;
    Bank scene_,portrait_,background_,choice_,prompt_;
    NativeTextureBank alphabet_,namekana_;
    std::string portraitName_,backgroundName_;
    OriginalRivalDialogState state_;
    bool loaded_=false;
};
}
