#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include <iostream>

// Live host -> recorded body/actor -> replay camera, without modifying saves.
int main(int argc,char** argv)try{
    if(argc!=4)throw std::runtime_error("Native root, course packs and NEW evidence directory required");
    const auto root=fs::absolute(argv[1]),packs=fs::absolute(argv[2]),out=fs::absolute(argv[3]);
    if(fs::exists(out))throw std::runtime_error("Preserve existing evidence");fs::create_directories(out);
    auto app=std::make_unique<App>();app->root=root;app->saveRoot=out/"userdata";app->validationMode=true;
    app->settings();app->frontend.initialize(root,true);app->hud.loadOriginal(root);app->audio.configure(root);
    app->originalCamera=OriginalChaseCamera::load(root);app->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
    app->renderer.initializeSceneCapture(960,720);
    for(const auto& definition:importedCourseDefinitions){
        app->importedRoot(definition.id)=packs/definition.folder;
        app->frontend.enableHakoneCourse(app->importedRoot(definition.id),definition.id);
    }
    unsigned checks=0,failures=0;float worst=0;
    auto check=[&](bool ok,const char* why){++checks;if(!ok){++failures;if(failures<8)std::cerr<<why<<'\n';}};
    std::ofstream csv(out/"poses.csv");csv<<"course,reverse,wet,tick,live_y,replay_y,error_m\n";
    for(unsigned id:{3u,9u,10u,11u,12u,13u,14u,15u})for(bool reverse:{false,true})for(bool wet:{false,true}){
        struct Pose {VehicleState actor;Vec3 body;float pitch,roll;OriginalChaseFrame camera;};
        std::vector<Pose> poses;
        app->paused=app->replayPlaybackActive=false;app->frontend.gameMode=original::OriginalGameMode::TimeAttack;
        app->frontend.course=app->courseIndex=int(id);app->frontend.car=1;
        app->frontend.battleProfile=original::makeOriginalFreshBattleProfile();app->frontend.battleProfile.setu(16,1);
        app->frontend.automatic=app->automatic=true;app->frontend.reverse=app->reverse=reverse;
        app->frontend.wet=app->wet=wet;app->frontend.night=app->night=courseRequiresNight(id);
        app->start();app->loadingActive=app->vsActive=app->preRaceDialogueActive=false;app->menu=false;
        for(unsigned tick=0;tick<720;++tick){
            DriverInput input;input.automatic=true;input.throttle=.7f;
            const auto p=app->projectRacePosition(app->vehicle.position);
            const float look=std::max(8.f,app->vehicle.speed*.5f);
            const auto aim=app->sampleRaceDistance(p.sample.distance+look).center-app->vehicle.position;
            const float angle=wrapAngle(std::atan2(aim.x,aim.z)-app->vehicle.yaw);
            input.steer=-std::clamp(3.5f*std::atan2(2*app->config.wheelbase*std::sin(angle),look)/recoveredSteeringLimit,-1.f,1.f);
            app->simulate(input);
            if(tick%6!=0||app->race.phase!=RacePhase::Running)continue;
            const auto live=app->bumperCamera.frame();
            poses.push_back({app->vehicle,app->playerBodyWorld,app->bodyPitch,app->bodyRoll,live});
            const auto cameraOwner=app->bumperCamera;
            const auto physics=app->presentedSession().rollbackDigest();const auto query=app->playerBody.query().words;
            app->replayDetailed=true;
            const auto replay=app->replayBumperFrame(app->vehicle);
            const float error=length(live.eye-replay.eye);worst=std::max(worst,error);
            csv<<id<<','<<reverse<<','<<wet<<','<<tick<<','<<live.eye.y<<','<<replay.eye.y<<','<<error<<'\n';
            check(error<.002f&&length(live.target-replay.target)<.002f,"Recorded bumper camera diverges from live pose");
            check(length(live.up-replay.up)<.002f&&live.verticalFieldOfView==replay.verticalFieldOfView,"Recorded camera direction/FOV differs");
            check(app->presentedSession().rollbackDigest()==physics&&app->playerBody.query().words==query,"Replay camera mutated driving/contact state");
            app->bumperCamera=cameraOwner;app->replayDetailed=false;
        }
        check(poses.size()>20,"Route did not produce enough real driving poses");
        app->replayDetailed=true;
        const auto physics=app->presentedSession().rollbackDigest();const auto query=app->playerBody.query().words;
        // Opposite seek directions and repeated paused paints must not retain
        // a previous pose's contact normal or advance the driving simulation.
        for(std::size_t i=0;i<poses.size();++i){
            const auto& p=poses[i%2?i/2:poses.size()-1-i/2];
            app->playerBodyWorld=p.body;app->bodyPitch=p.pitch;app->bodyRoll=p.roll;
            const auto first=app->replayBumperFrame(p.actor),repeat=app->replayBumperFrame(p.actor);
            check(length(first.eye-p.camera.eye)<.002f&&length(first.target-p.camera.target)<.002f,"Replay seek retained stale camera contact");
            check(length(first.eye-repeat.eye)==0&&length(first.up-repeat.up)==0,"Paused replay camera moved");
        }
        check(app->presentedSession().rollbackDigest()==physics&&app->playerBody.query().words==query,"Replay seeking changed live driving state");
        // IDR1 has no displayed-body data; an arbitrary leftover body cannot
        // change its raw recorded actor camera.
        app->replayDetailed=false;const auto& p=poses.front();app->bodyPitch=p.pitch;app->bodyRoll=p.roll;
        app->playerBodyWorld={10000,10000,10000};
        auto expected=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
        const auto old=expected.update(p.actor.position,{-p.pitch,wrapAngle(p.actor.yaw-pi),-p.roll});
        const auto legacy=app->replayBumperFrame(p.actor);
        check(length(old.eye-legacy.eye)==0,"Legacy replay acquired a synthetic body offset");
    }
    std::ofstream(out/"report.txt")<<"checks "<<checks<<" failures "<<failures<<" worst_camera_error_m "<<worst<<'\n';
    std::cout<<"checks "<<checks<<" failures "<<failures<<" worst_camera_error_m "<<worst<<'\n';
    return failures?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
