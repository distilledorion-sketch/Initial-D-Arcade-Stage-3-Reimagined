#pragma once
#include "native_assets.h"
#include "original_card_check.h"
#include "original_tuning.h"
#include <map>
#include <string>
namespace idas3::original {
// 10A700, the original installed-part flags at owner+1168..1177.
std::array<bool,16> originalSavedPartFlags(const OriginalBattleProfile&);
// iSelCheck: a review of the selected car's actual profile, not a second save.
class OriginalSavedData {
public:
    void load(const std::filesystem::path& root);
    void begin(OriginalBattleProfile&);
    void change(int delta);
    void moveRow(int direction){if(ready())input_.rowDirection=direction>0?1:-1;}
    void confirm(){if(ready())input_.confirm=true;}
    void cancel(){if(ready())input_.cancel=true;}
    void nextPage(){if(ready())input_.nextPage=true;}
    OriginalCardCheckEvents advance(OriginalBattleProfile&);
    void paint(std::span<std::uint32_t>,int,int,const OriginalBattleProfile&)const;
    void paintBackground(std::span<std::uint32_t>,int,int)const;
    bool ready()const{return state_.phase452==1;}
    std::uint32_t fadeArgb()const{return state_.overlay460?std::min(state_.fade464,15u)*255/15<<24:0;}
    const OriginalCardCheckState& state()const{return state_;}
private:
    struct Bank{NativeModel model;NativeTextureBank textures;};
    std::map<std::string,Bank> banks_;
    NativeTextureBank optionFont_;
    std::array<std::uint16_t,9216> glyphMap_{};
    std::array<std::pair<std::uint16_t,float>,35> spacing_{};
    OriginalTuningData tuning_;
    OriginalCardCheckState state_;
    OriginalCardCheckInput input_;
    unsigned background_=0,countdown_=2479,animationFrame_=0;
    bool japanese_=false;
};
}
