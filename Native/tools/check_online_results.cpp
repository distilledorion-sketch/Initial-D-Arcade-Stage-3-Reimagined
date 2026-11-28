#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include <iostream>

int main(int argc,char** argv)try{
    if(argc!=3&&argc!=4)throw std::runtime_error("Native root, new evidence directory and optional arcade language required");
    const auto root=fs::absolute(argv[1]),out=fs::absolute(argv[2]);
    if(fs::exists(out))throw std::runtime_error("Preserve existing evidence");fs::create_directories(out);
    setOriginalUiLanguage(argc==4?std::stoi(argv[3]):0);
    unsigned checks=0;auto check=[&](bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);};
    for(unsigned fps:{30,60,120,144,240}){
        OnlineResultScreen screen;screen.begin(2000,14000);OriginalDrivingClock clock;
        unsigned ticks=0,transition=0;
        for(unsigned i=0;i<fps*11&&!transition;++i)clock.advance(1./fps,[&]{
            if(screen.page!=0)return;
            ++ticks;screen.input(false,false,0,0);screen.advance(false);
            if(screen.page==1)transition=ticks;
        });
        check(transition==610&&screen.frame==0,"Linked finish duration/input depends on host cadence");
        FixedClock points;
        for(unsigned i=0;i<fps*2;++i)points.advance(1./fps,[&]{screen.advance(false);});
        check(screen.page==1&&screen.points.displayedBalance==14000&&screen.frame==120,"Points UI clock changed");
    }
    {
        for(bool lost:{false,true}){
            OnlineResultScreen screen;screen.begin(1000,9000,true,lost);
            for(unsigned i=0;i<130;++i)screen.advance(false);
            screen.input(true,false,0,16);screen.advance(false);
            for(unsigned i=0;i<10;++i)screen.advance(false);
            check(screen.page==0&&screen.finish.done(),"Skip bypassed pending result save");
            screen.input(false,false,0,0);screen.advance(false);
            check(screen.page==1&&!screen.frame&&!screen.decision,"Skip did not reach points after save");
        }
        OnlineResultScreen screen;screen.begin(0,9000);screen.page=2;
        screen.input(true,false,1,0);screen.input(false,false,0,1);screen.advance(true);
        check(!screen.confirming&&screen.countdown==879,"Focus loss retained a queued confirmation");
        screen.input(false,false,0,0);
        for(unsigned i=0;i<879;++i)screen.advance(true);
        check(screen.selected==1&&screen.confirming&&!screen.decision,"Original countdown did not choose No");
        for(unsigned i=0;i<41;++i)screen.advance(true);
        check(screen.decision==2,"Timed-out Continue did not finish its confirmation dwell");
        screen.begin(0,9000);screen.page=2;screen.input(true,false,-1,2|8);screen.advance(true);
        check(screen.selected==1&&screen.confirming&&screen.recordFailed,"Departed-peer confirmation attempted a rematch");
        for(unsigned i=0;i<41;++i)screen.advance(true);
        check(screen.decision==2,"Departed peer did not return to Mode");
    }
    auto app=std::make_unique<App>();app->root=root;app->saveRoot=out/"userdata";app->validationMode=true;app->settings();
    app->frontend.initialize(root,true);app->hud.loadOriginal(root);app->audio.configure(root);
    check(app->renderer.initializeSceneCapture(640,480),"Menu capture renderer");
    app->demoPreparationStarted=true;
    app->originalCamera=OriginalChaseCamera::load(root);app->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
    app->saveSlots=LocalSaveSlots(app->saveRoot/"saves");app->activeSaveSlot=0;
    app->profiles=LocalDriverProfiles(app->saveSlots.profileDirectory(0));
    auto offline=original::makeOriginalFreshBattleProfile();offline.setu(16,0);offline.setu(72,7890);
    auto target=offline;target.setu(16,8);target.setu(72,12000);
    check(app->saveSlots.adopt(0,offline)&&app->saveSlots.adopt(1,target),"Fixture slots");
    check(app->profiles.save(0,offline),"Offline fixture");
    LocalDriverProfiles rewards(app->saveSlots.profileDirectory(1));check(rewards.save(8,target),"Online fixture");
    for(unsigned slot=0;slot<2;++slot)for(int winner:{0,1,2,-1}){
        app->frontend.car=0;app->loadedProfileCar=0;app->frontend.battleProfile=offline;app->battleProfile=offline;
        const auto prior=rewards.load(8).profile;
        Idas3MultiplayerConfig config{sizeof(config),1,0,0,0,0,8,0,slot,1};
        app->startMultiplayer(config,&prior,&offline);
        app->multiplayer.rewardSelection=onlineSlotCarSelection(1,8);
        bool rejected=false;try{app->setMultiplayerResult(winner);}catch(const std::logic_error&){rejected=true;}
        check(rejected,"Unfinished race awarded points");
        app->race.phase=RacePhase::Finished;app->setMultiplayerResult(winner);
        const auto earned=rewards.load(8).profile;auto expected=prior;
        const unsigned amount=winner<0?0:1000u+(winner==int(slot)?1000u:0u);expected.setu(72,prior.u(72)+amount);
        check(earned.words==expected.words&&app->multiplayer.pointsEarned==amount,"Award changed fields besides selected-car balance");
        check(app->profiles.load(0).profile.words==offline.words,"Award touched another save/car");
        app->setMultiplayerResult(winner);check(rewards.load(8).profile.words==earned.words,"Duplicate result awarded twice");
        check(app->onlineResult.page==0,"Settled result did not start original finish presentation");
        app->onlineResult.input(false,false,0,1);app->advanceHostClock(.1,[]{});app->advanceOnlineResult(.1);
        check(app->onlineResult.frame==0,"Blocked result input advanced presentation");
        app->clock.reset();app->onlineResult.input(true,false,0,0);
        app->advanceOnlineResult(.1);check(app->onlineResult.frame==0,"Rendering advanced source finish timing");
        app->advanceHostClock(OriginalDrivingClock::step,[]{});
        check(app->onlineResult.page==0&&app->onlineResult.frame==1,"Driving confirmation skipped the readiness update");
        for(unsigned tick=1;tick<609;++tick)app->advanceHostClock(OriginalDrivingClock::step,[]{});
        check(app->onlineResult.page==0&&app->onlineResult.finish.done()&&!app->onlineResult.finish.blank&&
            !app->onlineResultPreview,"Exit End did not unblank before the next dispatch");
        app->advanceHostClock(OriginalDrivingClock::step,[]{});
        check(app->onlineResult.page==1&&app->onlineResultPagePending&&!app->onlineResultPreview,"Source callback materialized preview assets");
        app->advanceOnlineResult(.1);
        check(app->onlineResult.page==1&&app->onlineResultPreview&&app->onlineResult.frame==0,"EOS result handoff consumed old host elapsed time");
        check(app->onlineResultPoints.profileMode==2&&app->onlineResultPoints.points[3]==amount,
            "Online points were not supplied to the original battle result panel");
        check(app->onlineResultPoints.resultStatus==(winner<0?2:winner==int(slot)?0:1)&&
            app->onlineResultPoints.draw==(winner==2),"Online result artwork outcome is incorrect");
        for(unsigned tick=0;tick<94;++tick)app->advanceOnlineResult(1./60.);
        check(app->onlineResultPoints.points[4]==earned.u(72),"Original point count did not reach saved balance");
        app->setMultiplayerResult(winner);
        check(app->onlineResult.page==1&&app->onlineResultPoints.points[4]==earned.u(72),"Duplicate result restarted original screens");
        app->onlineResult.input(true,false,0,0);
        for(unsigned tick=0;tick<40;++tick)app->advanceOnlineResult(1./60.);
        check(app->onlineResult.page==2&&app->onlineContinueArtwork.loaded(),"Original Continue artwork was not opened");
        if(slot==0&&winner==0){
            const auto savedScreen=app->onlineResult;
            const auto savedProfile=app->frontend.battleProfile.words;
            const int arcadeLanguage=originalUiLanguage();
            for(unsigned message=0;message<3;++message){
                app->onlineResult=savedScreen;
                app->onlineResult.peerLeft=message==0;app->onlineResult.peerReady=message==1;
                app->onlineResult.decision=message==2?1:0;
                std::vector<std::uint32_t> english;
                for(int language=0;language<3;++language){
                    setCustomMenuLanguage(language);check(app->renderOnlineResult(),"Localized Continue render failed");
                    const auto& pixels=app->onlineResultPixels;
                    if(!language)english=pixels;
                    else{
                        unsigned changed=0;
                        for(unsigned i=0;i<pixels.size();++i)if(pixels[i]!=english[i]){
                            ++changed;const auto x=i%640,y=i/640;
                            check(x>=23&&x<617&&y>=431,"Custom language changed arcade artwork or clipped a caption");
                        }
                        check(changed>50,"Online status caption did not switch language");
                    }
                    check(originalUiLanguage()==arcadeLanguage&&app->frontend.battleProfile.words==savedProfile,
                        "Online caption language changed artwork setting or driver profile");
                    std::ofstream image(out/("continue-"+std::to_string(message)+"-language-"+std::to_string(language)+".ppm"),std::ios::binary);
                    image<<"P6\n640 480\n255\n";
                    for(auto pixel:pixels){const char rgb[]{char(pixel>>16),char(pixel>>8),char(pixel)};image.write(rgb,3);}
                    check(bool(image),"Could not write online caption evidence");
                }
            }
            setCustomMenuLanguage(0);app->onlineResult=savedScreen;
        }
        for(unsigned selected:{0,1}){
            unsigned labels=0;
            for(int y=0;y<480;y+=8)for(int x=0;x<640;x+=8){
                const auto hit=app->onlineContinueArtwork.hitContinue(float(x),float(y),selected);
                if(hit>=0)labels|=1u<<hit;
            }
            check(labels==3&&app->onlineContinueArtwork.hitContinue(-100,-100,selected)==-1,
                "Authored Continue labels do not supply both pointer targets");
        }
        const auto countdown=app->onlineResult.countdown;
        app->onlineResult.input(false,false,1,1);app->advanceOnlineResult(.1);
        check(app->onlineResult.countdown==countdown&&app->onlineResult.selected==0,"Blocked Continue changed selection or timer");
        app->onlineResult.input(true,false,-1,0);app->advanceOnlineResult(1./60.);
        check(app->onlineResult.confirming&&!app->onlineResult.decision,"Continue skipped its original confirmation dwell");
        for(unsigned tick=0;tick<41;++tick)app->advanceOnlineResult(1./60.);
        check(app->onlineResult.decision==1&&app->multiplayer.active,"Yes must await the managed peer handshake");
        app->onlineResult.input(false,true,0,0);app->advanceOnlineResult(1./60.);
        check(app->onlineResult.decision==2,"No while waiting did not request disconnection");
        check(rewards.load(8).profile.words==earned.words,"Original result presentation changed awarded save/tuning");
        rejected=false;try{app->setMultiplayerResult(winner==0?1:0);}catch(const std::logic_error&){rejected=true;}
        check(rejected,"Conflicting duplicate result accepted");
        app->leaveMultiplayer();
        check(app->onlineResult.page==-1&&!app->onlineResultPreview,"Original post-race owner survived mode return");
        check(app->menu&&app->frontend.stage==FrontendStage::Mode&&!app->multiplayer.active,"Post-race exit must return to mode selection");
        auto restored=app->frontend.battleProfile;restored.setu(1176,offline.u(1176));
        check(restored.words==offline.words,"Exit replaced offline driver's profile beyond initializing its menu timer");
        const auto lobbyProfile=app->frontend.battleProfile.words;
        const auto songs=app->audio.selectionStatistics().songsStarted;
        app->setHostDrivingControlsBlocked(true);
        unsigned audible=0;
        for(unsigned frame=0;frame<32;++frame){
            app->commands(1./60.);check(app->render(1./60.),"Returned lobby render");app->updateAudioScene();
            for(unsigned sample=0;sample<735;++sample){const auto pcm=app->audio.renderStereo(0,0,0,0,false);audible+=pcm[0]!=0||pcm[1]!=0;}
        }
        check(app->renderer.sceneCapture()->frame().screenFadeArgb==0,"Returned lobby retained Mode's black entry fade");
        check(app->audio.selectionPlaying()&&app->audio.selectionStatistics().cue==unsigned(original::OriginalSelectionMusicCue::Select)&&
            app->audio.selectionStatistics().songsStarted==songs+1&&audible>0,"Returned lobby did not start its original SELECT score exactly once");
        check(app->frontend.battleProfile.words==lobbyProfile&&app->frontend.stage==FrontendStage::Mode,
            "Returned lobby presentation changed the saved car or selected a game mode");
        app->setHostDrivingControlsBlocked(false);
    }
    // The current offline car also retains its award on restore.
    app->frontend.car=0;app->loadedProfileCar=0;app->frontend.battleProfile=offline;
    Idas3MultiplayerConfig config{sizeof(config),1,0,0,0,0,0,8,0,1};app->startMultiplayer(config,&offline,&target);
    app->multiplayer.rewardSelection=onlineSlotCarSelection(0,0);app->race.phase=RacePhase::Finished;app->setMultiplayerResult(0);
    app->leaveMultiplayer();check(app->frontend.battleProfile.u(72)==9890&&app->profiles.load(0).profile.u(72)==9890,"Current car award was undone on return");
    // A rematch keeps the online menu above Mode. Its first render and a long
    // lobby wait must not invalidate the full profile already read by C#.
    original::OriginalBattleProfile lobbyCar,afterWait;
    check(app->onlineCarProfile(0,lobbyCar)==1,"Returned active save is missing");
    app->demoPreparationStarted=true;app->setHostDrivingControlsBlocked(true);
    const auto menuSamples=app->audio.selectionSamplePosition();unsigned lobbyAudible=0;
    for(unsigned frame=0;frame<160;++frame){
        check(app->render(.25),"Covered mode render");app->updateAudioScene();
        for(unsigned sample=0;sample<11025;++sample){const auto pcm=app->audio.renderStereo(0,0,0,0,false);lobbyAudible+=pcm[0]!=0||pcm[1]!=0;}
    }
    check(app->renderer.sceneCapture()->frame().screenFadeArgb==0,"Covered Mode remained dark after forty seconds");
    check(app->audio.selectionPlaying()&&app->audio.selectionStatistics().cue==unsigned(original::OriginalSelectionMusicCue::Select)&&
        app->audio.selectionSamplePosition()>menuSamples&&lobbyAudible>0,"Covered Mode stopped its menu music");
    check(app->onlineCarProfile(0,afterWait)==1&&afterWait.words==lobbyCar.words,
        "Covered Mode changed the saved-car snapshot before rematch");
    check(app->frontend.stage==FrontendStage::Mode,"Covered Mode auto-selected a game mode");
    app->setHostDrivingControlsBlocked(false);
    for(unsigned frame=0;frame<4;++frame)check(app->render(.25),"Uncovered mode render");
    check(app->frontend.battleProfile.u(1176)<afterWait.u(1176),"Mode did not resume after closing overlay");
    offline=app->frontend.battleProfile;app->startMultiplayer(config,&offline,&target);app->multiplayer.rewardSelection=onlineSlotCarSelection(0,0);
    app->disconnectMultiplayer();bool rejected=false;try{app->setMultiplayerResult(0);}catch(const std::logic_error&){rejected=true;}
    check(rejected&&app->profiles.load(0).profile.u(72)==9890,"Disconnected race awarded points");app->leaveMultiplayer();
    auto peer=std::make_unique<App>();peer->root=root;peer->saveRoot=out/"peer-userdata";peer->validationMode=true;peer->settings();
    peer->frontend.initialize(root,true);peer->hud.loadOriginal(root);peer->audio.configure(root);
    peer->originalCamera=OriginalChaseCamera::load(root);peer->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
    for(unsigned winner=0;winner<2;++winner){
        config.version=2;config.localSlot=0;app->startMultiplayer(config,&offline,&target);app->enableAuthority(909+winner,true,false);
        auto guest=config;guest.localCar=config.remoteCar;guest.remoteCar=config.localCar;guest.localSlot=1;
        peer->startMultiplayer(guest,&target,&offline);peer->enableAuthority(909+winner,true,false);
        for(auto* owner:{app.get(),peer.get()}){
            // Private, same-state terminal boundary on both numerical peers.
            // The real wire confirmation and App result guard still run.
            auto& rules=const_cast<original::OriginalRaceRules&>(owner->authorityRace->rules(winner));
            auto state=rules.state();state.phase=original::OriginalRacePhase::Finished;state.times.finishTime=120004;
            rules.restoreNumericalState(state);
            original::OriginalHostInputState input;const auto idle=original::adaptOriginalHostInput(input,{},true,false,0);
            check(owner->authorityLink->step(idle),"Authority boundary step");
            check(owner->authorityWinner()==-3,"Unverified crossing exposed a winner");
        }
        for(unsigned exchange=0;exchange<3;++exchange){
            const auto hostPacket=app->authorityLink->packet(),guestPacket=peer->authorityLink->packet();
            app->receiveAuthority(guestPacket);peer->receiveAuthority(hostPacket);
        }
        for(auto* owner:{app.get(),peer.get()}){
            check(owner->authorityWinner()==int(winner),"First finisher did not settle through verified wire history");
            check(owner->authorityFinishTicks[winner]==1200&&owner->authorityFinishTicks[1-winner]==0,"Trailing car acquired a made-up finish time");
            rejected=false;try{owner->setMultiplayerResult(1-int(winner));}catch(const std::logic_error&){rejected=true;}
            check(rejected,"Unverified winner overrode the shared result");
            owner->setMultiplayerResult(int(winner));check(owner->race.phase==RacePhase::Finished,"Verified result did not end both local presentations");
        }
        // Real App host/guest path after a verified terminal boundary. Only
        // boundary seeding above is synthetic; source clock, driver mapper,
        // authority wire/ACKs, camera presenter and result owner are actual.
        const auto exchange=[&]{
            const auto host=app->authorityLink->packet(),guest=peer->authorityLink->packet();
            app->receiveAuthority(guest);peer->receiveAuthority(host);
        };
        const auto cameraKey=[](const App& a){
            return std::array{a.originalCamera.frame().cameraWorld.elements,
                a.bumperCamera.frame().cameraWorld.elements,
                a.previousCameraFrame.cameraWorld.elements,a.previousBumperFrame.cameraWorld.elements};
        };
        for(auto* owner:{app.get(),peer.get()}){
            owner->setMultiplayerGo(true);owner->vsActive=false;owner->active=true;
            owner->clock.reset();owner->setHostDrivingControlsBlocked(false);
            owner->input={};owner->input.connected=true;
            owner->input.pad.Gamepad.sThumbLX=12000;owner->input.pad.Gamepad.bRightTrigger=65;
            owner->input.pad.Gamepad.bLeftTrigger=32;
            owner->input.originalThrottleAnalog=16578;owner->input.originalBrakeAnalog=8191;
            // This fixture checks the dedicated wheel's full-resolution ADCs.
            owner->controllerResponse=original::ControllerResponse::FlycastWheel;
            owner->input.pad.Gamepad.wButtons=XINPUT_GAMEPAD_B;
            owner->steeringSmoothing.setAmount(0);
            const auto oldView=owner->drivingView;
            owner->input.pressed['C']=true;owner->input.pressedButtons=XINPUT_GAMEPAD_Y;
            owner->commands(0);check(owner->drivingView==oldView,"Finish accepted GO-only camera toggle");
            for(auto phase:{RacePhase::Countdown,RacePhase::Running}){
                owner->race.phase=phase;const auto before=owner->drivingView;
                owner->commands(0);check(owner->drivingView!=before,"Nonfinish camera toggle was removed");
            }
            owner->race.phase=RacePhase::Finished;owner->setDrivingView(oldView);
            owner->input.pressed={};owner->input.pressedButtons=0;
            // An actual modal freezes both counter and camera. Simulation
            // may still process neutral peer inputs; its world is not paused.
            owner->onlineResult.input(true,true,0,1);const auto camera=cameraKey(*owner);
            owner->advanceHostClock(OriginalDrivingClock::step,[&]{
                const auto d=owner->driver();check(d.throttle==0&&d.brake==0&&!d.shiftUp,"Modal passed driving to authority");
                owner->simulate(d);
            });
            check(owner->onlineResult.frame==0&&cameraKey(*owner)==camera,"Blocked authority presenter advanced finish camera/counter");
            owner->onlineResult.input(false,false,0,0);owner->clock.reset();
        }
        exchange();exchange();
        std::array<unsigned,2> attempts{},accepted{},blankCalls{};
        std::array<std::uint64_t,2> stallFrame{},stallDigest{};
        const auto sourceTick=[&](App& owner){
            const unsigned slot=owner.multiplayer.config.localSlot;
            ++attempts[slot];
            // Timing callback already ran. Every call through final Exit
            // remains a genuine driving owner; the next dispatch is menu.
            const auto d=owner.driver();
            if(attempts[slot]<=609){
                check(d.originalThrottleAnalog==16578&&d.originalBrakeAnalog==8191&&d.shiftUp,
                    "Finish changed precise pedal/upshift before source owner ended");
            }else check(d.throttle==0&&d.brake==0&&d.originalThrottleAnalog<0&&!d.shiftUp,
                "Catch-up source ticks leaked old driving sample into points");
            if(owner.onlineFinishTickViews){
                // The pre-solver callback has already drawn. The real
                // authority presenter must not update either camera again.
                const auto drawn=cameraKey(owner);owner.simulate(d);
                check(cameraKey(owner)==drawn,"Authority presenter double-ticked Finish camera");
            }else{
                const auto before=cameraKey(owner);owner.simulate(d);
                check(cameraKey(owner)==before,"Exit/points advanced the retained race camera");
            }
            if(attempts[slot]==1)check(owner.onlineResult.finish.counter==0&&
                owner.onlineResult.finish.visibleHudCode==0,"First settled call skipped readiness/HUD lag");
            if(owner.onlineResult.finish.blank)++blankCalls[slot];
            const auto frame=owner.authorityLink->timeline().frame();
            if(!owner.authorityStalled){
                ++accepted[slot];
                const auto records=owner.authorityLink->timeline().outgoing(frame-1,1);
                bool found=false;
                for(const auto& record:records)if(record.slot==slot){
                    found=true;
                    check(record.input.analog.throttle==(attempts[slot]<=609?16578:0x4000)&&
                        record.input.analog.brake==(attempts[slot]<=609?8191:0x4000),
                        "Accepted local packet lost precise finish controls/neutral handoff");
                    check(record.input.pressedByte==(attempts[slot]==1?0x20:0),
                        "Terminal packet lost the initial upshift or repeated a held edge");
                }
                check(found,"Accepted local authority frame had no input record");
            }else if(!stallFrame[slot]){stallFrame[slot]=frame;stallDigest[slot]=owner.authorityRace->digest();}
            else check(frame==stallFrame[slot]&&owner.authorityRace->digest()==stallDigest[slot],
                "Presentation synthesized authority movement during transport outage");
            if(attempts[slot]==609)check(owner.onlineResult.page==0&&owner.onlineResult.finish.done()&&
                !owner.onlineResult.finish.blank,"609th call skipped End-unblank boundary");
        };
        // Normal peers share packets at30 host FPS (two source calls often
        // share one sample). The second outcome deliberately loses transport
        // at15 FPS: ACK stalls must not strand the settled presentation.
        const unsigned hz=winner?15:30;
        for(unsigned hostFrame=0;hostFrame<hz*12&&app->onlineResult.page==0;++hostFrame){
            app->advanceHostClock(1./hz,[&]{sourceTick(*app);});
            peer->advanceHostClock(1./hz,[&]{sourceTick(*peer);});
            if(!winner){exchange();exchange();}
        }
        for(auto* owner:{app.get(),peer.get()}){
            const auto slot=owner->multiplayer.config.localSlot;
            check(attempts[slot]>=610&&owner->onlineResult.page==1&&owner->onlineResult.frame==0,
                "EOS settled result was stranded or points consumed catch-up ticks");
            check(blankCalls[slot]==5,"Hardware blank persisted for wrong number of completed updates");
            check(owner->onlineResultPagePending&&!owner->onlineResultPreview,"Source callback loaded result assets");
            owner->advanceOnlineResult(.1);check(owner->onlineResultPreview&&owner->onlineResult.frame==0,
                "Materialization advanced new points with prior host time");
            if(winner)check(stallFrame[slot]&&accepted[slot]<attempts[slot],"Transport-outage fixture did not stall real authority");
            else check(accepted[slot]==attempts[slot]&&owner->authorityLink->verifiedPeerFrames()>600,
                "Connected terminal authority stopped accepting/verifying both drivers");
        }
        if(!winner)check(app->authorityLink->timeline().confirmedDigest()==peer->authorityLink->timeline().confirmedDigest(),
            "Postfinish local/remote accepted history did not converge");
        // A nonmodal save may delay only the EOS replacement for SSNC. It
        // does not consume another driving phase or allow queued confirmation.
        app->beginOnlineResult();app->onlineResult.input(true,true,0,16);app->clock.reset();
        for(unsigned tick=0;tick<620;++tick)app->advanceHostClock(OriginalDrivingClock::step,[]{});
        check(app->onlineResult.page==0&&app->onlineResult.finish.done()&&!app->onlineFinishAcceptsDriving(),
            "Pending save froze source owner or allowed postowner controls");
        app->onlineResult.input(true,true,0,0);app->advanceHostClock(OriginalDrivingClock::step,[]{});
        check(app->onlineResult.page==1&&app->onlineResult.frame==0,"Completed save did not release EOS handoff");
        app->disconnectMultiplayer();const auto stopped=app->authorityRace->frame();unsigned calls=0;
        app->advanceHostClock(.25,[&]{++calls;app->simulate(app->driver());});
        check(!calls&&app->authorityRace->frame()==stopped&&app->onlineResult.page==-1,
            "Disconnect resumed stale source presentation/authority controls");

        for(auto* owner:{app.get(),peer.get()}){
            owner->leaveMultiplayer();check(!owner->authorityResultFrame,"Result frame survived a rematch reset");
        }
    }
    std::ofstream(out/"PASS.txt")<<checks<<" native result/garage/persistence/mode-return assertions; returned lobby clears the source fade, plays SELECT once, and retains its complete saved-car snapshot through forty seconds of audible covered presentation\n";
    std::cout<<"PASS "<<checks<<" online result application checks\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
