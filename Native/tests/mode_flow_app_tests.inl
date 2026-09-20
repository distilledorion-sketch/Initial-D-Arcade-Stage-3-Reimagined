// Include from main.cpp after App's definition. Development diagnostic only;
// caller sets validationMode before App startup so no user saves are written.
// Tests actual native App::simulate/visit entry points, not a guest runtime.
int runModeFlowAppTests(App& app){
    const auto output=app.saveRoot.parent_path();
    if(app.saveRoot.empty()||!fs::exists(output/"ISOLATED_MODE_FLOW_TEST.txt"))
        throw std::runtime_error("Mode-flow diagnostic requires marked isolated userdata");
    const bool wasValidation=app.validationMode;app.validationMode=true;
    struct RestoreValidation {App& app;bool before;~RestoreValidation(){app.validationMode=before;}} restore{app,wasValidation};
    fs::create_directories(output);std::ofstream report(output/"mode-flow-app.log");
    unsigned checks=0;
    const auto require=[&](bool pass,const char* message){++checks;if(!pass){report<<"FAIL "<<message<<'\n';report.flush();throw std::runtime_error(message);}};
    app.multiplayer.active=false;app.paused=false;app.loadingActive=false;
    app.preRaceDialogueActive=app.legendVisitActive=false;
    app.frontend.car=0;app.frontend.automatic=true;
    // Whole intro owner schedule independently recovered by executing the
    // original 05B680/05B760 routines in intro-owner-20260918/check_owner.cpp.
    // Exercise actual App timing, pause and fractional render deltas here.
    const auto introTrig=original::OriginalFscaTable::load(app.root/"data/original_physics/fsca_table.bin");
    for(unsigned divisor:{1u,2u,4u}){
        app.startShowcaseCamera.reset({0.f,0.f,0.f},0.f,0,false,introTrig);
        app.vsBanner.begin({});app.vsActive=true;app.vsSeconds=0;
        app.vsPhase=1;app.vsFrame=app.vsShot=0;
        for(unsigned tick=1;tick<=241;++tick){
            if(tick==80||tick==170){
                const auto frame=app.vsFrame;app.paused=true;app.advanceStartPresentation(.25);
                require(app.vsFrame==frame&&app.vsBanner.sourceTick()==tick-1,"Paused intro clock is held");
                app.paused=false;
            }
            for(unsigned part=0;part<divisor;++part)app.advanceStartPresentation(1.0/(60.0*divisor));
            require(app.vsActive==(tick<241),"Intro completes on source counter240 without added name hold");
            require(app.vsBanner.sourceTick()==tick,"Intro animation and owner share source clock");
            if(tick<241){
                require(app.vsPhase==(tick<=120?1u:2u)&&app.vsShot==(tick<=120?0u:1u),"Intro camera switches at source counter120");
                require(app.vsFrame==(tick<=120?tick:tick-120),"Presentation ABI retains local shot frame");
            }
        }
    }
    const auto startTa=[&](unsigned course){
        app.frontend.gameMode=original::OriginalGameMode::TimeAttack;
        app.frontend.battleProfile=original::makeOriginalFreshBattleProfile();
        app.frontend.battleProfile.setu(16,0);app.frontend.battleProfile.setu(1180,1);
        app.courseIndex=int(course);app.reverse=false;app.wet=false;app.night=false;
        app.frontend.course=int(course);app.frontend.reverse=false;app.frontend.wet=false;
        app.start();
        require(app.originalHandling&&app.race.originalTiming&&!app.battle&&!app.bunta,"TA starts native solo rules");
        require(!app.pendingResultSetup&&!app.resultVisit.initialized,"TA starts without stale result owner");
        for(unsigned frame=0;frame<600&&app.race.phase==RacePhase::Countdown;++frame)app.simulate({});
        require(app.race.phase==RacePhase::Running,"TA countdown releases into running");
    };
    const auto checkQueued=[&](bool timeout){
        require(app.race.phase==RacePhase::Finished&&app.race.timeUp==timeout,"TA actual finish/timeout transition");
        require(!app.finishBannerDone,"TA transition precedes finish announcement completion");
        require(app.pendingResultSetup.has_value()&&!app.resultVisit.initialized,"TA points queued on transition frame");
        require(app.battleResults.profileMode==1&&app.battleResults.resultStatus==(timeout?2u:0u),"TA correct result status and owner mode");
        require(app.resultsReady==!timeout,"TA only completed race has timing summary");
        if(!timeout){
            require(app.battleResults.sectionCount==app.race.sectionCapacity,"Points result includes final section");
            require(app.battleResults.sectionTimes6000[app.battleResults.sectionCount-1]==app.race.elapsed6000,"Final section terminates at finish timestamp");
            std::uint32_t sum=0,previous=0;
            for(unsigned i=0;i<app.battleResults.sectionCount;++i){sum+=app.battleResults.sectionTimes6000[i]-previous;previous=app.battleResults.sectionTimes6000[i];}
            require(sum==app.race.elapsed6000,"Points section durations sum to total time");
        }else require(app.battleResults.sectionCount==unsigned(app.race.sector),"Timeout does not invent a completed section");
        const auto words=app.battleProfile.words;const auto points=app.battleResults.points;
        const auto finishTime=app.race.elapsed6000;const auto oldSpeed=app.vehicle.speed;
        const auto oldPosition=app.vehicle.position;
        DriverInput held;held.throttle=1;held.steer=1;held.shiftUp=true;
        for(unsigned frame=0;frame<120;++frame){
            held.steer=frame<60?1.f:-1.f;
            app.simulate(held);
            const auto& controls=app.originalSession.vehicle().controls;
            require(controls.steeringHighByte==(frame<60?208u:48u)&&std::abs(controls.steering)>.5f,"Finished car still receives left and right steering");
            require(controls.throttle==0.f,"Held accelerator never reaches finished solver");
            require(app.finishFadeTicks==0,"Finish road view never fades before record handoff");
            const bool record=(app.results.recordFlags&OriginalResultsState::newRecord)!=0;
            require(app.finishBannerDone==(!timeout&&frame==119&&(record||app.audio.raceMusicFinished())),"TA record handoff retains two-second timing; no-record waits for finish audio");
        }
        report<<"Post-finish held throttle: speed "<<oldSpeed<<" -> "<<app.vehicle.speed<<", travel "<<length(app.vehicle.position-oldPosition)<<", throttle "<<app.vehicle.throttle<<'\n';
        require(app.race.elapsed6000==finishTime,"Post-finish braking preserves finish timestamp");
        require(app.originalSession.raceAutomaticBrakeByte()!=0&&app.vehicle.throttle<=.001f,"Held throttle is suppressed after finish");
        require(std::abs(app.vehicle.speed)<=std::abs(oldSpeed)+.1f,"Finished car cannot accelerate");
        if(std::abs(oldSpeed)>2.f)require(length(app.vehicle.position-oldPosition)>.01f,"Finished moving car must brake rather than freeze");
        require(app.pendingResultSetup.has_value()&&!app.resultVisit.initialized,"TA pending points survives Finished early return");
        require(app.battleProfile.words==words&&app.battleResults.points==points,"TA Finished frames do not award twice");
    };
    // Controlled position fixtures traverse real course gates monotonically.
    // Only the published pose is positioned for each rules sample; no timer,
    // phase, result, progression or finish flag is forced.
    for(unsigned course:{0u,1u,3u}){
        startTa(course);
        DriverInput gas;gas.throttle=1;for(unsigned frame=0;frame<180;++frame)app.simulate(gas);
        const auto goal=app.originalRace.rules().goalIndex;
        for(std::int32_t relative=0;relative<=goal+8&&app.race.phase==RacePhase::Running;relative+=2){
            const auto gate=app.originalRace.gate(relative);
            app.vehicle.position={gate.center[0],gate.center[1],gate.center[2]};
            app.simulate(gas);
        }
        checkQueued(false);
        require(!app.timeAttackSnapshot.drivingPath.empty(),"Finished race retains recorded driving path");
        require(!original::originalTimeAttackDrivingLines(course,0,app.timeAttackSnapshot).empty(),"Finished path converts to authored full map");
        report<<"PASS TA gate fixture course="<<course<<" elapsed6000="<<app.race.elapsed6000<<" points="<<app.timeAttackPoints.total<<" pending before announcement\n";
    }
    startTa(3);
    for(unsigned frame=0;frame<20000&&app.race.phase!=RacePhase::Finished;++frame)app.simulate({});
    checkQueued(true);
    report<<"PASS TA natural idle timeout elapsed6000="<<app.race.elapsed6000<<" pending points="<<app.timeAttackPoints.total<<'\n';

    app.pendingResultSetup.reset();app.resultVisit={};app.input={};
    app.frontend.gameMode=original::OriginalGameMode::BuntaChallenge;
    app.frontend.stage=FrontendStage::Course;app.menu=true;
    const auto seedBunta=[&](unsigned course,unsigned level){
        auto p=original::makeOriginalFreshBattleProfile();p.setu(0,2);p.setu(16,0);p.setu(72,10000);p.setu(1180,1);
        p.setu(4,course);p.setu(1080+course*4,level);p.setu(8,1);
        app.frontend.battleProfile=p;app.battleProfile=p;
        app.buntaVisitActive=app.timeAttackVisitActive=false;
    };
    seedBunta(3,5);app.battle=false;app.bunta=false; // outgoing TA must not suppress the selected Bunta mode
    require(app.beginBuntaVisit(true),"Bunta pre-race entered from selected frontend mode");
    require(app.buntaVisitActive&&app.extraModeVisitActive(),"Bunta active scene gating");
    require(app.buntaVisit.dialogueState().buntaChallenge&&app.buntaVisit.dialogueState().character==31&&app.buntaVisit.dialogueState().kind==5,"Bunta pre-race original script and character");
    require(!app.raceMusicOpponentEligible(),"Bunta dialogue excludes song selection input");
    const auto beforeMenu=std::array<int,4>{int(app.frontend.gameMode),int(app.frontend.stage),app.frontend.course,app.frontend.car};
    app.input.pressed[VK_F5]=app.input.pressed[VK_RETURN]=app.input.pressed[VK_ESCAPE]=true;
    const auto beforeTicks=app.race.ticks;app.commands(1./60.);
    require(beforeMenu==std::array<int,4>{int(app.frontend.gameMode),int(app.frontend.stage),app.frontend.course,app.frontend.car}&&beforeTicks==app.race.ticks,"Bunta owner consumes input before hidden menu/restart commands");
    app.input={};
    for(unsigned frame=0;frame<80&&!app.buntaVisit.finished();++frame)app.buntaVisit.advance(app.battleProfile,{true});
    require(app.buntaVisit.finished(),"Bunta pre-race skip reaches original closed state");

    for(unsigned course:{3u,8u}){
        seedBunta(course,16);if(course==8){app.frontend.battleProfile.setu(1092,15);app.battleProfile.setu(1092,15);}
        app.battle=true;app.bunta=true;app.battleResults.resultStatus=0;
        require(app.beginBuntaVisit(false),"Bunta win post-race owner starts");
        require(app.battleProfile.u(1080+course*4)==15&&app.frontend.battleProfile.u(1080+course*4)==15,"Bunta level16 sentinel normalized and propagated to frontend");
        require(app.buntaVisit.dialogueState().kind==31,"Bunta max-level win selects authored kind31");
    }
    for(unsigned status:{1u,2u}){
        seedBunta(3,5);app.battleResults.resultStatus=status;
        require(app.beginBuntaVisit(false),"Bunta loss/timeup post-race owner starts");
        require(app.buntaVisit.dialogueState().kind==37&&app.battleProfile.u(1092)==5,"Bunta loss/timeup uses loss script without changing level");
    }
    app.buntaVisitActive=false;app.multiplayer.active=true;
    require(!app.beginBuntaVisit(true)&&!app.beginBuntaVisit(false)&&!app.buntaVisitActive,"Network races cannot enter local Bunta scenes");
    app.multiplayer.active=false;app.frontend.gameMode=original::OriginalGameMode::TimeAttack;
    require(!app.beginBuntaVisit(true)&&!app.beginBuntaVisit(false),"TA cannot enter Bunta scenes");
    app.battle=false;app.bunta=false;app.race.phase=RacePhase::Finished;app.race.timeUp=false;
    app.results.recordFlags=0;app.timeAttackCourseRankingQualified=false;
    app.audio.beginResultMusic();
    require(app.beginTimeAttackVisit(false),"TA continuation owner starts");
    app.updateAudioScene();
    require(app.audio.raceTimingStatistics().scene==0,"TA continuation retains result music instead of replaying WIN.bin");
    app.timeAttackVisitActive=false;
    report<<"PASS "<<checks<<" native App mode-flow checks. Native CPU diagnostic; controlled-position finish fixtures and natural timeout. No user save writes, no physical device test, no Unity rendering claim.\n";
    return 0;
}
