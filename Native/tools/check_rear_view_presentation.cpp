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
    app.frontend.car=1;app.frontend.battleProfile=original::makeOriginalFreshBattleProfile();app.frontend.battleProfile.setu(16,1);
    app.frontend.gameMode=original::OriginalGameMode::LegendOfTheStreets;original::selectOriginalRival(app.frontend.battleProfile,8);
    app.start();app.best={};app.bestTime=0;
    for(unsigned i=0;i<360;++i)app.simulate({});
    const auto capture=[&](const char* name){if(!app.render(0)||!app.renderer.saveBitmap((output/name).wstring()))throw std::runtime_error(app.renderer.error);};
    capture("bumper-rear-view.bmp");
    // Arranged visibility probe: place the ordinary rival presentation behind
    // the player without changing or claiming a natural driving outcome.
    const auto delta=normalized(app.rearCameraFrame.target-app.rearCameraFrame.eye)*14.f+right(app.vehicle.yaw)*1.f;
    app.rivalVehicle.position=app.previousRival.position=app.vehicle.position+delta;
    app.rivalBodyWorld=app.previousRivalBodyWorld=app.playerBodyWorld+delta;
    app.rivalVehicle.yaw=app.previousRival.yaw=app.vehicle.yaw;
    app.rivalPitch=app.previousRivalPitch=app.bodyPitch;app.rivalRoll=app.previousRivalRoll=app.bodyRoll;
    capture("opponent-behind.bmp");
    const auto ticks=app.race.ticks;const auto camera=app.rearCameraFrame.cameraWorld.elements;
    app.paused=true;capture("paused-rear-view.bmp");app.render(0);
    if(ticks!=app.race.ticks||camera!=app.rearCameraFrame.cameraWorld.elements)throw std::runtime_error("Rear rendering advanced camera/simulation");
    app.drivingView=OriginalDrivingView::Chase;capture("chase-no-mirror.bmp");
    app.renderer.resize(2560,1004);app.drivingView=OriginalDrivingView::Bumper;capture("wide-rear-view.bmp");
    app.start();capture("restart-rear-view.bmp");
    std::cout<<"PASS original rear-view application binding, arranged rear opponent, pause, camera toggle, widescreen resize and race restart.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
