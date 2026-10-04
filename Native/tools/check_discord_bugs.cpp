#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include <iostream>

int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("Native asset root and NEW evidence directory required");
    const auto root=fs::absolute(argv[1]),out=fs::absolute(argv[2]);
    if(fs::exists(out))throw std::runtime_error("Preserve existing evidence");fs::create_directories(out);
    unsigned checks=0;auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);};
    auto app=std::make_unique<App>();app->root=root;app->saveRoot=out/"userdata";app->validationMode=true;app->settings();
    app->frontend.initialize(root,true);app->hud.loadOriginal(root);app->audio.configure(root);
    app->originalCamera=OriginalChaseCamera::load(root);app->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
    // Verify the actual host camera against a fresh live camera with pitched,
    // rolled and wrapped poses. Seeking must not retain previous camera state.
    auto live=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
    for(unsigned i=0;i<30;++i){
        Vec3 position{float(i*7),float(i%3),float(i*13)};
        Vec3 angles{.13f*float(int(i%5)-2),.4f*float(i),.04f*float(int(i%7)-3)};
        VehicleState recorded;recorded.position=position;recorded.yaw=wrapAngle(angles.y+pi);
        app->bodyPitch=-angles.x;app->bodyRoll=-angles.z;
        const auto a=app->replayBumperFrame(recorded),b=live.update(position,angles);
        check(length(a.eye-b.eye)<.002f&&length(a.target-b.target)<.002f&&length(a.up-b.up)<.002f,"Replay bumper pose differs from live camera");
        check(a.verticalFieldOfView==b.verticalFieldOfView,"Replay bumper FOV differs from live camera");
    }
    for(unsigned id=0;id<16;++id)for(bool reverse:{false,true}){
        app->menu=true;app->frontend.course=int(id);app->frontend.reverse=reverse;
        check(app->presenceCourseCondition()==int(id*2+reverse),"Menu presence has wrong course");
        app->menu=false;app->reverse=reverse;app->courseIndex=id<9?int(id):3;
        if(id>=9){app->importedCourse.emplace();app->importedCourse->id=id;}else app->importedCourse.reset();
        check(app->presenceCourseCondition()==int(id*2+reverse),"Race presence leaked its physics donor course");
    }
    app->importedCourse.reset();
    // New cars choose a package after AT/MT without buying any parts. Use
    // the real frontend transitions and host save commit in an isolated slot.
    app->saveSlots=LocalSaveSlots(out/"saves");app->activeSaveSlot=0;
    app->profiles=LocalDriverProfiles(app->saveSlots.profileDirectory(0));
    app->driverSetup=LocalDriverSetup(app->saveSlots.profileDirectory(0));
    auto driver=original::makeOriginalFreshBattleProfile();driver.setu(76,1);driver.setu(44,1);
    check(app->saveSlots.adopt(0,driver),"Fixture save failed");
    for(unsigned package=0;package<4;++package){
        auto& f=app->frontend;f.car=int(package+1);f.battleProfile=original::makeOriginalFreshBattleProfile();
        f.battleProfile.setu(16,unsigned(f.car));f.battleProfile.setu(72,12345);f.battleProfile.setu(68,package%2);
        f.changingSavedCar=true;f.savedDriverSelected=true;f.automatic=package%2==0;
        f.selectSavedCarTransmission(true);f.advance(1);f.confirm();
        for(unsigned tick=0;tick<240&&f.stage==FrontendStage::Transmission;++tick){const auto before=f.stage;f.advance(1./60);app->advanceSavedCarSelection(before);}
        check(f.stage==FrontendStage::TuningCourse,"New car skipped its package selection");
        f.advance(1);f.change(int(package));f.confirm();
        for(unsigned tick=0;tick<240&&f.stage==FrontendStage::TuningCourse;++tick){const auto before=f.stage;f.advance(1./60);app->advanceSavedCarSelection(before);}
        check(f.stage==FrontendStage::Mode&&!f.changingSavedCar,"Package selection did not finish at Mode");
        const auto p=app->profiles.load(unsigned(f.car)).profile;
        check(p.byte(152)==package&&p.u(68)==package%2,"Package or transmission choice was lost");
        check(p.u(72)==12345&&p.byte(153)==0,"Package selection granted points or full tuning");
        check(p.u(76)==1&&p.u(44)==1,"Package selection changed the save's driver name");
        for(unsigned offset=156;offset<=166;++offset)check(p.byte(offset)==0,"Package selection granted a part");
    }
    auto& f=app->frontend;f.battleProfile=original::makeOriginalFreshBattleProfile();f.car=4;f.battleProfile.setu(16,4);
    f.battleProfile.setByte(152,2);f.battleProfile.setByte(156,3);f.battleProfile.setu(72,6789);
    check(!app->needsFullTuneCourseSelection(),"An upgraded car was offered a destructive route change");
    f.battleProfile.setu(16,29);f.battleProfile.setByte(156,0);
    check(!app->needsFullTuneCourseSelection(),"Single-package GC8V was offered other packages");
    f.battleProfile.setu(16,4);f.changingSavedCar=true;f.selectSavedCarTuningCourse();f.advance(1);f.back();
    check(f.stage==FrontendStage::Car,"Cancelling package selection did not return to the car picker");
    // Start real Bunta sessions; the completed marker must remain on disk but
    // must not wrap through the original low-four-bit pace lookup.
    f.changingSavedCar=false;app->menu=false;
    for(unsigned level:{0u,14u,15u,16u}){
        f.car=0;f.gameMode=original::OriginalGameMode::BuntaChallenge;f.battleProfile=original::makeOriginalFreshBattleProfile();
        f.battleProfile.setu(0,2);f.battleProfile.setu(1080,level);original::selectOriginalBuntaCourse(f.battleProfile,0);
        app->start();
        check(app->originalSession.rivalPaceInputs().progress0C901604[0]==std::min(level,15u),"Bunta pace wrapped after completion");
        check(f.battleProfile.u(1080)==level&&app->battleProfile.u(1080)==level,"Bunta fix changed saved completion");
    }
    // Every Tsubaki SRA page uses the same screen orientation for its route
    // and driven trace in either travel direction. Other courses stay intact.
    ImportedCourse course;course.id=15;course.checkpoints={0,5,10,15,20};
    original::OriginalTimeAttackTelemetrySnapshot trace;
    for(unsigned i=0;i<=20;++i){course.center.push_back({float(i*i),0,float(i*40)});trace.drivingPath.push_back({course.center.back(),i,0xff00ff00});}
    for(bool reverse:{false,true}){
        const auto pages=course.analysisMaps(trace,reverse);check(pages.size()==5,"SRA page count changed");
        for(const auto& page:pages){
            check(!page.road.empty()&&page.road.front().from[1]<page.road.back().to[1],"Tsubaki SRA remains vertically mirrored");
            for(const auto& line:page.driving)check(line.from[1]<=line.to[1],"Tsubaki trace was not flipped with the road");
        }
    }
    course.id=9;const auto other=course.analysisMaps(trace,false);
    check(other[0].road.front().from[1]>other[0].road.back().to[1],"Tsubaki fix flipped a different course");
    // The existing engine command is the owner of the visual pulse. Rendering
    // it repeatedly must not advance it or consume the shared driving RNG.
    check(app->renderer.initialize(nullptr,960,720,true),"Offscreen backfire renderer failed");
    app->backfire.load(root);
    for(unsigned muffler=1;muffler<=3;++muffler){
        f.car=19;f.battleProfile=original::makeOriginalFreshBattleProfile();f.battleProfile.setu(16,19);
        f.battleProfile.setByte(162,std::uint8_t(muffler));f.battleProfile.setByte(166,1);
        app->loadSelectedCar();app->audio.selectOriginalEngine(root,f.battleProfile);
        auto gains=app->audio.outputGains();gains.effects=0;app->audio.setOutputGains(gains);
        std::uint32_t seed=123;original::OriginalEngineControlInput input;input.rpm=7000;input.gear=3;
        bool sawFlash=false;
        for(unsigned tick=0;tick<100;++tick){
            input.throttle=tick<60?1.f:0.f;app->audio.stepOriginalEngine(input,seed);app->audio.finishSoundFrame(seed);
            if(app->audio.backfireFrame()==0){sawFlash=true;break;}
        }
        check(sawFlash,"Evo III accepted misfire cue did not produce a flash with Effects muted");
        const auto appearance=original::originalPlayerAppearanceConfig(f.battleProfile);
        check(app->renderer.loadTextures(app->originalTextures),"Evo textures failed");
        const auto base=unsigned(app->originalTextures.size());
        check(app->renderer.loadTextures(app->backfire.textures,true),"Backfire textures failed");
        app->renderer.vehicleLights=false;app->renderer.opponentLights=false;app->renderer.nearClip=.05f;
        app->renderer.overrideClearColor=true;app->renderer.clearColor={.12f,.14f,.16f,1};
        for(int frame:{-1,0,1}){
            Mesh mesh;mesh.originalCar(app->originalModel,app->carPresentation.pose({},false,false),{0,0,0},0);
            const auto before=mesh.vertices.size();
            const bool added=app->backfire.append(mesh,frame,appearance,{0,0,0},0,0,0,base);
            check(added==(frame>=0)&&((mesh.vertices.size()>before)==added),"Backfire frame admission failed");
            if(added){
                for(auto i=before;i<mesh.vertices.size();++i){const auto p=mesh.vertices[i].position;
                    check(p.z<-1.9f&&p.z>-3.5f&&p.x>.1f&&p.x<1.1f&&p.y>-.18f&&p.y<.18f,"Backfire detached from the rear exhaust");}
            }
            check(app->renderer.draw(mesh,{3,1.4f,-5.5f},{0,.65f,0},false,false),"Evo backfire draw failed");
            check(app->renderer.saveBitmap((out/("evo-"+std::to_string(muffler)+"-"+std::to_string(frame)+".bmp")).wstring()),"Evo backfire capture failed");
        }
        for(unsigned repeat=0;repeat<20;++repeat){Mesh mesh;app->backfire.append(mesh,app->audio.backfireFrame(),appearance,{10,3,20},.7f,.1f,-.15f,base);}
        check(app->audio.backfireFrame()==0,"Rendering advanced the backfire simulation clock");
        app->audio.applyConfirmedOnlineAudio({},{});check(app->audio.backfireFrame()==1,"Backfire did not advance one confirmed frame");
        app->audio.applyConfirmedOnlineAudio({},{});check(app->audio.backfireFrame()==-1,"Backfire did not end after two frames");
        app->audio.playRaceCue(2,7);app->audio.resetRaceEffects();check(app->audio.backfireFrame()==-1,"Race restart retained a flash");
        Mesh excluded;check(!app->backfire.append(excluded,0,original::OriginalCarAppearanceConfig(0),{},0,0,0,base),"Other car received Evo backfire");
        check(!app->backfire.append(excluded,0,original::OriginalCarAppearanceConfig(19),{},0,0,0,base),"Stock exhaust received Evo backfire");
        // Feed the actual original engine controller through the host's peer
        // confirmation path, for either local slot. No remote sound is added.
        const auto tables=original::OriginalEngineTables::load(root);
        for(unsigned local:{0u,1u}){
            app->clearAuthority();app->multiplayer.config.localSlot=local;
            original::OriginalEngineControlState engine;original::resetOriginalEngineControl(engine);
            const auto config=configureProfileEngineSound(f.battleProfile);
            auto remoteSeed=123u;bool remoteFlash=false;
            original::OnlineRaceFrame confirmed;
            for(unsigned tick=0;tick<100;++tick){
                input.throttle=tick<60?1.f:0.f;
                confirmed.engine[1-local]=original::stepOriginalEngineControl(tables,config,engine,input,remoteSeed);
                app->confirmedAuthorityFrame(confirmed);
                if(app->remoteBackfireFrame()==0){remoteFlash=true;break;}
            }
            check(remoteFlash,"Confirmed remote misfire command did not reach visual clock");
            check(app->audio.backfireFrame()==-1,"Remote cue flashed the local car");
            Mesh remoteMesh;check(app->backfire.append(remoteMesh,app->remoteBackfireFrame(),appearance,{10,3,20},.7f,.1f,-.15f,base,3),"Remote exhaust flash missing");
            for(const auto& range:remoteMesh.ranges)check(range.viewMask==3&&range.emissive,"Opponent flash excluded from mirror or incorrectly lit");
            for(unsigned repaint=0;repaint<20;++repaint)check(app->remoteBackfireFrame()==0,"Remote flash moved without a confirmed tick");
            confirmed.engine[1-local].clear();app->confirmedAuthorityFrame(confirmed);
            check(app->remoteBackfireFrame()==1,"Remote flash did not advance");
            app->confirmedAuthorityFrame(confirmed);check(app->remoteBackfireFrame()==-1,"Remote flash did not expire");
            confirmed.engine[local]={{original::OriginalEngineCommandTarget::RaceCue1424A0,0,0,7}};
            app->confirmedAuthorityFrame(confirmed);check(app->remoteBackfireFrame()==-1,"Local cue flashed the remote car");
            confirmed.engine[1-local]=confirmed.engine[local];app->confirmedAuthorityFrame(confirmed);
            check(app->remoteBackfireFrame()==0,"Remote cue fixture failed");
            app->clearAuthority();check(app->remoteBackfireFrame()==-1,"Rematch retained opponent flash");
        }
    }
    // Verify the renderer's actual opponent submission and independent car
    // appearance. This fixture does not send packets or contact Steam.
    {
        auto local=original::makeOriginalFreshBattleProfile(),remote=local;
        local.setu(16,0);remote.setu(16,19);remote.setByte(162,1);remote.setByte(166,1);
        Idas3MultiplayerConfig config{sizeof(config),2,0,0,0,0,0,19,0,1};
        app->startMultiplayer(config,&local,&remote);app->enableAuthority(76544321,true,false);
        app->setMultiplayerGo(true);app->vsActive=false;app->paused=true;
        original::OnlineRaceFrame frame;frame.engine[1]={{original::OriginalEngineCommandTarget::RaceCue1424A0,0,0,7}};
        app->confirmedAuthorityFrame(frame);
        const auto digest=app->authorityRace->digest();
        const auto remoteRanges=[&]{unsigned count=0;for(const auto& range:app->raceMesh.ranges)
            if(range.texture>=app->backfireTextureBase&&range.texture<app->backfireTextureBase+app->backfire.textures.size()){
                check(range.viewMask==3&&range.emissive,"Live opponent flame not visible in mirror");++count;
            }return count;};
        check(app->render(0)&&remoteRanges()>0,"Live online renderer omitted opponent exhaust flash");
        check(app->authorityRace->digest()==digest&&app->remoteBackfireFrame()==0,"Rendering altered confirmed simulation/effect timeline");
        frame.engine[1].clear();app->confirmedAuthorityFrame(frame);app->confirmedAuthorityFrame(frame);
        check(app->render(0)&&remoteRanges()==0,"Live online renderer retained expired opponent flash");
        frame.engine[1]={{original::OriginalEngineCommandTarget::RaceCue1424A0,0,0,7}};app->confirmedAuthorityFrame(frame);
        app->disconnectMultiplayer();check(app->remoteBackfireFrame()==-1,"Disconnect retained opponent flash");
        app->leaveMultiplayer();
    }
    // Exercise the actual race renderer, including its texture offsets and
    // source path coordinate. The verification view faces Myogi's sun.
    f.car=0;f.course=0;f.reverse=false;f.night=false;f.wet=false;f.gameMode=original::OriginalGameMode::TimeAttack;
    f.battleProfile=original::makeOriginalFreshBattleProfile();f.battleProfile.setu(16,0);
    app->courseIndex=0;app->reverse=false;app->night=false;app->wet=false;app->start();
    app->environment.load(root);app->paused=true;app->vsActive=false;app->drivingView=OriginalDrivingView::Natural;
    const auto sun=app->environment.sunDirection(0);app->vehicle.yaw=std::atan2(sun.x,sun.z);app->previous=app->vehicle;
    app->cameraReady=false;app->texturesPending=true;
    check(app->render(0),"Actual daytime race render failed");
    auto flareRanges=[&]{unsigned n=0;for(const auto& r:app->raceMesh.ranges)if(r.texture>=app->flareTextureBase&&r.texture<app->flareTextureBase+7)++n;return n;};
    check(flareRanges()>0,"Race never submitted sun flare geometry");
    check(app->renderer.saveBitmap((out/"myogi-sun.bmp").wstring()),"Race sun capture failed");
    app->night=true;check(app->render(0),"Night race render failed");check(flareRanges()==0,"Actual night race retained sun flare");
    app->night=false;app->wet=true;check(app->render(0),"Wet race render failed");check(flareRanges()==0,"Actual wet race retained sun flare");
    // Mirror binding must use the actor anchor, not the separate grounded body.
    // Match the new report's Akina/FD3S/Bunta setting. The opponent position is
    // deliberately arranged for a repeatable framing comparison, not a race run.
    f.car=22;f.gameMode=original::OriginalGameMode::BuntaChallenge;
    f.battleProfile=original::makeOriginalFreshBattleProfile();f.battleProfile.setu(16,22);f.battleProfile.setu(0,2);
    original::selectOriginalBuntaCourse(f.battleProfile,3);app->paused=false;app->start();
    app->loadingActive=app->vsActive=app->preRaceDialogueActive=false;
    for(unsigned i=0;i<360;++i)app->simulate({});
    app->paused=true;app->race.phase=RacePhase::Running;app->originalRaceOwnerFrame=360;
    const auto& drive=app->presentedSession().vehicle().drive;
    const Vec3 actorPosition{drive.f(0),drive.f(4),drive.f(8)},actorAngles{drive.f(0x0c),drive.f(0x10),drive.f(0x14)};
    const auto expectedMirror=app->originalCamera.rearView(actorPosition,actorAngles);
    const auto bodyPosition=app->playerBodyWorld;
    for(auto drivingView:{OriginalDrivingView::Bumper,OriginalDrivingView::Chase,OriginalDrivingView::Natural}){
        app->drivingView=drivingView;
        for(unsigned car=0;car<35;++car){
            app->playerBodyWorld=actorPosition+Vec3{.1f,originalCarRideHeight(car),-.05f};
            app->advanceOriginalCamera();
            check(length(app->rearCameraFrame.eye-expectedMirror.eye)<.00001f,"Mirror camera inherited the body lift or ground offset");
            check(length(app->rearCameraFrame.up-expectedMirror.up)<.00001f,"Mirror camera lost road banking");
        }
    }
    app->playerBodyWorld=bodyPosition;app->drivingView=OriginalDrivingView::Bumper;
    app->advanceOriginalCamera();
    const auto delta=normalized(expectedMirror.target-expectedMirror.eye)*4.3f;
    app->rivalVehicle.position=app->previousRival.position=app->vehicle.position+delta;
    app->rivalBodyWorld=app->previousRivalBodyWorld=app->rivalBody.update(app->presentedSession().collision(),unsigned(app->loadedRivalCar),app->rivalVehicle.position);
    app->rivalVehicle.yaw=app->previousRival.yaw=app->vehicle.yaw;
    app->rivalPitch=app->previousRivalPitch=app->bodyPitch;app->rivalRoll=app->previousRivalRoll=app->bodyRoll;
    app->previous=app->vehicle;app->previousPlayerBodyWorld=app->playerBodyWorld;app->texturesPending=true;
    auto oldAnchor=bodyPosition;oldAnchor.y+=std::bit_cast<float>(0x3ca3d70au);
    const auto oldMirror=app->originalCamera.rearView(oldAnchor,actorAngles);
    const auto captureMirror=[&](const OriginalRearViewFrame& pose,const char* file){
        app->rearCameraFrame=app->previousRearCameraFrame=pose;
        check(app->render(0),"Akina mirror render failed");
        check(app->renderer.saveBitmap((out/file).wstring()),"Akina mirror capture failed");
    };
    captureMirror(oldMirror,"akina-mirror-before.bmp");captureMirror(expectedMirror,"akina-mirror-after.bmp");
    check(oldMirror.eye.y-expectedMirror.eye.y>.25f,"Reported mirror-height fixture did not include the old body lift");
    std::ofstream(out/"mirror-height.txt")<<"Old eye Y: "<<oldMirror.eye.y<<"\nCorrected eye Y: "<<expectedMirror.eye.y<<"\nBody lift removed: "<<oldMirror.eye.y-expectedMirror.eye.y<<"\n";
    std::ofstream(out/"PASS.txt")<<checks<<" actual-host checks: bumper/rear camera, course presence, saved-car packages, Bunta completion, Tsubaki maps, Evo misfire timing/geometry and environment race integration.\n";
    std::cout<<"PASS "<<checks<<" Discord regression checks\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
