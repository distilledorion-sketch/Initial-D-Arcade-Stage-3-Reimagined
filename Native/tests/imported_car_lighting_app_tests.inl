// Private App regression. Sweep lighting coordinates, not a driving benchmark.
int runImportedCarLightingAppTests(App& app){
    std::ofstream log(app.saveRoot.parent_path()/"imported-car-lighting.txt");
    unsigned checks=0,scenarios=0,samples=0;
    auto check=[&](bool ok,const char* why){++checks;if(!ok){log<<"FAIL "<<why<<'\n';log.flush();throw std::runtime_error(why);}};
    app.validationMode=true;
    for(unsigned id:{9u,10u})for(unsigned condition=0;condition<8;++condition)for(bool online:{false,true}){
        if(app.multiplayer.active)app.leaveMultiplayer();
        app.returnToCourseSelection(true);
        if(online){
            Idas3MultiplayerConfig config{sizeof(config),1,id,condition&1,(condition>>1)&1,(condition>>2)&1,0,1,condition&1,0};
            app.startMultiplayer(config);
        }else{
            app.frontend.gameMode=original::OriginalGameMode::TimeAttack;
            app.frontend.course=int(id);app.frontend.car=0;
            app.reverse=app.frontend.reverse=bool(condition&1);
            app.wet=app.frontend.wet=bool(condition&2);app.night=app.frontend.night=bool(condition&4);
            app.start();
        }
        check(bool(app.importedCourse),"Imported App race started");
        check(!app.carLightGain.hasTable(),"Imported route must not borrow Akina shadow stream");
        check(app.rivalVisible==online,"Real online startup publishes rival lighting");
        check(app.raceLightSets.has_value()&&app.raceLightSets->hasRival==online,"Car lighting scopes retained");
        check(app.playerProjectedHeadlight.enabled()==app.night,"Player night headlight retained");
        if(online)check(app.rivalProjectedHeadlight.enabled()==app.night,"Opponent night headlight retained");
        const auto period=app.originalPath.period();
        const auto donor=original::OriginalCarLightGain::load(app.root,3,false,false,app.reverse);
        if(id==9){
            check(period>int(donor.values().size()-1),"Hakone exceeds donor shadow stream");
            bool reproduced=false;
            try{donor.evaluate(int(donor.values().size()-1),0.f);}catch(const std::invalid_argument& e){reproduced=std::string(e.what())=="Original car light coordinate outside source stream";}
            check(reproduced,"Old donor lookup reproduces reported exception");
        }
        const auto ambient=app.playerCarLight.ambient,rivalAmbient=app.rivalCarLight.ambient;
        // Exercise both setters throughout the complete imported path, including
        // beyond finish and the exact donor boundary. Opponent moves in reverse
        // coordinate order so it reaches the previous crash boundary first.
        for(int index=0;index<period;++index)for(float fraction:{0.f,.5f,1.f}){
            app.playerLightCoordinate={index,fraction};app.rivalLightCoordinate={period-1-index,fraction};
            app.playerCarLight.gain=.37f;app.rivalCarLight.gain=.61f;
            app.advanceCarLightGain();++samples;
            check(app.playerCarLight.gain==(app.night?.37f:1.f),"Player gain safe across entire imported road");
            check(app.rivalCarLight.gain==((app.night||!online)?.61f:1.f),"Opponent gain safe across entire imported road");
            check(app.playerCarLight.ambient==ambient&&app.rivalCarLight.ambient==rivalAmbient,"Shadow update preserves ambient");
        }
        // Actual path projection at 70..76%, spanning the reported uphill area.
        for(unsigned percent=70;percent<=76;++percent){
            const auto& course=*app.importedCourse;
            const int progress=course.rules(app.reverse).goalIndex*int(percent)/100;
            const int index=course.rules(app.reverse).startIndex+progress;
            const int source=app.reverse?period-index-1:index;
            original::OriginalRacePoint midpoint{};
            for(unsigned axis=0;axis<3;++axis)midpoint[axis]=(course.center[source][axis]+course.center[source+1][axis])*.5f;
            app.playerLightCoordinate={index,0.f};
            check(app.originalPath.project(midpoint,app.playerLightCoordinate),"Reported area projects to imported path");
            app.rivalLightCoordinate=app.playerLightCoordinate;
            app.advanceCarLightGain();
        }
        log<<"PASS course="<<id<<" condition="<<condition<<" online="<<online<<" pathCells="<<period<<" donorCells="<<donor.values().size()-1<<'\n';log.flush();++scenarios;
    }
    app.leaveMultiplayer();app.returnToCourseSelection(true);
    app.frontend.gameMode=original::OriginalGameMode::TimeAttack;app.frontend.course=app.courseIndex=3;
    app.reverse=app.frontend.reverse=false;app.wet=app.frontend.wet=false;app.night=app.frontend.night=false;
    app.start();
    check(!app.importedCourse&&app.carLightGain.hasTable(),"Returning to original Akina reloads its shadow stream");
    app.playerLightCoordinate={123,.37f};app.advanceCarLightGain();
    check(app.playerCarLight.gain==app.carLightGain.evaluate(123,.37f),"Original Akina still uses authored shading");
    log<<"PASS "<<checks<<" checks; "<<scenarios<<" App race setups; "<<samples<<" complete-route light updates. Both tracks, directions, weather, day/night, solo/online.\n";
    return 0;
}
