// Runs only through the existing isolated mode-flow diagnostic entry point.
int runRearViewAppTests(App& app){
    std::ofstream log(app.saveRoot.parent_path()/"rear-view-native.txt");
    unsigned checks=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok){log<<"FAIL "<<why<<'\n';log.flush();throw std::runtime_error(why);}};
    app.validationMode=true;app.replayPlaybackActive=false;app.paused=false;
    app.loadingActive=app.preRaceDialogueActive=app.legendVisitActive=false;
    app.frontend.car=0;app.loadedProfileCar=0;
    for(unsigned mode=0;mode<4;++mode){
        app.frontend.battleProfile=original::makeOriginalFreshBattleProfile();
        auto& profile=app.frontend.battleProfile;
        profile.setu(0,mode==1?0u:mode==2?2u:1u);
        profile.setu(4,3);profile.setu(16,0);profile.setu(24,13);profile.setu(72,10000);
        profile.setu(1080+3*4,4);profile.setu(1180,1);
        app.frontend.gameMode=mode==1?original::OriginalGameMode::LegendOfTheStreets:
            mode==2?original::OriginalGameMode::BuntaChallenge:original::OriginalGameMode::TimeAttack;
        app.frontend.course=app.courseIndex=3;
        app.frontend.reverse=app.reverse=app.frontend.night=app.night=app.frontend.wet=app.wet=false;
        if(mode==3){Idas3MultiplayerConfig config{sizeof(config),1,3,0,0,0,0,1,0,0};app.startMultiplayer(config);}
        else app.start();
        app.vsActive=app.loadingActive=app.preRaceDialogueActive=false;
        app.originalRaceOwnerFrame=240;
        for(auto view:{OriginalDrivingView::Bumper,OriginalDrivingView::Chase}){
            app.drivingView=view;
            for(auto phase:{RacePhase::Countdown,RacePhase::Running}){
                app.race.phase=phase;
                const auto ticks=app.race.ticks;
                Idas3UiBeginFrame(app.renderer.width,app.renderer.height);
                check(app.render(0),"Race render failed");
                check(app.renderer.sceneCapture()->frame().viewCount==2,"Race/camera combination omitted its rear view");
                check(app.hud.lastBattlePresentation().mirrorEnabled,"Rear view is missing its HUD frame");
                bool clearLocalBody=true;
                const auto& captured=app.renderer.sceneCapture()->frame();
                for(unsigned i=0;i<captured.rangeCount;++i)if(captured.ranges[i].lightScope==2)clearLocalBody&=captured.ranges[i].viewMask==1;
                check(clearLocalBody,"Local car body obstructs the rear camera");
                check(app.race.ticks==ticks,"Rendering mirror advanced simulation");
                log<<"mode="<<mode<<" view="<<int(view)<<" phase="<<int(phase)<<" mirror=visible\n";
            }
        }
        app.paused=true;
        check(app.render(0)&&app.renderer.sceneCapture()->frame().viewCount==2,"Paused race lost mirror");
        app.paused=false;
        check(app.render(0,false)&&app.renderer.sceneCapture()->frame().viewCount==1&&!app.hud.lastBattlePresentation().mirrorEnabled,"Explicit mirror suppression lost camera/frame agreement");
        app.vsActive=true;check(!app.raceRearViewActive(),"Showcase enabled mirror");app.vsActive=false;
        app.race.phase=RacePhase::Finished;check(!app.raceRearViewActive(),"Finished race enabled mirror");
        app.race.phase=RacePhase::Running;app.replayPlaybackActive=true;
        check(!app.raceRearViewActive(),"Replay used stale live-race rear camera");app.replayPlaybackActive=false;
        app.menu=true;check(!app.raceRearViewActive(),"Menu enabled race mirror");app.menu=false;
        if(mode==3)app.leaveMultiplayer();
    }
    log<<"PASS "<<checks<<" rear-view checks across all four racing modes and three driving cameras\n";
    return 0;
}
