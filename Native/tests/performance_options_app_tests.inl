// Private presentation regression; no ordinary player profiles are loaded.
int runPerformanceOptionsAppTests(App& app){
    std::ofstream log(app.saveRoot.parent_path()/"performance-options-native.txt");
    unsigned checks=0;
    auto check=[&](bool ok,const char* why){++checks;if(!ok){log<<"FAIL "<<why<<'\n';log.flush();throw std::runtime_error(why);}};
    app.validationMode=true;
    WetWeather weather;std::array<WetWeather::Car,2> cars{};
    cars[0].visible=true;cars[0].speed=20;
    cars[0].contactsValid=true;cars[0].rearNormals={Vec3{0,1,0},Vec3{0,1,0}};
    cars[0].rearContacts={Vec3{-.72f,0,-1.25f},Vec3{.72f,0,-1.25f}};
    for(int i=0;i<24;++i)weather.advance(1./60.,true,false,cars);
    weather.build({0,2,-10},{0,1,0},true);unsigned fullSpray=0;
    for(unsigned i=0;i<weather.count;++i)if(weather.quads[i].waterTrail)++fullSpray;
    weather.build({0,2,-10},{0,1,0},true,4);unsigned reducedSpray=0;bool left=false,right=false;
    for(unsigned i=0;i<weather.count;++i)if(weather.quads[i].waterTrail){++reducedSpray;left|=weather.quads[i].center.x<0;right|=weather.quads[i].center.x>0;}
    check(reducedSpray>0&&reducedSpray<fullSpray&&left&&right,"Reduced spray keeps both wheels with fewer quads");
    for(unsigned course:{9u,10u}){
        if(app.multiplayer.active)app.leaveMultiplayer();
        app.returnToCourseSelection(true);
        Idas3MultiplayerConfig config{sizeof(config),1,course,0,1,0,0,1,0,0};
        app.startMultiplayer(config);
        app.vsActive=app.loadingActive=app.preRaceDialogueActive=false;
        app.race.phase=RacePhase::Running;
        for(unsigned i=0;i<3;++i)app.simulate({});
        const auto ticks=app.race.ticks;
        const auto position=app.vehicle.position;
        const auto profile=app.battleProfile.words;
        const auto seed=app.originalSession.contactCompletion().randomSeed0C37C778;
        unsigned fullCount=0,fullVertices=0;
        for(int detail=0;detail<2;++detail){
            app.performanceRainDetail=detail;
            check(app.render(0),"Quality render succeeds");
            const auto& frame=app.renderer.sceneCapture()->frame();
            check(frame.viewCount==2,"Mirror remains enabled at every quality level");
            if(detail==0){fullCount=app.wetWeather.count;fullVertices=frame.vertexCount;check(fullCount>0,"Full wet effects visible");}
            if(detail==1)check(app.wetWeather.count>0&&app.wetWeather.count<fullCount&&frame.vertexCount<fullVertices,"Reduced rain emits fewer vertices");
            check(app.race.ticks==ticks&&length(app.vehicle.position-position)==0&&app.battleProfile.words==profile&&
                app.originalSession.contactCompletion().randomSeed0C37C778==seed&&app.wet,"Quality does not change race ticks, car, profile, driving RNG or wet condition");
            log<<"course="<<course<<" detail="<<detail<<" views="<<frame.viewCount<<" weatherQuads="<<app.wetWeather.count<<" vertices="<<frame.vertexCount<<'\n';
        }
        app.performanceRainDetail=0;
        check(app.render(0)&&app.wetWeather.count==fullCount&&app.renderer.sceneCapture()->frame().viewCount==2,"Restoring Original restores weather and mirror");
    }
    log<<"PASS "<<checks<<" checks\n";return 0;
}
