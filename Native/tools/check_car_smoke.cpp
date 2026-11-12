#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include "../src/original_car_smoke_system.h"
#include "../tests/roadside_dust_race_fixture.inl"
#include <iostream>
#include "../tests/online_car_smoke_app_tests.inl"
#include "../tests/car_smoke_mirror_checks.inl"

// Actual driving outputs independently reconstruct the integrated controller.
// No synthetic contact/intensity overrides or audio device. Both standalone
// density rendering and actual race scene rendering must show the same state.
int main(int argc,char** argv)try{
    if(argc==4&&std::string(argv[3])=="--mirror-stop-sequence")return checkMirrorStopSequence(fs::absolute(argv[1]),fs::absolute(argv[2]));
    if(argc==4&&std::string(argv[3])=="--stationary-shoulder")return checkStationaryShoulderSmoke(fs::absolute(argv[1]),fs::absolute(argv[2]));
    if(argc==4&&std::string(argv[3])=="--stopped-mirror")return checkStoppedMirrorSmoke(fs::absolute(argv[1]),fs::absolute(argv[2]));
    if(argc!=3)throw std::runtime_error("Native root and NEW evidence directory required");
    const auto root=fs::absolute(argv[1]),out=fs::absolute(argv[2]);
    if(fs::exists(out))throw std::runtime_error("Preserve existing evidence");fs::create_directories(out);
    unsigned checks=0;const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);};
    auto app=std::make_unique<App>();app->root=root;app->saveRoot=out/"userdata";app->validationMode=true;app->settings();
    app->frontend.initialize(root,true);app->hud.loadOriginal(root);app->audio.configure(root);
    app->originalCamera=OriginalChaseCamera::load(root);app->bumperCamera=OriginalChaseCamera::load(root,OriginalDrivingView::Bumper);
    check(app->renderer.initialize(nullptr,960,720,true),"Offscreen smoke renderer failed");
    const auto textures=NativeTextureBank::load(root/"data/original_assets/effects/smoke/textures.idastex");
    RoadsideDustPresentation display;display.load(root,textures);
    Renderer isolated;check(isolated.initialize(nullptr,960,720,true),"Isolated renderer failed");
    check(isolated.loadTextures(textures),"Smoke texture upload failed");
    app->demoPreparationStarted=true;app->replayRecordingFlags=0;
    std::ofstream trace(out/"trace.csv");trace<<"scenario,tick,intensity,contacts,emitters,white,brown,particles,rng,speedKmh,slip,brake,steering\n";
    unsigned whiteTicks=0,brownTicks=0,wetTicks=0,mixedTicks=0,images=0;
    for(unsigned scenario=0;scenario<4;++scenario){
        prepareRoadsideDustRaceFixture(*app,scenario,500,scenario>=2?0.f:.55f);
        app->archiveThisRace=true;app->archivePublished=false;
        app->recording.beginCapture(true);app->recording.beginSmokeCapture();
        CarSmokeState expected;expected.reset(app->wet?1:0);bool captured=false;
        const auto& smoke=expected.system();
        for(unsigned tick=0;tick<600;++tick){
            auto input=roadsideDustDiagnosticInput(tick);
            if(scenario>=2){
                // Stay on the road long enough to reach speed, then use the
                // real brake input. The earlier dust-only shoulder fixture
                // never produced a nonzero tire-smoke intensity at night.
                const auto projection=app->course.project(app->vehicle.position,app->segment);
                const float lookahead=std::max(12.f,app->vehicle.speed*.65f);
                const auto target=app->course.sample(projection.sample.distance+lookahead).center-app->vehicle.position;
                const auto angle=wrapAngle(std::atan2(target.x,target.z)-app->vehicle.yaw);
                input.steer=-std::clamp(std::atan2(2*app->config.wheelbase*std::sin(angle),lookahead)/recoveredSteeringLimit,-.75f,.75f);
                if(tick>=240&&tick%120<30){input.throttle=0;input.brake=1;}
                // Also exercise a real sharp turn. Straight-line braking
                // alone emits tiny short-lived clouds obscured by the body.
                if(scenario==2&&tick>=300&&tick<320)input.steer=.8f;
            }
            app->simulate(input);
            const auto digest=app->presentedSession().rollbackDigest();
            const auto& actor=app->presentedSession().publishedActors().player0C8FF388;
            std::array<original::OriginalCarSmokeInput,2> cars;
            auto& car=cars[0];car.available=true;
            const std::array<float,3> delta{std::bit_cast<float>(actor[3]),std::bit_cast<float>(actor[4]),std::bit_cast<float>(actor[5])};
            car.owner=original::originalRoadsideDustOwner(app->actorVisualMatrix(actor),app->carPresentation.originalEffectOrigins(),delta);
            for(unsigned wheel=0;wheel<4;++wheel){
                car.wheelSurfaces[wheel]=std::uint8_t(actor[29]>>(wheel*8));
                car.intensity[wheel]=std::uint8_t(actor[21]>>(wheel*8));
            }
            check(app->recording.smokeFrames.size()==tick+1,"Live race missed the pre-effect shared seed");
            expected.advanceRecorded({7,unsigned(app->night),unsigned(app->wet),7,2,(app->battleProfile.u(1180)&128)!=0},cars,
                app->recording.smokeFrames.back().randomBefore);
            const auto random=expected.randomState();
            check(app->displayedCarSmoke().digest()==expected.digest(),"Normal race smoke differs from independently bound source actor inputs");
            unsigned white=0,brown=0;
            for(auto index:smoke.order()){white+=smoke.cloud(index).color==0xffffffff;brown+=smoke.cloud(index).color==0xff8b5439;}
            whiteTicks+=white>0;brownTicks+=brown>0;wetTicks+=scenario==3&&white>0;mixedTicks+=white>0&&brown>0;
            trace<<scenario<<','<<tick<<','<<actor[21]<<','<<actor[29]<<','<<smoke.order().size()<<','<<white<<','<<brown<<','<<smoke.particleCount()<<','<<random<<','<<app->vehicle.speedKmh()<<','<<app->vehicle.slip<<','<<input.brake<<','<<input.steer<<'\n';
            check(smoke.order().size()<=4&&smoke.particleCount()<=60,"Shared smoke pools exceeded");
            if(scenario>=2)check(brown==0,"Dust admitted during night/rain");
            if(!captured&&tick>60&&smoke.particleCount()>3){
                const auto before=smoke;const auto rngBefore=random;
                const auto eye=app->vehicle.position-forward(app->vehicle.yaw)*8+Vec3{0,1.6f,0};
                const auto target=app->vehicle.position+Vec3{0,.6f,0};
                const auto fov=1.2333658933f;
                Mesh visible;
                if(display.appendSystem(smoke,visible,eye,target,{0,1,0},fov,4.f/3.f,0,1)==0)continue;
                for(unsigned repeat=0;repeat<20;++repeat){Mesh repaint;
                    display.appendSystem(smoke,repaint,eye,target,{0,1,0},fov,4.f/3.f,0,1);
                    check(repaint.vertices.size()==visible.vertices.size()&&std::memcmp(repaint.vertices.data(),visible.vertices.data(),visible.vertices.size()*sizeof(Vertex))==0,"Smoke repaints differ");
                }
                check(smoke.particleCount()==before.particleCount()&&random==rngBefore,"Render changed smoke clock/RNG");
                auto& renderer=isolated;renderer.overrideClearColor=true;renderer.clearColor={.12f,.18f,.28f,1};
                renderer.verticalFieldOfView=fov;renderer.nearClip=.05f;
                renderer.vehicleLights=renderer.opponentLights=false;
                const auto capture=[&](const Mesh& mesh,const char* suffix){
                    check(renderer.draw(mesh,eye,target,false,false),"Shared smoke draw failed");
                    const auto path=out/("scenario-"+std::to_string(scenario)+suffix+".bmp");
                    check(renderer.saveBitmap(path.wstring()),"Smoke readback failed");
                    std::ifstream in(path,std::ios::binary);BITMAPFILEHEADER header{};in.read(reinterpret_cast<char*>(&header),sizeof header);in.seekg(header.bfOffBits);
                    std::vector<unsigned> pixels(960*720);in.read(reinterpret_cast<char*>(pixels.data()),pixels.size()*4);check(bool(in),"Truncated readback");return pixels;
                };
                const auto on=capture(visible,"-on"),off=capture(Mesh{},"-off");unsigned changed=0;
                for(unsigned i=0;i<on.size();++i)changed+=(on[i]&0xffffff)!=(off[i]&0xffffff);
                if(changed>100){
                    check(app->render(0),"Integrated race render failed");
                    const auto saved=app->carSmoke;const auto savedHash=saved.digest();
                    const auto scene=[&](const char* suffix){
                        check(app->render(0),"Integrated smoke comparison render failed");
                        const auto path=out/("race-"+std::to_string(scenario)+suffix+".bmp");
                        check(app->renderer.saveBitmap(path.wstring()),"Integrated race screenshot failed");
                        std::ifstream in(path,std::ios::binary);BITMAPFILEHEADER header{};in.read(reinterpret_cast<char*>(&header),sizeof header);in.seekg(header.bfOffBits);
                        std::vector<unsigned> pixels(960*720);in.read(reinterpret_cast<char*>(pixels.data()),pixels.size()*4);check(bool(in),"Race screenshot truncated");return pixels;
                    };
                    const auto raceOn=scene("-on");app->carSmoke.reset(app->wet?1:0);const auto raceOff=scene("-off");
                    app->carSmoke=saved;const auto raceRestored=scene("-restored");unsigned raceChanged=0;
                    for(unsigned i=0;i<raceOn.size();++i){
                        check(raceOn[i]==raceRestored[i],"Race repaint changed restored smoke pixels");
                        raceChanged+=(raceOn[i]&0xffffff)!=(raceOff[i]&0xffffff);
                    }
                    check(app->carSmoke.digest()==savedHash,"Race draw advanced smoke state/RNG");
                    check(app->drivingEffects.smokeCount()==0,"Host smoke duplicated recovered smoke");
                    // Fresh, small particles can be entirely behind the car.
                    // Keep driving until the actual chase view sees a cloud;
                    // do not remove depth testing or change effect strength.
                    if(raceChanged>0){captured=true;++images;std::cout<<"scenario="<<scenario<<" tick="<<tick<<" changedPixels="<<changed<<" racePixels="<<raceChanged<<" white="<<white<<" brown="<<brown<<'\n'<<std::flush;}
                }
            }
            check(app->presentedSession().rollbackDigest()==digest,"Smoke probe changed driving state");
        }
        check(captured,"Scenario produced no visible recovered smoke");
    }
    check(whiteTicks&&brownTicks&&wetTicks,"Live source smoke/dust coverage missing");
    const auto onlineChecks=runOnlineCarSmokeAppTests(*app,out);checks+=onlineChecks;
    std::ofstream report(out/"PASS.txt");
    report<<"PASS "<<checks<<" checks; 2400 offline driving frames and 960 online App frames plus bounded stalls; onlineChecks="<<onlineChecks<<" whiteTicks="<<whiteTicks<<" brownTicks="<<brownTicks<<" wetTicks="<<wetTicks<<" mixedTicks="<<mixedTicks<<" images="<<images<<".\n"
        <<"Original actor intensity/contact bytes; private visual RNG; bounded shared smoke/dust; normal App state matches independently bound inputs; unchanged driving digest; restored full-scene pixels; no duplicate host smoke; offscreen without audio. New combined replay payload and full-scene source RNG scheduling remain pending.\n";
    std::cout<<"PASS "<<checks<<" checks, 2400 offline and 960 online App frames, "<<images<<" offline and 2 online offscreen captures.\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
