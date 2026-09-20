#define wWinMain includedGameEntry
#include "../src/main.cpp"
#undef wWinMain
#include <iostream>
#include <map>
#include <numeric>
#include <iomanip>

// Existing test-access friendship permits an exact copy of the last App HUD.
// No second paint call, substitute overlay, or production class edit is used.
namespace idas3 {
struct CourseMapTestAccess {
    static std::vector<std::uint32_t> pixels(const Hud& hud){return {hud.pixels,hud.pixels+std::size_t(hud.width)*hud.height};}
};
}
namespace {
using Bytes=std::vector<char>;
Bytes readBytes(const fs::path& path){std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Unreadable benchmark file");return Bytes(std::istreambuf_iterator<char>(f),{});}
struct SavedFile{Bytes data;fs::file_time_type time;bool operator==(const SavedFile&)const=default;};
std::map<fs::path,SavedFile> snapshot(const fs::path& directory){
    std::map<fs::path,SavedFile> result;
    if(fs::exists(directory))for(const auto& file:fs::recursive_directory_iterator(directory))if(file.is_regular_file())
        result.emplace(file.path().lexically_relative(directory),SavedFile{readBytes(file.path()),file.last_write_time()});
    return result;
}
std::string utf8(const wchar_t* text){
    const int count=WideCharToMultiByte(CP_UTF8,0,text,-1,nullptr,0,nullptr,nullptr);if(count<=1)return {};
    std::string value(std::size_t(count),0);WideCharToMultiByte(CP_UTF8,0,text,-1,value.data(),count,nullptr,nullptr);value.pop_back();return value;
}
std::string defaultHardwareAdapter(){
    using Microsoft::WRL::ComPtr;ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    const D3D_FEATURE_LEVEL requested=D3D_FEATURE_LEVEL_11_0;D3D_FEATURE_LEVEL obtained{};
    const auto hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        &requested,1,D3D11_SDK_VERSION,&device,&obtained,&context);
    if(FAILED(hr))return "Default hardware adapter probe unavailable";
    ComPtr<IDXGIDevice> dxgi;ComPtr<IDXGIAdapter> adapter;DXGI_ADAPTER_DESC desc{};
    if(FAILED(device.As(&dxgi))||FAILED(dxgi->GetAdapter(&adapter))||FAILED(adapter->GetDesc(&desc)))return "Default hardware adapter description unavailable";
    std::ostringstream result;result<<utf8(desc.Description)<<"; vendor0x"<<std::hex<<desc.VendorId<<" device0x"<<desc.DeviceId
        <<"; LUID0x"<<unsigned(desc.AdapterLuid.HighPart)<<':'<<desc.AdapterLuid.LowPart<<std::dec
        <<"; dedicatedVideoMemory="<<desc.DedicatedVideoMemory<<" bytes";return result.str();
}
struct Distribution {double minimum{},median{},p95{},maximum{},mean{};};
Distribution distribution(std::vector<double> values){
    if(values.empty())throw std::invalid_argument("No GPU timings");std::sort(values.begin(),values.end());
    const auto n=values.size();return {values.front(),(values[(n-1)/2]+values[n/2])*.5,values[std::size_t(std::ceil(.95*double(n)))-1],values.back(),std::accumulate(values.begin(),values.end(),0.)/double(n)};
}
struct Scene{const char* label;unsigned course;bool night,battle;unsigned enemy;};
}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::invalid_argument("Marked isolated root and output directory required");
    const auto isolated=fs::canonical(argv[1]),output=fs::absolute(argv[2]);
    if(isolated.parent_path()!=fs::canonical(fs::current_path()/"work")||!fs::exists(isolated/"COURSE_ENVIRONMENT_GPU_TEST_ROOT.txt"))
        throw std::runtime_error("Refusing unmarked or non-work benchmark root");
    if(fs::exists(isolated/"userdata")&&(GetFileAttributesW((isolated/"userdata").c_str())&FILE_ATTRIBUTE_REPARSE_POINT))
        throw std::runtime_error("Benchmark userdata must not redirect to real saves");
    const auto realRoot=fs::canonical(isolated/"data").parent_path();
    const auto realSaves=snapshot(realRoot/"userdata"),privateSaves=snapshot(isolated/"userdata");
    fs::create_directories(output);unsigned checks=0;
    auto check=[&](bool value,const std::string& message){++checks;if(!value)throw std::runtime_error(message);};
    const auto adapter=defaultHardwareAdapter();std::cout<<"Default D3D11 hardware adapter probe: "<<adapter<<'\n'<<std::flush;
    std::ofstream summary(output/"summary.csv"),raw(output/"samples.csv"),report(output/"report.txt");
    summary<<"scene,lighting,vertices,ranges,mirror,width,height,samples,gpu_min_ms,gpu_median_ms,gpu_p95_ms,gpu_max_ms,gpu_mean_ms,cpu_submit_median_ms\n";
    raw<<"scene,block,lighting,sample,gpu_ms,cpu_submit_ms\n";
    report<<"Hardware offscreen course environment GPU comparison\nDefault D3D11 hardware adapter probe: "<<adapter
        <<"\nThe Renderer uses the identical default-hardware D3D11 creation arguments; its private adapter is not directly queried."
        <<"\n1280x720, no window/swapchain/present. Actual App mesh, source fog, HUD pixels, camera and optional mirror are frozen."
        <<"\nOnly Renderer.courseLighting changes: source state vs nullptr legacy course-light path."
        <<"\nPer branch24 warmup and120 measured frames in alternating paired blocks. Each sample reads existing GPU timestamp queries after draw; CPU submission excludes readback."
        <<"\nOffscreen query timings exclude gameplay/physics, UI paint, scene construction, presentation/vsync and overall host frame scheduling; no playable-FPS claim.\n\n";
    constexpr unsigned warmup=24,blocks=8,blockFrames=15;
    for(const auto scene:{Scene{"akina-night",3,true,false,0},Scene{"myogi-day",0,false,false,0},Scene{"happo-night-battle-mirror",4,true,true,19}}){
        auto app=std::make_unique<App>();app->root=isolated;app->validationMode=true;app->settings();
        app->originalCamera=OriginalChaseCamera::load(isolated);app->bumperCamera=OriginalChaseCamera::load(isolated,OriginalDrivingView::Bumper);
        app->frontend.initialize(isolated,true);app->hud.loadOriginal(isolated);app->audio.configure(isolated);
        app->courseIndex=int(scene.course);app->night=scene.night;app->wet=false;app->reverse=false;
        app->frontend.car=0;app->automatic=app->frontend.automatic=true;app->frontend.battleProfile=original::makeOriginalFreshBattleProfile();
        app->frontend.gameMode=scene.battle?original::OriginalGameMode::LegendOfTheStreets:original::OriginalGameMode::TimeAttack;
        if(scene.battle){app->frontend.battleProfile.setu(0,0);original::selectOriginalRival(app->frontend.battleProfile,scene.enemy);}
        app->start();DriverInput input;input.automatic=true;input.throttle=.55f;
        for(unsigned i=0;i<240;++i)app->simulate(input);
        app->clock.reset();app->drivingView=OriginalDrivingView::Bumper;
        check(app->renderer.initialize(nullptr,1280,720,false),app->renderer.error);
        check(app->render(0,true),app->renderer.error);
        const auto appImage=output/(std::string(scene.label)+"-app.bmp");check(app->renderer.saveBitmap(appImage.wstring()),app->renderer.error);
        const auto overlay=idas3::CourseMapTestAccess::pixels(app->hud);
        const auto target=app->previousBumperFrame.target;const auto eye=app->camera;
        std::optional<OriginalRearViewFrame> rear;
        if(scene.battle){
            rear=app->rearCameraFrame;
            rear->eye=app->previousRearCameraFrame.eye;rear->target=app->previousRearCameraFrame.target;
            rear->up=normalized(app->previousRearCameraFrame.up);
        }
        check(app->raceLighting.has_value()&&app->renderer.courseLighting==&*app->raceLighting,"Actual App source lighting was not active");
        check(app->renderer.courseFog==&app->raceFog,"Actual App source fog was not active");
        const auto* source=app->renderer.courseLighting;const auto frozenFog=app->raceFog;
        const auto frozenDrive=app->originalSession.vehicle().drive.words;const auto frozenActor=app->originalSession.actor().words;
        const auto frozenSeed=app->originalSession.contactCompletion().randomSeed0C37C778;
        const auto frozenOwnerFrame=app->originalRaceOwnerFrame;const auto frozenPathIndex=app->courseLightPathIndex;
        auto draw=[&](bool enabled){
            app->renderer.courseLighting=enabled?source:nullptr;
            check(app->renderer.draw(app->raceMesh,eye,target,app->night,app->wet,overlay.data(),false,nullptr,rear?&*rear:nullptr),app->renderer.error);
        };
        draw(true);const auto directImage=output/(std::string(scene.label)+"-direct-source.bmp");
        check(app->renderer.saveBitmap(directImage.wstring()),app->renderer.error);
        check(readBytes(appImage)==readBytes(directImage),"Direct frozen draw differs from actual App camera/HUD/mirror frame");
        draw(false);check(app->renderer.saveBitmap((output/(std::string(scene.label)+"-direct-legacy.bmp")).wstring()),app->renderer.error);
        app->renderer.measureGpuFrame=true;std::array<std::vector<double>,2> gpuTimes,cpuTimes;
        auto timed=[&](bool enabled,bool keep,unsigned block){
            const auto begin=std::chrono::steady_clock::now();draw(enabled);const auto end=std::chrono::steady_clock::now();
            double gpu=0;check(app->renderer.readGpuMilliseconds(gpu),app->renderer.error);
            const double cpu=std::chrono::duration<double,std::milli>(end-begin).count();
            check(std::isfinite(gpu)&&gpu>=0&&std::isfinite(cpu),"Invalid GPU/CPU timestamp sample");
            if(keep){const auto index=gpuTimes[unsigned(enabled)].size();gpuTimes[unsigned(enabled)].push_back(gpu);cpuTimes[unsigned(enabled)].push_back(cpu);
                raw<<scene.label<<','<<block<<','<<(enabled?"source":"legacy")<<','<<index<<','<<std::setprecision(10)<<gpu<<','<<cpu<<'\n';}
        };
        for(unsigned i=0;i<warmup;++i){timed(i%2==0,false,0);timed(i%2!=0,false,0);}
        for(unsigned block=0;block<blocks;++block){
            for(unsigned turn=0;turn<2;++turn){const bool enabled=(block+turn)%2==0;for(unsigned frame=0;frame<blockFrames;++frame)timed(enabled,true,block);}
        }
        app->renderer.measureGpuFrame=false;app->renderer.courseLighting=source;
        for(bool enabled:{false,true}){
            const auto result=distribution(gpuTimes[unsigned(enabled)]),cpu=distribution(cpuTimes[unsigned(enabled)]);
            summary<<scene.label<<','<<(enabled?"source":"legacy")<<','<<app->raceMesh.vertices.size()<<','<<app->raceMesh.ranges.size()<<','<<scene.battle
                <<",1280,720,"<<gpuTimes[unsigned(enabled)].size()<<','<<std::setprecision(10)<<result.minimum<<','<<result.median<<','<<result.p95<<','<<result.maximum<<','<<result.mean<<','<<cpu.median<<'\n';
            report<<scene.label<<' '<<(enabled?"source":"legacy")<<": GPU median "<<result.median<<" ms, p95 "<<result.p95<<" ms, range "<<result.minimum<<".."<<result.maximum
                <<" ms; CPU submit median "<<cpu.median<<" ms; "<<app->raceMesh.vertices.size()<<" vertices / "<<app->raceMesh.ranges.size()<<" ranges.\n";
        }
        const auto native=distribution(gpuTimes[1]),legacy=distribution(gpuTimes[0]);
        report<<"Source minus legacy median: "<<native.median-legacy.median<<" ms.\n\n";
        check(app->originalSession.vehicle().drive.words==frozenDrive&&app->originalSession.actor().words==frozenActor&&app->originalSession.contactCompletion().randomSeed0C37C778==frozenSeed
            &&app->originalRaceOwnerFrame==frozenOwnerFrame&&app->courseLightPathIndex==frozenPathIndex,"Benchmark advanced frozen gameplay state");
        check(app->raceFog.table==frozenFog.table&&app->raceFog.packedDensity==frozenFog.packedDensity&&app->raceFog.colorRgb==frozenFog.colorRgb,"Benchmark changed source fog");
        raw.flush();summary.flush();report.flush();
        std::cout<<scene.label<<": source GPU median "<<native.median<<"ms / legacy "<<legacy.median<<"ms ("<<native.median-legacy.median<<"ms).\n"<<std::flush;
    }
    check(snapshot(realRoot/"userdata")==realSaves&&snapshot(isolated/"userdata")==privateSaves,"Saved driver files changed during benchmark");
    report<<"PASS "<<checks<<" checks;720 measured GPU samples,144 warmup frames; exact App/direct source image equality in all3 scenes; "<<realSaves.size()<<" actual save files unchanged in bytes/timestamps. No visible window or audio device.\n";
    std::cout<<"PASS "<<checks<<" checks;720 measured samples,144 warmups; saves unchanged.\n";
    return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
