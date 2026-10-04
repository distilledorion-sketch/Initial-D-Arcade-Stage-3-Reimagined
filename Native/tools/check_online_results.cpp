#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include <iostream>

int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("Native root and new evidence directory required");
    const auto root=fs::absolute(argv[1]),out=fs::absolute(argv[2]);
    if(fs::exists(out))throw std::runtime_error("Preserve existing evidence");fs::create_directories(out);
    unsigned checks=0;auto check=[&](bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);};
    auto app=std::make_unique<App>();app->root=root;app->saveRoot=out/"userdata";app->validationMode=true;app->settings();
    app->frontend.initialize(root,true);app->hud.loadOriginal(root);app->audio.configure(root);
    app->originalCamera=OriginalChaseCamera::load(root);app->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
    app->saveSlots=LocalSaveSlots(app->saveRoot/"saves");app->activeSaveSlot=0;
    app->profiles=LocalDriverProfiles(app->saveSlots.profileDirectory(0));
    auto offline=original::makeOriginalFreshBattleProfile();offline.setu(16,0);offline.setu(72,7890);
    auto target=offline;target.setu(16,8);target.setu(72,12000);
    check(app->saveSlots.adopt(0,offline)&&app->saveSlots.adopt(1,target),"Fixture slots");
    check(app->profiles.save(0,offline),"Offline fixture");
    LocalDriverProfiles rewards(app->saveSlots.profileDirectory(1));check(rewards.save(8,target),"Online fixture");
    for(unsigned slot=0;slot<2;++slot)for(int winner:{0,1,2,-1}){
        app->frontend.car=0;app->loadedProfileCar=0;app->frontend.battleProfile=offline;app->battleProfile=offline;
        const auto prior=rewards.load(8).profile;
        Idas3MultiplayerConfig config{sizeof(config),1,0,0,0,0,8,0,slot,1};
        app->startMultiplayer(config,&prior,&offline);
        app->multiplayer.rewardSelection=onlineSlotCarSelection(1,8);
        bool rejected=false;try{app->setMultiplayerResult(winner);}catch(const std::logic_error&){rejected=true;}
        check(rejected,"Unfinished race awarded points");
        app->race.phase=RacePhase::Finished;app->setMultiplayerResult(winner);
        const auto earned=rewards.load(8).profile;auto expected=prior;
        const unsigned amount=winner<0?0:1000u+(winner==int(slot)?1000u:0u);expected.setu(72,prior.u(72)+amount);
        check(earned.words==expected.words&&app->multiplayer.pointsEarned==amount,"Award changed fields besides selected-car balance");
        check(app->profiles.load(0).profile.words==offline.words,"Award touched another save/car");
        app->setMultiplayerResult(winner);check(rewards.load(8).profile.words==earned.words,"Duplicate result awarded twice");
        rejected=false;try{app->setMultiplayerResult(winner==0?1:0);}catch(const std::logic_error&){rejected=true;}
        check(rejected,"Conflicting duplicate result accepted");
        app->leaveMultiplayer();
        check(app->menu&&app->frontend.stage==FrontendStage::Mode&&!app->multiplayer.active,"Post-race exit must return to mode selection");
        check(app->frontend.battleProfile.words==offline.words,"Exit replaced offline driver's profile");
    }
    // The current offline car also retains its award on restore.
    app->frontend.car=0;app->loadedProfileCar=0;app->frontend.battleProfile=offline;
    Idas3MultiplayerConfig config{sizeof(config),1,0,0,0,0,0,8,0,1};app->startMultiplayer(config,&offline,&target);
    app->multiplayer.rewardSelection=onlineSlotCarSelection(0,0);app->race.phase=RacePhase::Finished;app->setMultiplayerResult(0);
    app->leaveMultiplayer();check(app->frontend.battleProfile.u(72)==9890&&app->profiles.load(0).profile.u(72)==9890,"Current car award was undone on return");
    offline=app->frontend.battleProfile;app->startMultiplayer(config,&offline,&target);app->multiplayer.rewardSelection=onlineSlotCarSelection(0,0);
    app->disconnectMultiplayer();bool rejected=false;try{app->setMultiplayerResult(0);}catch(const std::logic_error&){rejected=true;}
    check(rejected&&app->profiles.load(0).profile.u(72)==9890,"Disconnected race awarded points");app->leaveMultiplayer();
    auto peer=std::make_unique<App>();peer->root=root;peer->saveRoot=out/"peer-userdata";peer->validationMode=true;peer->settings();
    peer->frontend.initialize(root,true);peer->hud.loadOriginal(root);peer->audio.configure(root);
    peer->originalCamera=OriginalChaseCamera::load(root);peer->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
    for(unsigned winner=0;winner<2;++winner){
        config.version=2;config.localSlot=0;app->startMultiplayer(config,&offline,&target);app->enableAuthority(909+winner,true,false);
        auto guest=config;guest.localCar=config.remoteCar;guest.remoteCar=config.localCar;guest.localSlot=1;
        peer->startMultiplayer(guest,&target,&offline);peer->enableAuthority(909+winner,true,false);
        for(auto* owner:{app.get(),peer.get()}){
            // Private, same-state terminal boundary on both numerical peers.
            // The real wire confirmation and App result guard still run.
            auto& rules=const_cast<original::OriginalRaceRules&>(owner->authorityRace->rules(winner));
            auto state=rules.state();state.phase=original::OriginalRacePhase::Finished;state.times.finishTime=120004;
            rules.restoreNumericalState(state);
            original::OriginalHostInputState input;const auto idle=original::adaptOriginalHostInput(input,{},true,false,0);
            check(owner->authorityLink->step(idle),"Authority boundary step");
            check(owner->authorityWinner()==-3,"Unverified crossing exposed a winner");
        }
        for(unsigned exchange=0;exchange<3;++exchange){
            const auto hostPacket=app->authorityLink->packet(),guestPacket=peer->authorityLink->packet();
            app->receiveAuthority(guestPacket);peer->receiveAuthority(hostPacket);
        }
        for(auto* owner:{app.get(),peer.get()}){
            check(owner->authorityWinner()==int(winner),"First finisher did not settle through verified wire history");
            check(owner->authorityFinishTicks[winner]==1200&&owner->authorityFinishTicks[1-winner]==0,"Trailing car acquired a made-up finish time");
            rejected=false;try{owner->setMultiplayerResult(1-int(winner));}catch(const std::logic_error&){rejected=true;}
            check(rejected,"Unverified winner overrode the shared result");
            owner->setMultiplayerResult(int(winner));check(owner->race.phase==RacePhase::Finished,"Verified result did not end both local presentations");
            owner->leaveMultiplayer();check(!owner->authorityResultFrame,"Result frame survived a rematch reset");
        }
    }
    std::ofstream(out/"PASS.txt")<<checks<<" native result/garage/persistence/mode-return assertions\n";
    std::cout<<"PASS "<<checks<<" online result application checks\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
