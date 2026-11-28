#pragma once
#include "native_assets.h"
#include "original_card_check.h"
#include "native_multiplayer.h"
#include "original_tuning.h"
#include <map>
#include <string>
namespace idas3::original {
// Resolve a Saved Data field against the loaded regional cardcheck bank.
unsigned originalCardCheckChunk(const NativeModel&,unsigned field);
// 10A700, the original installed-part flags at owner+1168..1177.
std::array<bool,16> originalSavedPartFlags(const OriginalBattleProfile&);
// iSelCheck: a review of the selected car's actual profile, not a second save.
class OriginalSavedData {
public:
    void load(const std::filesystem::path& root);
    void begin(OriginalBattleProfile&,const Idas3BattleRecord* onlineRecord=nullptr);
    // Cosmetic inspection playback. This short source page cannot edit
    // options, renew tuning or write any part of the saved profile.
    void beginInspectionCompletion(const OriginalBattleProfile&);
    bool inspectionCompletion()const{return inspectionCompletion_;}
    bool inspectionOverlayVisible()const{return inspectionCompletion_&&renewal_.writePhase532==1;}
    unsigned animationFrame()const{return animationFrame_;}
    void change(int delta);
    void moveRow(int direction){if(ready()&&!inspectionCompletion_)input_.rowDirection=direction>0?1:-1;}
    void confirm(){if(ready())input_.confirm=true;}
    void cancel(){if(ready()&&!inspectionCompletion_)input_.cancel=true;}
    void nextPage(){if(ready()&&!inspectionCompletion_)input_.nextPage=true;}
    OriginalCardCheckEvents advance(OriginalBattleProfile&);
    struct CarPicker{unsigned frame,seconds,page=0;};
    void paint(std::span<std::uint32_t>,int,int,const OriginalBattleProfile&,const CarPicker* picker=nullptr)const;
    bool ready()const{return state_.phase452==1;}
    std::uint32_t fadeArgb()const{return state_.overlay460?std::min(state_.fade464,15u)*255/15<<24:0;}
    const OriginalCardCheckState& state()const{return state_;}
private:
    struct Bank{NativeModel model;NativeTextureBank textures;};
    std::map<std::string,Bank> banks_;
    NativeTextureBank optionFont_;
    NativeImage logoBackdrop_;
    std::array<std::uint16_t,9216> glyphMap_{};
    std::array<std::pair<std::uint16_t,float>,35> spacing_{};
    OriginalTuningData tuning_;
    OriginalCardCheckState state_;
    OriginalCardCheckInput input_;
    OriginalRenewCardCheckState renewal_;
    bool inspectionCompletion_=false;
    unsigned background_=0,countdown_=2479,animationFrame_=0;
    bool japanese_=false,rawJapanese_=false;
    Idas3BattleRecord versus_{};
};
}
