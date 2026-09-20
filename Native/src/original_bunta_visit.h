#pragma once
#include "original_rival_dialog_scene.h"
#include "original_legend_return.h"

namespace idas3::original {
// Bunta-only HBunta children1/6/7. Reuses the original script interpreter and
// painter, with Bunta's own 408 authored records and challenge portrait bank.
class OriginalBuntaVisit {
public:
    struct Setup { bool beforeRace=true;std::uint32_t resultStatus=0,playerCar=0; };
    struct Input { bool skip=false; };
    static bool available(const std::filesystem::path& root);
    void load(const std::filesystem::path& root){scene_.load(root);}
    bool loaded()const{return scene_.loaded()&&scene_.data().hasBunta();}
    void begin(OriginalBattleProfile&,const Setup&);
    void advance(OriginalBattleProfile&,const Input&);
    bool active()const{return active_;}
    bool finished()const{return finished_;}
    bool beforeRace()const{return setup_.beforeRace;}
    const OriginalRivalDialogState& dialogueState()const{return scene_.state();}
    void paint(std::span<std::uint32_t> target,int width,int height)const{scene_.paint(target,width,height);}
    std::uint32_t fadeArgb()const{return scene_.fadeArgb();}
    std::vector<OriginalLegendReturnEvent> takeEvents(){auto out=std::move(events_);events_.clear();return out;}
private:
    OriginalRivalDialogScene scene_;
    Setup setup_{};
    bool active_=false,finished_=false,closing_=false;
    std::vector<OriginalLegendReturnEvent> events_;
};
// Source0F88C0's selected script kind from182C00/182760/182A60. Win is
// called after result progression and normalizes the source level16 sentinel.
std::uint32_t originalBuntaDialogKind(OriginalBattleProfile&,bool beforeRace,std::uint32_t resultStatus);
}
