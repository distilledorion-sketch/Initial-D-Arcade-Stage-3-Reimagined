#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include "../tests/online_race_test_driver.h"
#include <iostream>

int main(int argc,char** argv)try{
    if(argc!=3&&argc!=4)throw std::runtime_error("Native root, NEW evidence directory and optional imported-pack parent required");
    const auto root=fs::absolute(argv[1]),out=fs::absolute(argv[2]);
    if(fs::exists(out))throw std::runtime_error("Preserve prior evidence");fs::create_directories(out);
    auto app=std::make_unique<App>();app->root=root;app->saveRoot=out/"userdata";app->validationMode=true;app->settings();
    app->frontend.initialize(root,true);app->hud.loadOriginal(root);app->audio.configure(root);
    app->originalCamera=OriginalChaseCamera::load(root);app->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
    app->vsBanner.load(root);app->vsBannerLoaded=true;
    if(!app->renderer.initialize(nullptr,960,720,true))throw std::runtime_error("Offscreen setup failed");
    app->demoPreparationStarted=true;app->replayRecordingFlags=2;
    if(argc==4)for(unsigned id:{10u,16u,11u}){
        const auto& definition=importedCourseDefinition(id);auto& pack=app->importedRoot(id);
        pack=fs::absolute(argv[3])/definition.folder;
        if(!fs::is_regular_file(pack/"course.id"))throw std::runtime_error("Imported fixture unavailable");
        app->frontend.enableHakoneCourse(pack,int(id));
    }
    unsigned checks=0,rollbacks=0,poseChecks=0,predictedDifferences=0,finishCases=0,flashFrames=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);};
    const auto sameLamp=[](const OriginalHeadlightState& a,const OriginalHeadlightState& b){return a.counter==b.counter&&a.maximumPhase==b.maximumPhase&&a.phase==b.phase&&a.visible==b.visible&&a.fraction==b.fraction;};
    for(unsigned appearance=0;appearance<36;++appearance){
        auto profile=original::makeOriginalFreshBattleProfile();profile.setu(16,appearance==35?22:appearance);
        if(appearance==35)profile.setByte(156,3); // FD's mapped front2 disables the pop-up motor.
        auto presentation=CarPresentation::loadPlayerProfile(root,profile);
        for(unsigned tick=0;tick<100;++tick){const bool on=tick>2&&tick<54;const auto before=presentation.headlightState();
            const auto next=presentation.nextHeadlightState(on);check(sameLamp(before,presentation.headlightState()),"Lamp preview mutated its live owner");
            presentation.advanceOriginalFrame(on);check(sameLamp(next,presentation.headlightState()),"Showcase lamp preview disagrees with source presentation");}
    }
    for(unsigned scenario=0;scenario<(argc==4?6u:3u);++scenario)for(unsigned local=0;local<2;++local){
        const auto id=910001+scenario*2+local;
        const unsigned course=scenario<3?7:scenario==3?10:scenario==4?16:11;
        const bool imported=scenario>=3;
        Idas3MultiplayerConfig config{sizeof(config),2,course,scenario==1||scenario==4?1u:0u,scenario==2||scenario==3?1u:0u,scenario==1||scenario==5?1u:0u,local?19u:0u,local?0u:19u,local,1};
        auto player=original::makeOriginalFreshBattleProfile(),opponent=player;
        player.setu(16,config.localCar);opponent.setu(16,config.remoteCar);
        for(auto* profile:{&player,&opponent})if(profile->u(16)==19){profile->setByte(164,63);profile->setByte(162,std::uint8_t(1+scenario%3));profile->setByte(166,1);}
        app->startMultiplayer(config,&player,&opponent);
        // Exercise the real showcase-control caller as well as ordinary ticks.
        if(scenario==2)app->validationMode=false;
        app->enableAuthority(id,true,false,true);app->validationMode=true;
        app->setMultiplayerGo(true);app->loadingActive=app->preRaceDialogueActive=app->vsActive=false;
        const auto selected=app->authorityRace->setup();
        check(bool(selected.imported)==imported&&(!imported||selected.imported->id==course),"Wrong imported recording fixture");
        check(selected.showcaseFrames==(scenario==2?241u:0u),"Showcase prefix fixture was not enabled");
        original::OnlineRaceSimulation baseline(root,selected),peer(root,selected);
        original::OnlineRaceLink link(peer,id,local!=0);
        std::array<original::OriginalHostInputState,2> adapters{};
        struct Expected {original::OnlineRaceFrame frame;std::array<OnlineReplayRecording::Lamps,2> lamps;std::uint64_t smoke;std::array<Vec3,2> body;std::array<int,2> flash;};
        std::vector<Expected> expected;std::array<OriginalCarBodyPosition,2> bodies;
        std::array<EngineAudio,2> effectReference;
        for(unsigned slot=0;slot<2;++slot){effectReference[slot].configure(root);effectReference[slot].selectOriginalEngine(root,selected.profiles[slot]);}
        for(unsigned slot=0;slot<2;++slot){const auto& car=baseline.car(slot);const auto& d=car.vehicle().drive;
            bodies[slot].update(car.collision(),selected.profiles[slot].u(16),{d.f(0),d.f(4),d.f(8)});}
        const auto verify=[&](){
            const auto confirmed=app->authorityLink->timeline().confirmed();
            check(app->recording.smokeFrames.size()==(imported?0:confirmed),"Recording contains speculative or missing source effects");
            std::size_t pose=0;
            for(unsigned tick=0;tick<confirmed;++tick){const auto& e=expected.at(tick);
                if(!imported)check(std::memcmp(&app->recording.smokeFrames[tick],&e.frame.smokeInputs[local],sizeof(ReplayCarSmokeFrame))==0,"Saved smoke retained predicted contacts");
                else check(!e.frame.smokeAvailable,"Imported race fabricated original smoke input");
                if(!e.frame.start.runRules)continue;
                check(pose<app->recording.frames.size()&&pose<app->rivalRecording.frames.size(),"Confirmed pose missing");
                for(unsigned slot=0;slot<2;++slot){const auto& actual=(slot==local?app->recording:app->rivalRecording).frames[pose];const auto& wanted=e.frame.replayPoses[slot];
                    check(actual.tick==pose+1&&std::memcmp(&actual.position,&wanted.position,sizeof(Vec3))==0&&actual.yaw==wanted.yaw&&actual.speed==wanted.speed&&actual.gear==wanted.gear,"Saved pose contains visual correction or speculative motion");
                    check(actual.detail.rpm==wanted.detail.rpm&&actual.detail.steering==wanted.detail.steering&&actual.detail.suspension==wanted.detail.suspension&&actual.detail.rotation==wanted.detail.rotation,"Saved pose telemetry is from another tick");
                    if(slot==local){
                        check(actual.telemetryAvailable,"Confirmed analysis missing");
                        check(std::memcmp(&actual.telemetry,&wanted.telemetry,7*sizeof(float))==0,"Analysis retained predicted physics measurements");
                        check((actual.telemetry.flags&7u)==(wanted.telemetry.flags&7u),"Shift edges or wall flags are from another frame");
                        check((actual.telemetry.flags>>5)==(tick<500?1u:4u),"Delayed confirmation consumed a later steering source");
                    }
                    check(actual.backfire==e.flash[slot],"Recorded flash used predicted, later or wrong-player cues");
                    check(actual.detail.elapsed==wanted.detail.elapsed&&actual.detail.sections==wanted.detail.sections,"Saved clocks are from another tick");
                    if(course==16&&slot==local){
                        check(actual.detail.bodyPosition.x==e.body[slot].x&&actual.detail.bodyPosition.z==e.body[slot].z&&
                            std::isfinite(actual.detail.bodyPosition.y)&&std::isfinite(actual.detail.pitch)&&std::isfinite(actual.detail.roll),"Imported body presentation corrupted the saved pose");
                    }else check(std::memcmp(&actual.detail.bodyPosition,&e.body[slot],sizeof(Vec3))==0,"Saved body placement is not canonical");
                    const auto& lamps=e.lamps[slot];
                    if(actual.detail.lights!=unsigned(lamps.enabled)||actual.detail.lightCounter!=lamps.state.counter||actual.detail.lightPhase!=lamps.state.phase||actual.detail.lightFraction!=lamps.state.fraction)
                        std::cerr<<"Lamp mismatch scenario="<<scenario<<" local="<<local<<" source="<<tick<<" slot="<<slot<<" actual="<<actual.detail.lights<<','<<actual.detail.lightCounter<<','<<actual.detail.lightPhase<<','<<actual.detail.lightFraction<<" expected="<<lamps.enabled<<','<<lamps.state.counter<<','<<lamps.state.phase<<','<<lamps.state.fraction<<'\n';
                    check(actual.detail.lights==unsigned(lamps.enabled)&&actual.detail.lightCounter==lamps.state.counter&&actual.detail.lightPhase==lamps.state.phase&&actual.detail.lightFraction==lamps.state.fraction,"Delayed confirmation consumed a later lamp state");
                }
                check(app->recording.frames[pose].dustStep==(imported?0:tick+1),"Saved pose and effects have different clocks");++pose; ++poseChecks;
            }
            if(pose){auto telemetry=app->recording.telemetryBytes();
                check(telemetry.size()==16+pose*32,"Confirmed analysis stream missing or misaligned");
                const auto& ticks=app->authorityReplay.steeringTicks();
                check(ticks[0]==0&&ticks[1]+ticks[4]==pose,"Steering counts include showcase or speculative frames");}
            check(app->recording.frames.size()==pose&&app->rivalRecording.frames.size()==pose,"Recording contains predicted/duplicate poses");
        };
        std::vector<std::uint8_t> staleLocal,staleRemote;
        const auto exchange=[&](){for(unsigned settle=0;settle<6;++settle){
                auto a=app->authorityLink->packet(),b=link.packet();app->receiveAuthority(b);link.receive(a);
                app->receiveAuthority(b);link.receive(a); // Duplicate delivery cannot duplicate saved ticks.
                if(!staleLocal.empty()){app->receiveAuthority(staleRemote);link.receive(staleLocal);} // Reordered earlier packets.
                link.reconcile();staleLocal=std::move(a);staleRemote=std::move(b);
            }verify();};
        for(unsigned tick=0;tick<784;++tick){
            std::array<original::OriginalHostControls,2> controls;std::array<original::OriginalVehicleInputs,2> inputs;
            for(unsigned slot=0;slot<2;++slot){controls[slot]=onlineRaceTestControls(baseline,slot,tick);inputs[slot]=original::adaptOriginalHostInput(adapters[slot],controls[slot],true,false,0);}
            Expected e;e.frame=baseline.step(inputs);e.smoke=imported?0:baseline.smoke(local).digest();
            for(unsigned slot=0;slot<2;++slot){
                effectReference[slot].applyConfirmedOnlineAudio(e.frame.engine[slot],e.frame.tires[slot]);
                e.flash[slot]=effectReference[slot].backfireFrame();flashFrames+=e.frame.start.runRules&&e.flash[slot]>=0;
                const auto& p=e.frame.replayPoses[slot].position;
                e.body[slot]=bodies[slot].update(baseline.car(slot).collision(),selected.profiles[slot].u(16),p);
                if(course==10&&slot==local){const auto& q=bodies[slot].query();
                    e.body[slot]=Vec3{p.x,q.f(16),p.z}+Vec3{q.f(0),q.f(4),q.f(8)}*originalCarRideHeight(selected.profiles[slot].u(16));}
            }
            if(tick%23==0){const auto& a=app->presentedSession().actor();
                app->playerProjectedHeadlight.request(!app->playerProjectedHeadlight.enabled(),app->presentedSession().collision(),app->playerBody.query(),{a.f(0),a.f(4),a.f(8)});}
            app->setAuthorityRemoteHeadlights(tick+1,(tick/31)%2);
            DriverInput d;d.steer=controls[local].steering;d.throttle=controls[local].throttle;d.brake=controls[local].brake;d.shiftDown=controls[local].shiftDown;d.shiftUp=controls[local].shiftUp;d.automatic=true;
            app->input.steeringSource=tick<500?1:4;
            if(tick<selected.showcaseFrames)check(app->advanceShowcaseControls(d),"Showcase stalled unexpectedly");else app->simulate(d);
            check(!app->authorityStalled&&link.step(inputs[1-local]),"Unexpected early stall");
            predictedDifferences+=std::memcmp(&app->vehicle.position,&e.frame.replayPoses[local].position,sizeof(Vec3))!=0;
            e.lamps[local]={app->carPresentation.headlightState(),app->playerProjectedHeadlight.enabled()};
            e.lamps[1-local]={app->rivalPresentation.headlightState(),app->rivalProjectedHeadlight.enabled()};expected.push_back(e);
            if(tick<720&&(tick%17==16||tick==719))exchange();
        }
        const auto savedCount=app->recording.frames.size(),savedSmoke=app->recording.smokeFrames.size();
        DriverInput held;held.throttle=1;held.automatic=true;
        for(unsigned attempt=0;attempt<8;++attempt){app->simulate(held);check(app->authorityStalled,"Full prediction window did not stall");}
        check(app->recording.frames.size()==savedCount&&app->recording.smokeFrames.size()==savedSmoke,"Stall recorded additional frames");
        exchange();check(app->authorityLink->verifiedPeerFrames()==784&&link.verifiedPeerFrames()==784,"Recovery did not verify all history");
        check(app->authorityRace->digest()==baseline.digest()&&peer.digest()==baseline.digest(),"Recorder changed numerical race state");
        check(app->authorityLink->timeline().metrics().rollbacks>0,"Fixture never corrected predicted input");
        rollbacks+=unsigned(app->authorityLink->timeline().metrics().rollbacks);
        const auto bytes=app->recording.sharedBytes(unsigned(app->recording.frames.back().tick*100));
        const auto word=[&](unsigned offset){std::uint32_t value;std::memcpy(&value,bytes.data()+offset,4);return value;};
        check(bytes.size()>100&&word(24)==4&&word(16)==100&&word(20)==168&&word(96)==(imported?0u:136u)&&word(92)==(imported?0u:784u),"Online recording serialized the wrong source timeline format");
        if(!imported){CarSmokeState smoke;ReplayCarSmokePlayback playback;playback.load(std::span(bytes).subspan(word(16)+word(20)*word(8)),smoke);
            for(unsigned tick:{783u,0u,300u,720u,10u,783u}){playback.seek(tick+1,smoke);check(smoke.digest()==expected[tick].smoke,"Corrected online recording cannot seek exact source smoke");}}
        else check(bytes.size()==100+168*word(8),"Imported recording has a fabricated effects payload");
        for(unsigned i=0;i<word(8);++i)check(word(100+i*168+164)==unsigned(app->recording.frames[i].backfire+1),"Serialized online flash changed");
        const auto stem="scenario-"+std::to_string(scenario)+"-slot-"+std::to_string(local);
        std::ofstream file(out/(stem+".idr2"),std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());check(bool(file),"Could not save evidence replay");
        app->validationMode=false;
        if(scenario<2){
            // Isolated source-rule boundary, applied identically to both peers.
            // This checks actual confirmed finish publication, not a full lap.
            for(auto* sim:{app->authorityRace.get(),&peer}){
                if(scenario==0){auto& rules=const_cast<original::OriginalRaceRules&>(sim->rules(0));auto state=rules.state();
                    state.progress.index=rules.rules().goalIndex;rules.restoreNumericalState(state);}
                else for(unsigned slot=0;slot<2;++slot)check(const_cast<original::OriginalRaceRules&>(sim->rules(slot)).retire(),"Could not seed timeout result");
            }
            const auto before=app->recording.frames.size();
            original::OriginalHostInputState remoteInput;const auto controls=original::adaptOriginalHostInput(remoteInput,{0,1,0},true,false,0);
            app->simulate(held);check(link.step(controls),"Finish peer step failed");
            for(unsigned settle=0;settle<6;++settle){auto a=app->authorityLink->packet(),b=link.packet();app->receiveAuthority(b);link.receive(a);link.reconcile();}
            check(app->archivePublished&&!app->authorityReplay.active()&&app->recording.frames.size()==before+1,"Confirmed finish did not close recording on its own frame");
            check(app->localReplayJson.find(scenario==0?"\"outcome\":0":"\"outcome\":1")!=std::string::npos,"Confirmed result metadata used stale presentation state");
            check(scenario!=0||(local==0?(app->recording.finishTicks6000!=0&&app->rivalRecording.finishTicks6000==0):
                (app->recording.finishTicks6000==0&&app->rivalRecording.finishTicks6000!=0)),"Loser received a fictional finish time");
            const auto ended=app->recording.frames.size(),sourceEnded=app->recording.smokeFrames.size();
            for(unsigned tick=0;tick<3;++tick){app->simulate(held);check(link.step(controls),"Post-finish peer step failed");
                for(unsigned settle=0;settle<3;++settle){auto a=app->authorityLink->packet(),b=link.packet();app->receiveAuthority(b);link.receive(a);link.reconcile();}}
            check(app->recording.frames.size()==ended&&app->recording.smokeFrames.size()==sourceEnded,"Post-finish callbacks extended saved replay");++finishCases;
        }else{
            app->publishLocalReplay();check(app->localReplayJson.find("\"outcome\":2")!=std::string::npos,"Partial online recording published a finish");
        }
        app->validationMode=true;
        const auto finalBytes=app->recording.sharedBytes(unsigned(app->recording.frames.back().tick*100));
        check(app->archivePublished&&!app->localReplayRival.empty()&&app->localReplayPlayer==finalBytes,"App archive did not preserve corrected paired recording");
        // The managed library normally adds the encoded opponent byte count.
        auto archiveJson=app->localReplayJson;archiveJson.pop_back();archiveJson+=",\"opponentBytes\":"+std::to_string(app->localReplayRival.size())+",\"telemetryBytes\":"+std::to_string(app->localReplayTelemetry.size())+"}";
        std::ofstream archive(out/(stem+".idreplay"),std::ios::binary);const auto size=std::uint32_t(archiveJson.size());
        for(unsigned i=0;i<4;++i)archive.put(char((size>>(8*i))&255));archive<<archiveJson;
        for(const auto* part:{&app->localReplayPlayer,&app->localReplayRival,&app->localReplayTelemetry})archive.write(reinterpret_cast<const char*>(part->data()),part->size());
        check(bool(archive),"Could not save paired archive evidence");
        app->leaveMultiplayer();check(!app->authorityRace&&!app->authorityReplay.active(),"Recorder survived room teardown");
        std::cout<<"PASS "<<stem<<" source_ticks="<<(imported?0:784)<<" pose_frames="<<word(8)<<" showcase="<<selected.showcaseFrames<<" course="<<course<<'\n'<<std::flush;
    }
    check(predictedDifferences>0&&finishCases==4&&flashFrames>0,"Missing prediction/finish/backfire coverage");
    std::cout<<"PASS checks="<<checks<<" pose_checks="<<poseChecks<<" rollbacks="<<rollbacks<<" predicted_pose_differences="<<predictedDifferences<<" finish_cases="<<finishCases<<" flash_frames="<<flashFrames<<"; real App confirmed replay, both slots, day/night/wet, showcase prefix, lamp and accepted flash history, stall/recovery, source seeking, paired archive and cleanup.\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
