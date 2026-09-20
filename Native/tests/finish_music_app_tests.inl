// Actual App audio routing with bounded outcome fixtures in a marked private
// player. Race rules are not simulated here; existing mode-flow checks cover
// their natural finish events. No scores, profiles or packets are fabricated.
int runFinishMusicAppTests(App& app){
    const auto output=app.saveRoot.parent_path();
    if(app.saveRoot.empty()||!fs::exists(output/"ISOLATED_MODE_FLOW_TEST.txt")||app.multiplayer.active)
        throw std::runtime_error("Finish-music fixtures require isolated offline userdata");
    std::ofstream report(output/"finish-music-app.log");unsigned checks=0;
    const auto require=[&](bool ok,const char* message){++checks;if(!ok){report<<"FAIL "<<message<<'\n';report.flush();throw std::runtime_error(message);}};
    const auto savedRace=app.race;const auto savedMp=app.multiplayer;
    const auto savedResult=app.battleResult;
    const bool savedMenu=app.menu,savedPaused=app.paused,savedBattle=app.battle,savedBunta=app.bunta;
    const bool savedLoading=app.loadingActive,savedVs=app.vsActive,savedPre=app.preRaceDialogueActive,
        savedLegend=app.legendVisitActive,savedBuntaVisit=app.buntaVisitActive,savedTaVisit=app.timeAttackVisitActive;
    const auto profile=app.battleProfile.words,frontendProfile=app.frontend.battleProfile.words;
    const auto restore=[&]{
        app.multiplayer=savedMp;app.race=savedRace;app.battleResult=savedResult;
        app.menu=savedMenu;app.paused=savedPaused;app.battle=savedBattle;app.bunta=savedBunta;
        app.loadingActive=savedLoading;app.vsActive=savedVs;app.preRaceDialogueActive=savedPre;
        app.legendVisitActive=savedLegend;app.buntaVisitActive=savedBuntaVisit;app.timeAttackVisitActive=savedTaVisit;
        app.audio.endResultMusic();app.audio.scene(true,false,false);
    };
    struct Restore {const decltype(restore)& fn;~Restore(){fn();}} cleanup{restore};
    app.menu=app.paused=app.loadingActive=app.vsActive=app.preRaceDialogueActive=
        app.legendVisitActive=app.buntaVisitActive=app.timeAttackVisitActive=false;
    const auto begin=[&](bool battle,bool bunta,bool won,bool timeup){
        app.audio.endResultMusic();app.audio.scene(true,false,false);app.multiplayer={};
        app.battle=battle;app.bunta=bunta;app.battleResult=won?original::OriginalLegendResult::Win:original::OriginalLegendResult::Loss;
        app.race.phase=RacePhase::Finished;app.race.timeUp=timeup;
    };
    const auto scene=[&]{return app.audio.raceTimingStatistics().scene;};
    const auto reject=[&](int winner){bool rejected=false;try{app.setMultiplayerResult(winner);}catch(const std::exception&){rejected=true;}require(rejected,"Invalid result transition accepted");};
    struct Offline {bool battle,bunta,won,timeup;int expected;};
    for(const auto c:std::array<Offline,8>{{
        {false,false,true,false,2},{false,false,false,true,3},
        {true,false,true,false,2},{true,false,false,false,5},{true,false,false,true,3},
        {true,true,true,false,2},{true,true,false,false,5},{true,true,false,true,3}}}){
        begin(c.battle,c.bunta,c.won,c.timeup);app.updateAudioScene();
        require(scene()==c.expected,"Offline result chose incorrect finish stream");
        for(unsigned i=0;i<64;++i)app.audio.renderStereo(800,0,0,0,false);
        const auto cursor=app.audio.raceTimingStatistics().streamFrame,serial=double(app.audio.outputResetSerial());
        app.updateAudioScene();require(app.audio.raceTimingStatistics().streamFrame==cursor&&double(app.audio.outputResetSerial())==serial,"Repeated finished frame restarted stream");
        app.updateAudioScene(true);for(unsigned i=0;i<64;++i)app.audio.renderStereo(800,0,0,0,false);
        require(app.audio.raceTimingStatistics().streamFrame==cursor,"Offline pause advanced finish stream");
        app.updateAudioScene();reject(0);
        report<<"PASS offline battle="<<c.battle<<" bunta="<<c.bunta<<" win="<<c.won<<" timeout="<<c.timeup<<" scene="<<scene()<<'\n';
    }
    for(unsigned slot=0;slot<2;++slot)for(int winner=-1;winner<=2;++winner){
        begin(false,false,true,winner==-1);app.multiplayer.active=true;app.multiplayer.config.localSlot=slot;
        app.race.phase=RacePhase::Running;app.updateAudioScene();reject(winner);
        app.race.phase=RacePhase::Finished;app.updateAudioScene();
        require(scene()==6&&app.audio.raceMusicFinished(),"Online finish played victory before settled result");
        for(unsigned i=0;i<10;++i)app.updateAudioScene();require(scene()==6,"Online pending finish fell back to WIN");
        reject(-2);reject(3);app.setMultiplayerResult(winner);
        const int expected=winner==-1?3:winner==2?6:winner==int(slot)?2:5;
        require(scene()==expected,"Settled online result does not match local winning slot");
        for(unsigned i=0;i<64;++i)app.audio.renderStereo(800,0,0,0,false);
        const auto cursor=app.audio.raceTimingStatistics().streamFrame;const auto serial=app.audio.outputResetSerial();
        app.setMultiplayerResult(winner);app.updateAudioScene();
        require(scene()==expected&&app.audio.raceTimingStatistics().streamFrame==cursor&&app.audio.outputResetSerial()==serial,"Duplicate online result replayed stinger");
        reject(winner==0?1:0);require(scene()==expected,"Conflicting result altered active finish stream");
        app.multiplayer.disconnected=true;app.updateAudioScene();
        require(scene()==0&&(app.audio.raceTimingStatistics().flags&4u)&&app.audio.raceMusicFinished(),"Disconnected result retained victory stream");
        reject(winner);
        report<<"PASS online slot="<<slot<<" winner="<<winner<<" scene="<<expected<<" pending/duplicate/conflict/disconnect guards\n";
    }
    begin(true,false,false,false);app.audio.beginResultMusic();app.updateAudioScene();
    require(scene()==0&&app.audio.originalMusicCue()==2,"Loss replaced original common RESULT sequence");
    require(app.battleProfile.words==profile&&app.frontend.battleProfile.words==frontendProfile,"Audio routing modified progression");
    report<<"PASS finish music App: "<<checks<<" checks; offline/online result routing and unchanged profiles\n";
    std::ofstream json(output/"finish-music-app-report.json");
    json<<"{\"passed\":true,\"checks\":"<<checks<<",\"offlineCases\":8,\"onlineCases\":8,\"profilesUnchanged\":true}\n";
    return 0;
}
