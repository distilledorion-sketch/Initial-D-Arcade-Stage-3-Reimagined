#include "original_session_exit.h"
#include "original_rival_dialog_scene.h"
#include "original_results.h"
#include <fstream>
#include <iostream>
using namespace idas3;
using namespace idas3::original;
namespace {
unsigned checks=0;
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
using Pixels=std::vector<std::uint32_t>;
void save(const std::filesystem::path& path,const Pixels& pixels){
    std::ofstream file(path,std::ios::binary);file<<"P6\n640 480\n255\n";
    for(auto p:pixels){const char rgb[]{char(p>>16),char(p>>8),char(p)};file.write(rgb,3);}
    check(bool(file),"Could not write presentation evidence");
}
}
int main(int argc,char** argv)try{
    check(argc==3||argc==4,"Native root, evidence directory, optional arcade language required");
    setOriginalUiLanguage(argc==4?std::stoi(argv[3]):0);
    const std::filesystem::path root=argv[1],out=argv[2];std::filesystem::create_directories(out);
    OriginalSessionExit exit;exit.begin(root,true);
    using Stage=OriginalSessionExit::Stage;
    const auto paint=[&]{Pixels p(640*480,0xff000000);exit.paint(p,640,480);return p;};
    const auto saving=paint();save(out/"saving.ppm",saving);
    check(paint()==saving&&exit.frame==0,"Repaint advanced save presentation");
    check(std::count_if(saving.begin(),saving.begin()+120*640,[](auto p){return(p&0xffffff)!=0;})>60000,
        "Original card-save screen is missing its scrolling background");
    exit.step(false,false);exit.step(false,false);
    const auto scrolled=paint();check(scrolled!=saving,"Card-save background is frozen");
    check(paint()==scrolled,"Card background advances during repaint");
    for(unsigned i=0;i<400;++i)exit.step(true,false);
    check(exit.stage==Stage::SavingCard,"Failed save was bypassed by input or timeout");
    exit.step(false,true);check(exit.stage==Stage::RetrieveCard,"Successful save did not release retrieval screen");
    save(out/"retrieve.ppm",paint());check(paint()!=saving,"Retrieval instructions are missing");
    for(unsigned i=0;i<29;++i)exit.step(true,true);
    check(exit.stage==Stage::RetrieveCard,"Held confirm skipped retrieval immediately");
    exit.step(true,true);check(exit.stage==Stage::GameOver,"Retrieval did not lead to Game Over");
    const auto first=paint();for(unsigned i=0;i<40;++i)exit.step(false,true);
    const auto complete=paint();save(out/"game-over.ppm",complete);
    check(first!=complete,"Game Over authored strips did not animate");
    check(std::count_if(complete.begin(),complete.end(),[](auto p){return(p&0xffffff)!=0;})>10000,"Game Over artwork is blank");
    for(unsigned i=0;i<262;++i)exit.step(false,true);
    check(exit.stage==Stage::Finished,"Game Over never completed");
    exit.begin(root,false);check(exit.stage==Stage::GameOver,"Guest was sent through card retrieval");
    exit.step(true,true);check(exit.active(),"Game Over skipped all presentation on confirm");
    for(unsigned i=0;i<32;++i)exit.step(false,true);
    check(!exit.active(),"Confirmed Game Over failed to exit");
    OriginalRivalDialogScene choices;choices.load(root);
    // Canonical 0F0E40/0F0E9E cadence: off for 0,1,8,9; on for 2..7.
    // Verify actual imported pixels for every outcome/choice, not only draw counts.
    for(unsigned kind=0;kind<4;++kind)for(unsigned selected=0;selected<2;++selected){
        const auto capture=[&](bool confirming,unsigned frame){Pixels p(640*480,0xff253545);
            choices.paintChoice(p,640,480,OriginalLegendChoiceKind(kind),selected,600,confirming,frame);return p;};
        const auto idle=capture(false,0),hidden=capture(true,0);
        check(idle!=hidden,"Confirmed option does not visibly flash");
        for(unsigned f=0;f<10;++f){
            check(capture(true,f)==((f<2||f>=8)?hidden:idle),"Confirmation flash differs from source cadence");
            check(capture(false,f)==idle,"Unconfirmed options incorrectly flash");
        }
        const auto stem="choice-"+std::to_string(kind)+"-"+std::to_string(selected);
        save(out/(stem+"-on.ppm"),idle);save(out/(stem+"-off.ppm"),hidden);
    }
    const auto results=OriginalBattleResults::load(root);
    OriginalBattleResultsState state;state.profileMode=0;
    const auto result=[&]{Pixels p(640*480,0xff253545);results.paint(p,640,480,state);return p;};
    const auto ordinary=result();state.levelNoticeFlags=0x200000;
    const auto promoted=result();save(out/"points-no-level-notice.ppm",promoted);
    check(promoted==ordinary,"Racer level-up incorrectly appears on the points screen");
    const auto raceHud=OriginalRaceHud::load(root);
    const auto finish=[&](unsigned frame){Pixels p(640*480,0xff253545);OriginalHudState hud;
        hud.finishBanner=OriginalHudState::FinishBanner::win;raceHud.paint(p,640,480,hud);
        results.paintFinishNotice(p,640,480,state,frame);return p;};
    const auto racer=finish(0);save(out/"racer-level-up-finish.ppm",racer);
    state.levelNoticeFlags=0;check(finish(0)!=racer,"Finish screen is missing Racer Level Up");
    state.levelNoticeFlags=0x200000;check(finish(0)==racer&&finish(299)==racer,"Finish notice advances on repaint or disappears before points");
    state.profileMode=1;const auto ta=finish(0);state.levelNoticeFlags=0;
    check(finish(0)==ta,"Time Attack displayed a battle rank award");
    state.profileMode=2;state.levelNoticeFlags=0x300000;const auto bunta=finish(0);
    check(finish(180)==bunta&&finish(181)!=bunta,"Driver level-up never follows Bunta notice on finish screen");
    const auto buntaPoints=result();state.levelNoticeFlags=0;
    check(result()==buntaPoints,"Bunta/racer notices leaked into points");
    std::cout<<"PASS "<<checks<<" session exit, save barrier, authored screens, choice flashing and rank notice checks\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
