int runPlayerReplayAppTests(App& a){
    if(a.saveRoot.filename()!="userdata"||!fs::is_regular_file(a.saveRoot.parent_path()/"ISOLATED_MODE_FLOW_TEST.txt"))throw std::logic_error("Private replay fixture required");
    const auto out=a.saveRoot.parent_path();unsigned checks=0;
    const auto require=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);};
    a.validationMode=true;a.paused=a.loadingActive=a.preRaceDialogueActive=a.legendVisitActive=a.vsActive=false;
    a.replayRecordingFlags=0;a.frontend.gameMode=original::OriginalGameMode::TimeAttack;a.frontend.course=a.courseIndex=0;a.frontend.car=0;
    a.frontend.battleProfile=original::makeOriginalFreshBattleProfile();a.start();require(!a.archiveThisRace,"Disabled personal TA retained");
    a.replayRecordingFlags=8;a.start();require(a.archiveThisRace,"Community sharing did not force capture");
    a.replayRecordingFlags=4;a.frontend.gameMode=original::OriginalGameMode::LegendOfTheStreets;
    a.frontend.battleProfile.setu(1180,0x0800c081);original::selectOriginalRival(a.frontend.battleProfile,1);a.start();
    require(a.archiveThisRace&&a.archiveMode==2,"Legend recording option not applied");
    a.vsActive=false;a.preRaceDialogueActive=false;
    a.input.steeringSource=3;a.steeringDeadzone=.2f;
    for(unsigned i=0;i<850;++i)a.simulate({});
    a.input.steeringSource=0;
    require(a.recording.frames.size()>100&&a.rivalRecording.frames.size()==a.recording.frames.size(),"Legend did not capture both cars");
    require(a.rivalRecording.frames.back().position.x==a.rivalVehicle.position.x,"Legend opponent pose differs");
    a.validationMode=false;a.publishLocalReplay();require(!a.localReplayJson.empty()&&a.sharedFinishJson.empty(),"Legend replay entered shared queue");
    require(a.localReplayTelemetry.size()==16+32*a.recording.frames.size()&&a.localReplayTelemetry[0]=='I'&&a.localReplayTelemetry[3]=='1',"Legend replay omitted IDT1 telemetry");
    require(a.localReplayJson.find("\"steeringTicks\":[0,0,0,"+std::to_string(a.recording.frames.size())+",0]")!=std::string::npos,"Legend replay omitted steering source counts");
    require(a.localReplayJson.find("\"steeringDeadzone\":0.2,")!=std::string::npos&&a.localReplayJson.find("\"steeringSettingsChanged\":0")!=std::string::npos,"Legend replay omitted steering settings");
    require((a.localReplayTelemetry[16+28]>>5)==3,"Legend IDT1 omitted the steering source");
    const auto write=[&](const char* mode){std::ofstream(out/(std::string(mode)+".json"))<<a.localReplayJson;for(unsigned part=0;part<2;++part){const auto& bytes=part?a.localReplayRival:a.localReplayPlayer;std::ofstream f(out/(std::string(mode)+(part?"-opponent.idr":"-player.idr")),std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());}};
    write("legend");a.localReplayJson.clear();a.localReplayPlayer.clear();a.localReplayRival.clear();a.localReplayTelemetry.clear();
    a.validationMode=true;a.replayRecordingFlags=2;
    Idas3MultiplayerConfig config{sizeof(config),2,0,0,0,0,0,1,0,1};
    auto local=original::makeOriginalFreshBattleProfile(),remote=local;remote.setu(16,1);
    a.startMultiplayer(config,&local,&remote);a.enableAuthority(7654321,true,false);
    original::OnlineRaceSetup setup;setup.condition=0;setup.boost=false;setup.profiles={local,remote};setup.automatic={true,true};
    original::OnlineRaceSimulation peer(a.root,setup);
    std::vector<ReplayFrame> expectedOpponent;
    original::OnlineRaceLink link(peer,7654321,false,[&](const original::OnlineRaceFrame& frame,std::uint64_t){
        if(frame.start.runRules)expectedOpponent.push_back(frame.replayPoses[1]);
    });
    a.setMultiplayerGo(true);a.multiplayer.localName="LOCAL TEST";a.multiplayer.remoteName="OPPONENT TEST";
    DriverInput driving;driving.throttle=1;original::OriginalHostInputState peerState;
    const auto peerInput=original::adaptOriginalHostInput(peerState,{0,.65f,0,false,false},true,false,0);
    for(unsigned i=0;i<1000;++i){
        link.receive(a.authorityLink->packet());link.reconcile();a.receiveAuthority(link.packet());
        require(link.step(peerInput),"Peer stalled");a.simulate(driving);
    }
    link.receive(a.authorityLink->packet());link.reconcile();
    require(expectedOpponent.size()>=a.rivalRecording.frames.size(),"Peer did not confirm the recorded frames");
    for(std::size_t i=0;i<a.rivalRecording.frames.size();++i){
        const auto& frame=a.rivalRecording.frames[i];const auto& expected=expectedOpponent[i];
        require(frame.detail.rpm==expected.detail.rpm&&frame.gear==expected.gear,"Opponent RPM or gear not exact");
        require(frame.speed==expected.speed&&frame.detail.elapsed==expected.detail.elapsed,"Opponent speed or clock not exact");
    }
    require(a.recording.frames.size()>100&&a.rivalRecording.frames.size()==a.recording.frames.size(),"Authority capture omitted either car");
    a.validationMode=false;a.publishLocalReplay();require(!a.localReplayJson.empty()&&a.sharedFinishJson.empty(),"Online replay entered upload queue");write("online");
    auto pending=a.localReplayJson;a.publishLocalReplay();require(a.localReplayJson==pending,"Duplicate local publish");
    a.localReplayJson.clear();a.publishLocalReplay();require(a.localReplayJson.empty(),"Acknowledged replay reappeared");
    a.validationMode=true;a.leaveMultiplayer();a.menu=true;a.localReplayJson=pending;
    std::ofstream(out/"capture-report.json")<<"{\"passed\":true,\"checks\":"<<checks<<",\"networkUploads\":0}";return 0;
}
