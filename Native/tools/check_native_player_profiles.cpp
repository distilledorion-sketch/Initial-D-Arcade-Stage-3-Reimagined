// Offscreen application check. Driver profiles remain in memory.
#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include "original_tuning.h"
#include <iostream>
namespace {
std::uint64_t poseHash(const NativeAssembly& assembly){
    std::uint64_t hash=14695981039346656037ull;
    const auto word=[&](unsigned value){hash^=value;hash*=1099511628211ull;};
    for(const auto& instance:assembly.instances){word(instance.chunk);for(float value:instance.transform)word(std::bit_cast<unsigned>(value));}
    return hash;
}
}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::invalid_argument("game-root output-directory required");
    const fs::path output=argv[2];fs::create_directories(output);
    App app;app.root=argv[1];app.validationMode=true;app.settings();
    app.originalCamera=OriginalChaseCamera::load(app.root);app.bumperCamera=OriginalChaseCamera::load(app.root,OriginalDrivingView::Bumper);
    app.frontend.initialize(app.root,true);app.hud.loadOriginal(app.root);
    if(!app.renderer.initialize(nullptr,1280,720,true))throw std::runtime_error(app.renderer.error);
    const auto tuning=original::OriginalTuningData::load(app.root);
    std::ofstream report(output/"live-profiles.csv");report<<"car,fresh_word,tuned_word,body_changed,plate_changed\n";
    unsigned changedCars=0,changedPlates=0;
    for(unsigned car:{0u,9u,22u,30u,33u}){
        auto fresh=original::makeOriginalFreshBattleProfile();fresh.setu(16,car);
        app.frontend.car=int(car);app.frontend.battleProfile=fresh;app.frontend.driverProfileLoaded();app.loadSelectedCar();
        if(!app.carPresentation.usesPlayerProfile())throw std::runtime_error("Ordinary app uses captured player assembly");
        const auto baseHash=poseHash(app.carPresentation.pose({}));const auto plateHash=poseHash(app.carPresentation.profilePlateAssembly());
        const auto freshWord=app.loadedAppearanceWord;
        auto tuned=fresh;tuned.setByte(152,0);tuned.setByte(153,0);tuned.setu(1180,0);
        for(unsigned step=0;!(tuned.u(1180)&0x400);++step){
            if(step>=tuning.car(car).packages[0].steps.size())throw std::runtime_error("Tuning package does not terminate");
            original::applyOriginalTuningCommand(tuned,tuning,1);
        }
        app.frontend.battleProfile=tuned;app.loadSelectedCar();
        const auto word=original::originalPlayerAppearanceConfig(tuned).word;
        if(word==freshWord||word!=app.loadedAppearanceWord)throw std::runtime_error("Same-car same-paint saved parts did not reload");
        const bool bodyChanged=poseHash(app.carPresentation.pose({}))!=baseHash;
        const bool plateChanged=poseHash(app.carPresentation.profilePlateAssembly())!=plateHash;
        if(!bodyChanged)throw std::runtime_error("Live tuned geometry unchanged");
        ++changedCars;changedPlates+=unsigned(plateChanged);
        report<<car<<','<<freshWord<<','<<word<<','<<bodyChanged<<','<<plateChanged<<'\n';report.flush();
        app.courseIndex=3;app.night=false;app.wet=false;app.reverse=false;app.frontend.gameMode=original::OriginalGameMode::TimeAttack;
        app.start();app.best={};app.bestTime=0;app.drivingView=OriginalDrivingView::Chase;
        for(unsigned frame=0;frame<480;++frame){DriverInput input;input.automatic=true;input.throttle=.5f;input.steer=std::sin(float(frame)*.025f)*.14f;app.simulate(input);}
        if(app.loadedAppearanceWord!=word)throw std::runtime_error("Race entry lost saved body parts");
        app.clock.accumulator=FixedClock::step*.5;
        if(!app.render(0)||!app.renderer.saveBitmap((output/("car-"+std::to_string(car)+"-tuned.bmp")).wstring()))throw std::runtime_error("Tuned race render failed");
        app.frontend.battleProfile=fresh;app.loadSelectedCar();app.carPresentation.resetHeadlights();
        if(app.loadedAppearanceWord!=freshWord||poseHash(app.carPresentation.pose({}))!=baseHash||poseHash(app.carPresentation.profilePlateAssembly())!=plateHash)
            throw std::runtime_error("Restoring the profile failed to restore original geometry");
        std::cout<<"car "<<car<<": same-paint parts reload, race, plate and restore passed\n";
    }
    if(!changedPlates)throw std::runtime_error("Application test did not exercise changed plate placement");
    std::cout<<"PASS "<<changedCars<<" live tuned cars, "<<changedPlates<<" changed plate placements, same-car appearance reload, 2400 driving frames and profile restoration.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
