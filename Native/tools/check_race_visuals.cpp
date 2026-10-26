#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include <iostream>
// GPU offscreen capture only. No window or audio output device is opened.
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("Native root and new evidence folder required");
    const auto root=fs::absolute(argv[1]),out=fs::absolute(argv[2]);
    if(fs::exists(out))throw std::runtime_error("Preserve existing evidence");
    fs::create_directories(out);
    auto app=std::make_unique<App>();app->root=root;app->saveRoot=out/"userdata";
    app->validationMode=true;app->settings();app->frontend.initialize(root,true);
    app->hud.loadOriginal(root);app->audio.configure(root);
    app->originalCamera=OriginalChaseCamera::load(root);
    app->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
    if(!app->renderer.initialize(nullptr,960,720,true))throw std::runtime_error(app->renderer.error);
    app->demoPreparationStarted=true;app->replayRecordingFlags=0;
    for(unsigned car:{0u,8u,19u})for(bool night:{false,true}){
        app->frontend.car=car;app->frontend.battleProfile=original::makeOriginalFreshBattleProfile();
        app->frontend.battleProfile.setu(16,car);app->frontend.gameMode=original::OriginalGameMode::TimeAttack;
        app->frontend.course=app->courseIndex=3;app->frontend.night=app->night=night;
        app->frontend.wet=app->wet=false;app->frontend.reverse=app->reverse=false;
        app->drivingView=OriginalDrivingView::Chase;
        app->start();app->loadingActive=app->vsActive=app->preRaceDialogueActive=false;
        app->race.phase=RacePhase::Countdown;app->originalRaceOwnerFrame=1;
        for(unsigned frame=0;frame<3;++frame)app->advanceProjectedHeadlights();
        if(!app->render(0))throw std::runtime_error(app->renderer.error);
        auto name="car-"+std::to_string(car)+(night?"-night":"-day");
        if(!app->renderer.saveBitmap((out/(name+".bmp")).wstring()))throw std::runtime_error("Capture failed");
    }
    std::cout<<"PASS six offscreen car/light captures; no window or audio device opened\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
