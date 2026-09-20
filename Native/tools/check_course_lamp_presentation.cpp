#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include <iostream>
std::vector<std::uint32_t> pixels(const fs::path& path){
    std::ifstream file(path,std::ios::binary);BITMAPFILEHEADER header{};BITMAPINFOHEADER info{};
    file.read(reinterpret_cast<char*>(&header),sizeof(header));file.read(reinterpret_cast<char*>(&info),sizeof(info));file.seekg(header.bfOffBits);
    std::vector<std::uint32_t> out(std::size_t(info.biWidth)*std::abs(info.biHeight));file.read(reinterpret_cast<char*>(out.data()),out.size()*4);if(!file)throw std::runtime_error("Lamp capture readback");return out;
}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("game-root output-directory required");
    App app;app.root=argv[1];app.validationMode=true;app.settings();const fs::path output=argv[2];fs::create_directories(output);
    app.originalCamera=OriginalChaseCamera::load(app.root);app.bumperCamera=OriginalChaseCamera::load(app.root,OriginalDrivingView::Bumper);
    app.frontend.initialize(app.root,true);app.hud.loadOriginal(app.root);app.audio.configure(app.root);
    if(!app.renderer.initialize(nullptr,1280,720,false))throw std::runtime_error(app.renderer.error);
    std::ofstream report(output/"application.csv");report<<"course,lamp_index,distance,visible_changed_pixels,billboard_vertices,billboard_ranges,render_gpu_ms\n";
    unsigned visible=0,total=0;
    for(unsigned enemy:{5u,18u,15u}){
        app.frontend.car=1;app.frontend.battleProfile=original::makeOriginalFreshBattleProfile();app.frontend.battleProfile.setu(16,1);
        app.frontend.gameMode=original::OriginalGameMode::LegendOfTheStreets;original::selectOriginalRival(app.frontend.battleProfile,enemy);
        app.start();app.best={};app.bestTime=0;for(unsigned i=0;i<360;++i)app.simulate({});
        const auto& assembly=app.courseScene.assemblyForPathIndex(0);std::vector<Vec3> lamps;
        for(const auto& instance:assembly.instances)if(instance.billboard)lamps.push_back({instance.transform[3],instance.transform[7],instance.transform[11]});
        if(lamps.empty())throw std::runtime_error("App did not load recovered course lamps");
        for(unsigned lampIndex=0;lampIndex<lamps.size();++lampIndex){
            const auto lamp=lamps[lampIndex];std::size_t changed=0;unsigned vertices=0,ranges=0;double gpu=0;
            for(float lead:{15.f,25.f,40.f}){
                const auto nearest=app.course.project(lamp);app.progress=std::max(0.f,nearest.sample.distance-lead);const auto point=app.course.sample(app.progress);
                const auto f=normalized(point.tangent);const float yaw=std::atan2(f.x,f.z);const Vec3 angles{std::asin(std::clamp(f.y,-1.f,1.f)),yaw-pi,0};
                app.vehicle.position=app.previous.position=point.center;app.vehicle.yaw=app.previous.yaw=yaw;
                app.playerBodyWorld=app.previousPlayerBodyWorld=point.center+Vec3{0,originalCarRideHeight(1),0};
                app.bodyPitch=app.previousPitch=-angles.x;app.bodyRoll=app.previousRoll=0;
                app.originalCamera.reset();app.bumperCamera.reset();app.previousCameraFrame=app.originalCamera.update(point.center,angles);app.previousBumperFrame=app.bumperCamera.update(point.center,angles);
                app.rearCameraFrame=app.previousRearCameraFrame=app.bumperCamera.rearView(app.playerBodyWorld,angles);
                app.rivalVehicle.position=app.previousRival.position=point.center-f*14.f;app.rivalBodyWorld=app.previousRivalBodyWorld=app.playerBodyWorld-f*14.f;
                app.rivalVehicle.yaw=app.previousRival.yaw=yaw;app.rivalPitch=app.previousRivalPitch=app.bodyPitch;app.rivalRoll=app.previousRivalRoll=0;
                app.clock.accumulator=0;app.drivingView=OriginalDrivingView::Bumper;
                if(!app.render(0))throw std::runtime_error(app.renderer.error);
                const auto frame=app.bumperCamera.frame();Mesh without=app.raceMesh;vertices=ranges=0;
                std::erase_if(without.ranges,[&](const auto& range){if(range.billboard){vertices+=range.count;++ranges;return true;}return false;});
                if(!app.renderer.draw(without,frame.eye,frame.target,true,app.wet)||!app.renderer.saveBitmap((output/"without-lamps.bmp").wstring()))throw std::runtime_error(app.renderer.error);
                app.renderer.measureGpuFrame=true;
                if(!app.renderer.draw(app.raceMesh,frame.eye,frame.target,true,app.wet)||!app.renderer.readGpuMilliseconds(gpu)||!app.renderer.saveBitmap((output/"with-lamps.bmp").wstring()))throw std::runtime_error(app.renderer.error);
                app.renderer.measureGpuFrame=false;
                const auto before=pixels(output/"without-lamps.bmp"),after=pixels(output/"with-lamps.bmp");changed=0;
                for(std::size_t i=0;i<before.size();++i)changed+=before[i]!=after[i];
                if(changed){
                    if(lampIndex==0){
                        if(!app.render(0)||!app.renderer.saveBitmap((output/("course"+std::to_string(app.courseIndex)+"-bumper.bmp")).wstring()))throw std::runtime_error(app.renderer.error);
                        app.drivingView=OriginalDrivingView::Chase;if(!app.render(0)||!app.renderer.saveBitmap((output/("course"+std::to_string(app.courseIndex)+"-chase.bmp")).wstring()))throw std::runtime_error(app.renderer.error);
                        app.drivingView=OriginalDrivingView::Bumper;
                        fs::copy_file(output/"without-lamps.bmp",output/("course"+std::to_string(app.courseIndex)+"-before.bmp"),fs::copy_options::overwrite_existing);
                        fs::copy_file(output/"with-lamps.bmp",output/("course"+std::to_string(app.courseIndex)+"-after.bmp"),fs::copy_options::overwrite_existing);
                    }
                    break;
                }
            }
            report<<app.courseIndex<<','<<lampIndex<<','<<app.progress<<','<<changed<<','<<vertices<<','<<ranges<<','<<gpu<<'\n';report.flush();
            visible+=changed!=0;++total;
        }
    }
    if(total!=49||visible<40)throw std::runtime_error("Too few original lamp vicinity fixtures produce visible effects");
    std::cout<<"PASS "<<visible<<"/"<<total<<" arranged original lamp vicinity fixtures show visible effects; original position/order, ordinary App scene loading, bumper/chase and mirror binding. This is not matched original-hardware imagery.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
