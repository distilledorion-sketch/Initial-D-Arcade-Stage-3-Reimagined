#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include <iostream>

// No window, audio device, live rooms or user saves. Compare the actual App
// presentation's audio input with complete neutral driving frames.
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("Native root and NEW evidence directory required");
    const auto root=fs::absolute(argv[1]),out=fs::absolute(argv[2]);
    if(fs::exists(out))throw std::runtime_error("Preserve existing evidence");
    fs::create_directories(out);
    auto app=std::make_unique<App>();app->root=root;app->saveRoot=out/"userdata";app->validationMode=true;app->settings();
    app->frontend.initialize(root,true);app->hud.loadOriginal(root);app->audio.configure(root);app->loadStartPresentation();
    app->originalCamera=OriginalChaseCamera::load(root);app->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
    if(!app->renderer.initializeSceneCapture(640,480))throw std::runtime_error("Scene capture unavailable");
    Idas3UiEnable(1);app->demoPreparationStarted=true;app->replayRecordingFlags=0;
    std::ofstream summary(out/"summary.csv");
    summary<<"car,wet,max_audio_difference,max_tach_difference,showcase_last,countdown_first\n";
    unsigned cases=0;
    for(int car:{0,7,23})for(bool wet:{false,true}){
    app->input.down['W']=false;app->validationMode=true;
    app->frontend.gameMode=original::OriginalGameMode::TimeAttack;
    app->frontend.car=car;app->frontend.course=3;app->frontend.automatic=false;app->frontend.wet=wet;
    app->frontend.battleProfile=original::makeOriginalFreshBattleProfile();app->start();
    app->validationMode=false;app->beginVsBanner();
    if(!app->vsActive)throw std::runtime_error("Showcase not started");
    original::OriginalDrivingSession reference;
    const auto& initial=app->originalSession;
    const auto grid=original::originalStartPose(initial.selection().physics.conditionCode,1);
    reference.reset(root,initial.selection(),grid.position,grid.angles);
    original::OriginalHostInputState referenceInput;
    const auto idle=original::adaptOriginalHostInput(referenceInput,{},false,false,0);
    original::warmupOriginalRaceSession(reference,idle,reference.platformFrame(),0);
    std::ofstream trace(out/("rpm-"+std::to_string(car)+"-"+std::to_string(wet)+".csv"));
    trace<<"frame,throttle,showcase_raw,showcase_tach,showcase_audio,countdown_reference_audio,countdown_reference_raw\n";
    float maxAudioDifference=0,maxTachDifference=0;
    for(unsigned frame=0;frame<241;++frame){
        const bool held=frame<90||frame>=160;
        app->input.down['W']=held;
        const auto controls=app->driver();
        const auto input=original::adaptOriginalHostInput(referenceInput,{controls.steer,controls.throttle,controls.brake},false,false,0);
        const auto effect=reference.tick(input);
        app->advanceStartPresentation(1./60.);
        const auto& actual=app->audio.showcaseEngineControl();
        maxAudioDifference=std::max(maxAudioDifference,std::abs(actual.rpm-effect.completion.engineValue));
        maxTachDifference=std::max(maxTachDifference,std::abs(app->showcaseTransmission.tach1c-effect.completion.engineValue));
        trace<<frame<<','<<controls.throttle<<','<<app->showcaseTransmission.filtered18<<','<<app->showcaseTransmission.tach1c<<','<<actual.rpm<<','<<effect.completion.engineValue<<','<<reference.vehicle().transmission.filtered18<<'\n';
    }
    const auto before=app->audio.showcaseEngineControl().rpm;
    if(app->audio.engineStatistics().simulationFrames!=241)
        throw std::runtime_error("Showcase bypassed the live driving audio controller");
    if(app->vsActive||app->originalRaceOwnerFrame!=0||app->originalRaceStart.remaining()!=240||app->originalSession.vehicle().transmission.gear00!=0)
        throw std::runtime_error("Showcase advanced countdown or engaged a gear");
    const auto controls=app->driver();
    const auto next=reference.tick(original::adaptOriginalHostInput(referenceInput,
        {controls.steer,controls.throttle,controls.brake},false,false,0));
    app->simulate(app->driver());
    summary<<car<<','<<wet<<','<<maxAudioDifference<<','<<maxTachDifference<<','<<before<<','<<app->vehicle.rpm<<'\n';summary.flush();
    if(maxAudioDifference!=0||maxTachDifference!=0)throw std::runtime_error("Showcase rev response differs from complete neutral driving frames");
    if(app->vehicle.rpm!=next.completion.engineValue||app->originalRaceOwnerFrame!=1||app->originalRaceStart.remaining()!=239)
        throw std::runtime_error("Countdown discarded showcase RPM or advanced at the wrong boundary");
    if(app->audio.engineStatistics().simulationFrames!=242)
        throw std::runtime_error("Countdown reset the live engine controller");
    ++cases;
    }
    std::cout<<"PASS "<<cases<<" showcase rev curves, 241 frames each, and continuous first countdown frame\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
