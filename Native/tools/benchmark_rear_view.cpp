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
    app.frontend.gameMode=original::OriginalGameMode::LegendOfTheStreets;original::selectOriginalRival(app.frontend.battleProfile,8);app.start();app.best={};app.bestTime=0;
    for(unsigned i=0;i<190;++i)app.simulate({});
    std::cout<<"resolution,course_fraction,mirror_off_mean_ms,mirror_on_mean_ms,added_ms,mirror_off_p95_ms,mirror_on_p95_ms\n";
    for(auto [w,h]:{std::pair{1280,720},std::pair{2560,1004}}){
        if(!app.renderer.resize(w,h))throw std::runtime_error(app.renderer.error);
        for(float fraction:{0.f,.25f,.5f,.75f}){
            app.progress=app.course.length*fraction;app.clock.accumulator=0;
            for(unsigned i=0;i<16;++i)if(!app.render(0,(i&1)!=0))throw std::runtime_error(app.renderer.error);
            std::array<std::vector<double>,2> times;
            // Alternate order for each pair to reduce changing host-load bias.
            for(unsigned i=0;i<60;++i)for(unsigned j=0;j<2;++j){const bool mirror=((i+j)&1)!=0;
                const auto start=std::chrono::steady_clock::now();if(!app.render(0,mirror))throw std::runtime_error(app.renderer.error);
                times[mirror].push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());}
            double means[2]{};for(unsigned i=0;i<2;++i){for(auto t:times[i])means[i]+=t;means[i]/=times[i].size();std::sort(times[i].begin(),times[i].end());}
            std::cout<<w<<'x'<<h<<','<<fraction<<','<<means[0]<<','<<means[1]<<','<<means[1]-means[0]<<','<<times[0][56]<<','<<times[1][56]<<std::endl;
        }
    }
    std::cout<<"Paired hardware offscreen CPU render/submission measurements. Fixed source camera, selected course assemblies; not interactive FPS or isolated GPU timings.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
