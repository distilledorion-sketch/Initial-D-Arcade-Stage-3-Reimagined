// Exercise production authority headlight messages, light composition and HUD
// rendering. No network peer or user save is required by this private fixture.
int runNightBattlePresentationAppTests(App& app){
    std::ofstream log(app.saveRoot.parent_path()/"night-battle-presentation.txt");unsigned checks=0;
    const auto check=[&](bool ok,const char* message){++checks;if(!ok){log<<"FAIL "<<message<<'\n';log.flush();throw std::runtime_error(message);}};
    app.validationMode=true;app.replayRecordingFlags=0;
    for(unsigned local:{0u,1u})for(unsigned night:{0u,1u}){
        Idas3MultiplayerConfig config{sizeof(config),2,3,0,0,night,local?8u:0u,local?0u:8u,local,0};
        app.startMultiplayer(config);app.enableAuthority(880001+local*2+night,local!=0,false,true);
        app.setMultiplayerGo(true);app.loadingActive=app.preRaceDialogueActive=app.vsActive=false;
        app.originalRaceOwnerFrame=240;app.multiplayer.received=true;
        const auto digest=app.authorityRace->digest();
        std::array<float,3> litAmbient{};
        for(unsigned stage=0;stage<3;++stage){
            const bool enabled=stage!=1;const std::uint64_t sequence=(stage+1)*2;
            app.setAuthorityRemoteHeadlights(sequence,unsigned(enabled));
            app.setAuthorityRemoteHeadlights(sequence-1,unsigned(!enabled));
            check(app.rivalProjectedHeadlight.enabled()==enabled,"Stale message changed opponent headlights");
            // This is the same source callback as the next authority frame.
            app.publishProjectedHeadlights(2.f,app.originalRace.state().progress.index);
            check(bool(app.multiplayer.remote.flags&Idas3MpHeadlights)==enabled,"Headlight state was not published to the HUD");
            if(stage==0)litAmbient=app.rivalCarLight.ambient;
            if(night&&stage==1){
                check(std::all_of(app.rivalCarLight.ambient.begin(),app.rivalCarLight.ambient.end(),[](float a){return a==0;}),"Lights-off rival did not darken at night");
                check(std::any_of(litAmbient.begin(),litAmbient.end(),[](float a){return a>0;}),"Lights-on rival had no ambient to restore");
            }else check(app.rivalCarLight.ambient==litAmbient,"Day lighting or restored headlights changed rival ambient");
            for(bool hide:{true,false}){
                app.hud.setLightsOffAdvantage(hide);check(app.render(0),"Could not render battle presentation");
                const auto& hud=app.hud.lastBattlePresentation();
                check(hud.online&&hud.advantageHidden==bool(night&&hide&&!enabled),"Actual authority headlight state did not reach the online HUD");
                check(app.renderer.rivalLighting&&app.renderer.rivalLighting->ambient==app.rivalCarLight.ambient,"Darkened opponent lighting did not reach rendering");
                check(app.authorityRace->digest()==digest,"Presentation toggle changed physics state");
            }
        }
        app.leaveMultiplayer();app.returnToCourseSelection(true);
    }
    app.hud.setLightsOffAdvantage(true);
    log<<"PASS "<<checks<<" checks: host/guest, day/night, absolute and stale headlight messages, darken/restore, HUD options, unchanged physics.\n";
    return 0;
}
