#include "original_bunta_visit.h"
#include <algorithm>
#include <stdexcept>

namespace idas3::original {
bool OriginalBuntaVisit::available(const std::filesystem::path& root){
    const auto base=root/"data/original_assets/rival_dialog";
    return OriginalRivalDialogScene::available(root)&&std::filesystem::exists(base/"bunta.idasdialog")
        &&std::filesystem::exists(base/"banks/rival_bunta_challenge/rival_bunta_challenge.idasmesh")
        &&std::filesystem::exists(base/"banks/rival_bunta_challenge/textures/textures.idastex");
}
std::uint32_t originalBuntaDialogKind(OriginalBattleProfile& profile,bool before,std::uint32_t status){
    const auto course=profile.u(4);
    if(course>8)throw std::out_of_range("Original Bunta dialogue course");
    auto level=profile.u(1080+course*4);
    if(!before&&status==0&&level==16){level=15;profile.setu(1080+course*4,15);}
    // Snow's opening and victory scenes deliberately use Akina's level when
    // it exceeds10. Loss reads Snow's own saved level (182A88), not Akina's.
    if(course==8&&profile.u(1092)>10&&(before||status==0))level=profile.u(1092);
    const auto kind=level+(before?0u:(status==0?16u:32u));
    if(kind>=51)throw std::out_of_range("Original Bunta dialogue saved level");
    return kind;
}
void OriginalBuntaVisit::begin(OriginalBattleProfile& profile,const Setup& setup){beginImpl(profile,setup,nullptr);}
void OriginalBuntaVisit::begin(OriginalBattleProfile& profile,const Setup& setup,std::uint32_t& sharedRandom){beginImpl(profile,setup,&sharedRandom);}
void OriginalBuntaVisit::beginImpl(OriginalBattleProfile& profile,const Setup& setup,std::uint32_t* sharedRandom){
    if(!loaded())throw std::runtime_error("Original Bunta dialogue data not loaded");
    setup_=setup;events_.clear();finished_=false;closing_=false;active_=true;
    OriginalRivalDialogSetup dialog;
    dialog.enemy=30;dialog.buntaChallenge=true;dialog.buntaCourse=profile.u(4);
    dialog.kind=originalBuntaDialogKind(profile,setup.beforeRace,setup.resultStatus);
    dialog.playerCar=setup.playerCar;
    //0F88C0 does not copy the player's name or draw the Legend name plate.
    if(sharedRandom)scene_.beginWithSharedRandom(profile,dialog,*sharedRandom);
    else scene_.begin(profile,dialog);
    using C=OriginalLegendReturnCommand;
    events_.push_back({C::SoundSet,2,0});
    events_.push_back({C::MusicRequest,setup.beforeRace?33u:(setup.resultStatus==0?34u:35u),1});
}
void OriginalBuntaVisit::advance(OriginalBattleProfile& profile,const Input& input){advanceImpl(profile,input,nullptr);}
void OriginalBuntaVisit::advance(OriginalBattleProfile& profile,const Input& input,std::uint32_t& sharedRandom){advanceImpl(profile,input,&sharedRandom);}
void OriginalBuntaVisit::advanceImpl(OriginalBattleProfile& profile,const Input& input,std::uint32_t* sharedRandom){
    if(!active_||finished_)return;
    // Pre-race182E20 checks skip before child Main; post182840/182B20
    // updates child first through0F0560, then handles the skip bit.
    if(setup_.beforeRace&&input.skip)skipOriginalRivalDialog(scene_.state());
    if(sharedRandom)scene_.stepWithSharedRandom(profile,*sharedRandom);
    else scene_.step(profile);
    if(!setup_.beforeRace&&input.skip)skipOriginalRivalDialog(scene_.state());
    if(!closing_&&scene_.ready()){
        closeOriginalRivalDialog(scene_.state());closing_=true;
        events_.push_back({OriginalLegendReturnCommand::MusicFade,0,0});
    }
    if(scene_.closed()){
        finished_=true;active_=false;
        events_.push_back({OriginalLegendReturnCommand::Finish,0,0});
    }
}
}
