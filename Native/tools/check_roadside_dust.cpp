#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include "../tests/roadside_dust_race_fixture.inl"
#include <iostream>

// Actual source driving and App rendering, offscreen and without audio output.
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("Native root and NEW evidence directory required");
    const auto root=fs::absolute(argv[1]),out=fs::absolute(argv[2]);
    if(fs::exists(out))throw std::runtime_error("Preserve existing evidence");fs::create_directories(out);
    unsigned checks=0;const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);};
    auto app=std::make_unique<App>();app->root=root;app->saveRoot=out/"userdata";app->validationMode=true;app->settings();
    app->frontend.initialize(root,true);app->hud.loadOriginal(root);app->audio.configure(root);
    app->originalCamera=OriginalChaseCamera::load(root);app->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
    check(app->renderer.initialize(nullptr,960,720,true),"Offscreen race renderer failed");
    app->demoPreparationStarted=true;app->replayRecordingFlags=0;
    std::ofstream trace(out/"trace.csv");trace<<"scenario,tick,contacts,clouds,particles,speed,x,y,z,rawContacts\n";
    for(unsigned scenario=0;scenario<4;++scenario){
        prepareRoadsideDustRaceFixture(*app,scenario);
        check(app->roadsideDust.system().order().empty(),"Race start retained previous dust");
        bool hadDust=false,captured=false;unsigned maximumParticles=0;
        for(unsigned tick=0;tick<360;++tick){
            app->simulate(roadsideDustDiagnosticInput(tick));
            const auto& system=app->roadsideDust.system();const auto contacts=app->presentedSession().publishedActors().player0C8FF388[29];
            hadDust|=!system.order().empty();maximumParticles=std::max(maximumParticles,unsigned(system.particleCount()));
            trace<<scenario<<','<<tick<<','<<contacts<<','<<system.order().size()<<','<<system.particleCount()<<','<<app->vehicle.speedKmh()
                <<','<<app->vehicle.position.x<<','<<app->vehicle.position.y<<','<<app->vehicle.position.z<<','<<app->presentedSession().actor().u(116)<<'\n';
            check(system.order().size()<=4&&system.particleCount()<=60,"Race dust exceeded bounded pools");
            if(scenario>=2)check(system.order().empty(),"Dust leaked into night/rain race");
            if(!captured&&tick>=60&&system.particleCount()>=6){
                const auto digest=app->presentedSession().rollbackDigest();
                check(app->render(0),"Actual race dust render failed");
                const auto savedDust=app->roadsideDust;
                const auto savedSmoke=app->carSmoke;
                check(app->renderer.saveBitmap((out/("race-"+std::to_string(scenario)+"-dust.bmp")).wstring()),"Dust race screenshot failed");
                app->roadsideDust.reset();app->carSmoke.reset();check(app->render(0),"Dust comparison render failed");
                check(app->renderer.saveBitmap((out/("race-"+std::to_string(scenario)+"-baseline.bmp")).wstring()),"Baseline race screenshot failed");
                app->roadsideDust=savedDust;app->carSmoke=savedSmoke;
                const auto pixels=[&](const char* suffix){
                    std::ifstream in(out/("race-"+std::to_string(scenario)+suffix+".bmp"),std::ios::binary);
                    BITMAPFILEHEADER header{};in.read(reinterpret_cast<char*>(&header),sizeof header);in.seekg(header.bfOffBits);
                    std::vector<unsigned> result(960*720);in.read(reinterpret_cast<char*>(result.data()),result.size()*4);
                    check(bool(in),"Race pixel readback failed");return result;
                };
                const auto visible=pixels("-dust"),baseline=pixels("-baseline");
                unsigned changed=0,brown=0;
                for(unsigned i=0;i<visible.size();++i)if(visible[i]!=baseline[i]){
                    ++changed;const unsigned r=(visible[i]>>16)&255,g=(visible[i]>>8)&255,b=visible[i]&255;
                    const unsigned br=(baseline[i]>>16)&255,bg=(baseline[i]>>8)&255,bb=baseline[i]&255;
                    // Source destination-alpha blending can darken the green
                    // shoulder while adding brown. Check its shift toward red
                    // relative to both other channels, not positive RGB deltas.
                    brown+=r*bg>g*br&&r*bb>b*br;
                }
                std::cout<<"draw tick="<<tick<<" changedPixels="<<changed<<" brownPixels="<<brown<<'\n';
                check(changed>100&&brown>20,"Actual race dust produced no visible brown contribution");
                check(app->presentedSession().rollbackDigest()==digest,"Drawing changed race simulation");
                // Explicitly exercise ordinary rendered frames without stepping:
                // pause/network wait cannot keep spawning particles.
                const auto count=system.particleCount();for(unsigned repaint=0;repaint<4;++repaint)check(app->render(0),"Repeated race render failed");
                check(system.particleCount()==count,"Race repaint advanced particle lifecycle");captured=true;
            }
        }
        if(scenario<2)check(hadDust&&captured,"Actual source contacts never produced drawable race dust");
        std::cout<<"scenario="<<scenario<<" dust="<<hadDust<<" maxParticles="<<maximumParticles<<" capture="<<captured<<'\n';
    }
    std::ofstream(out/"PASS.txt")<<checks<<" actual App checks; source solver contacts, dry day forward/reverse, excluded night/rain, bounded pools, real D3D race drawing, render state isolation. No online peer or packaged Unity claim.\n";
    std::cout<<"PASS "<<checks<<" actual App roadside dust checks\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
