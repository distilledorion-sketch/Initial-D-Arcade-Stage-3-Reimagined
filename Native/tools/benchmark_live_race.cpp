#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include <iostream>
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("game-root required");
    App app;app.root=argv[1];app.validationMode=true;app.settings();
    app.originalCamera=OriginalChaseCamera::load(app.root);app.bumperCamera=OriginalChaseCamera::load(app.root,OriginalDrivingView::Bumper);
    app.frontend.initialize(app.root,true);app.hud.loadOriginal(app.root);
    if(!app.renderer.initialize(nullptr,2560,1004,false))throw std::runtime_error(app.renderer.error);
    app.frontend.car=1;app.frontend.battleProfile=original::makeOriginalFreshBattleProfile();app.frontend.battleProfile.setu(16,1);
    app.frontend.gameMode=original::OriginalGameMode::LegendOfTheStreets;app.frontend.battleProfile.setu(0,0);original::selectOriginalRival(app.frontend.battleProfile,8);app.frontend.course=3;app.frontend.rivalChoice=0;
    app.courseIndex=3;app.night=true;app.wet=false;app.reverse=false;app.start();app.best={};app.bestTime=0;
    for(unsigned tick=0;tick<190;++tick)app.simulate({});
    std::cout<<"resolution,course_fraction,vertices,ranges,mesh_cpu_ms,render_cpu_mean_ms,render_cpu_p95_ms\n";
    for(auto [width,height]:{std::pair{1280,720},std::pair{2560,1004}}){
        if(!app.renderer.resize(width,height))throw std::runtime_error(app.renderer.error);
        for(float fraction:{0.f,.25f,.50f,.75f}){
            app.progress=app.course.length*fraction;
            const auto point=app.course.sample(app.progress);app.vehicle.position=app.previous.position=point.center;
            app.clock.accumulator=0;Mesh mesh;double meshMs=0;
            for(unsigned i=0;i<20;++i){mesh.vertices.clear();mesh.ranges.clear();const auto before=std::chrono::steady_clock::now();app.scenery(mesh,app.progress);meshMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-before).count();}
            for(unsigned i=0;i<8;++i)if(!app.render(0))throw std::runtime_error(app.renderer.error);
            std::vector<double> times;
            for(unsigned i=0;i<40;++i){const auto before=std::chrono::steady_clock::now();if(!app.render(0))throw std::runtime_error(app.renderer.error);times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-before).count());}
            double total=0;for(double t:times)total+=t;std::sort(times.begin(),times.end());
            std::cout<<width<<'x'<<height<<','<<fraction<<','<<mesh.vertices.size()<<','<<mesh.ranges.size()<<','<<meshMs/20<<','<<total/times.size()<<','<<times[38]<<std::endl;
        }
    }
    std::cout<<"Hardware offscreen CPU submission measurements; no vsync presentation or isolated GPU timestamp claim.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
