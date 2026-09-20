#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include <iostream>
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("game-root required");
    App app;app.root=argv[1];app.validationMode=true;app.settings();app.frontend.initialize(app.root,true);app.hud.loadOriginal(app.root);
    app.courseIndex=3;app.night=true;app.wet=false;app.load();
    app.race.originalTiming=true;app.race.phase=RacePhase::Running;app.race.remaining6000=300000;app.race.elapsed6000=150000;app.race.sectionCapacity=4;
    app.vehicle.rpm=6500;app.vehicle.speed=30;app.vehicle.gear=3;
    UiState state;state.menu=false;state.night=true;state.originalHandling=true;state.course=&app.course;state.race=&app.race;state.car=&app.vehicle;state.frontend=&app.frontend;
    state.battle=true;state.battleEnemy=8;state.battleRivalCar=22;state.battleHudFrame=150;state.battleRivalPositionFraction=.5f;state.battleAdvantage=-20;state.musicName="Test";
    const auto emptyMap=[&]{auto c=app.course;c.points.clear();return c;}();
    std::cout<<"resolution,map,mean_ms,p95_ms,hash\n";
    for(auto [width,height]:{std::pair{1280,720},std::pair{2560,1004}}){app.hud.resize(width,height);
        for(bool map:{true,false}){state.course=map?&app.course:&emptyMap;std::vector<double> times;std::uint64_t hash=14695981039346656037ull;
            for(unsigned frame=0;frame<32;++frame){state.progress=100.f*frame;state.battleHudFrame=150+frame;app.race.elapsed6000=150000+100*frame;app.race.remaining6000=300000-100*frame;
                const auto before=std::chrono::steady_clock::now();const auto pixels=app.hud.paint(state);const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-before).count();
                if(frame>=2)times.push_back(elapsed);
                for(std::size_t p=0;p<std::size_t(width)*height;++p){hash^=pixels[p];hash*=1099511628211ull;}
            }
            double total=0;for(auto t:times)total+=t;std::sort(times.begin(),times.end());
            std::cout<<width<<'x'<<height<<','<<map<<','<<total/times.size()<<','<<times[28]<<','<<hash<<std::endl;
        }
    }
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
