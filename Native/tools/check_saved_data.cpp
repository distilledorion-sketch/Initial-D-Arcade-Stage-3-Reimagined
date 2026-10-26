#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include <iostream>

// Real application input, disk persistence and GPU composition, all offscreen.
// Never opens an audio device and never reads or writes the player's saves.
int main(int argc,char** argv)try{
    if(argc<3||argc>4)throw std::runtime_error("Native root, NEW evidence directory, optional language required");
    const auto root=fs::absolute(argv[1]),out=fs::absolute(argv[2]);
    if(fs::exists(out))throw std::runtime_error("Preserve existing evidence");
    fs::create_directories(out);setOriginalUiLanguage(argc==4?std::stoi(argv[3]):0);
    unsigned checks=0;auto check=[&](bool b,const char* reason){++checks;if(!b)throw std::runtime_error(reason);};
    auto app=std::make_unique<App>();app->root=root;app->saveRoot=out/"userdata";app->settings();
    app->frontend.initialize(root,true);app->hud.loadOriginal(root);app->audio.configure(root);
    app->originalCamera=OriginalChaseCamera::load(root);
    app->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
    check(app->renderer.initialize(nullptr,640,480,true),"Offscreen renderer initialization");
    app->demoPreparationStarted=true;app->replayRecordingFlags=0;
    auto fixture=original::makeOriginalFreshBattleProfile();
    fixture.setu(0,1);fixture.setu(16,0);fixture.setu(1180,1);fixture.setu(76,5);
    for(unsigned i=0;i<5;++i)fixture.setu(44+4*i,162);
    fixture.setu(72,5000);fixture.setu(1140,50);fixture.setu(1160,12345);fixture.setByte(156,1);
    original::registerOriginalPersonalTimeAttackRecord(fixture,original::originalRecordPartition(6,false),1185888,1,{});
    check(app->saveSlots.adopt(0,fixture),"First isolated save metadata");
    app->useSaveSlot(0);check(app->profiles.save(0,fixture)&&app->driverSetup.markComplete(0),"First isolated profile");
    auto other=fixture;other.setu(16,19);other.setu(72,43210);other.setByte(1184,1);
    check(app->profiles.save(19,other)&&app->driverSetup.markComplete(19),"Other car profile");
    check(app->saveSlots.adopt(1,other),"Second save metadata");
    app->useSaveSlot(1);check(app->profiles.save(19,other)&&app->driverSetup.markComplete(19),"Second isolated profile");
    app->menu=true;app->openSaveFile(0);
    check(app->frontend.stage==FrontendStage::SavedData,"Continue an existing save skipped Saved Data");
    const auto render=[&](double dt){check(app->render(dt),"Application rendering failed");};
    const auto step=[&](int key=0){app->input={};if(key){app->input.pressed[key]=true;app->input.down[key]=true;}app->commands(1./60.);render(1./60.);app->input={};};
    const auto capture=[&](const std::string& name){check(app->renderer.saveBitmap((out/(name+".bmp")).wstring()),"Offscreen capture failed");};
    for(unsigned i=0;i<17;++i)step();
    for(unsigned page:{0u,1u,2u,3u,5u,6u,7u}){
        check(app->frontend.savedData()->state().page492==page,"View Change page routing");
        capture("saved-"+std::to_string(page));if(page!=7)step('C');
    }
    // Enter BGM, choose FIX and commit with ordinary native input.
    step('Q');step(VK_RETURN);step(VK_RIGHT);step();step(VK_RIGHT);step(VK_RETURN);
    check(app->frontend.battleProfile.byte(1184)==2,"Config setting did not commit");
    check(app->profiles.load(0).profile.byte(1184)==2,"Config setting was not persisted by the App");
    check(app->profiles.load(19).profile.words==other.words,"Config modified a different car");
    check(LocalDriverProfiles(app->saveSlots.profileDirectory(1)).load(19).profile.words==other.words,"Config crossed save slots");
    const auto updated=app->profiles.load(0).profile;
    for(unsigned offset=0;offset<1228;offset+=4)if(offset!=1184){
        if(updated.u(offset)!=fixture.u(offset))std::cerr<<"Profile offset "<<offset<<": "<<fixture.u(offset)<<" -> "<<updated.u(offset)<<'\n';
        check(updated.u(offset)==fixture.u(offset),"Browsing/config changed earned progress");
    }
    step('C');step(VK_RETURN);for(unsigned i=0;i<18;++i)step();
    check(app->frontend.stage==FrontendStage::Transmission,"End failed to resume transmission selection");
    app->openSaveFile(0);check(app->frontend.battleProfile.byte(1184)==2,"Reopening lost config");
    app->openSaveFile(1);check(app->frontend.battleProfile.byte(1184)==1,"Reopening another save inherited first config");

    // At144Hz, sample real held/edge controls and allow the60Hz menu owner
    // to consume them. Cover keyboard, pad stick and the canonical key packet
    // produced by wheel bindings; these tests do not touch physical devices.
    constexpr std::array<unsigned,5> optionOffsets{1184,1186,1187,1188,1189},optionCounts{3,3,9,2,2};
    for(unsigned device=0;device<3;++device){
        app->openSaveFile(0);for(unsigned i=0;i<17;++i)step();
        for(unsigned i=0;i<6;++i)step('C');
        check(app->frontend.savedData()->state().page492==7,"Configuration control test did not reach options");
        const auto send=[&](int key,WORD buttons,SHORT stick=0){
            for(unsigned frame=0;frame<6;++frame){
                const auto old=app->input;app->input={};
                if(frame<3){if(key){app->input.down[key]=true;app->input.pressed[key]=!old.down[key];}
                    app->input.connected=device==1;app->input.pad.Gamepad.wButtons=buttons;
                    app->input.pad.Gamepad.sThumbLX=stick;app->input.pressedButtons=buttons&~old.pad.Gamepad.wButtons;}
                app->commands(1./144.);render(1./144.);
            }
        };
        for(unsigned row=0;row<5;++row){
            send(device==1?0:VK_DOWN,device==1?XINPUT_GAMEPAD_DPAD_DOWN:0);
            check(app->frontend.savedData()->state().row1460==row,"Device selected the wrong configuration row");
            const unsigned old=app->frontend.battleProfile.byte(optionOffsets[row]);
            send(device==1?0:VK_RIGHT,0,device==1?24000:0);
            check(app->frontend.savedData()->state().draft1544[row]==(old+1)%optionCounts[row],"Device failed to change highlighted option");
            check(app->profiles.load(0).profile.byte(optionOffsets[row])==old,"Draft option was saved before confirmation");
            send(device==0?VK_ESCAPE:0,device?XINPUT_GAMEPAD_B:0);
            check(app->frontend.savedData()->state().draft1544[row]==old,"Device cancel did not restore stored value");
            send(device==1?0:VK_LEFT,0,device==1?-24000:0);
            send(device==1?0:VK_RETURN,device==1?XINPUT_GAMEPAD_A:0);
            check(app->profiles.load(0).profile.byte(optionOffsets[row])==(old+optionCounts[row]-1)%optionCounts[row],"Device option edit was not persisted");
        }
        const auto committed=app->frontend.battleProfile.words;
        app->openSaveFile(0);check(app->frontend.battleProfile.words==committed,"Configuration edits did not survive reopening the save");
        check(app->profiles.load(19).profile.words==other.words,"Configuration edits changed another car");
        check(LocalDriverProfiles(app->saveSlots.profileDirectory(1)).load(19).profile.words==other.words,"Configuration edits changed another save slot");
    }

    app->validationMode=true;app->openSaveFile(0);
    app->frontend.gameMode=original::OriginalGameMode::TimeAttack;
    app->frontend.course=app->courseIndex=3;app->frontend.night=app->night=true;
    app->frontend.wet=app->wet=app->frontend.reverse=app->reverse=false;
    app->start();app->loadingActive=app->vsActive=app->preRaceDialogueActive=false;
    app->race.phase=RacePhase::Finished;app->race.timeUp=false;app->race.elapsed6000=1185888;
    check(app->beginTimeAttackVisit(false),"Completed finish did not open TA visit");
    using Stage=original::OriginalTimeAttackVisit::Stage;
    check(app->timeAttackVisit.stage()==Stage::Ranking,"Real completed run did not open ranking");
    check(app->timeAttackVisit.setup().passwordSalt==12345,"Password salt was not taken from selected save");
    for(unsigned i=0;i<35;++i)render(1./60.);
    app->input.pressed[VK_RETURN]=true;render(1./60.);app->input={};
    for(unsigned i=0;i<55;++i)render(1./60.);
    check(app->timeAttackVisit.stage()==Stage::Password,"Completed ranking skipped password");
    capture("finish-password");const auto next=app->timeAttackRankingPreview->nextState();const auto before=app->battleProfile.words;
    render(0);render(0);check(app->timeAttackRankingPreview->nextState().frames==next.frames&&app->battleProfile.words==before,"Password repaint mutated rotation or progress");
    for(unsigned i=0;i<120;++i)render(1./60.);capture("finish-password-rotated");
    app->input.pressed[VK_RETURN]=true;render(1./60.);app->input={};for(unsigned i=0;i<40;++i)render(1./60.);
    check(app->timeAttackVisit.stage()==Stage::Continue,"Password failed to reach Continue");capture("finish-continue");
    for(unsigned view=0;view<4;++view){
        for(unsigned i=0;i<64;++i)render(1./60.);
        capture("finish-continue-"+std::to_string(view));
    }
    app->race.timeUp=true;check(app->beginTimeAttackVisit(false),"Time-up failed to open visit");
    check(app->timeAttackVisit.stage()==Stage::Continue,"Time-up incorrectly opened password");
    app->timeAttackVisitActive=false;app->race.phase=RacePhase::Running;
    app->battleProfile.setByte(1189,0);app->input={};app->input.down[VK_ESCAPE]=app->input.down['C']=true;
    app->commands(1./60.);check(app->race.phase==RacePhase::Running,"Disabled Forced Quit retired the race");
    app->battleProfile.setByte(1189,1);app->multiplayer.active=true;app->commands(1./60.);
    check(app->race.phase==RacePhase::Running,"Forced Quit bypassed the online race owner");
    app->multiplayer.active=false;app->commands(1./60.);
    check(app->race.phase==RacePhase::Finished&&app->race.timeUp,"Enabled Forced Quit did not retire offline race");
    std::ofstream(out/"PASS.txt")<<checks<<" checks; real Continue/save/input/config isolation and finish/password flow; offscreen, no audio device\n";
    std::cout<<"PASS "<<checks<<" saved-data application checks\n";
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
