// Private visual fixtures. App's real source finish/timeout transitions have a
// separate regression in mode_flow_app_tests.inl; these select authored scenes
// quickly for Unity rendering and controller-route verification.
void prepareModeFlowFixture(App& app,unsigned scene){
    if(scene>=240&&scene<=242){
        if(app.multiplayer.active)app.leaveMultiplayer();
        app.validationMode=true;app.replayPlaybackActive=false;app.paused=false;
        app.frontend.gameMode=original::OriginalGameMode::TimeAttack;
        app.frontend.course=app.courseIndex=scene==242?11:3;app.frontend.reverse=app.reverse=false;
        app.frontend.night=app.night=true;app.frontend.wet=app.wet=false;
        app.frontend.car=scene==241?19:0;app.start();
        app.loadingActive=app.vsActive=app.preRaceDialogueActive=false;app.menu=false;
        app.race.phase=RacePhase::Running;
        app.race.remaining6000=120*6000;app.drivingView=OriginalDrivingView::Chase;
        app.input={};return;
    }
    if(scene==207||scene==208){
        if(app.multiplayer.active)app.leaveMultiplayer();
        app.validationMode=true;app.menu=true;app.frontend.gameMode=original::OriginalGameMode::TimeAttack;
        app.frontend.course=scene==208?11:10;app.frontend.stage=FrontendStage::Course;app.frontend.reverse=false;
        app.frontend.advance(120);app.input={};return;
    }
    if(scene>=230&&scene<=237){
        if(app.multiplayer.active)app.leaveMultiplayer();
        const unsigned variant=(scene-230)/2;
        app.validationMode=true;app.frontend.gameMode=original::OriginalGameMode::TimeAttack;
        app.frontend.course=app.courseIndex=10;app.frontend.reverse=app.reverse=(scene&1)!=0;
        app.frontend.night=app.night=variant>=2;app.frontend.wet=app.wet=(variant&1)!=0;
        app.frontend.car=0;app.start();app.loadingActive=app.vsActive=app.preRaceDialogueActive=false;
        app.menu=false;app.paused=true;app.replayPlaybackActive=true;app.drivingView=OriginalDrivingView::Chase;
        const unsigned index=app.reverse?unsigned(app.course.points.size()-1)-3420:175;
        const auto sample=app.course.sample(app.course.cumulative[index]);
        app.vehicle.position=sample.center;app.vehicle.yaw=std::atan2(sample.tangent.x,sample.tangent.z);app.vehicle.speed=0;
        app.previous=app.vehicle;app.progress=sample.distance;app.cameraReady=false;
        app.race.phase=RacePhase::Running;app.race.elapsed6000=0;app.race.remaining6000=120*6000;
        app.input={};return;
    }
    if(scene>=210&&scene<=225){
        if(app.multiplayer.active)app.leaveMultiplayer();
        const unsigned variant=(scene-210)/4,shot=(scene-210)%4;
        app.validationMode=true;app.frontend.gameMode=original::OriginalGameMode::TimeAttack;
        app.frontend.course=app.courseIndex=7;app.frontend.reverse=app.reverse=(variant&1)!=0;
        app.frontend.night=app.night=(variant&2)!=0;app.frontend.wet=app.wet=true;
        app.frontend.car=0;app.start();app.loadingActive=app.vsActive=app.preRaceDialogueActive=false;
        if(fs::is_regular_file(app.saveRoot.parent_path()/"UNSHELTERED_BASELINE.txt"))app.weatherShelter.clear();
        app.menu=false;app.paused=true;app.replayPlaybackActive=true;app.drivingView=OriginalDrivingView::Bumper;
        const unsigned forwardIndices[]{2380,2416,2488,2560};
        const unsigned index=app.reverse?unsigned(app.course.points.size()-1)-forwardIndices[shot]:forwardIndices[shot];
        const auto sample=app.course.sample(app.course.cumulative[index]);
        app.vehicle.position=sample.center;app.vehicle.yaw=std::atan2(sample.tangent.x,sample.tangent.z);app.vehicle.speed=30;
        app.previous=app.vehicle;app.progress=sample.distance;app.cameraReady=false;
        app.courseLightPathIndex=int(index);app.race.phase=RacePhase::Running;app.race.elapsed6000=70*6000;app.race.remaining6000=120*6000;
        std::array<WetWeather::Car,2> cars{{{app.vehicle.position,app.vehicle.yaw,30,true},{}}};
        app.wetWeather.reset();app.wetWeather.advance(.2,true,false,cars);
        app.input={};return;
    }

    if(scene==206){
        if(app.multiplayer.active)app.leaveMultiplayer();
        app.validationMode=true;app.paused=false;app.frontend.gameMode=original::OriginalGameMode::TimeAttack;
        app.frontend.car=0;app.frontend.battleProfile=original::makeOriginalFreshBattleProfile();
        std::ofstream log(app.saveRoot.parent_path()/"course-start-cells.txt");
        for(unsigned condition=0;condition<18;++condition){
            app.frontend.course=app.courseIndex=int(condition/2);app.frontend.reverse=app.reverse=(condition%2)!=0;
            app.frontend.wet=app.wet=false;app.frontend.night=app.night=false;app.start();
            if(app.courseLightPathIndex!=app.originalCoordinate.index)throw std::runtime_error("Intro course cell did not initialize from grid");
            const auto cell=app.courseLightPathIndex;app.simulate({});
            if(app.courseLightPathIndex!=cell)throw std::runtime_error("First driving tick changes the initial scenery cell");
            log<<condition<<','<<cell<<'\n';
        }
        log<<"PASS 18 original course/direction starts retain their initial scenery cell across the first driving tick.\n";
        return;
    }
    if(scene>=200&&scene<=205){
        if(app.multiplayer.active)app.leaveMultiplayer();
        app.validationMode=false;app.paused=false;
        app.loadingActive=app.preRaceDialogueActive=app.legendVisitActive=app.timeAttackVisitActive=app.buntaVisitActive=false;
        app.frontend.gameMode=original::OriginalGameMode::TimeAttack;
        app.frontend.car=0;app.frontend.course=app.courseIndex=2;
        app.frontend.reverse=app.reverse=true;app.frontend.wet=app.wet=false;app.frontend.night=app.night=false;
        app.frontend.battleProfile=original::makeOriginalFreshBattleProfile();
        if(scene>=202){
            const Idas3MultiplayerConfig config{sizeof(config),1,2,1,0,0,0,23,(scene-202)/2,1};
            app.startMultiplayer(config);app.multiplayer.localName="CAMERA TEST";app.multiplayer.remoteName="OPPONENT";
            app.beginVsBanner();
        }else app.start();
        if(app.courseLightPathIndex!=app.originalCoordinate.index)throw std::runtime_error("Akagi intro loaded the wrong scenery cell");
        std::ofstream log(app.saveRoot.parent_path()/("camera-"+std::to_string(scene)+".csv"));
        log<<"tick,shot,x,y,z,road_y,clearance\n";
        for(unsigned tick=0;tick<(scene%2?180u:60u);++tick){
            app.advanceStartPresentation(1.0/60.0);
            const auto& frame=app.startShowcaseCamera.frame();auto query=app.playerBody.query();
            for(unsigned axis=0;axis<3;++axis)query.setf(32+axis*4,frame.eye[axis]);
            original::OriginalTriangleSearchTrace trace;original::OriginalSurfaceScratch surface;
            const bool hit=original::queryOriginalCollisionSurface(app.presentedSession().collision(),query,trace,surface);
            log<<tick<<','<<app.vsShot<<','<<frame.eye[0]<<','<<frame.eye[1]<<','<<frame.eye[2]<<','<<(hit?query.f(16):0)<<','<<(hit?frame.eye[1]-query.f(16):999)<<'\n';
        }
        if(!app.vsActive)throw std::runtime_error("Akagi camera fixture lost showcase");
        app.paused=true;app.input={};return;
    }
    if(scene==146){
        const auto check=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
        app.validationMode=true;app.returnToCourseSelection(true);
        app.frontend.stage=FrontendStage::Title;
        check(app.canFullTune(),"Title Full Tune unavailable");
        app.resultVisit.initialized=true;app.race.phase=RacePhase::Finished;
        check(app.canFullTune(),"Completed result blocked frontend Full Tune");
        app.fullTuneSelecting=true;
        check(app.canFullTune(),"Interrupted car selection blocked frontend Full Tune");
        app.beginFullTune();
        check(app.fullTuneSelecting&&app.frontend.stage==FrontendStage::SaveSelect&&!app.resultVisit.initialized,"Full Tune failed to restart save selection");
        app.fullTuneActive=true;check(!app.canFullTune(),"Active upgrades can be interrupted");app.fullTuneActive=false;
        app.multiplayer.active=true;check(!app.canFullTune(),"Online Full Tune permitted");app.multiplayer.active=false;
        app.menu=false;app.fullTuneSelecting=false;app.resultVisit.initialized=true;
        check(!app.canFullTune(),"Active result upgrade owner can be interrupted");
        app.returnToCourseSelection(true);
        return;
    }
    if(scene>=140&&scene<=145){
        const auto check=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
        if(scene==140){
            app.validationMode=false;app.multiplayer.active=false;app.returnToCourseSelection(true);
            app.useSaveSlot(0);app.frontend.car=0;app.loadedProfileCar=0;app.frontend.stage=FrontendStage::Mode;
            app.battleProfile=original::makeOriginalFreshBattleProfile();app.battleProfile.setu(1180,1);
            app.frontend.battleProfile=app.battleProfile;app.saveResultProfile();app.driverSetup.markComplete(0);app.saveSlots.adopt(0,app.battleProfile);
            check(app.canFullTune(),"Selected saved driver cannot full tune");
            app.multiplayer.active=true;check(!app.canFullTune(),"Online full tune allowed");app.multiplayer.active=false;
            app.frontend.stage=FrontendStage::Title;check(app.canFullTune(),"Full Tune must be available from attract options");
        }else if(scene==141){
            check(app.fullTuneActive&&app.battleProfile.u(72)==999999,"Gameplay action failed to grant 999999");
            check(!app.canFullTune(),"Reentrant full tune allowed");
            const auto before=app.battleProfile;const auto tick=app.race.ticks;
            for(int i=0;i<60;++i)app.simulate({});
            check(app.battleProfile.words==before.words&&app.race.ticks==tick,"Full tune ran racing physics");
            unsigned visits=0;
            while(app.fullTuneActive&&app.resultVisit.tuning.kind!=original::OriginalTuningChildKind::optionalPart&&visits++<128){
                unsigned frames=0;while(!app.resultAnimationFrame.finished&&frames++<10000)app.advanceResultVisit();
                check(frames<10000,"Mandatory upgrade stuck");check(app.render(0),"Mandatory render failed");
            }
            check(app.fullTuneActive&&(app.battleProfile.u(1180)&0xc00)==0xc00,"Mandatory upgrades did not reach optional choices");
            check(app.battleProfile.u(72)==999999,"Mandatory upgrades spent points");
            check(app.profiles.load(0).profile.words==app.battleProfile.words,"Mandatory tuning not saved");
        }else if(scene==142){
            check(app.fullTuneActive&&app.battleProfile.u(72)<999999,"Controller confirm failed to purchase optional part");
            check(app.profiles.load(0).profile.u(72)==app.battleProfile.u(72),"Optional cost not saved");
        }else if(scene==143){
            unsigned visits=0;
            while(app.fullTuneActive&&visits++<128){
                unsigned frames=0;while(!app.resultAnimationFrame.finished&&frames++<10000){
                    app.resultSelectionAxis=1;app.resultConfirmPending=frames==20;app.advanceResultVisit();
                }
                check(frames<10000,"Optional upgrade stuck");check(app.render(0),"Optional render failed");
            }
            check(!app.fullTuneActive&&app.menu&&app.frontend.stage==FrontendStage::Mode,"Full tune failed to return to Choose a Mode");
            check(app.profiles.load(0).profile.words==app.battleProfile.words,"Final profile did not persist");
            check(!fs::exists(app.userdataRoot()/"last_run.csv")&&!fs::exists(app.userdataRoot()/"time_attack_records_v1.csv"),"Full tune recorded a fictitious race");
        }else if(scene==144){
            app.useSaveSlot(2);
            app.frontend.car=29;app.loadedProfileCar=29;app.battleProfile=original::makeOriginalFreshBattleProfile();
            app.battleProfile.setu(16,29);app.battleProfile.setu(1180,0xc01);app.battleProfile.setByte(164,app.tuningTables->car(29).performance.back().words[0]);
            app.frontend.battleProfile=app.battleProfile;app.fullTuneSelecting=true;app.beginSelectedFullTune();
            check(!app.fullTuneActive&&app.menu&&app.frontend.stage==FrontendStage::Mode,"Fully tuned car without optional parts failed to exit");
            check(app.profiles.load(29).profile.u(72)==999999,"No-upgrade grant not saved");
        }else{
            check(app.fullTuneActive&&app.activeSaveSlot==0&&app.frontend.car!=0,"Different new car did not reach tuning in the selected save");
            check(app.battleProfile.u(64)==1&&app.battleProfile.u(76)==5,"Normal color/name choices were not preserved");
            check(app.driverSetup.load(unsigned(app.frontend.car)).status==LocalDriverSetup::Status::Complete&&app.saveSlots.at(0).car==unsigned(app.frontend.car),"New driver setup was not saved");
            check(app.battleProfile.u(72)==999999,"New driver missed point grant");
            unsigned visits=0;
            while(app.fullTuneActive&&visits++<256){
                unsigned frames=0;while(!app.resultAnimationFrame.finished&&frames++<10000){app.resultSelectionAxis=1;app.resultConfirmPending=frames==20;app.advanceResultVisit();}
                check(frames<10000,"New car upgrade stuck");check(app.render(0),"New car upgrade render failed");
            }
            check(app.menu&&!app.fullTuneActive&&app.frontend.stage==FrontendStage::Mode&&app.activeSaveSlot==0,"New driver did not retain save at Choose a Mode");
            const auto tuned=app.profiles.load(unsigned(app.frontend.car)).profile;
            check((tuned.u(1180)&0xc00)==0xc00&&tuned.u(64)==1&&tuned.u(76)==5,"New driver upgrades/color/name not persisted");
            const auto other=LocalDriverProfiles(app.saveSlots.profileDirectory(0)).load(0).profile;
            check(other.u(64)==0&&other.u(76)==0,"New driver changed the other save's paint or name");
        }
        return;
    }
    if(scene>=100&&scene<135){
        prepareModeFlowFixture(app,6);
        app.battleProfile.setu(16,scene-100);app.frontend.battleProfile=app.battleProfile;
        app.timeAttackRankingPreview=std::make_unique<original::OriginalTuningPreviewPresentation>();
        app.timeAttackRankingPreview->load(app.root,app.battleProfile,false,true);
        app.timeAttackRankingTexturesLoaded=false;return;
    }
    if(scene>=66&&scene<=69){
        app.validationMode=true;app.multiplayer.active=false;
        app.paused=app.loadingActive=app.preRaceDialogueActive=app.legendVisitActive=false;
        app.timeAttackVisitActive=app.buntaVisitActive=false;
        app.frontend.gameMode=original::OriginalGameMode::TimeAttack;
        app.frontend.car=0;app.frontend.automatic=true;
        app.frontend.battleProfile=original::makeOriginalFreshBattleProfile();
        app.frontend.battleProfile.setu(1180,1);
        app.courseIndex=app.frontend.course=3;
        app.reverse=app.wet=app.night=false;
        app.frontend.reverse=app.frontend.wet=app.frontend.night=false;
        app.start();
        for(unsigned i=0;i<600&&app.race.phase==RacePhase::Countdown;++i)app.simulate({});
        DriverInput gas;gas.throttle=1;
        for(unsigned i=0;i<180;++i)app.simulate(gas);
        // Seed pre-race comparison clocks only. The production finish gate
        // must derive the announcement flags itself; never force the flags.
        app.results.bestTimes6000={scene==66?UINT32_MAX:100u,scene<=67?UINT32_MAX:100u,scene<=68?UINT32_MAX:100u};
        for(int i=0;i<=app.originalRace.rules().goalIndex+8&&app.race.phase==RacePhase::Running;i+=2){
            const auto gate=app.originalRace.gate(i);
            app.vehicle.position={gate.center[0],gate.center[1],gate.center[2]};app.simulate(gas);
        }
        if(app.race.phase!=RacePhase::Finished||!app.pendingResultSetup)throw std::runtime_error("Finish fixture did not cross goal");
        constexpr unsigned expected[]{0x78000000u,0x68000000u,0x48000000u,0u};
        if(app.results.recordFlags!=expected[scene-66])throw std::runtime_error("Finish fixture record classification failed");
        app.validationMode=false;app.menu=app.paused=app.vsActive=false;app.input={};return;
    }
    if(scene>=21&&scene<=65){
        const unsigned course=(scene-21)%9;
        constexpr unsigned captureTicks[]{90,15,40,65,1};
        const auto captureTick=captureTicks[(scene-21)/9];
        constexpr unsigned first[]{0,3,6,9,18,14,21,24,17},count[]{3,3,3,5,3,3,3,6,1};
        auto profile=original::makeOriginalFreshBattleProfile();
        original::selectOriginalRival(profile,first[course]+count[course]-1);
        for(unsigned i=first[course];i<first[course]+count[course];++i)profile.setByte(116+i,0x10);
        app.battleProfile=profile;
        if(!app.legendVisitLoaded){app.legendVisit.load(app.root);app.legendVisitLoaded=true;}
        app.legendVisit.begin(app.battleProfile,{});
        if(!app.legendVisit.courseCleared())throw std::runtime_error("Fixture did not reach first course victory");
        app.paused=false;app.legendSkipHeld=true;
        for(unsigned i=0;i<2000&&!app.legendVisit.courseClearRunning();++i)app.advanceLegendVisit(1.0/60.0);
        app.legendSkipHeld=false;
        const auto stream=app.audio.legendStreamStatistics();
        if(stream.stream!=16||!stream.playing||stream.volume!=127)throw std::runtime_error("Conquered audio commands did not reach the mixer");
        if(!app.legendVisit.courseClearRunning())throw std::runtime_error("Conquered phase did not start");
        const auto beforeCues=app.audio.oneShotStatistics().cues;
        for(unsigned i=0;i<captureTick;++i)app.advanceLegendVisit(1.0/60.0);
        const auto expectedCues=captureTick>30?2u:1u;
        if(app.audio.oneShotStatistics().cues!=beforeCues+expectedCues)throw std::runtime_error("Conquered arrival sound did not reach the audio owner");
        app.menu=app.loadingActive=app.preRaceDialogueActive=app.timeAttackVisitActive=app.buntaVisitActive=app.vsActive=false;
        app.legendVisitActive=true;app.paused=true;app.input={};return;
    }

    if(scene>=15&&scene<=16){
        prepareModeFlowFixture(app,4);
        const unsigned course=scene==15?0:3,direction=scene==15?1:0;
        original::OriginalRaceRules rules;rules.reset(app.root,{course*2+direction,2,0},{},{});
        original::OriginalTimeAttackTelemetry path;
        for(int i=0;i<=rules.rules().goalIndex;i+=2){
            const auto gate=rules.gate(i);const auto index=path.recordProgress(course,direction,unsigned(i),unsigned(i)*100);
            const unsigned state=(unsigned(i)/80)%3;
            path.recordDrivingPoint(gate.center,index,state==0?1.f:0.f,state==1?1.f:0.f);
        }
        auto setup=app.timeAttackVisitSetup();setup.condition=course*2+direction;
        setup.telemetry.drivingPath=path.snapshot(course,{}, {}, {}).drivingPath;setup.telemetry.valid=true;
        app.timeAttackVisit.beginLecture(setup);app.timeAttackVisitActive=true;app.input={};return;
    }
    if(scene>=17&&scene<=20){
        const unsigned enemy=scene==17?1:scene==18?2:scene==19?29:30;
        app.loadingActive=app.preRaceDialogueActive=app.legendVisitActive=app.timeAttackVisitActive=app.buntaVisitActive=false;
        app.frontend.gameMode=original::OriginalGameMode::LegendOfTheStreets;
        app.frontend.car=0;app.frontend.battleProfile=original::makeOriginalFreshBattleProfile();
        app.frontend.battleProfile.setu(1180,0x0800c081);original::selectOriginalRival(app.frontend.battleProfile,enemy);
        app.frontend.course=original::originalRival(enemy).course;app.frontend.stage=FrontendStage::Rival;
        app.battleProfile=app.frontend.battleProfile;app.validationMode=false;app.start();
        // Advance the actual intro owner: advancing only the text produced
        // settled names over the initial rear camera instead of the matching shot.
        for(unsigned i=0;i<160;++i)app.advanceStartPresentation(1.0/60.0);
        if(!app.vsActive||app.vsShot!=1||!app.vsBanner.animationSettled())throw std::runtime_error("Intro fixture did not reach the settled front shot");
        app.paused=true;app.input={};return;
    }
    if(scene>14)throw std::invalid_argument("Unknown mode-flow fixture");
    app.validationMode=false;app.audio.endResultMusic();
    app.loadingActive=app.preRaceDialogueActive=app.legendVisitActive=app.vsActive=false;
    app.buntaVisitActive=app.timeAttackVisitActive=app.paused=false;
    app.resultVisit={};app.resultAnimationFrame={};app.pendingResultSetup.reset();app.timeAttackCourseRankingQualified=false;
    app.frontend.car=0;app.frontend.make=6;app.frontend.course=3;
    app.frontend.reverse=app.frontend.wet=app.frontend.night=false;
    app.courseIndex=3;app.reverse=app.wet=app.night=false;
    app.frontend.automatic=app.automatic=true;
    auto profile=original::makeOriginalFreshBattleProfile();
    profile.setu(0,scene<4?2u:1u);profile.setu(16,0);profile.setu(72,10000);profile.setu(1180,profile.u(1180)|1u);
    profile.setu(1080+3*4,4);profile.setu(76,5);
    for(unsigned i=0;i<5;++i)profile.setu(44+4*i,30+i);
    app.loadedProfileCar=0;app.frontend.battleProfile=profile;
    app.menu=true;app.frontend.stage=FrontendStage::Course;
    app.frontend.gameMode=scene<4?original::OriginalGameMode::BuntaChallenge:original::OriginalGameMode::TimeAttack;
    if(scene>=10&&scene<=13){
        app.battle=app.bunta=false;app.frontend.prepareRankingAttractFixture(scene-10);
        app.input={};app.updateAudioScene();return;
    }
    if(scene<4){
        original::selectOriginalBuntaCourse(app.frontend.battleProfile,3);
        app.battleProfile=app.frontend.battleProfile;app.battle=app.bunta=true;
        app.load();app.frontend.advance(120);
        if(scene==1)app.beginBuntaVisit(true);
        else if(scene>1){
            app.menu=false;app.race.phase=RacePhase::Finished;
            app.battleResults.resultStatus=scene==2?0u:1u;app.beginBuntaVisit(false);
        }
    }else{
        app.start();app.vsActive=false;app.race.phase=RacePhase::Finished;
        app.race.timeUp=false;app.race.elapsed6000=540000;app.finishBannerDone=true;
        app.results.condition=6;app.results.carId=0;app.results.recordFlags=OriginalResultsState::personalBest;
        app.results.bestTimes6000={600000,620000,650000};
        if(scene==14){
            // Capture the real common AResultTA points owner. These controlled
            // clocks/profile inputs are a visual fixture, not a driving replay.
            app.battleProfile=profile;app.frontend.battleProfile=profile;
            app.timeAttackPoints=original::calculateOriginalTimeAttackPoints(profile,
                {0,app.race.elapsed6000,600000,620000,650000});
            app.battleResults={};app.battleResults.resultStatus=0;app.battleResults.profileMode=1;
            app.battleResults.totalTicks6000=app.race.elapsed6000;
            app.battleResults.sectionTimes6000={120000,245000,375000,540000};
            app.battleResults.sectionCount=app.battleResults.sectionCapacity=4;
            app.battleResults.circuitLayout=false;
            app.battleResults.points={app.timeAttackPoints.participation,app.timeAttackPoints.finish,
                app.timeAttackPoints.recordBonus,app.timeAttackPoints.total,profile.u(72)};
            app.timeAttackLectureDone=true;app.timeSummaryDone=true;
            app.resultConfirmPending=false;app.battleResultsClock.reset();
            app.beginResultVisit({app.timeAttackPoints.total,app.timeAttackPoints.balanceBeforeCap,false});
            app.input={};app.updateAudioScene();return;
        }
        app.timeAttackTrace.clear();
        for(unsigned i=0;i<5400;++i)app.timeAttackTrace.recordFrame(100.f+float(i%120),float(i),1.f,0.f,false,i*100);
        TimeAttackEntry entry{6,0,0,540000};for(unsigned i=0;i<5;++i)entry.nameGlyphs[i]=std::uint8_t(profile.u(44+4*i));
        app.records.record(entry);
        app.timeAttackCourseRankingQualified=scene==5;
        // Explicit synthetic source-input fixtures exercise all three actual
        // classifier paths, without pretending a driving replay occurred.
        auto& analysisInput=app.timeAttackAnalysisInput;analysisInput={};
        analysisInput.course=3;analysisInput.car=0;analysisInput.finishTicks6000=540000;
        analysisInput.previousBestTicks6000=600000;
        analysisInput.currentSections6000={120000,125000,130000,165000};
        analysisInput.previousSections6000={135000,140000,145000,180000};
        if(scene==7){analysisInput.finishTicks6000=0;analysisInput.currentSections6000={120000,125000,0,0};}
        analysisInput.resultStatus=scene==7?2u:0u;
        analysisInput.maxSteeringDelta=.2f;analysisInput.acceleratorFraction=scene==7?.45f:.9f;analysisInput.brakeFraction=.05f;
        analysisInput.convertedEventCount=scene==8?26u:0u;analysisInput.wallCount=scene==8?36u:2u;
        analysisInput.ditchCount=2;analysisInput.maxGearUsed=5;
        auto& telemetry=app.timeAttackSnapshot;telemetry={};telemetry.valid=true;
        telemetry.maxSpeedKph=147;telemetry.acceleratorFraction=analysisInput.acceleratorFraction;telemetry.brakeFraction=analysisInput.brakeFraction;
        telemetry.maxSteeringDelta=analysisInput.maxSteeringDelta;telemetry.convertedEventCount=analysisInput.convertedEventCount;
        telemetry.wallCount=analysisInput.wallCount;telemetry.ditchCount=analysisInput.ditchCount;telemetry.maxGearUsed=5;telemetry.averagedFrames=5400;
        telemetry.startIntervalTicks6000=4321;
        std::uint32_t fixtureSeed=1234;
        app.timeAttackAnalysis=original::analyzeOriginalTimeAttack(analysisInput,fixtureSeed);app.timeAttackAnalysisPrepared=true;
        app.race.timeUp=scene==7;
        if(scene==4||scene>=7)app.beginTimeAttackVisit(true);
        else {if(scene==6)app.results.recordFlags=0;app.audio.beginResultMusic();app.beginTimeAttackVisit(false);}
    }
    app.input={};app.updateAudioScene();
}
