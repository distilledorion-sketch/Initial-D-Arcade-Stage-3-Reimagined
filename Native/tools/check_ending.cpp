#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include <iostream>
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("Native root and NEW evidence directory required");
    const auto root=fs::absolute(argv[1]),out=fs::absolute(argv[2]);
    if(fs::exists(out))throw std::runtime_error("Preserve existing evidence");fs::create_directories(out);
    unsigned checks=0;const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);};
    auto app=std::make_unique<App>();app->root=root;app->saveRoot=out/"userdata";app->validationMode=true;
    app->settings();app->frontend.initialize(root,true);app->hud.loadOriginal(root);app->audio.configure(root);
    check(app->renderer.initialize(nullptr,960,720,true),"Ending renderer");
    app->originalCamera=OriginalChaseCamera::load(root);
    app->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
    // Exercise real Legend capture with the optional replay archive disabled.
    app->frontend.gameMode=original::OriginalGameMode::LegendOfTheStreets;
    app->frontend.battleProfile=original::makeOriginalFreshBattleProfile();
    app->frontend.car=0;app->frontend.battleProfile.setu(16,0);
    original::selectOriginalRival(app->frontend.battleProfile,30);
    app->replayRecordingFlags=0;app->start();
    DriverInput input;input.throttle=1;input.automatic=true;
    for(unsigned i=0;i<1800;++i)app->simulate(input);
    check(!app->archiveThisRace&&app->endingReplay.size()>600,"Ending depends on optional replay saving");
    check(length(app->endingReplay.frame(0).cars[0].body-app->endingReplay.frame(600).cars[0].body)>10,"Ending did not record moving cars");
    check(app->endingReplay.frame(600).cars[1].visible,"Ending lost opponent");
    const auto footage=app->endingReplay;
    const auto physics=app->originalSession.rollbackDigest();
    const auto raceTicks=app->race.ticks;
    // Boundary coverage: overwrite oldest, preserve chronological order,
    // stop capture after sealing, hold the final pose rather than wrapping.
    EndingReplay ring;
    for(unsigned i=0;i<4001;++i){auto f=footage.frame(0);f.pathIndex=int(i);ring.record(f,*app->courseLightFsca);}
    ring.seal();check(ring.ready()&&ring.size()==3600,"Ending buffer bound");
    check(ring.frame(0).pathIndex==401&&ring.frame(3599).pathIndex==4000&&ring.frame(9999).pathIndex==4000,"Ending ring ordering/hold");
    ring.record(footage.frame(0),*app->courseLightFsca);check(ring.frame(3599).pathIndex==4000,"Sealed ending accepted live data");
    app->menu=true;
    const auto before=app->battleProfile.words;
    for(unsigned fps:{30u,60u,144u,240u}){
        app->endingReplay=footage;
        app->beginEnding();check(app->extraModeVisitActive(),"Ending input ownership");
        app->commands(1);check(app->endingActive,"Generic command interrupted ending");
        unsigned renders=0;
        while(app->endingActive&&renders<fps*100){
            app->advanceEnding(1./fps);++renders;
            if(fps==60&&(renders==600||renders==2400||renders==4600||renders==5100)){
                check(app->renderEnding(0),"Ending frame");
                check(app->ending.timeline.finalCard||app->endingTexturesLoaded,"Driving backdrop was not rendered");
                check(app->renderer.saveBitmap((out/("ending-"+std::to_string(renders)+".bmp")).wstring()),"Ending capture");
            }
            if(renders==fps*10){
                check(app->audio.legendStreamStatistics().stream==12&&app->audio.legendStreamStatistics().playing,"Ending music not playing");
                const auto tick=app->ending.timeline.frame;app->paused=true;app->advanceEnding(.2);app->paused=false;
                check(app->ending.timeline.frame==tick,"Paused credits moved");
            }
        }
        check(app->legendRunCompleted&&!app->endingActive&&app->frontend.stage==FrontendStage::Title,"Ending did not reach title");
        check(app->ending.timeline.totalFrame==5460,"Ending depends on render FPS");
        check(!app->audio.legendStreamStatistics().playing,"Ending stream leaked into title");
        check(app->battleProfile.words==before,"Ending re-awarded or changed profile");
        check(app->originalSession.rollbackDigest()==physics&&app->race.ticks==raceTicks,"Ending stepped race simulation");
        check(!app->endingReplay.ready()&&app->endingReplay.size()==0,"Ending retained race recording after exit");
    }
    app->endingReplay=footage;
    app->beginEnding();app->input.down[VK_ESCAPE]=true;
    check(app->renderEnding(1./60)&&app->ending.timeline.frame==1,"Held dialogue skip skipped credits");
    app->input.down[VK_ESCAPE]=false;app->renderEnding(0);
    app->input.down[VK_ESCAPE]=true;app->renderEnding(1./60);
    check(app->ending.timeline.phase==6,"Start did not skip roll");
    for(unsigned i=0;i<250;++i)app->advanceEnding(1./60);
    check(app->endingActive&&app->ending.timeline.finalCard,"Skipping roll lost final card");
    app->input.down[VK_ESCAPE]=false;app->renderEnding(0);
    app->input.down[VK_ESCAPE]=true;app->renderEnding(1./60);
    check(!app->endingActive&&app->legendRunCompleted,"Start did not skip final card");
    app->load();
    check(runLegendRunSmoke(*app,out/"legend-run")==0,"Full Legend result-to-ending flow");
    std::ofstream(out/"PASS.txt")<<checks<<" checks; real two-car Legend recording with archives disabled, bounded ring/hold/cleanup, driving backdrop, ending roll/final artwork/music, 30/60/144/240 FPS, pause, held-input guard, skip, unchanged physics/profile.\n";
    std::cout<<"PASS "<<checks<<" ending application checks\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
