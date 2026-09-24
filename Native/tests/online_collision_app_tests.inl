#include "online_race_test_driver.h"

// The production App start/leave/authority lifecycle, with a numerical second
// peer and delayed, duplicate packets. This does not emulate Steam or rendering.
int runOnlineCollisionAppTests(App& app){
    if(app.saveRoot.filename()!="userdata"||!fs::is_regular_file(app.saveRoot.parent_path()/"ISOLATED_MODE_FLOW_TEST.txt")||app.multiplayer.active)
        throw std::logic_error("Online collision fixtures require isolated offline userdata");
    std::ofstream log(app.saveRoot.parent_path()/"online-collision-app.log");unsigned checks=0;
    const auto require=[&](bool ok,const char* why){++checks;if(!ok){log<<"FAIL "<<why<<'\n';log.flush();throw std::runtime_error(why);}};
    app.validationMode=true;app.replayRecordingFlags=0;app.replayPlaybackActive=false;
    std::vector<std::uint8_t> previousPacket;
    std::array<std::uint64_t,2> firstEnabledDigest{};
    constexpr unsigned frames=900;
    for(unsigned local=0;local<2;++local)for(unsigned race=0;race<3;++race){
        const bool collisions=race!=1;
        const std::uint64_t raceId=800001+local*3+race;
        std::array<original::OriginalBattleProfile,2> profiles;
        for(unsigned slot=0;slot<2;++slot){profiles[slot]=original::makeOriginalFreshBattleProfile();profiles[slot].setu(16,slot);profiles[slot].setByte(164,5);}
        Idas3MultiplayerConfig config{sizeof(config),2,0,0,0,0,local,1-local,local,local==0?1u:0u};
        app.startMultiplayer(config,&profiles[local],&profiles[1-local]);
        app.enableAuthority(raceId,local!=0,false,collisions);
        const auto selected=app.authorityRace->setup();
        require(selected.collisions==collisions&&app.authorityRace->frame()==0&&app.authorityRace->contactFrames()==0,"Race setup retained a prior collision rule/frame");
        require(!app.authorityFinished[0]&&!app.authorityFinished[1]&&app.authorityConfirmedWinner==-3,"Race setup retained a prior finish gate");
        require(length(app.authorityVisualOffset[0])==0&&length(app.authorityVisualOffset[1])==0,"Race setup retained visual correction offsets");
        if(!previousPacket.empty()){
            const auto before=app.authorityRace->digest();bool rejected=false;
            try{app.receiveAuthority(previousPacket);}catch(const std::invalid_argument&){rejected=true;}
            require(rejected&&before==app.authorityRace->digest(),"Prior-race input packet changed fresh collision state");
        }
        original::OnlineRaceSimulation baseline(app.root,selected),peer(app.root,selected);
        std::vector<std::uint64_t> expectedDigests,expectedEffects;
        std::uint64_t peerEmitted=0,expectedCues=0,physicalResponses=0;
        std::array<std::uint64_t,2> contactCues{};
        original::OnlineRaceLink link(peer,raceId,local!=0,[&](const auto& frame,auto digest){
            require(frame.frame==peerEmitted,"Confirmed peer effects repeated or skipped");
            require(digest==expectedDigests.at(frame.frame)&&onlineContactEffectsDigest(frame)==expectedEffects.at(frame.frame),"Delayed peer collision state/effects differ from baseline");
            ++peerEmitted;
        });
        app.setMultiplayerGo(true);app.loadingActive=app.preRaceDialogueActive=app.vsActive=false;
        const auto cueStart=app.audio.oneShotStatistics().cues;
        const auto exchange=[&]{
            const auto a=app.authorityLink->packet(),b=link.packet();
            app.receiveAuthority(b);link.receive(a);link.reconcile();
            app.receiveAuthority(b);link.receive(a);link.reconcile();
        };
        std::array<original::OriginalHostInputState,2> adapters{};
        for(unsigned frame=0;frame<frames;++frame){
            std::array<original::OriginalHostControls,2> controls;
            std::array<original::OriginalVehicleInputs,2> inputs;
            for(unsigned slot=0;slot<2;++slot){controls[slot]=onlineRaceTestControls(baseline,slot,frame);inputs[slot]=original::adaptOriginalHostInput(adapters[slot],controls[slot],selected.automatic[slot],false,0);}
            const auto effects=baseline.step(inputs);expectedDigests.push_back(baseline.digest());expectedEffects.push_back(onlineContactEffectsDigest(effects));
            const auto& driving=effects.driving[local];
            expectedCues+=effects.start.cue2+effects.start.cue3+driving.feedback142460.size()+unsigned(driving.completion.requestCue4);
            for(const auto& command:effects.engine[local])if(command.target!=original::OriginalEngineCommandTarget::Continuous)++expectedCues;
            for(unsigned slot=0;slot<2;++slot)if(effects.driving[slot].bodyCollision.active)contactCues[slot]+=effects.driving[slot].feedback142460.size();
            if(driving.bodyCollision.active){
                const auto& drive=baseline.car(local).vehicle().drive;
                if(drive.u(0x15C)&&(drive.f(0x260)!=0||drive.f(0x264)!=0))++physicalResponses;
            }
            DriverInput input;input.steer=controls[local].steering;input.throttle=controls[local].throttle;input.brake=controls[local].brake;
            input.shiftDown=controls[local].shiftDown;input.shiftUp=controls[local].shiftUp;input.automatic=selected.automatic[local];
            app.simulate(input);require(!app.authorityStalled&&link.step(inputs[1-local]),"Bounded delayed link stalled");
            if(frame%8==7)exchange();
        }
        for(unsigned flush=0;flush<4;++flush)exchange();
        require(app.authorityRace->digest()==baseline.digest()&&peer.digest()==baseline.digest(),"App/peer collision state differs after packet drain");
        require(app.authorityLink->verifiedPeerFrames()==frames&&link.verifiedPeerFrames()==frames,"Race history was not mutually verified");
        require(app.authorityLink->timeline().metrics().confirmedEffects==frames&&peerEmitted==frames,"Contact effects were not confirmed exactly once");
        require(app.authorityLink->timeline().metrics().rollbacks>0&&link.timeline().metrics().rollbacks>0,"Delayed contact fixture never exercised rollback");
        require(app.audio.engineStatistics().simulationFrames==frames&&app.audio.oneShotStatistics().cues-cueStart==expectedCues,"App lost or repeated confirmed collision/audio cues");
        log<<"MEASURE local_slot="<<local<<" race="<<race<<" collisions="<<collisions<<" contacts="<<baseline.contactFrames()
            <<" responses="<<physicalResponses<<" contact_cues="<<contactCues[0]<<','<<contactCues[1]<<" audio_cues="<<expectedCues<<'\n';log.flush();
        // Original1578C4 gates sounds on the prior signed X+Z displacement
        // and its latch. Opposite responses need not cue both drivers on a
        // continuous contact. Require cue coverage across the pair, and the
        // exact source-expected local dispatch count above for both App roles.
        require(collisions?(baseline.contactFrames()>0&&physicalResponses>0&&contactCues[0]+contactCues[1]>0):baseline.contactFrames()==0,"Physical contact and collision cues do not match selected rule");
        if(race==0)firstEnabledDigest[local]=baseline.digest();
        if(race==2)require(baseline.digest()==firstEnabledDigest[local],"Re-enabled race differs from the first enabled race");
        previousPacket=link.packet();
        log<<"PASS local_slot="<<local<<" race="<<race<<" collisions="<<collisions<<" contacts="<<baseline.contactFrames()
            <<" responses="<<physicalResponses<<" contact_cues="<<contactCues[0]<<','<<contactCues[1]<<" audio_cues="<<expectedCues
            <<" rollbacks="<<app.authorityLink->timeline().metrics().rollbacks<<'\n';log.flush();
        app.leaveMultiplayer();
        require(!app.authorityRace&&!app.authorityLink&&!app.multiplayer.active,"Leaving race retained authority/collision state");
    }
    log<<"PASS "<<checks<<" checks; six native App races, both local slots, ON/OFF/ON, real driving inputs, delayed/duplicate packets, stale-race rejection, exact confirmed contact/audio effects. No Steam/Unity transport or rendering test.\n";
    return 0;
}
