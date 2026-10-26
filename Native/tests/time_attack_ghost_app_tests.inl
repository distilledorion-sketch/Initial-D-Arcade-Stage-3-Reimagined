int runTimeAttackGhostAppTests(App& a){
    const auto output=a.saveRoot.parent_path();
    if(a.saveRoot.filename()!="userdata"||!fs::is_regular_file(output/"ISOLATED_MODE_FLOW_TEST.txt"))throw std::logic_error("Ghost test requires private userdata");
    unsigned checks=0;const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);};
    a.validationMode=true;a.multiplayer.active=false;a.paused=a.loadingActive=false;
    a.preRaceDialogueActive=a.legendVisitActive=false;a.replayRecordingFlags=0;
    a.useSaveSlot(0);a.frontend.gameMode=original::OriginalGameMode::TimeAttack;
    a.frontend.battleProfile=original::makeOriginalFreshBattleProfile();a.frontend.car=0;
    a.frontend.course=a.courseIndex=3;a.reverse=a.frontend.reverse=false;a.wet=a.frontend.wet=false;a.night=a.frontend.night=false;
    a.start();check(a.personalGhostContext()&&a.personalGhost.replay.frames.empty(),"New save has an unrelated ghost");
    for(unsigned frame=0;frame<600&&a.race.phase==RacePhase::Countdown;++frame)a.simulate({});
    check(a.race.phase==RacePhase::Running,"Ghost race did not leave countdown");
    DriverInput driving;driving.throttle=1;driving.automatic=true;
    for(unsigned frame=0;frame<600;++frame)a.simulate(driving);
    check(!a.archiveThisRace&&a.recording.frames.size()>=600,"Optional recordings disabled best-ghost capture");
    check(a.vehicle.speed>5,"Ghost fixture did not drive the car");
    // Exercise the real finish/save path; only the isolated native gate probe
    // moves the car. No managed upload client is attached to this fixture.
    a.validationMode=false;
    const auto goal=a.originalRace.rules().goalIndex;
    for(std::int32_t i=0;i<=goal+8&&a.race.phase==RacePhase::Running;i+=2){const auto gate=a.originalRace.gate(i);a.vehicle.position={gate.center[0],gate.center[1],gate.center[2]};a.simulate({});}
    a.validationMode=true;
    check(a.race.phase==RacePhase::Finished&&!a.race.timeUp,"Native finish unavailable");
    TimeAttackGhost saved;check(saved.load(a.personalGhostPath()),"Actual TA finish did not save ghost");
    const auto finish=saved.replay.finishTicks6000;
    check(finish==a.race.elapsed6000&&saved.car==0,"Ghost did not store exact finish");
    a.sharedFinishJson.clear();a.start();
    check(a.personalGhost.replay.finishTicks6000==finish,"Retry failed to load same best run");
    a.race.ticks=60;a.race.phase=RacePhase::Running;a.clock.accumulator=FixedClock::step*.25;
    check(std::abs(a.personalGhostTick()-59.25)<.00001,"Ghost not synchronized to presentation alpha");
    a.paused=true;check(a.personalGhostTick()==60,"Paused ghost advanced");a.paused=false;
    a.multiplayer.active=true;check(!a.personalGhostContext(),"Online race showed personal ghost");a.multiplayer.active=false;
    a.replayPlaybackActive=true;check(!a.personalGhostContext(),"Replay viewer showed personal ghost");a.replayPlaybackActive=false;
    a.battle=true;check(!a.personalGhostContext(),"Battle showed personal ghost");a.battle=false;
    a.frontend.car=1;a.frontend.automatic=a.automatic=false;a.start();
    check(a.personalGhost.replay.finishTicks6000==finish&&a.personalGhost.car==0,"Changing car/transmission lost track best");
    a.reverse=a.frontend.reverse=true;a.start();check(a.personalGhost.replay.frames.empty(),"Reverse loaded forward ghost");
    a.reverse=a.frontend.reverse=false;a.wet=a.frontend.wet=true;a.start();check(a.personalGhost.replay.frames.empty(),"Wet loaded dry ghost");
    a.wet=a.frontend.wet=false;a.useSaveSlot(1);a.start();check(a.personalGhost.replay.frames.empty(),"Other save inherited personal ghost");
    a.useSaveSlot(0);
    if(a.importedRoot(15).empty())a.importedRoot(15)=a.root.parent_path()/"RuntimeAssets/TSUBAKI";
    a.frontend.course=15;a.start();check(a.personalGhost.replay.frames.empty(),"Imported course loaded Akina ghost");
    check(TimeAttackGhost::saveBest(a.personalGhostPath(),saved.replay,saved.car)==TimeAttackGhost::Saved::Replaced,"Imported ghost save failed");
    a.start();check(a.personalGhost.replay.finishTicks6000==finish,"Imported course failed to load its ghost");
    // Restore Akina for the host's rendering check, with a real moving sample.
    a.courseIndex=a.frontend.course=3;a.frontend.car=0;a.start();
    for(unsigned frame=0;frame<600&&a.race.phase==RacePhase::Countdown;++frame)a.simulate({});
    driving.throttle=.55f;
    for(unsigned frame=0;frame<190;++frame)a.simulate(driving);
    a.paused=false;
    std::ofstream(output/"ghost-app-checks.txt")<<"PASS "<<checks<<" native ghost capture/finish/restart/context checks\n";
    return 0;
}
