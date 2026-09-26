// Included after main.cpp by the native scene bridge. This diagnostic requires
// private userdata and never writes ordinary driver profiles or leaderboards.
int runTimeAttackCompletionAppTests(App& app){
    const auto output=app.saveRoot.parent_path();
    if(app.saveRoot.empty()||app.saveRoot.filename()!="userdata"||
       !fs::is_regular_file(output/"ISOLATED_MODE_FLOW_TEST.txt"))
        throw std::runtime_error("Time Attack completion diagnostic requires marked isolated userdata");
    const bool oldValidation=app.validationMode;app.validationMode=true;
    struct Restore {App& app;bool validation;~Restore(){app.validationMode=validation;}} restore{app,oldValidation};
    std::ofstream report(output/"time-attack-completion-app.log");unsigned checks=0;
    const auto require=[&](bool pass,const char* message){++checks;if(!pass){report<<"FAIL "<<message<<'\n';report.flush();throw std::runtime_error(message);}};
    app.multiplayer.active=false;app.paused=app.loadingActive=false;
    app.preRaceDialogueActive=app.legendVisitActive=app.buntaVisitActive=app.timeAttackVisitActive=false;
    app.input={};app.frontend.car=0;app.frontend.automatic=true;app.records=TimeAttackRecords{};
    // A fast unrelated named leaderboard record must not become this driver's
    // personal previous best or affect which previous splits coaching sees.
    app.records.record({6,0,0,100000});
    // Aggregate cabinet records are no longer a displayed leaderboard source.
    // Supply this unrelated driver's time through the private shared snapshot.
    app.setSharedRecords(app.records,true);
    const auto partition=original::originalRecordPartition(6,false);
    const std::array<std::uint32_t,3> previousSplits{250000,520000,760000};
    const auto start=[&](std::uint32_t personal,std::uint32_t rankingBest=100000){
        auto profile=original::makeOriginalFreshBattleProfile();profile.setu(0,1);profile.setu(16,0);profile.setu(1180,129);
        profile.setu(176+4*partition.personalIndex(),personal);profile.setu(320+4*partition.personalIndex(),1);
        for(unsigned i=0;i<3;++i)profile.setu(500+4*(partition.personalIndex()*3+i),previousSplits[i]);
        // Distinct neighboring condition data detects accidental cross-race writes.
        profile.setu(176+4*(partition.personalIndex()+1),1234567);
        app.frontend.battleProfile=profile;app.frontend.gameMode=original::OriginalGameMode::TimeAttack;
        app.courseIndex=app.frontend.course=3;app.reverse=app.frontend.reverse=false;
        app.wet=app.frontend.wet=false;app.night=app.frontend.night=false;
        app.start();require(!app.timeAttackPersonalRegistered,"New race clears personal registration latch");
        require(app.results.bestTimes6000[2]==personal,"TA reads selected driver's actual personal best");
        require(app.results.bestTimes6000[0]==rankingBest&&app.results.bestTimes6000[1]==rankingBest,"Course/model records stay independent of personal card");
        require(!app.registerTimeAttackPersonalResult(),"Running/countdown cannot register result");
        for(unsigned frame=0;frame<600&&app.race.phase==RacePhase::Countdown;++frame)app.simulate({});
        require(app.race.phase==RacePhase::Running,"TA countdown reaches native running state");
    };
    const auto finish=[&]{
        const auto goal=app.originalRace.rules().goalIndex;
        for(std::int32_t i=0;i<=goal+8&&app.race.phase==RacePhase::Running;i+=2){const auto gate=app.originalRace.gate(i);
            app.vehicle.position={gate.center[0],gate.center[1],gate.center[2]};app.simulate({});}
        require(app.race.phase==RacePhase::Finished&&!app.race.timeUp,"Controlled native gates finish TA");
        require(app.pendingResultSetup.has_value(),"Finish queues real common result owner");
    };
    start(1000000);finish();const auto finishTicks=app.race.elapsed6000;
    require(app.timeAttackSnapshot.valid&&!app.timeAttackAnalysisPrepared,"Final solver publishes source telemetry before one-time analysis");
    require(app.timeAttackSnapshot.wallCount==app.originalSession.vehicle().drive.u(0x3f4)&&
        app.timeAttackSnapshot.maxGearUsed==app.originalSession.vehicle().drive.u(0x400),"Snapshot preserves source wall/top-gear statistics");
    auto expectedSeed=app.originalSession.contactCompletion().randomSeed0C37C778;
    require(original::originalPersonalTimeAttackRecord(app.battleProfile,partition).ticks6000==1000000,"Finish does not preempt lecture with personal write");
    require(app.beginTimeAttackVisit(true),"Actual App starts HLecture");
    require(app.timeAttackVisit.setup().oldBestTimes6000[2]==1000000,"Lecture gets original personal best before registration");
    require(app.timeAttackVisit.setup().sourceAnalysisAvailable&&app.timeAttackAnalysisPrepared,"Lecture receives prepared source advice");
    const auto& analysisInput=app.timeAttackAnalysisInput;
    require(analysisInput.previousBestTicks6000==1000000&&analysisInput.finishTicks6000==finishTicks,"Coaching compares prior personal total, never unrelated leaderboard");
    require(analysisInput.previousSections6000==std::array<std::uint32_t,4>{250000,270000,240000,240000},"Prior cumulative personal splits become section durations");
    const auto& currentTimes=app.originalRace.state().times;std::array<std::uint32_t,4> durations{};std::uint32_t preceding=0;
    for(unsigned i=0;i<3&&i<currentTimes.lapCount;++i){durations[i]=currentTimes.lapTimes[i]-preceding;preceding=currentTimes.lapTimes[i];}
    durations[3]=finishTicks-preceding;
    require(analysisInput.currentSections6000==durations,"Current source lap clocks become section durations");
    require(analysisInput.convertedEventCount==app.timeAttackSnapshot.convertedEventCount&&
        analysisInput.acceleratorFraction==app.timeAttackSnapshot.acceleratorFraction&&
        analysisInput.brakeFraction==app.timeAttackSnapshot.brakeFraction&&
        analysisInput.maxSteeringDelta==app.timeAttackSnapshot.maxSteeringDelta&&
        analysisInput.wallCount==app.timeAttackSnapshot.wallCount&&analysisInput.ditchCount==app.timeAttackSnapshot.ditchCount&&
        analysisInput.maxGearUsed==app.timeAttackSnapshot.maxGearUsed,"Coaching uses source metrics without host graph substitutions");
    const auto expectedAnalysis=original::analyzeOriginalTimeAttack(analysisInput,expectedSeed);
    require(expectedAnalysis.kind==app.timeAttackAnalysis.kind&&expectedAnalysis.lineAddresses==app.timeAttackAnalysis.lineAddresses&&
        expectedSeed==app.originalSession.contactCompletion().randomSeed0C37C778,"App advice/RNG equals bounded source classifier");
    app.prepareTimeAttackAnalysis();app.timeAttackVisitSetup();
    require(expectedSeed==app.originalSession.contactCompletion().randomSeed0C37C778&&
        app.timeAttackVisit.countdownTicks()==expectedAnalysis.countdownTicks,"Repeated setup cannot redraw praise or alter source timer");
    const auto beforeLecture=app.battleProfile.words;
    for(unsigned frame=0;frame<100&&!app.timeAttackVisit.finished();++frame)app.timeAttackVisit.advance({false,false,false,true});
    require(app.timeAttackVisit.finished()&&app.timeAttackVisit.route()==original::OriginalTimeAttackVisit::Route::Points,"Lecture original skip/fades route to points");
    require(app.battleProfile.words==beforeLecture&&!app.timeAttackPersonalRegistered,"Lecture does not rewrite personal card");
    app.timeAttackVisitActive=false;app.timeAttackLectureDone=true;
    app.beginResultVisit(*app.pendingResultSetup);app.pendingResultSetup.reset();
    for(unsigned frame=0;frame<10000&&!app.resultAnimationFrame.finished;++frame){app.resultConfirmPending=true;app.advanceResultVisit();}
    require(app.resultAnimationFrame.finished,"Real common points/tuning owner completes");
    require(original::originalPersonalTimeAttackRecord(app.battleProfile,partition).ticks6000==1000000,"Common points owner preserves previous personal time");
    const auto beforeRegistration=app.battleProfile.words;
    require(app.beginTimeAttackVisit(false),"Actual App enters post-results ranking/continue");
    require(app.timeAttackCourseRankingQualified&&app.timeAttackVisit.stage()==original::OriginalTimeAttackVisit::Stage::Ranking,"Qualifying actual finish shows leaderboard");
    const auto saved=original::originalPersonalTimeAttackRecord(app.battleProfile,partition);
    require(saved.ticks6000==finishTicks&&saved.night==0,"ARegist updates total and day/night on original card");
    const auto& times=app.originalRace.state().times;const auto count=std::min(times.sectionCount,3u);
    for(unsigned i=0;i<count;++i)require(saved.intermediate6000[i]==times.sectionTimes[i],"ARegist saves actual cumulative intermediates");
    for(unsigned i=0;i<app.battleProfile.words.size();++i){const unsigned offset=i*4;
        bool changedField=offset==176+4*partition.personalIndex()||offset==320+4*partition.personalIndex();
        for(unsigned j=0;j<count;++j)changedField|=offset==500+4*(partition.personalIndex()*3+j);
        if(!changedField)require(app.battleProfile.words[i]==beforeRegistration[i],"Personal commit preserves every unrelated profile field");
    }
    require(app.frontend.battleProfile.words==app.battleProfile.words,"Personal commit reaches active frontend profile");
    const auto once=app.battleProfile.words;require(!app.registerTimeAttackPersonalResult()&&app.battleProfile.words==once,"Personal registration is idempotent");
    report<<"PASS real finish/lecture/common-results/registration sequence ticks="<<finishTicks<<"; previous personal1000000, unrelated leaderboard100000\n";

    // Both slower and exact-tie finishes preserve prior total and splits.
    for(unsigned previous:{1u,finishTicks}){
        start(previous);finish();if(previous==finishTicks)require(app.race.elapsed6000==previous,"Repeat gate fixture establishes exact tie");
        const auto old=app.battleProfile.words;
        require(!app.registerTimeAttackPersonalResult(),"Slower/tied run does not register improvement");
        require(app.battleProfile.words==old,"Slower/tied run preserves all card words");
    }
    start(1000000);
    for(unsigned frame=0;frame<20000&&app.race.phase!=RacePhase::Finished;++frame)app.simulate({});
    require(app.race.phase==RacePhase::Finished&&app.race.timeUp,"Natural idle reaches original time-up");
    const auto timeup=app.battleProfile.words;
    require(!app.registerTimeAttackPersonalResult()&&app.battleProfile.words==timeup,"Time-up awards no personal record");
    require(app.beginTimeAttackVisit(true),"Time-up still receives original coaching");
    require(app.timeAttackVisit.setup().sourceAnalysisAvailable&&app.timeAttackAnalysis.kind==0&&
        app.timeAttackAnalysisInput.finishTicks6000==0&&app.timeAttackVisit.countdownTicks()==1279,"Time-up uses source zero finish and kind0 advice/timer");
    app.timeAttackVisitActive=false;
    start(0);finish();const auto fresh=app.battleProfile.words;
    app.multiplayer.active=true;app.prepareTimeAttackAnalysis();require(!app.timeAttackAnalysisPrepared&&!app.registerTimeAttackPersonalResult()&&app.battleProfile.words==fresh,"Network state cannot analyze or write local TA card");app.multiplayer.active=false;
    app.battle=true;app.prepareTimeAttackAnalysis();require(!app.timeAttackAnalysisPrepared&&!app.registerTimeAttackPersonalResult()&&app.battleProfile.words==fresh,"Battle state cannot analyze or write TA card");app.battle=false;
    app.battleProfile.setu(1180,app.battleProfile.u(1180)|0x20000u);app.prepareTimeAttackAnalysis();
    require(!app.timeAttackAnalysisPrepared,"Source suppression flag prevents advice and RNG consumption");
    app.battleProfile.setu(1180,app.battleProfile.u(1180)&~0x20000u);
    require(app.results.bestTimes6000[2]==0,"Different driver's fresh card has no borrowed personal best");
    require(app.registerTimeAttackPersonalResult(),"First completed personal record registers");
    // A busy community board must remain visible when this run is slower
    // than every top-ten entry and earns no record flags. Use actual native
    // gates and the common-results render handoff, not a forced Ranking stage.
    TimeAttackRecords fullBoard;
    for(unsigned i=0;i<10;++i)fullBoard.record({6,0,i,i+1});
    app.setSharedRecords(fullBoard,true);
    start(1,1);finish();
    require(!app.timeAttackCourseRankingQualified&&app.results.recordFlags==0,"Actual finish outside top ten earns no ranking or record flags");
    app.timeAttackLectureDone=true;app.timeSummaryDone=true;app.finishBannerDone=true;
    app.beginResultVisit(*app.pendingResultSetup);app.pendingResultSetup.reset();
    for(unsigned frame=0;frame<10000&&!app.resultAnimationFrame.finished;++frame){app.resultConfirmPending=true;app.advanceResultVisit();}
    require(app.resultAnimationFrame.finished,"Nonrecord common results complete");
    require(app.render(0),"Common results render hands off to the post-race owner");
    require(app.timeAttackVisitActive&&app.timeAttackVisit.stage()==original::OriginalTimeAttackVisit::Stage::Ranking,"Actual eleventh-place finish opens leaderboard through production render handoff");
    const auto& rows=app.timeAttackVisit.rankingRows();
    require(rows.size()==10&&rows.front().word(0)==1&&rows.back().word(0)==10,"Leaderboard retains existing top ten without inserting a slower run");
    require(app.results.recordFlags==0&&!app.timeAttackVisit.setup().courseRankingQualified,"Opening leaderboard cannot promote the finish or award a record");
    report<<"PASS "<<checks<<" native App Time Attack completion checks. Controlled gate finishes and natural timeout; real native lecture/result owners; private validation mode. No Unity rendering or ordinary save writes.\n";
    return 0;
}
