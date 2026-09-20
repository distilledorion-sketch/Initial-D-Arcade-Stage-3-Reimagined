#include "../src/original_headlight_projection.h"
#include <chrono>
namespace idas3::original {
struct ProjectorMeasurement {
    inline static bool enabled=true,measuring=false;
    inline static double milliseconds=0;
    inline static unsigned calls=0,queries=0;
    static void clear(){milliseconds=0;calls=queries=0;}
};
class MeasuredHeadlightProjection:public OriginalHeadlightProjection {
public:
    unsigned advance(const Matrix&matrix,const Query&query){
        if(!ProjectorMeasurement::measuring)return OriginalHeadlightProjection::advance(matrix,query);
        const auto begin=std::chrono::steady_clock::now();
        const unsigned count=ProjectorMeasurement::enabled?OriginalHeadlightProjection::advance(matrix,query):0;
        const auto end=std::chrono::steady_clock::now();
        ProjectorMeasurement::milliseconds+=std::chrono::duration<double,std::milli>(end-begin).count();
        ++ProjectorMeasurement::calls;ProjectorMeasurement::queries+=count;return count;
    }
};
}
// Reuse the established actual-App inclusion, readback, save isolation and
// adapter probe. Its old benchmark entrypoint is compiled but never invoked.
#define OriginalHeadlightProjection MeasuredHeadlightProjection
#define main includedCourseEnvironmentBenchmark
#include "benchmark_course_environment_application.cpp"
#undef main
#undef OriginalHeadlightProjection

namespace {
DriverInput benchmarkInput(const App&app,unsigned frame){
    DriverInput input;input.automatic=true;input.throttle=frame%300<250?.6f:.15f;
    const auto at=app.course.project(app.vehicle.position,app.segment);const float ahead=std::max(12.f,app.vehicle.speed*.65f);
    const auto direction=app.course.sample(at.sample.distance+ahead).center-app.vehicle.position;
    const float error=wrapAngle(std::atan2(direction.x,direction.z)-app.vehicle.yaw);
    input.steer=-std::clamp(std::atan2(2*app.config.wheelbase*std::sin(error),ahead)/recoveredSteeringLimit,-.6f,.6f);return input;
}
bool projectedRange(const MeshRange&range){return range.original&&range.tsp==0x4489a464u&&(range.gmp&0x200u)!=0;}
}
int main(int argc,char**argv)try{
    if(argc!=3)throw std::invalid_argument("Marked private root and evidence directory required");
    const auto isolated=fs::canonical(argv[1]),output=fs::absolute(argv[2]);
    if(isolated.parent_path()!=fs::canonical(fs::current_path()/"work")||!fs::exists(isolated/"PROJECTED_HEADLIGHT_BENCHMARK_ROOT.txt"))
        throw std::runtime_error("Refusing unmarked/non-work benchmark root");
    if(fs::exists(isolated/"userdata")&&(GetFileAttributesW((isolated/"userdata").c_str())&FILE_ATTRIBUTE_REPARSE_POINT))
        throw std::runtime_error("Benchmark userdata cannot redirect to real saves");
    const auto realRoot=fs::canonical(isolated/"data").parent_path();
    const auto realSaves=snapshot(realRoot/"userdata"),privateSaves=snapshot(isolated/"userdata");
    fs::create_directories(output);unsigned checks=0;
    const auto check=[&](bool value,const char*message){++checks;if(!value)throw std::runtime_error(message);};
    const auto adapter=defaultHardwareAdapter();
    std::ofstream report(output/"report.txt"),cpuRaw(output/"cpu_samples.csv"),gpuRaw(output/"gpu_samples.csv"),summary(output/"summary.csv");
    cpuRaw<<"scene,frame,enabled,simulate_ms,working_update_ms,working_calls,road_queries\n";
    gpuRaw<<"scene,block,sample,enabled,gpu_ms,submit_ms\n";
    summary<<"scene,metric,enabled,samples,min_ms,median_ms,p95_ms,max_ms,mean_ms\n";
    report<<"Projected-headlight actual App performance diagnostic\nDefault D3D11 hardware adapter probe: "<<adapter
        <<"\nRenderer uses the same default hardware creation arguments; its private adapter is not directly queried."
        <<"\nCPU: two actual App instances, identical inputs/state, source projector working update enabled versus skipped; publication/other simulation retained."
        <<"\n240 paired warmup ticks, then1024 measured paired ticks, alternating execution order every frame. Per-call clock overhead is present in both variants."
        <<"\nGPU: frozen actual App mesh, source fog/light state, exact HUD/camera/mirror,1280x720 hardware offscreen; only projected range counts differ."
        <<"\n24 warmup frames per branch,120 timestamp samples per branch in alternating15-frame blocks."
        <<"\nThese timings exclude presentation/vsync, audio-device output, asset loading and overall host scheduling. They do not predict playable FPS."
        <<"\nCPU whole-tick differences include scheduling noise. Isolated working-update time includes the source transform and actual road queries plus timer overhead, not publication or setup.\n\n";
    auto summarize=[&](const char*scene,const char*metric,bool enabled,const std::vector<double>&values){const auto d=distribution(values);
        summary<<scene<<','<<metric<<','<<enabled<<','<<values.size()<<','<<std::setprecision(10)<<d.minimum<<','<<d.median<<','<<d.p95<<','<<d.maximum<<','<<d.mean<<'\n';return d;};
    using Meter=original::ProjectorMeasurement;
    for(const auto scene:{Scene{"akina-night",3,true,false,0},Scene{"myogi-day-control",0,false,false,0},Scene{"happo-night-battle-mirror",4,true,true,19}}){
        auto initialize=[&]{
            auto app=std::make_unique<App>();app->root=isolated;app->validationMode=true;app->settings();
            app->originalCamera=OriginalChaseCamera::load(isolated);app->bumperCamera=OriginalChaseCamera::load(isolated,OriginalDrivingView::Bumper);
            app->frontend.initialize(isolated,true);app->hud.loadOriginal(isolated);app->audio.configure(isolated);
            app->courseIndex=int(scene.course);app->night=scene.night;app->wet=false;app->reverse=false;
            app->frontend.car=0;app->automatic=app->frontend.automatic=true;app->frontend.battleProfile=original::makeOriginalFreshBattleProfile();
            app->frontend.gameMode=scene.battle?original::OriginalGameMode::LegendOfTheStreets:original::OriginalGameMode::TimeAttack;
            if(scene.battle){app->frontend.battleProfile.setu(0,0);original::selectOriginalRival(app->frontend.battleProfile,scene.enemy);}
            app->drivingView=OriginalDrivingView::Bumper;Meter::measuring=false;app->start();return app;
        };
        auto source=initialize(),control=initialize();std::array<std::vector<double>,2> ticks,working;
        std::uint64_t queries=0;
        Meter::measuring=true;
        for(unsigned frame=0;frame<1264;++frame){
            const auto input=benchmarkInput(*source,frame);
            for(unsigned turn=0;turn<2;++turn){const bool enabled=(frame+turn)%2==0;auto&app=enabled?*source:*control;
                Meter::enabled=enabled;Meter::clear();const auto begin=std::chrono::steady_clock::now();app.simulate(input);const auto end=std::chrono::steady_clock::now();
                const double elapsed=std::chrono::duration<double,std::milli>(end-begin).count();
                check(Meter::calls==(scene.night?(scene.battle?3u:1u):0u),"Measured projector source-pass count changed");
                check(enabled||Meter::queries==0,"Skipped projector still queried the road");
                if(frame>=240){ticks[unsigned(enabled)].push_back(elapsed);working[unsigned(enabled)].push_back(Meter::milliseconds);
                    if(enabled)queries+=Meter::queries;
                    cpuRaw<<scene.label<<','<<frame<<','<<enabled<<','<<std::setprecision(10)<<elapsed<<','<<Meter::milliseconds<<','<<Meter::calls<<','<<Meter::queries<<'\n';}
            }
            const auto&a=source->originalSession;const auto&b=control->originalSession;
            check(a.vehicle().drive.words==b.vehicle().drive.words&&a.actor().words==b.actor().words,"Projector timing gate changed driving");
            check(a.publishedActors().player0C8FF388==b.publishedActors().player0C8FF388&&a.publishedActors().secondary0C8FF430==b.publishedActors().secondary0C8FF430,"Projector timing gate changed published player/rival state");
            check(a.contactCompletion().randomSeed0C37C778==b.contactCompletion().randomSeed0C37C778&&a.platformFrame()==b.platformFrame(),"Projector timing gate changed RNG/clock");
            check(source->playerBody.query().words==control->playerBody.query().words&&source->rivalBody.query().words==control->rivalBody.query().words,"Projector timing gate changed body-query state");
        }
        Meter::measuring=false;Meter::enabled=true;
        const auto fullOn=summarize(scene.label,"simulate",true,ticks[1]),fullOff=summarize(scene.label,"simulate",false,ticks[0]);
        const auto updateOn=summarize(scene.label,"working_update",true,working[1]),updateOff=summarize(scene.label,"working_update",false,working[0]);
        report<<scene.label<<": CPU whole tick median on/off "<<fullOn.median<<" / "<<fullOff.median<<" ms; difference "<<fullOn.median-fullOff.median
            <<" ms. Working update median "<<updateOn.median<<" ms, p95 "<<updateOn.p95<<" ms (skipped timer median "<<updateOff.median<<" ms); "<<queries<<" road queries in1024 measured ticks.\n";
        auto&app=*source;app.clock.reset();check(app.renderer.initialize(nullptr,1280,720,false),app.renderer.error.c_str());check(app.render(0,true),app.renderer.error.c_str());
        const auto appImage=output/(std::string(scene.label)+"-app.bmp");check(app.renderer.saveBitmap(appImage.wstring()),app.renderer.error.c_str());
        const auto overlay=idas3::CourseMapTestAccess::pixels(app.hud);const auto eye=app.camera,target=app.previousBumperFrame.target;
        std::optional<OriginalRearViewFrame> rear;if(scene.battle){rear=app.rearCameraFrame;rear->eye=app.previousRearCameraFrame.eye;rear->target=app.previousRearCameraFrame.target;rear->up=normalized(app.previousRearCameraFrame.up);}
        Mesh excluded=app.raceMesh;unsigned ranges=0;for(auto&range:excluded.ranges)if(projectedRange(range)){range.count=0;++ranges;}
        check(ranges==(scene.night?(scene.battle?2u:1u):0u),"Frozen App projection range count changed");
        const auto frozenPlayer=app.playerProjectedHeadlight.projection().records(),frozenRival=app.rivalProjectedHeadlight.projection().records();
        const auto frozenDrive=app.originalSession.vehicle().drive.words;const auto frozenSeed=app.originalSession.contactCompletion().randomSeed0C37C778;
        const auto frozenFrame=app.originalRaceOwnerFrame;
        auto draw=[&](bool enabled){check(app.renderer.draw(enabled?app.raceMesh:excluded,eye,target,app.night,app.wet,overlay.data(),false,nullptr,rear?&*rear:nullptr),app.renderer.error.c_str());};
        draw(true);const auto directImage=output/(std::string(scene.label)+"-direct.bmp");check(app.renderer.saveBitmap(directImage.wstring()),app.renderer.error.c_str());
        check(readBytes(appImage)==readBytes(directImage),"Frozen direct benchmark frame differs from actual App");
        draw(false);const auto noImage=output/(std::string(scene.label)+"-excluded.bmp");check(app.renderer.saveBitmap(noImage.wstring()),app.renderer.error.c_str());
        if(!scene.night)check(readBytes(appImage)==readBytes(noImage),"Day control geometry changed");
        app.renderer.measureGpuFrame=true;std::array<std::vector<double>,2> gpu,submit;
        auto timed=[&](bool enabled,bool keep,unsigned block){const auto begin=std::chrono::steady_clock::now();draw(enabled);const auto end=std::chrono::steady_clock::now();
            double milliseconds=0;check(app.renderer.readGpuMilliseconds(milliseconds),app.renderer.error.c_str());
            const double elapsed=std::chrono::duration<double,std::milli>(end-begin).count();
            if(keep){gpu[unsigned(enabled)].push_back(milliseconds);submit[unsigned(enabled)].push_back(elapsed);
                gpuRaw<<scene.label<<','<<block<<','<<gpu[unsigned(enabled)].size()<<','<<enabled<<','<<std::setprecision(10)<<milliseconds<<','<<elapsed<<'\n';}};
        for(unsigned i=0;i<24;++i){timed(i%2==0,false,0);timed(i%2!=0,false,0);}
        for(unsigned block=0;block<8;++block)for(unsigned turn=0;turn<2;++turn)for(unsigned frame=0;frame<15;++frame)timed((block+turn)%2==0,true,block);
        app.renderer.measureGpuFrame=false;
        const auto gpuOn=summarize(scene.label,"gpu_draw",true,gpu[1]),gpuOff=summarize(scene.label,"gpu_draw",false,gpu[0]);
        summarize(scene.label,"cpu_submit",true,submit[1]);summarize(scene.label,"cpu_submit",false,submit[0]);
        check(app.playerProjectedHeadlight.projection().records()==frozenPlayer&&app.rivalProjectedHeadlight.projection().records()==frozenRival&&app.originalSession.vehicle().drive.words==frozenDrive&&app.originalSession.contactCompletion().randomSeed0C37C778==frozenSeed&&app.originalRaceOwnerFrame==frozenFrame,"GPU benchmark advanced frozen source state");
        report<<"GPU on/off median "<<gpuOn.median<<" / "<<gpuOff.median<<" ms; difference "<<gpuOn.median-gpuOff.median<<" ms; on/off p95 "<<gpuOn.p95<<" / "<<gpuOff.p95
            <<" ms; "<<app.raceMesh.vertices.size()<<" uploaded vertices, "<<app.raceMesh.ranges.size()<<" ranges, "<<ranges<<" projected ranges.\n\n";
        report.flush();cpuRaw.flush();gpuRaw.flush();summary.flush();
        std::cout<<scene.label<<": projector CPU median "<<updateOn.median<<"ms; GPU on/off "<<gpuOn.median<<'/'<<gpuOff.median<<"ms.\n"<<std::flush;
    }
    check(snapshot(realRoot/"userdata")==realSaves&&snapshot(isolated/"userdata")==privateSaves,"Benchmark modified saved profiles");
    report<<"PASS "<<checks<<" checks;3792 paired CPU ticks includingwarmups,6144 measured whole-tick samples,720 GPU samples; exact App/direct captures; "<<realSaves.size()<<" actual savefiles unchanged.\n";
    std::cout<<"PASS "<<checks<<" checks; saves unchanged; no visible window/audio device.\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
