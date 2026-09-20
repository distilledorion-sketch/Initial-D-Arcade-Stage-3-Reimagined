int runSharedRecordResetAppTests(App& app){
    const auto output=app.saveRoot.parent_path();
    if(app.saveRoot.filename()!="userdata"||!fs::is_regular_file(output/"ISOLATED_MODE_FLOW_TEST.txt"))throw std::runtime_error("Record reset checks require isolated saves");
    unsigned checks=0;auto require=[&](bool ok,const char* reason){++checks;if(!ok)throw std::runtime_error(reason);};
    app.validationMode=true;app.multiplayer.active=false;app.paused=app.loadingActive=false;
    app.preRaceDialogueActive=app.legendVisitActive=app.buntaVisitActive=app.timeAttackVisitActive=false;
    for(unsigned course:{0u,1u,3u,9u,10u}){
        auto profile=original::makeOriginalFreshBattleProfile();profile.setu(0,1);profile.setu(16,0);profile.setu(1180,129);
        const std::array<std::uint32_t,3> splits{300000,600000,900000};
        if(course<9)require(original::registerOriginalPersonalTimeAttackRecord(profile,original::originalRecordPartition(course*2,false),1200000,0,splits),"Personal fixture could not be created");
        app.importedPersonalRecords=TimeAttackRecords{};
        TimeAttackEntry personal{course*2,0,0,1200000};personal.nameGlyphs={164,169,179,170,180};app.importedPersonalRecords.record(personal);
        app.records=TimeAttackRecords{};app.records.record(personal);
        app.personalRecordProfiles={};app.personalRecordProfiles[0]=profile;app.personalRecordsDirty=true;
        app.frontend.battleProfile=profile;app.frontend.car=0;app.frontend.gameMode=original::OriginalGameMode::TimeAttack;
        app.frontend.course=int(course);app.courseIndex=course<9?int(course):3;app.reverse=app.frontend.reverse=false;app.wet=app.frontend.wet=false;app.night=app.frontend.night=false;
        TimeAttackRecords remoteRows;TimeAttackEntry remote{course*2,0,0,5000000};remoteRows.record(remote);
        app.setSharedRecords(remoteRows,true);
        require(app.frontend.courseRecordTimes()==std::array<std::uint32_t,3>{1200000,1200000,1200000},"Online table hid a faster personal best");
        require(app.displayedTimeAttackRecords().entries().size()==2,"Online rankings must contain personal and remote rows");
        remote.ticks6000=900000;remoteRows={};remoteRows.record(remote);app.setSharedRecords(remoteRows,true);
        require(app.frontend.courseRecordTimes()==std::array<std::uint32_t,3>{900000,900000,1200000},"Online best must coexist with the personal target");
        auto own=app.personalDisplayRecords.entries().front();own.metadataUnknown=false;own.manual=true;
        remoteRows.record(own);app.setSharedRecords(remoteRows,true);
        require(app.displayedTimeAttackRecords().entries().size()==2,"Published personal best must not appear twice");
        require(app.displayedTimeAttackRecords().entries()[1].manual&&!app.displayedTimeAttackRecords().entries()[1].metadataUnknown,"Duplicate must retain remote metadata");
        own.nameGlyphs[0]=own.nameGlyphs[0]==162?163:162;remoteRows.record(own);app.setSharedRecords(remoteRows,true);
        require(app.displayedTimeAttackRecords().entries().size()==3,"Different drivers with equal times must remain distinct");
        app.setSharedRecords({},true);
        require(app.frontend.courseRecordTimes()==std::array<std::uint32_t,3>{1200000,1200000,1200000},"Empty current leaderboard hid personal records");
        require(app.displayedTimeAttackRecords().entries().size()==1,"Removed remote rows survived a fresh empty snapshot");
        const auto before=app.frontend.battleProfile.words;
        app.start();require(app.results.bestTimes6000==std::array<std::uint32_t,3>{1200000,1200000,1200000}&&app.results.suppliedRecordTargets,"Combined personal targets did not reach race HUD");
        require(std::equal(before.begin()+44,before.begin()+233,app.frontend.battleProfile.words.begin()+44)&&app.records.entries().size()==1&&app.records.entries()[0].ticks6000==1200000,"Clearing online display changed personal data");
        app.menu=true;app.sharedRecordsEnabled=false;
        require(app.displayedTimeAttackRecords().best(course*2,0,0).course==1200000,"Local times disappeared when sharing was disabled");
    }
    // Each of the five save slots and the legacy card directory must be
    // isolated, including original/imported tracks, weather and both routes.
    app.records={};app.sharedRecords={};
    for(unsigned condition=0;condition<22;++condition)for(unsigned weather=0;weather<2;++weather){
        app.records.record({condition,weather,0,60000});app.sharedRecords.record({condition,weather,0,80000});
    }
    for(int slot=-1;slot<5;++slot){
        app.useSaveSlot(slot);app.sharedRecordsEnabled=false;
        require(app.displayedTimeAttackRecords().entries().empty(),"Fresh card inherited another save or cabinet's records");
        for(unsigned car:{0u,34u}){
            auto p=original::makeOriginalFreshBattleProfile();p.setu(16,car);p.setu(1180,129);p.setu(44,164+unsigned(slot+1));
            const unsigned ticks=1000000+unsigned(slot+1)*100000+car*1000;
            const std::array<std::uint32_t,3> splits{200000,400000,600000};
            for(unsigned condition=0;condition<18;++condition)for(unsigned weather=0;weather<2;++weather){
                if(condition>=16&&!weather)continue;
                require(original::registerOriginalPersonalTimeAttackRecord(p,original::originalRecordPartition(condition,weather!=0),ticks,1,splits),"Could not seed card partition");
            }
            require(app.profiles.save(car,p),"Could not save isolated card");
            // Exercise valid backup recovery for the second car.
            if(car==34){require(app.profiles.save(car,p),"Could not seed backup");std::ofstream(app.profiles.path(car))<<"corrupt fixture";}
            for(unsigned condition=18;condition<22;++condition)for(unsigned weather=0;weather<2;++weather){
                TimeAttackEntry e{condition,weather,car,ticks};e.nameGlyphs[0]=std::uint8_t(p.u(44));app.importedPersonalRecords.record(e);
            }
        }
        require(app.importedPersonalRecords.save(app.importedPersonalPath),"Could not seed imported personal times");
    }
    for(int slot=4;slot>=-1;--slot){
        app.useSaveSlot(slot);app.sharedRecordsEnabled=false;
        for(unsigned car=0;car<35;++car){
            const auto before=app.profiles.load(car).profile.words;
            app.frontend.car=int(car);app.frontend.battleProfile=app.profiles.load(car).profile;
            for(unsigned condition=0;condition<22;++condition)for(unsigned weather=0;weather<2;++weather){
                if(condition>=16&&condition<18&&!weather)continue;
                app.frontend.course=int(condition/2);app.frontend.reverse=(condition%2)!=0;app.frontend.wet=weather!=0;
                const unsigned own=1000000+unsigned(slot+1)*100000;
                const unsigned model=(car==0||car==34)?own+car*1000:0;
                require(app.frontend.courseRecordTimes()==std::array<std::uint32_t,3>{own,model,model},"Offline course/model/personal times crossed card ownership");
                app.sharedRecordsEnabled=true;
                require(app.frontend.courseRecordTimes()==std::array<std::uint32_t,3>{80000,car==0?80000u:model,model},"Live leaderboard hid personal model/card target");
                app.sharedRecordsEnabled=false;
                require(app.displayedTimeAttackRecords().best(condition,weather,car).course==own,"Disconnect retained remote time");
            }
            require(app.profiles.load(car).profile.words==before,"Reading records mutated card data");
        }
        for(const auto& row:app.displayedTimeAttackRecords().entries())require(row.nameGlyphs[0]==164+unsigned(slot+1),"Ranking included another player's name");
    }
    app.sharedRecordsEnabled=false;app.menu=true;
    std::ofstream(output/"record-separation-native.txt")<<"PASS "<<checks<<" combined online/personal rankings, duplicate suppression, remote removals, all five saves + legacy cards, 35 car selectors, original/imported routes/weather, backup recovery, offline ownership, unchanged personal records\n";
    return 0;
}
int runSharedTimeAppTests(App& app){
    const auto output=app.saveRoot.parent_path();
    if(app.saveRoot.filename()!="userdata"||!fs::is_regular_file(output/"ISOLATED_MODE_FLOW_TEST.txt"))throw std::runtime_error("Shared time tests require isolated saves");
    const bool priorValidation=app.validationMode;struct Restore{App& a;bool v;~Restore(){a.validationMode=v;a.sharedRecordsEnabled=false;}}restore{app,priorValidation};
    std::ofstream log(output/"shared-native.log"),runs(output/"native-finishes.jsonl");unsigned checks=0;
    auto require=[&](bool ok,const char* msg){++checks;if(!ok)throw std::runtime_error(msg);};
    app.multiplayer.active=false;app.paused=app.loadingActive=false;app.input={};
    app.preRaceDialogueActive=app.legendVisitActive=app.buntaVisitActive=app.timeAttackVisitActive=false;
    std::string lastPayload;std::vector<std::uint8_t> lastReplay;
    for(unsigned course:{0u,1u,3u,9u,10u}){
        app.validationMode=true;app.sharedFinishJson.clear();app.setSharedRecords({},true);
        TimeAttackEntry remote{course*2,0,0,5000000};remote.nameGlyphs={162,163,164,221,221};app.sharedRecords.record(remote);
        auto profile=original::makeOriginalFreshBattleProfile();profile.setu(0,1);profile.setu(16,0);profile.setu(1180,129);
        {unsigned glyphs[]={181,166,180,181,220};for(unsigned i=0;i<5;++i)profile.setu(44+4*i,glyphs[i]);}
        app.frontend.battleProfile=profile;app.frontend.car=0;app.frontend.gameMode=original::OriginalGameMode::TimeAttack;
        app.frontend.course=int(course);app.courseIndex=course<9?int(course):3;app.reverse=app.frontend.reverse=false;app.wet=app.frontend.wet=false;app.night=app.frontend.night=false;
        app.start();require(app.results.bestTimes6000[0]==5000000&&app.results.bestTimes6000[1]==5000000,"Shared course/model clocks use actual remote times even slower than defaults");
        for(unsigned i=0;i<600&&app.race.phase==RacePhase::Countdown;++i)app.simulate({});
        require(app.sharedFinishJson.empty(),"Countdown cannot submit a run");
        app.validationMode=false;
        const auto goal=app.originalRace.rules().goalIndex;
        for(std::int32_t i=0;i<=goal+8&&app.race.phase==RacePhase::Running;i+=2){
            auto gate=app.originalRace.gate(i);app.vehicle.position={gate.center[0],gate.center[1],gate.center[2]};DriverInput drive;drive.throttle=1;app.simulate(drive);
            const auto& captured=app.recording.frames.back();
            require(captured.detail.rpm==app.vehicle.rpm&&captured.speed==app.vehicle.speed,"Replay captures actual RPM and speed, not estimates");
            require(captured.detail.bodyPosition.x==app.playerBodyWorld.x&&captured.detail.bodyPosition.y==app.playerBodyWorld.y&&captured.detail.bodyPosition.z==app.playerBodyWorld.z&&captured.detail.pitch==app.bodyPitch&&captured.detail.roll==app.bodyRoll,"Replay captures actual body transform");
            require(captured.detail.suspension==app.wheelPose.suspensionY&&captured.detail.rotation==app.wheelPose.rotationRadians&&captured.detail.steering==app.wheelPose.steeringRadians,"Replay captures actual wheel animation");
            require(captured.detail.elapsed==app.race.elapsed6000&&captured.detail.remaining==app.raceFeedback.displayedRemaining,"Replay captures actual displayed clocks");
        }
        require(app.race.phase==RacePhase::Finished&&!app.race.timeUp,"Original gates produce a completed time attack");
        require(!app.sharedFinishJson.empty(),"New finish publishes an upload payload");runs<<app.sharedFinishJson<<'\n';
        require(!app.sharedFinishReplay.empty(),"Completed race publishes its matching replay");
        {std::ofstream replay(output/("native-replay-"+std::to_string(course)+".idr"),std::ios::binary);replay.write(reinterpret_cast<const char*>(app.sharedFinishReplay.data()),app.sharedFinishReplay.size());}
        auto payload=app.sharedFinishJson;lastPayload=payload;lastReplay=app.sharedFinishReplay;for(unsigned i=0;i<10;++i)app.simulate({});require(app.sharedFinishJson==payload,"Result frames do not duplicate the payload");
        app.sharedFinishJson.clear();for(unsigned i=0;i<10;++i)app.simulate({});require(app.sharedFinishJson.empty(),"Acknowledged payload remains consumed");
        require(app.sharedRecords.entries().size()==1&&app.sharedRecords.entries()[0].ticks6000==5000000,"Local finish does not mutate remote cache");
    }
    app.validationMode=true;app.sharedRecordsEnabled=false;app.sharedFinishJson.clear();app.frontend.course=app.courseIndex=3;app.start();
    for(unsigned i=0;i<30000&&app.race.phase!=RacePhase::Finished;++i)app.simulate({});
    require(app.race.timeUp&&app.sharedFinishJson.empty(),"Timeout does not submit a completed run");
    // Leave one controlled, completed run for the marked Unity integration
    // diagnostic to upload. The remote test installation is hidden afterward.
    app.sharedFinishJson=lastPayload;app.sharedFinishReplay=lastReplay;app.menu=true;
    log<<"PASS "<<checks<<" shared record selection, original/imported finish payloads, one-shot consumption and timeout checks\n";return 0;
}
