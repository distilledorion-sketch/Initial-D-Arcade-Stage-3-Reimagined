// Original-solver calibration and read-only integration, in private saves.
int runHudDriftAppTests(App& app){
    std::ofstream log(app.saveRoot.parent_path()/"hud-drift-live.txt");
    app.validationMode=true;app.replayPlaybackActive=false;app.paused=false;
    unsigned checks=0,totalLit=0;
    const auto check=[&](bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);};
    for(unsigned car:{0u,15u,19u})for(bool wet:{false,true}){
        app.frontend.gameMode=original::OriginalGameMode::TimeAttack;
        app.frontend.course=app.courseIndex=3;app.frontend.car=int(car);
        app.frontend.night=app.night=false;app.frontend.wet=app.wet=wet;
        app.start();app.loadingActive=app.vsActive=app.preRaceDialogueActive=false;app.menu=false;
        unsigned lit=0,walls=0,air=0,pushes=0,clearTicks=0;float peakAngle=0,peakClear=0,topSpeed=0,steer=0;
        unsigned runs[3]{},longest[3]{};
        for(unsigned frame=0;frame<2400;++frame){
            // Follow the road with ordinary controls; no fabricated drift samples.
            DriverInput input;input.automatic=true;input.throttle=1;
            if(app.race.phase==RacePhase::Running){
                const auto projection=app.projectRacePosition(app.vehicle.position);
                const float look=std::max(6.f,app.vehicle.speed*.35f);
                const auto aim=app.sampleRaceDistance(projection.sample.distance+look).center-app.vehicle.position;
                const float angle=wrapAngle(std::atan2(aim.x,aim.z)-app.vehicle.yaw);
                const float demand=std::clamp(3.5f*std::atan2(2*app.config.wheelbase*std::sin(angle),look)/recoveredSteeringLimit,-1.f,1.f);
                steer+=std::clamp(demand-steer,-.07f,.07f);input.steer=-steer;
            }
            app.simulate(input);
            if(frame%120==0){
                const auto before=app.presentedSession().vehicle().drive.words;
                const auto hudBefore=app.hudDrift;app.advanceHudDrift();
                check(before==app.presentedSession().vehicle().drive.words,"HUD drift query changed original physics");app.hudDrift=hudBefore;
            }
            peakAngle=std::max(peakAngle,app.hudDrift.slipDegrees());topSpeed=std::max(topSpeed,app.vehicle.speedKmh());
            walls+=app.vehicle.wallContact;lit+=app.hudDrift.drifting();
            const auto& d=app.presentedSession().vehicle().drive;
            const bool push=std::abs(d.f(0x258))+std::abs(d.f(0x25C))+std::abs(d.f(0x260))+std::abs(d.f(0x264))>.01f;
            const auto& q=app.hudDriftRoadQuery;
            const bool grounded=q.f(4)>.2f&&std::abs(d.f(4)-q.f(16))<.3f;
            air+=!grounded;pushes+=push;
            clearTicks=grounded&&!push&&!app.vehicle.wallContact?clearTicks+1:0;
            const bool clear=clearTicks>15&&app.vehicle.speedKmh()>=25;
            if(clear)peakClear=std::max(peakClear,app.hudDrift.slipDegrees());
            for(unsigned t=0;t<3;++t){runs[t]=clear&&app.hudDrift.slipDegrees()>=4+2*t?runs[t]+1:0;longest[t]=std::max(longest[t],runs[t]);}
            check(!app.vehicle.wallContact||!app.hudDrift.drifting(),"Wall contact qualified as drift");
        }
        log<<"car "<<car<<" wet "<<wet<<" frames 2400 lit "<<lit<<" walls "<<walls<<" air "<<air<<" pushes "<<pushes<<" peak_degrees "<<peakAngle<<" peak_clear "<<peakClear<<" longest_4_6_8 "<<longest[0]<<' '<<longest[1]<<' '<<longest[2]<<" top_kmh "<<topSpeed<<'\n';log.flush();
        // This steady-throttle dry line never sustains eight degrees of clean
        // body slip. The same line in the wet does: retain both controls.
        check(wet?lit>0:lit==0,wet?"Wet driving never activated drift":"Gripping dry trajectory activated drift");totalLit+=lit;
        app.resetHudDrift();check(app.hudDrift.opacity()==0&&!app.hudDrift.drifting(),"Race reset retained drift lamp");
    }
    // Leave one real, qualified sliding frame available for a Unity capture.
    app.frontend.car=0;app.frontend.wet=app.wet=true;app.start();
    app.loadingActive=app.vsActive=app.preRaceDialogueActive=false;app.menu=false;
    bool found=false;float steer=0;
    for(unsigned frame=0;frame<3600&&!found;++frame){
        DriverInput input;input.automatic=true;input.throttle=1;
        if(app.race.phase==RacePhase::Running){
            const auto p=app.projectRacePosition(app.vehicle.position);const float look=std::max(6.f,app.vehicle.speed*.35f);
            const auto aim=app.sampleRaceDistance(p.sample.distance+look).center-app.vehicle.position;
            const float angle=wrapAngle(std::atan2(aim.x,aim.z)-app.vehicle.yaw);
            const float demand=std::clamp(3.5f*std::atan2(2*app.config.wheelbase*std::sin(angle),look)/recoveredSteeringLimit,-1.f,1.f);
            steer+=std::clamp(demand-steer,-.07f,.07f);input.steer=-steer;
        }
        app.simulate(input);found=app.hudDrift.drifting()&&app.hudDrift.opacity()>.99f;
    }
    check(found,"No fully lit live drift capture frame");app.paused=true;
    const auto before=app.hudDrift.opacity();app.advanceHudDrift();check(app.hudDrift.opacity()==before,"Pause advanced drift animation");
    Idas3UiBeginFrame(app.renderer.width,app.renderer.height);if(!app.render(0))throw std::runtime_error(app.renderer.error);
    log<<"PASS "<<checks<<" native App checks; "<<totalLit<<" qualified live drift frames; FR/FF/AWD dry and wet; no source-state changes.\n";
    return 0;
}
