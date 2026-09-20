// Reuse the actual App, isolated-save checks, adapter query and readback
// helpers. The old benchmark entrypoint is compiled but never invoked.
#define main includedCourseLightingPerformanceFixture
#include "benchmark_course_environment_application.cpp"
#undef main

int main(int argc,char**argv)try{
    if(argc!=3)throw std::invalid_argument("Marked private root and output directory required");
    const auto isolated=fs::canonical(argv[1]),output=fs::absolute(argv[2]);
    if(isolated.parent_path()!=fs::canonical(fs::current_path()/"work")||!fs::exists(isolated/"CAR_LIGHTING_BENCHMARK_ROOT.txt"))
        throw std::runtime_error("Refusing unmarked car-light benchmark root");
    if(fs::exists(isolated/"userdata")&&(GetFileAttributesW((isolated/"userdata").c_str())&FILE_ATTRIBUTE_REPARSE_POINT))
        throw std::runtime_error("Car-light benchmark cannot redirect real userdata");
    const auto realRoot=fs::canonical(isolated/"data").parent_path();
    const auto realSaves=snapshot(realRoot/"userdata"),privateSaves=snapshot(isolated/"userdata");
    fs::create_directories(output);unsigned checks=0;auto check=[&](bool value,const char*why){++checks;if(!value)throw std::runtime_error(why);};
    LARGE_INTEGER frequency{};check(QueryPerformanceFrequency(&frequency),"CPU counter frequency unavailable");
    std::ofstream report(output/"report.txt"),cpuCsv(output/"cpu_samples.csv"),gpuCsv(output/"gpu_samples.csv"),summary(output/"summary.csv");
    cpuCsv<<"scene,block,iterations,mean_callback_ms\n";gpuCsv<<"scene,block,enabled,sample,gpu_ms\n";summary<<"scene,metric,enabled,samples,min_ms,median_ms,p95_ms,max_ms,mean_ms\n";
    report<<"Car ARRAY/embedded SPOT actual App diagnostic\nDefault hardware adapter probe: "<<defaultHardwareAdapter()
        <<"\nCPU performance-counter frequency: "<<frequency.QuadPart<<" Hz. Batched means amortize timer/loop overhead; individual calls are not timed."
        <<"\nCPU: frozen natural420-tick pose; actual App publishCarLighting for each active car (private path projection+world SPOT), updateCarAmbient, composeRaceCarLights, advanceCarLightGain."
        <<"\n600 warmup callbacks,200 measured blocks of500 callbacks. This is isolated new callback cost, not a whole-tick delta; source projector/audio/solver are not rerun."
        <<"\nGPU: actual frozen1280x720 App geometry, fog, HUD, camera/mirror; only source course/player/rival light pointers change."
        <<"\nEnabled uses three final source scopes; baseline uses the prior course-only scope and null player/rival pointers. Native/projected geometry and native beam flags remain identical."
        <<"\n24 warmups and120 timestamp samples per branch, alternating15-frame blocks. Daylight GPU shade is an explicitly controlled source presentation coordinate using the darkest authored cell; CPU uses natural pose coordinates."
        <<"\nNo visible window, Present/vsync, audio device or save writes. No overall/playable FPS claim.\n\n";
    auto summarize=[&](const char*scene,const char*metric,bool enabled,const std::vector<double>&values){const auto d=distribution(values);
        summary<<scene<<','<<metric<<','<<enabled<<','<<values.size()<<','<<std::setprecision(10)<<d.minimum<<','<<d.median<<','<<d.p95<<','<<d.maximum<<','<<d.mean<<'\n';return d;};
    for(const auto scene:{Scene{"happo-night-battle-mirror",4,true,true,19},Scene{"akina-day-shade",3,false,false,0}}){
        auto app=std::make_unique<App>();app->root=isolated;app->validationMode=true;app->settings();
        app->originalCamera=OriginalChaseCamera::load(isolated);app->bumperCamera=OriginalChaseCamera::load(isolated,OriginalDrivingView::Bumper);
        app->frontend.initialize(isolated,true);app->hud.loadOriginal(isolated);app->audio.configure(isolated);
        app->frontend.car=0;app->automatic=app->frontend.automatic=true;app->frontend.battleProfile=original::makeOriginalFreshBattleProfile();
        app->frontend.gameMode=scene.battle?original::OriginalGameMode::LegendOfTheStreets:original::OriginalGameMode::TimeAttack;
        if(scene.battle){app->frontend.battleProfile.setu(0,0);original::selectOriginalRival(app->frontend.battleProfile,scene.enemy);}
        app->courseIndex=int(scene.course);app->reverse=false;app->night=scene.night;app->wet=false;app->start();
        DriverInput input;input.automatic=true;input.throttle=.55f;for(unsigned i=0;i<420;++i)app->simulate(input);
        const auto actor=app->originalSession.actor().words;const auto drive=app->originalSession.vehicle().drive.words;
        const auto published=app->originalSession.publishedActors();const auto seed=app->originalSession.contactCompletion().randomSeed0C37C778;
        const auto playerQuery=app->playerBody.query().words,rivalQuery=app->rivalBody.query().words;
        const auto frame=app->originalRaceOwnerFrame;const auto fog=app->raceFog;
        auto cpuCallback=[&](){app->publishCarLighting(false);app->updateCarAmbient(false,app->projectedLightPriorAdvantage,false);
            if(app->rivalVisible){app->publishCarLighting(true);app->updateCarAmbient(true,app->projectedLightPriorAdvantage,app->rivalProjectedHeadlight.enabled());}
            app->composeRaceCarLights();app->advanceCarLightGain();};
        for(unsigned i=0;i<600;++i)cpuCallback();std::vector<double> cpuTimes;
        constexpr unsigned iterations=500,blocks=200;
        for(unsigned block=0;block<blocks;++block){const auto begin=std::chrono::steady_clock::now();for(unsigned i=0;i<iterations;++i)cpuCallback();
            const auto end=std::chrono::steady_clock::now();const auto value=std::chrono::duration<double,std::milli>(end-begin).count()/iterations;
            cpuTimes.push_back(value);cpuCsv<<scene.label<<','<<block<<','<<iterations<<','<<std::setprecision(10)<<value<<'\n';}
        const auto cpu=summarize(scene.label,"isolated_callback",true,cpuTimes);
        if(!scene.night){const auto values=app->carLightGain.values();check(!values.empty(),"Daylight source table absent");
            const auto lowest=std::min_element(values.begin(),values.end()-1);check(*lowest<.95f,"No authored shade discriminator");
            app->playerLightCoordinate={int(lowest-values.begin()),0};app->advanceCarLightGain();
            report<<scene.label<<" controlled GPU shade: source index="<<app->playerLightCoordinate.index<<", gain="<<app->playerCarLight.gain<<".\n";}
        app->clock.reset();app->drivingView=scene.battle?OriginalDrivingView::Bumper:OriginalDrivingView::Chase;
        check(app->renderer.initialize(nullptr,1280,720,false),app->renderer.error.c_str());check(app->render(0,true),app->renderer.error.c_str());
        const auto appImage=output/(std::string(scene.label)+"-app.bmp");check(app->renderer.saveBitmap(appImage.wstring()),"App image failed");
        const auto overlay=idas3::CourseMapTestAccess::pixels(app->hud);const auto eye=app->camera;
        const auto target=scene.battle?app->previousBumperFrame.target:app->previousCameraFrame.target;
        std::optional<OriginalRearViewFrame> rear;if(scene.battle){rear=app->rearCameraFrame;rear->eye=app->previousRearCameraFrame.eye;rear->target=app->previousRearCameraFrame.target;rear->up=normalized(app->previousRearCameraFrame.up);}
        const auto*course=app->renderer.courseLighting,*player=app->renderer.playerLighting,*rival=app->renderer.rivalLighting;
        check(course==&app->raceLightSets->course&&player==&app->raceLightSets->player,"Final source ARRAYs are not bound");
        auto draw=[&](bool enabled){app->renderer.courseLighting=enabled?course:&*app->raceLighting;
            app->renderer.playerLighting=enabled?player:nullptr;app->renderer.rivalLighting=enabled?rival:nullptr;
            check(app->renderer.draw(app->raceMesh,eye,target,app->night,app->wet,overlay.data(),false,nullptr,rear?&*rear:nullptr),app->renderer.error.c_str());};
        draw(true);const auto direct=output/(std::string(scene.label)+"-direct-source.bmp");check(app->renderer.saveBitmap(direct.wstring()),"Direct source image failed");
        check(readBytes(direct)==readBytes(appImage),"Direct source draw differs from actual App frame");
        draw(false);const auto legacy=output/(std::string(scene.label)+"-direct-prior.bmp");check(app->renderer.saveBitmap(legacy.wstring()),"Prior scope image failed");
        check(readBytes(legacy)!=readBytes(direct),"Lighting branches did not discriminate");
        app->renderer.measureGpuFrame=true;std::array<std::vector<double>,2> gpu;
        auto timed=[&](bool enabled,bool keep,unsigned block){draw(enabled);double value=0;check(app->renderer.readGpuMilliseconds(value),app->renderer.error.c_str());
            check(std::isfinite(value)&&value>=0,"Invalid GPU interval");if(keep){gpu[enabled].push_back(value);gpuCsv<<scene.label<<','<<block<<','<<enabled<<','<<gpu[enabled].size()<<','<<std::setprecision(10)<<value<<'\n';}};
        for(unsigned i=0;i<24;++i){timed(false,false,0);timed(true,false,0);}
        for(unsigned block=0;block<8;++block)for(unsigned turn=0;turn<2;++turn)for(unsigned i=0;i<15;++i)timed((block+turn)%2==0,true,block);
        const auto on=summarize(scene.label,"GPU",true,gpu[1]),off=summarize(scene.label,"GPU",false,gpu[0]);
        report<<scene.label<<": isolated new callback median "<<cpu.median<<" ms, p95 "<<cpu.p95<<" ms. GPU source median "<<on.median
            <<" ms / prior "<<off.median<<" ms, delta "<<on.median-off.median<<" ms; p95 "<<on.p95<<" / "<<off.p95<<" ms. "
            <<app->raceMesh.vertices.size()<<" vertices / "<<app->raceMesh.ranges.size()<<" ranges.\n\n";
        check(app->originalSession.actor().words==actor&&app->originalSession.vehicle().drive.words==drive&&app->originalSession.contactCompletion().randomSeed0C37C778==seed&&app->originalRaceOwnerFrame==frame,
            "Benchmark changed original driver/RNG/owner state");
        check(app->originalSession.publishedActors().player0C8FF388==published.player0C8FF388&&app->originalSession.publishedActors().secondary0C8FF430==published.secondary0C8FF430&&
              app->playerBody.query().words==playerQuery&&app->rivalBody.query().words==rivalQuery,"Lighting callbacks changed published actors/body queries");
        check(app->raceFog.table==fog.table&&app->raceFog.packedDensity==fog.packedDensity&&app->raceFog.colorRgb==fog.colorRgb,"Benchmark changed source fog");
        std::cout<<scene.label<<": CPU "<<cpu.median<<" ms; GPU "<<on.median<<" / "<<off.median<<" ms (delta "<<on.median-off.median<<").\n"<<std::flush;
    }
    check(snapshot(realRoot/"userdata")==realSaves&&snapshot(isolated/"userdata")==privateSaves,"Car-light benchmark changed save files");
    report<<"PASS "<<checks<<" checks;201200 isolated callback executions,400 amortized CPU samples,480 measured GPU samples. "<<realSaves.size()<<" real save files unchanged in bytes/timestamps.\n";
    std::cout<<"PASS "<<checks<<" checks;400 CPU block samples /480 GPU samples; saves unchanged.\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
