#include "frontend.h"
#include "local_driver_profiles.h"
#include "original_legend_progress.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace idas3;
namespace {void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::invalid_argument("nativeRoot scratchParent required");
    const auto directory=std::filesystem::path(argv[2])/("story-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    LocalDriverProfiles store(directory);auto profile=store.load(0).profile;
    original::selectOriginalRival(profile,0);
    original::recordOriginalLegendResult(profile,0,0);
    original::updateOriginalPostRaceRank(profile);
    const auto lossPoints=original::awardOriginalLegendPoints(profile,1,500);
    require(lossPoints.participation==1000&&lossPoints.win==0&&lossPoints.advantage==0&&profile.u(72)==1000,"Timeout must award participation only");
    require(profile.byte(116)==1,"Timeout while ahead must retain a loss");
    require(store.save(0,profile),"Story loss save failed");
    profile=store.load(0).profile;
    require(profile.u(72)==1000,"Points balance was lost after restart");
    for(unsigned enemy=0;enemy<28;++enemy){
        original::selectOriginalRival(profile,enemy);
        require(original::recordOriginalLegendResult(profile,1,0)==original::OriginalLegendResult::Win,"Source win not recorded");
        original::updateOriginalPostRaceRank(profile);
        const auto before=profile.u(72);
        const auto award=original::awardOriginalLegendPoints(profile,0,62.5f);
        if(enemy==0)require(award.total==3500,"First Myogi win should award1000 participation,2000 win and500 advantage");
        original::refreshOriginalLegendCourseProgress(profile);
        require(store.save(0,profile),"Story win save failed");profile=store.load(0).profile;
        require((profile.byte(116+enemy)>>4)==1,"Win did not persist after restart");
        require(profile.u(72)==before+award.total,"Earned points did not persist");
    }
    require(original::originalLegendChoices(profile,7).count==5,"Base completion did not unlock first Tsuchisaka secret");
    original::selectOriginalRival(profile,28);original::recordOriginalLegendResult(profile,1,0);require(store.save(0,profile),"First secret save failed");
    profile=store.load(0).profile;require(original::originalLegendChoices(profile,7).count==6,"Second Tsuchisaka secret not restored");
    original::selectOriginalRival(profile,29);original::recordOriginalLegendResult(profile,1,0);require(store.save(0,profile),"Second secret save failed");
    Frontend restarted;restarted.initialize(argv[1]);restarted.car=0;restarted.battleProfile=store.load(0).profile;
    restarted.gameMode=original::OriginalGameMode::LegendOfTheStreets;restarted.stage=FrontendStage::Course;restarted.course=3;
    restarted.confirm();restarted.advance(31./60.);
    require(restarted.stage==FrontendStage::Rival&&restarted.rivalChoice==5&&restarted.battleProfile.u(24)==30,"Restarted menu did not expose Akina final rival");
    const auto& menu=restarted.paint(1280,720);require(menu.size()==1280*720,"Unlocked menu did not render");
    original::recordOriginalLegendResult(restarted.battleProfile,1,0);require(store.save(0,restarted.battleProfile),"Final rival save failed");
    require(store.load(0).profile.byte(146)==0x10,"Final rival completion missing");
    require(store.load(1).origin==LocalDriverProfiles::Origin::Fresh&&store.load(1).profile.byte(116)==0,"Other car inherited story progress");
    require(store.load(0).profile.byte(116)==0x11,"Original loss count was lost during campaign wins");
    // Native card storage must keep the confirmed source paint/transmission
    // fields alongside earned progress, and restore each car independently.
    const auto earned=store.load(0).profile;
    Frontend appearance;appearance.initialize(argv[1]);appearance.stage=FrontendStage::Car;
    appearance.make=6;appearance.car=0;appearance.battleProfile=earned;
    appearance.advance(8./60.);appearance.changeColor(-1);appearance.advance(1./60.);
    require(appearance.selectedColor()==2&&appearance.battleProfile.u(64)==0,"Paint preview committed before the source confirmation event");
    appearance.confirm();appearance.advance(1./60.);
    require(appearance.battleProfile.u(64)==2,"Confirmed factory color was not written to profile64");
    appearance.advance(162./60.);require(appearance.stage==FrontendStage::Car,"Car confirmation finished before164 updates");
    appearance.advance(1./60.);require(appearance.stage==FrontendStage::Transmission,"Car confirmation failed to enter Transmission");
    appearance.advance(15./60.);appearance.change(1);appearance.confirm();
    require(!appearance.inputReady()&&!appearance.confirmationInProgress()&&appearance.automatic,"Transmission accepted input before its16-update entry fade");
    appearance.advance(1./60.);appearance.change(1);appearance.confirm();appearance.advance(1./60.);
    require(!appearance.automatic&&appearance.battleProfile.u(68)==1,"MT confirmation did not write source profile68");
    appearance.advance(140./60.);require(appearance.stage==FrontendStage::Transmission,"Transmission ended before142 confirmation updates");
    appearance.advance(1./60.);require(appearance.stage==FrontendStage::Mode,"Transmission source completion did not reach native Mode");
    require(appearance.battleProfile.u(72)==earned.u(72),"Appearance selection changed earned points");
    for(unsigned i=116;i<147;++i)require(appearance.battleProfile.byte(i)==earned.byte(i),"Appearance selection changed recorded rival results");
    require(store.save(0,appearance.battleProfile),"Factory appearance and transmission save failed");
    LocalDriverProfiles reopened(directory);const auto restored=reopened.load(0);
    require(restored.origin==LocalDriverProfiles::Origin::Saved&&restored.profile.u(64)==2&&restored.profile.u(68)==1,"Factory color/MT did not survive reopening the profile store");
    Frontend restoredMenu;restoredMenu.stage=FrontendStage::Car;restoredMenu.make=6;restoredMenu.car=0;
    restoredMenu.advance(1./60.);restoredMenu.battleProfile=restored.profile;restoredMenu.driverProfileLoaded();restoredMenu.advance(7./60.);
    require(restoredMenu.selectedColor()==2,"Profile loading during entry did not restore the saved factory preview");
    Frontend other;other.stage=FrontendStage::Car;other.make=6;other.car=1;other.battleProfile=reopened.load(1).profile;
    other.advance(8./60.);other.changeColor(1);other.confirm();other.advance(1./60.);
    require(other.battleProfile.u(16)==1&&other.battleProfile.u(64)==1,"Second car color was not committed with its own identity");
    require(reopened.save(1,other.battleProfile),"Independent car color save failed");
    require(reopened.load(1).profile.u(64)==1&&reopened.load(1).profile.byte(116)==0&&reopened.load(1).profile.u(72)==0,"Second car inherited appearance or earned progress");
    require(reopened.load(0).profile.u(64)==2&&reopened.load(0).profile.u(68)==1&&reopened.load(0).profile.u(72)==earned.u(72),"Saving another car overwrote its neighbor's color, transmission or points");
    std::filesystem::remove_all(directory);
    std::cout<<"PASS controlled31-rival result/save/restart chain, original secret unlock order, restored final-opponent menu, source Car/Transmission confirmation and independent saved factory colors/MT.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
