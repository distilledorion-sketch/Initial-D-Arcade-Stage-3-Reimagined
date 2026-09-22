// Private integration fixture: real D3 race rules, results/tuning and persistence.
// Gate stepping shortens driving time; it is not a handling/pace benchmark.
void runHakoneTimeAttackTests(App& app,const fs::path& pack){
    const auto id=ImportedCourse::courseId(pack),recordCondition=id*2;
    const auto output=app.saveRoot.parent_path();
    if(app.saveRoot.filename()!="userdata"||!fs::is_regular_file(output/"HAKONE_TA_TEST.txt"))throw std::runtime_error("Hakone tests require marked isolated saves");
    std::ofstream log(output/"native-time-attack.txt");unsigned checks=0;
    auto check=[&](bool ok,const char* message){++checks;if(!ok){log<<"FAIL "<<message<<'\n';log.flush();throw std::runtime_error(message);}};
    check(app.importedRoot(id)==pack,"Installed Hakone pack registered before testing");
    auto previousMode=app.frontend.gameMode;
    app.frontend.gameMode=original::OriginalGameMode::TimeAttack;
    check(app.frontend.courseChoices().size()>=10,"Main Time Attack offers ten courses");
    app.frontend.gameMode=previousMode;app.validationMode=false;
    auto start=[&](unsigned scenario){
        app.timeAttackVisitActive=false;app.loadingActive=false;app.menu=false;app.input={};
        app.frontend.gameMode=original::OriginalGameMode::TimeAttack;app.frontend.course=int(id);app.courseIndex=3;
        app.reverse=app.frontend.reverse=bool(scenario&1);app.wet=app.frontend.wet=bool(scenario&2);app.night=app.frontend.night=bool(scenario&4);
        app.start();
        if(fs::is_regular_file(output/"PALETTE_RECOVERY_TEST.txt")){
            // The race/showroom already tolerates a legacy invalid stored color.
            // Results, upgrades, ranking and Continue must tolerate it too.
            const auto invalid=original::originalCarColorCounts.at(unsigned(app.frontend.car));
            app.battleProfile.setu(64,invalid);app.frontend.battleProfile.setu(64,invalid);
        }
        check(app.originalSession.selection().physics.conditionCode==((id==11?6u:id==10?2u:0u)+(scenario&1)),"Imported handling: Akina for Enna, Usui for Sadamine, Myogi for Hakone");
        for(unsigned i=0;i<600&&app.race.phase==RacePhase::Countdown;++i)app.simulate({});
        check(app.race.phase==RacePhase::Running,"Countdown starts Hakone");
    };
    auto finish=[&]{
        const auto goal=app.originalRace.rules().goalIndex;
        for(int i=0;i<=goal+10&&app.race.phase==RacePhase::Running;i+=2){const auto gate=app.originalRace.gate(i);app.vehicle.position={gate.center[0],gate.center[1],gate.center[2]};DriverInput d;d.throttle=(i%60<40)?1.f:0.f;d.brake=(i%60>=50)?1.f:0.f;app.simulate(d);}
        check(app.race.phase==RacePhase::Finished&&!app.race.timeUp,"Native gates reach finish");
        check(app.pendingResultSetup.has_value()&&app.resultsReady,"Finish prepares points and record summary");
        check(app.battleResults.sectionCount==4&&app.battleResults.sectionTimes6000[3]==app.race.elapsed6000,"All four checkpoint times reach result screen");
    };
    for(unsigned scenario=0;scenario<8;++scenario){
        start(scenario);const auto before=app.battleProfile.u(72);const auto originalWords=app.battleProfile.words;
        finish();check(app.timeAttackPoints.participation==1000&&app.timeAttackPoints.finish==2000,"Original completion points");
        check(app.timeAttackPoints.total>=3000&&app.timeAttackPoints.total<=5000,"Bounded original record bonuses");
        check(app.records.personalBest(app.results.condition,unsigned(app.wet),unsigned(app.frontend.car)).intermediate6000[2]>0,"Course-best comparison splits recorded");
        check(app.timeAttackSnapshot.valid&&!app.timeAttackSnapshot.drivingPath.empty(),"Actual driving telemetry captured");
        app.prepareTimeAttackAnalysis();const auto setup=app.timeAttackVisitSetup();
        check(setup.condition==recordCondition+(scenario&1)&&setup.customMaps.size()==5,"Hakone identity and five coaching maps");
        for(auto value:setup.analysisInput.currentSections6000)check(value>0,"Four coaching section times");
        for(const auto& page:setup.customMaps){check(!page.road.empty()&&!page.driving.empty(),"Each coaching page includes route and driving line");for(const auto& l:page.driving)for(const auto& point:{l.from,l.to})check(point[0]>=194.9f&&point[0]<=435.1f&&point[1]>=194.9f&&point[1]<=435.1f,"Driving lines clipped to map");}
        app.beginTimeAttackVisit(true);for(int i=0;i<100&&!app.timeAttackVisit.finished();++i)app.timeAttackVisit.advance({true,false,false,true});
        check(app.timeAttackVisit.route()==original::OriginalTimeAttackVisit::Route::Points,"Coaching routes to points");app.timeAttackVisitActive=false;
        app.beginResultVisit(*app.pendingResultSetup);app.pendingResultSetup.reset();
        for(int i=0;i<12000&&!app.resultAnimationFrame.finished;++i){app.resultConfirmPending=true;app.advanceResultVisit();}
        check(app.resultAnimationFrame.finished,"Original points and tuning lifecycle completes");
        check(app.resultVisit.animation.committedBalance==std::min(before+app.timeAttackPoints.total,999999999u),"Earned points applied once to profile");
        const auto balance=app.battleProfile.u(72);app.advanceResultVisit();check(app.battleProfile.u(72)==balance,"Points cannot repeat");
        app.beginTimeAttackVisit(false);check(app.timeAttackPersonalRegistered,"Personal registration reaches imported store");
        const auto personal=app.importedPersonalRecords.personalBest(recordCondition+(scenario&1),bool(scenario&2),unsigned(app.frontend.car));
        check(personal.ticks6000>0&&personal.intermediate6000[2]>0,"Personal time and splits saved");
        check(!app.registerTimeAttackPersonalResult(),"Personal registration idempotent");
        // The original D3 card record ranges must not receive Hakone times.
        for(unsigned offset=176;offset<932;offset+=4)check(app.battleProfile.u(offset)==originalWords[offset/4],"Original course personal records untouched");
        TimeAttackRecords disk;check(disk.load(app.userdataRoot()/"time_attack_records_v1.csv")&&disk.best(recordCondition+(scenario&1),bool(scenario&2),unsigned(app.frontend.car)).model>0,"Global records survive reload");
        TimeAttackRecords card;check(card.load(app.importedPersonalPath)&&card.personalBest(recordCondition+(scenario&1),bool(scenario&2),unsigned(app.frontend.car)).ticks6000==personal.ticks6000,"Personal records survive reload");
        const auto saved=app.profiles.load(unsigned(app.frontend.car));check(saved.profile.u(72)==app.battleProfile.u(72),"Points persisted to selected profile");
        log<<"PASS condition "<<scenario<<" time="<<app.race.elapsed6000<<" earned="<<app.timeAttackPoints.total<<" balance="<<balance<<'\n';log.flush();
    }
    start(0);const auto entries=app.records.entries().size();
    for(unsigned i=0;i<23000&&app.race.phase!=RacePhase::Finished;++i)app.simulate({});
    check(app.race.timeUp&&app.timeAttackPoints.total==1000,"Timeout awards participation only");
    check(app.records.entries().size()==entries&&!app.registerTimeAttackPersonalResult(),"Timeout cannot save a finish record");
    app.useSaveSlot(4);check(app.importedPersonalRecords.best(recordCondition,0,unsigned(app.frontend.car)).model==0,"Other save slot does not inherit personal times");app.useSaveSlot(-1);
    // Leave a repeat finish for actual Unity presentation/continue captures.
    start(0);finish();app.finishBannerDone=true;app.timeSummaryDone=true;
    log<<"PASS "<<checks<<" checks; controlled native gate finishes, natural timeout, points/tuning, records/reload, isolated slots.\n";
}
