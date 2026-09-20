// Bounded offscreen App profiling; no game-window input or driver writes.
#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include <iostream>
std::vector<char> bytes(const fs::path& path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::invalid_argument("game-root output-directory required");
    App app;app.root=argv[1];app.validationMode=true;app.settings();
    const fs::path output=argv[2];fs::create_directories(output);
    app.originalCamera=OriginalChaseCamera::load(app.root);app.bumperCamera=OriginalChaseCamera::load(app.root,OriginalDrivingView::Bumper);
    app.frontend.initialize(app.root,true);app.hud.loadOriginal(app.root);app.audio.configure(app.root);
    if(!app.renderer.initialize(nullptr,1280,720,false))throw std::runtime_error(app.renderer.error);
    std::ofstream report(output/"frames.csv");
    report<<"course,night,wet,battle,width,height,fraction,eye_x,eye_y,eye_z,vertices,ranges,cold_mesh_mean_ms,hot_mesh_mean_ms,cold_render_mean_ms,hot_render_mean_ms,cold_render_p95_ms,hot_render_p95_ms,gpu_mean_ms,identical_pixels\n";
    for(unsigned scenario=0;scenario<4;++scenario){
        app.frontend.car=1;app.frontend.battleProfile=original::makeOriginalFreshBattleProfile();app.frontend.battleProfile.setu(16,1);
        if(scenario<2){app.frontend.gameMode=original::OriginalGameMode::LegendOfTheStreets;original::selectOriginalRival(app.frontend.battleProfile,scenario==0?12:8);}
        else{app.frontend.gameMode=original::OriginalGameMode::TimeAttack;app.courseIndex=scenario==2?1:5;app.night=true;app.reverse=false;app.wet=scenario==3;}
        app.start();app.best={};app.bestTime=0;
        for(unsigned tick=0;tick<360;++tick)app.simulate({});
        for(auto [w,h]:{std::pair{1280,720},std::pair{2560,1004}}){
            if(!app.renderer.resize(w,h))throw std::runtime_error(app.renderer.error);
            for(float fraction:{.02f,.25f,.5f,.75f}){
                app.progress=app.course.length*fraction;const auto point=app.course.sample(app.progress);
                const auto f=normalized(point.tangent);const float yaw=std::atan2(f.x,f.z);
                const Vec3 angles{std::asin(std::clamp(f.y,-1.f,1.f)),yaw-pi,0};
                app.vehicle.position=app.previous.position=point.center;app.vehicle.yaw=app.previous.yaw=yaw;
                app.playerBodyWorld=app.previousPlayerBodyWorld=point.center+Vec3{0,originalCarRideHeight(1),0};
                app.bodyPitch=app.previousPitch=-angles.x;app.bodyRoll=app.previousRoll=0;
                app.originalCamera.reset();app.bumperCamera.reset();
                app.previousCameraFrame=app.originalCamera.update(point.center,angles);
                app.previousBumperFrame=app.bumperCamera.update(point.center,angles);
                app.rearCameraFrame=app.previousRearCameraFrame=app.bumperCamera.rearView(app.playerBodyWorld,angles);
                // An arranged rival behind the camera exercises the mirror.
                app.rivalVehicle.position=app.previousRival.position=point.center-f*14.f;
                app.rivalBodyWorld=app.previousRivalBodyWorld=app.playerBodyWorld-f*14.f;
                app.rivalVehicle.yaw=app.previousRival.yaw=yaw;
                app.rivalPitch=app.previousRivalPitch=app.bodyPitch;app.rivalRoll=app.previousRivalRoll=0;
                app.clock.accumulator=0;
                Mesh mesh;std::array<double,2> meshTimes{};
                for(unsigned repeat=0;repeat<30;++repeat)for(unsigned mode=0;mode<2;++mode){
                    const unsigned hot=(repeat+mode)&1;if(!hot)app.courseMeshCache.invalidate();
                    mesh.vertices.clear();mesh.ranges.clear();const auto start=std::chrono::steady_clock::now();app.scenery(mesh,app.progress);
                    meshTimes[hot]+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
                }
                for(unsigned i=0;i<6;++i)if(!app.render(0))throw std::runtime_error(app.renderer.error);
                std::array<std::vector<double>,2> times;
                for(unsigned i=0;i<24;++i)for(unsigned j=0;j<2;++j){
                    const unsigned hot=(i+j)&1;if(!hot)app.courseMeshCache.invalidate();
                    const auto start=std::chrono::steady_clock::now();if(!app.render(0))throw std::runtime_error(app.renderer.error);
                    times[hot].push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
                }
                std::array<double,2> mean{};
                for(unsigned i=0;i<2;++i){for(auto t:times[i])mean[i]+=t;mean[i]/=times[i].size();std::sort(times[i].begin(),times[i].end());}
                app.renderer.measureGpuFrame=true;double gpuTotal=0;
                for(unsigned i=0;i<3;++i){double gpu=0;if(!app.render(0)||!app.renderer.readGpuMilliseconds(gpu))throw std::runtime_error(app.renderer.error);gpuTotal+=gpu;}
                app.renderer.measureGpuFrame=false;
                app.courseMeshCache.invalidate();
                if(!app.render(0)||!app.renderer.saveBitmap((output/"cold.bmp").wstring()))throw std::runtime_error(app.renderer.error);
                if(!app.render(0)||!app.renderer.saveBitmap((output/"hot.bmp").wstring()))throw std::runtime_error(app.renderer.error);
                if(bytes(output/"cold.bmp")!=bytes(output/"hot.bmp"))throw std::runtime_error("Cached course changed App pixels");
                if(w==1280&&fraction==.25f)fs::copy_file(output/"hot.bmp",output/("course"+std::to_string(app.courseIndex)+".bmp"),fs::copy_options::overwrite_existing);
                report<<app.courseIndex<<','<<app.night<<','<<app.wet<<','<<app.battle<<','<<w<<','<<h<<','<<fraction<<','<<app.camera.x<<','<<app.camera.y<<','<<app.camera.z<<','<<mesh.vertices.size()<<','<<mesh.ranges.size()<<','<<meshTimes[0]/30<<','<<meshTimes[1]/30<<','<<mean[0]<<','<<mean[1]<<','<<times[0][22]<<','<<times[1][22]<<','<<gpuTotal/3<<','<<std::uint64_t(w)*h<<'\n';report.flush();
                std::cout<<"course "<<app.courseIndex<<' '<<w<<'x'<<h<<" fraction "<<fraction<<": CPU "<<mean[0]<<" -> "<<mean[1]<<" ms; GPU "<<gpuTotal/3<<" ms; pixels identical\n";
            }
        }
    }
    std::cout<<"PASS 32 camera/scene/resolution fixtures. Cold cache forces the old course transform; both paths retain frame-buffer capacity. GPU intervals are D3D11 timestamps; CPU includes HUD and submission. Arranged poses, no vsync or interactive FPS claim.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
