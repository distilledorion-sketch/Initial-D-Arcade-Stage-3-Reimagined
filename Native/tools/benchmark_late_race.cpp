#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include <iostream>
#include <iomanip>

// Frozen arranged scenes exercise the production CPU scene-publication path.
// This measures neither network/simulation cost nor Unity GPU/render time.
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("Native root and NEW evidence directory required");
    const auto root=fs::absolute(argv[1]),out=fs::absolute(argv[2]);
    if(fs::exists(out))throw std::runtime_error("Preserve existing evidence");fs::create_directories(out);
    auto a=std::make_unique<App>();a->root=root;a->saveRoot=out/"replay-viewer-session";a->validationMode=true;
    a->settings();a->frontend.initialize(root,true);a->hud.loadOriginal(root);a->audio.configure(root);
    a->originalCamera=OriginalChaseCamera::load(root);a->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
    if(!a->renderer.initializeSceneCapture(1280,720))throw std::runtime_error("Scene publication failed");
    Idas3UiEnable(1);
    std::ofstream csv(out/"samples.csv");csv<<"course,reverse,night,cars,fraction,frame,native_ms,vertices,ranges\n";
    std::ofstream summary(out/"summary.csv");summary<<"course,reverse,night,cars,fraction,median_ms,p95_ms,vertices,ranges\n";
    for(unsigned id:{2u,3u})for(bool night:{false,true})for(bool opponent:{false,true}){
        if(a->multiplayer.active)a->leaveMultiplayer();
        a->frontend.gameMode=original::OriginalGameMode::TimeAttack;a->frontend.course=a->courseIndex=int(id);
        a->frontend.reverse=a->reverse=true;a->frontend.night=a->night=night;a->frontend.wet=a->wet=false;
        a->frontend.car=0;a->frontend.battleProfile=original::makeOriginalFreshBattleProfile();
        a->replayPlaybackActive=false;
        if(opponent){Idas3MultiplayerConfig config{sizeof(config),2,id,1,0,unsigned(night),0,1,0,1};a->startMultiplayer(config);a->setMultiplayerGo(true);}
        else a->start();
        a->vsActive=a->loadingActive=a->preRaceDialogueActive=a->menu=a->paused=false;
        a->race.phase=RacePhase::Running;a->rivalVisible=opponent;a->drivingView=OriginalDrivingView::Bumper;
        for(float fraction:{.05f,.55f,.75f,.9f}){
            const auto p=a->course.sample(a->course.length*fraction);const auto f=normalized(p.tangent);
            a->vehicle.position=a->previous.position=p.center;a->vehicle.yaw=a->previous.yaw=std::atan2(f.x,f.z);
            a->playerBodyWorld=a->previousPlayerBodyWorld=p.center+Vec3{0,originalCarRideHeight(0),0};
            a->bodyPitch=a->previousPitch=std::atan2(-f.y,std::sqrt(f.x*f.x+f.z*f.z));a->bodyRoll=a->previousRoll=0;
            a->rivalVehicle.position=a->previousRival.position=p.center-f*10.f;
            a->rivalVehicle.yaw=a->previousRival.yaw=a->vehicle.yaw;
            a->rivalBodyWorld=a->previousRivalBodyWorld=a->rivalVehicle.position+Vec3{0,originalCarRideHeight(1),0};
            a->rivalPitch=a->previousRivalPitch=a->bodyPitch;a->rivalRoll=a->previousRivalRoll=0;
            a->progress=p.distance;a->originalCoordinate={int(p.segmentIndex),0};
            a->originalPath.project({p.center.x,p.center.y,p.center.z},a->originalCoordinate,true);
            a->courseLightPathIndex=a->originalCoordinate.index;a->clock.reset();
            // Bind the original projector geometry at the arranged pose once;
            // it is frozen during measurement, like the rest of this scene.
            a->replayPlaybackActive=true;a->replayLights=a->replayRivalLights=night;
            a->refreshReplayHeadlights();a->replayPlaybackActive=false;
            const Vec3 angles{-a->bodyPitch,wrapAngle(a->vehicle.yaw-pi),0};
            a->originalCamera.reset();a->bumperCamera.reset();
            a->previousCameraFrame=a->originalCamera.update(p.center,angles);
            a->previousBumperFrame=a->bumperCamera.update(p.center,angles);
            a->previousRearCameraFrame=a->rearCameraFrame=a->bumperCamera.rearView(p.center,angles);
            const auto physics=a->presentedSession().rollbackDigest();std::vector<double> times;
            for(unsigned i=0;i<90;++i){
                const auto begin=std::chrono::steady_clock::now();Idas3UiBeginFrame(1280,720);
                if(!a->render(0,true))throw std::runtime_error(a->renderer.error);
                const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
                if(i>=30){times.push_back(ms);const auto& s=a->renderer.sceneCapture()->frame();
                    csv<<id<<",1,"<<night<<','<<(opponent?2:1)<<','<<fraction<<','<<i<<','<<ms<<','<<s.vertexCount<<','<<s.rangeCount<<'\n';}
            }
            if(a->presentedSession().rollbackDigest()!=physics)throw std::runtime_error("Render mutated simulation");
            std::sort(times.begin(),times.end());const auto& s=a->renderer.sceneCapture()->frame();
            if(s.viewCount!=2)throw std::runtime_error("Expected the production rear-view camera");
            summary<<id<<",1,"<<night<<','<<(opponent?2:1)<<','<<fraction<<','<<times[30]<<','<<times[57]<<','<<s.vertexCount<<','<<s.rangeCount<<'\n';summary.flush();
            std::cout<<id<<" night="<<night<<" cars="<<(opponent?2:1)<<" at="<<fraction<<" median="<<times[30]<<"ms ranges="<<s.rangeCount<<'\n';
        }
    }
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
