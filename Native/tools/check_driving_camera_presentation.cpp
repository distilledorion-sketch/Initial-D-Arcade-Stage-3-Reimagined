// Application-level WARP diagnostic. The ordinary App code is exercised with
// validationMode set before any profile load; no interactive desktop control.
#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include <iostream>
int main(int argc,char** argv)try{
    if(argc!=3)throw std::invalid_argument("game-root output-directory required");
    const fs::path output=argv[2];fs::create_directories(output);
    App app;app.root=argv[1];app.validationMode=true;app.settings();
    app.originalCamera=OriginalChaseCamera::load(app.root);app.bumperCamera=OriginalChaseCamera::load(app.root,OriginalDrivingView::Bumper);
    app.frontend.initialize(app.root,true);app.hud.loadOriginal(app.root);app.courseIndex=3;app.wet=false;app.night=false;
    if(!app.renderer.initialize(nullptr,1280,720,true))throw std::runtime_error(app.renderer.error);
    std::ofstream report(output/"camera-and-pose.csv");report<<"car,simulated_frames,raw_angle_wraps,interpolated_poses,minimum_body_up_y\n";
    unsigned totalWraps=0,totalPoses=0;
    for(unsigned car:{0u,9u,22u}){
        app.frontend.car=int(car);app.frontend.battleProfile=original::makeOriginalFreshBattleProfile();app.frontend.battleProfile.setu(16,car);
        app.drivingView=OriginalDrivingView::Bumper;app.start();app.best={};app.bestTime=0;
        const auto capture=[&](const std::string& name){if(!app.render(0)||!app.renderer.saveBitmap((output/("car-"+std::to_string(car)+"-"+name+".bmp")).wstring()))throw std::runtime_error("Camera capture failed");};
        app.clock.accumulator=FixedClock::step*.5;capture("bumper-start");
        const auto expected=lerp(app.previousBumperFrame.eye,app.bumperCamera.frame().eye,.5f);
        if(length(app.camera-expected)>1e-5f||app.renderer.verticalFieldOfView!=app.bumperCamera.frame().verticalFieldOfView)throw std::runtime_error("Default bumper camera not applied");
        app.input.pressed['C']=true;app.commands(0);app.input.pressed['C']=false;
        if(app.drivingView!=OriginalDrivingView::Chase)throw std::runtime_error("C did not select chase view");
        capture("chase-start");
        if(length(app.camera-lerp(app.previousCameraFrame.eye,app.originalCamera.frame().eye,.5f))>1e-5f)throw std::runtime_error("Chase camera not applied");
        app.input.pressedButtons=XINPUT_GAMEPAD_Y;app.commands(0);app.input.pressedButtons=0;
        if(app.drivingView!=OriginalDrivingView::Bumper)throw std::runtime_error("Controller Y did not select bumper view");
        unsigned wraps=0,poses=0;float minUp=1;
        for(unsigned tick=0;tick<720;++tick){
            DriverInput input;input.automatic=true;input.throttle=.55f;input.steer=std::sin(float(tick)*.025f)*.18f;
            app.simulate(input);
            if(std::abs(app.bodyPitch-app.previousPitch)>pi||std::abs(app.bodyRoll-app.previousRoll)>pi)++wraps;
            for(float alpha:{.25f,.5f,.75f}){const auto angle=interpolateCarBodyAngles({app.previousPitch,app.previousRoll},{app.bodyPitch,app.bodyRoll},alpha);
                const float up=std::cos(angle.pitch)*std::cos(angle.roll);minUp=std::min(minUp,up);++poses;
                if(!std::isfinite(up)||up<.94f)throw std::runtime_error("Live player chassis flipped between source frames");}
        }
        totalWraps+=wraps;totalPoses+=poses;report<<car<<",720,"<<wraps<<','<<poses<<','<<minUp<<'\n';report.flush();
        app.clock.accumulator=FixedClock::step*.5;capture("bumper-moving");
        app.drivingView=OriginalDrivingView::Chase;capture("chase-moving");
        // A render-level reproducer for the user's 90-degree/half-turn glitch:
        // the two source roll representations describe almost the same pose.
        app.previousPitch=app.bodyPitch=0;app.previousRoll=app.bodyRoll=0;capture("wrap-baseline");
        app.previousRoll=-.001f;app.bodyRoll=-(2*pi-.001f);capture("wrap-midpoint");
        const auto bytes=[&](const std::string& name){std::ifstream f(output/("car-"+std::to_string(car)+"-"+name+".bmp"),std::ios::binary);return std::vector<unsigned char>{std::istreambuf_iterator<char>(f),{}};};
        const auto baseline=bytes("wrap-baseline"),midpoint=bytes("wrap-midpoint");
        if(baseline.size()!=midpoint.size()||baseline.size()<54)throw std::runtime_error("Pose render extent mismatch");
        unsigned changed=0;for(unsigned i=54;i<baseline.size();++i)if(std::abs(int(baseline[i])-int(midpoint[i]))>8)++changed;
        if(changed>2000)throw std::runtime_error("Wrapped roll changed the rendered car instead of staying upright");
        std::cout<<"car "<<car<<": "<<wraps<<" live wraps, "<<poses<<" upright poses, "<<changed<<" differing image bytes at wrapped midpoint\n";
    }
    if(!totalWraps)throw std::runtime_error("Driving test did not exercise angle wrap");
    std::cout<<"PASS default bumper, keyboard/controller view switching, source camera application, "<<totalWraps<<" live angle wraps, "<<totalPoses<<" stable interpolated chassis poses and three rendered wrap reproducers.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
