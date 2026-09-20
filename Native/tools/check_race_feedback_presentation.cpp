#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include <iostream>
int main(int argc,char** argv)try{
    if(argc!=3)throw std::invalid_argument("game-root output-directory required");
    App app;app.root=argv[1];app.validationMode=true;app.settings();
    app.originalCamera=OriginalChaseCamera::load(app.root);app.bumperCamera=OriginalChaseCamera::load(app.root,OriginalDrivingView::Bumper);
    app.frontend.initialize(app.root,true);app.hud.loadOriginal(app.root);app.audio.configure(app.root);
    if(!app.renderer.initialize(nullptr,1280,720,true))throw std::runtime_error(app.renderer.error);
    const fs::path output=argv[2];fs::create_directories(output);
    app.frontend.car=1;app.frontend.battleProfile=original::makeOriginalFreshBattleProfile();app.frontend.battleProfile.setu(16,1);app.frontend.battleProfile.setu(0,0);
    app.frontend.gameMode=original::OriginalGameMode::LegendOfTheStreets;original::selectOriginalRival(app.frontend.battleProfile,8);
    app.start();app.best={};app.bestTime=0;
    const auto capture=[&](const char* name){if(!app.render(0)||!app.renderer.saveBitmap((output/name).wstring()))throw std::runtime_error(app.renderer.error);};
    unsigned brakeFrames=0,releasedFrames=0,extensions=0,refillFrames=0;
    for(unsigned tick=0;tick<11000&&app.race.phase!=RacePhase::Finished;++tick){
        const auto projection=app.projectRacePosition(app.vehicle.position);
        const float look=std::max(10.f,std::max(0.f,app.vehicle.speed)*.65f);
        const auto target=app.sampleRaceDistance(projection.sample.distance+look).center-app.vehicle.position;
        const float angle=wrapAngle(std::atan2(target.x,target.z)-app.vehicle.yaw);
        DriverInput input;input.automatic=true;input.steer=-std::clamp(std::atan2(2*app.config.wheelbase*std::sin(angle),look)/recoveredSteeringLimit,-.75f,.75f);
        input.throttle=app.vehicle.speed<38?.9f:.1f;input.brake=app.vehicle.speed>40?.3f:0;
        app.simulate(input);
        const bool brake=(app.originalSession.publishedActors().secondary0C8FF430[92/4]&1)!=0;
        if(brake)++brakeFrames;else ++releasedFrames;
        if(tick==360){capture("night-opponent-bumper.bmp");app.drivingView=OriginalDrivingView::Chase;capture("night-opponent-chase.bmp");app.drivingView=OriginalDrivingView::Bumper;}
        if(app.raceFeedback.extensionTicks==119){++extensions;capture("time-extended.bmp");}
        if(app.raceFeedback.refilling)++refillFrames;
        if(extensions&&app.raceFeedback.extensionTicks==0)break;
    }
    if(!brakeFrames||!releasedFrames)throw std::runtime_error("Rival brake transitions were not exercised");
    // The driver probe is intentionally not a substitute for manual play. If
    // it misses a gate, still exercise the ordinary visible-feedback binding.
    std::cout<<"Natural drive: "<<extensions<<" extensions, "<<refillFrames<<" refill frames, "<<brakeFrames<<" rival brake frames / "<<releasedFrames<<" released frames.\n";
    app.start();for(unsigned i=0;i<300;++i)app.simulate({});
    app.raceFeedback.extend(app.race.remaining6000-120000);app.raceFeedback.tick(app.race.remaining6000);
    capture("message-binding.bmp");
    const auto remaining=app.raceFeedback.extensionTicks;
    for(unsigned i=0;i<3;++i)if(!app.render(0))throw std::runtime_error(app.renderer.error);
    if(app.raceFeedback.extensionTicks!=remaining)throw std::runtime_error("Rendering advances the message");
    app.paused=true;capture("paused.bmp");if(app.raceFeedback.extensionTicks!=remaining)throw std::runtime_error("Pause advances message");
    app.start();if(app.raceFeedback.extensionTicks||app.raceFeedback.refilling)throw std::runtime_error("Restart retained extension effect");
    std::cout<<"PASS original feedback application binding, repeated render/pause/reset, native race audio loads, opponent brake transitions and night captures.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
