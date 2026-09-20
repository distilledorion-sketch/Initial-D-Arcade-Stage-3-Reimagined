#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include <iostream>
std::vector<std::uint32_t> readPixels(const fs::path& path){
    std::ifstream f(path,std::ios::binary);BITMAPFILEHEADER head{};BITMAPINFOHEADER info{};
    f.read(reinterpret_cast<char*>(&head),sizeof(head));f.read(reinterpret_cast<char*>(&info),sizeof(info));f.seekg(head.bfOffBits);
    std::vector<std::uint32_t> pixels(std::size_t(info.biWidth)*std::abs(info.biHeight));f.read(reinterpret_cast<char*>(pixels.data()),pixels.size()*4);
    if(!f)throw std::runtime_error("Application capture readback");return pixels;
}
int main(int argc,char**argv)try{
    if(argc!=3)throw std::runtime_error("game-root output-directory required");
    App app;app.root=argv[1];app.validationMode=true;app.settings();const fs::path output=argv[2];fs::create_directories(output);
    app.originalCamera=OriginalChaseCamera::load(app.root);app.bumperCamera=OriginalChaseCamera::load(app.root,OriginalDrivingView::Bumper);
    app.frontend.initialize(app.root,true);app.hud.loadOriginal(app.root);app.audio.configure(app.root);app.load();
    if(!app.renderer.initialize(nullptr,1280,720,false))throw std::runtime_error(app.renderer.error);
    auto save=[&](std::string name){if(!app.renderer.saveBitmap((output/(name+".bmp")).wstring()))throw std::runtime_error(app.renderer.error);};
    std::ofstream report(output/"application.csv");report<<"course,position_offset,view,changed_pixels,gpu_off_ms,gpu_on_ms\n";
    unsigned fixtures=0;
    for(unsigned enemy:{5u,18u,15u}){
        app.frontend.car=1;app.frontend.battleProfile=original::makeOriginalFreshBattleProfile();app.frontend.battleProfile.setu(16,1);
        app.frontend.gameMode=original::OriginalGameMode::LegendOfTheStreets;original::selectOriginalRival(app.frontend.battleProfile,enemy);
        app.start();app.best={};app.bestTime=0;for(unsigned i=0;i<360;++i)app.simulate({});
        const auto lamps=app.courseScene.lampPositions();if(lamps.empty())throw std::runtime_error("No course lamps loaded");
        for(float offset:{-12.f,0.f,12.f})for(auto view:{OriginalDrivingView::Bumper,OriginalDrivingView::Chase}){
            const auto projected=app.course.project(lamps.front());app.progress=std::clamp(projected.sample.distance+offset,0.f,app.course.length);
            const auto point=app.course.sample(app.progress);const auto f=normalized(point.tangent);const float yaw=std::atan2(f.x,f.z);
            const Vec3 angles{std::asin(std::clamp(f.y,-1.f,1.f)),yaw-pi,0};
            app.vehicle.position=app.previous.position=point.center;app.vehicle.yaw=app.previous.yaw=yaw;
            app.playerBodyWorld=app.previousPlayerBodyWorld=point.center+Vec3{0,originalCarRideHeight(1),0};
            app.bodyPitch=app.previousPitch=-angles.x;app.bodyRoll=app.previousRoll=0;
            app.originalCamera.reset();app.bumperCamera.reset();app.previousCameraFrame=app.originalCamera.update(point.center,angles);app.previousBumperFrame=app.bumperCamera.update(point.center,angles);
            app.rearCameraFrame=app.previousRearCameraFrame=app.bumperCamera.rearView(app.playerBodyWorld,angles);
            app.rivalVehicle.position=app.previousRival.position=point.center-f*12;app.rivalBodyWorld=app.previousRivalBodyWorld=app.playerBodyWorld-f*12;
            app.rivalVehicle.yaw=app.previousRival.yaw=yaw;app.rivalPitch=app.previousRivalPitch=app.bodyPitch;app.rivalRoll=app.previousRivalRoll=0;
            app.clock.accumulator=0;app.drivingView=view;
            app.renderer.courseLampLighting=false;if(!app.render(0))throw std::runtime_error(app.renderer.error);save("lighting-off");const auto off=readPixels(output/"lighting-off.bmp");
            app.renderer.courseLampLighting=true;if(!app.render(0))throw std::runtime_error(app.renderer.error);save("lighting-on");const auto on=readPixels(output/"lighting-on.bmp");
            unsigned changed=0;for(std::size_t i=0;i<off.size();++i)changed+=off[i]!=on[i];
            if(changed<100)throw std::runtime_error("No visible surface illumination in application fixture");
            double gpu[2]{};app.renderer.measureGpuFrame=true;
            for(unsigned sample=0;sample<8;++sample)for(unsigned enabled=0;enabled<2;++enabled){
                app.renderer.courseLampLighting=enabled!=0;if(!app.render(0))throw std::runtime_error(app.renderer.error);
                double ms;if(!app.renderer.readGpuMilliseconds(ms))throw std::runtime_error(app.renderer.error);gpu[enabled]+=ms/8;
            }
            app.renderer.measureGpuFrame=false;app.renderer.courseLampLighting=true;
            report<<app.courseIndex<<','<<offset<<','<<(view==OriginalDrivingView::Bumper?"bumper":"chase")<<','<<changed<<','<<gpu[0]<<','<<gpu[1]<<'\n';report.flush();++fixtures;
            if(offset==0){const auto name="course"+std::to_string(app.courseIndex)+(view==OriginalDrivingView::Bumper?"-bumper":"-chase");
                fs::copy_file(output/"lighting-off.bmp",output/(name+"-off.bmp"),fs::copy_options::overwrite_existing);
                fs::copy_file(output/"lighting-on.bmp",output/(name+"-on.bmp"),fs::copy_options::overwrite_existing);}
        }
    }
    app.menu=true;app.frontend.stage=FrontendStage::Make;app.frontend.advance(0);app.frontend.stage=FrontendStage::Title;
    app.renderer.courseLampLighting=true;app.renderer.measureGpuFrame=false;
    unsigned current=0;for(unsigned frame:{0u,30u,871u,901u,925u,1100u,1800u,2500u,3792u,4693u,4720u}){
        app.frontend.advance(double(frame-current)/60.);current=frame;
        if(!app.render(0))throw std::runtime_error(app.renderer.error);save("attract-"+std::to_string(frame));
    }
    if(!app.frontend.showingGasstand()||app.frontend.attractScript()!=1)throw std::runtime_error("Attract loop not wired in application");
    std::ofstream timing(output/"attract-frame-times.csv");timing<<"width,height,mean_cpu_ms,p95_cpu_ms\n";
    for(auto size:{std::pair{640,480},std::pair{1280,720},std::pair{2560,1004}}){
        if(!app.renderer.resize(size.first,size.second))throw std::runtime_error(app.renderer.error);
        std::vector<double> samples;for(unsigned i=0;i<64;++i){
            const auto begin=std::chrono::steady_clock::now();if(!app.render(1./60))throw std::runtime_error(app.renderer.error);
            if(i>=4)samples.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());
        }
        double sum=0;for(double ms:samples)sum+=ms;std::sort(samples.begin(),samples.end());
        timing<<size.first<<','<<size.second<<','<<sum/samples.size()<<','<<samples[56]<<'\n';timing.flush();
    }
    if(!app.renderer.resize(1280,720))throw std::runtime_error(app.renderer.error);
    app.frontend.confirm();for(unsigned i=0;i<4;++i)if(!app.render(1./60))throw std::runtime_error(app.renderer.error);
    if(app.frontend.stage!=FrontendStage::Make)throw std::runtime_error("Cannot start game from conversation");save("start-from-attract");
    std::cout<<"PASS "<<fixtures<<" actual application illumination fixtures with moving player/rival poses, bumper/chase and mirror; animated attract captures and Start-to-menu transition. Arranged offscreen fixtures, not interactive FPS or original hardware parity.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
