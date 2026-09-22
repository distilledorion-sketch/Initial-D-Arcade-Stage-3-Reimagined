#define IDAS3_UNITY_PLUGIN 1
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "unity_bridge.h"
#include "unity_scene_capture.h"
#include "unity_audio_output.h"
#include "unity_ui_capture.h"
#include "main.cpp"
#include "../tests/mode_flow_app_tests.inl"
#include "../tests/time_attack_completion_app_tests.inl"
#include "../tests/finish_music_app_tests.inl"
#include "../tests/imported_car_lighting_app_tests.inl"
#include "../tests/driving_effects_app_tests.inl"
#include "../tests/shared_times_app_tests.inl"
#include "../tests/player_replays_app_tests.inl"
#include "shared_time_import.h"
#include "../tests/shared_import_app_tests.inl"
#include "../tests/performance_options_app_tests.inl"
#include "../tests/mode_flow_unity_fixture.inl"
#include <mutex>
#include <condition_variable>
#include <map>
#include <cwctype>
#include <cstring>
#include <limits>

static_assert(sizeof(Idas3UnityInput)==88);
static_assert(sizeof(Idas3UnityStatus)==80);
static_assert(sizeof(Idas3RivalStatus)==56);
static_assert(sizeof(Idas3Options)==40);
static_assert(sizeof(Idas3WheelState)==40);
static_assert(sizeof(Idas3RaceMusicState)==32);

namespace {
#if !defined(IDAS3_PORTABLE_SCENE)
using Microsoft::WRL::ComPtr;
#endif
enum class UnityOperation{Initialize,Frame,Shutdown};
struct UnityCommand {
    UnityOperation operation=UnityOperation::Frame;
    fs::path assets,saves;
#if !defined(IDAS3_PORTABLE_SCENE)
    ComPtr<ID3D11Texture2D> unityTexture;
#endif
    int width=1280,height=720;
    bool outputAudio=true;
    Idas3UnityInput input{};
};
struct UnityRuntime {
    std::mutex queueMutex,statusMutex,renderMutex;
    std::condition_variable completed;
    std::map<int,UnityCommand> commands;
    int nextToken=1,completedToken=0;
    Idas3UnityStatus status{sizeof(Idas3UnityStatus)};
    std::string error;
    std::unique_ptr<App> app; //render callback only
    std::unique_ptr<idas3::Hud> hudEditor;
    idas3::Mesh hudCar;idas3::NativeTextureBank hudCarTextures;
    std::vector<std::array<uint32_t,3>> hudCarRanges;std::vector<Idas3SceneTexture> hudCarImages;
    bool sceneMode=false;
    bool diagnosticCourseDriver=false;
    bool diagnosticTimerGrace=false;
    unsigned diagnosticExtensions=0;
    float diagnosticSteer=0;
    // A borrowed exported texture remains valid until shutdown, even when a
    // resize is rendered before Unity consumes its generation notification.
#if !defined(IDAS3_PORTABLE_SCENE)
    std::vector<ComPtr<ID3D11Texture2D>> exportedTextures;
#endif
    void* texture=nullptr;
    std::uint64_t frames=0,ticks=0,textureGeneration=0,rendererGeneration=0;
};
UnityRuntime& unityRuntime(){
    // Unity's managed host MUST drain Shutdown before unloading the plugin.
    // Avoid CRT/DllMain invoking App/D3D/waveOut destruction under loader lock.
    static auto* state=new UnityRuntime;return *state;
}
void unityError(const std::string& text,bool fatal=false)noexcept{
    try{auto& r=unityRuntime();std::lock_guard lock(r.statusMutex);r.error=text;if(fatal)r.status.state=2;}catch(...){}
}
fs::path unicodePath(const char* utf8){
    if(!utf8||!*utf8)throw std::invalid_argument("Asset root and save root must be explicit UTF8 paths");
#if defined(IDAS3_PORTABLE_SCENE)
    return fs::weakly_canonical(fs::absolute(fs::u8path(utf8)));
#else
    const int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,utf8,-1,nullptr,0);
    if(count<=1)throw std::invalid_argument("Invalid UTF8 path");
    std::wstring result(std::size_t(count),L'\0');
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,utf8,-1,result.data(),count))throw std::invalid_argument("Invalid UTF8 path");
    result.resize(std::size_t(count)-1);return fs::weakly_canonical(fs::absolute(fs::path(result)));
#endif
}
bool within(const fs::path& path,const fs::path& directory){
    auto p=path.begin();for(auto d=directory.begin();d!=directory.end();++d,++p){
#if defined(IDAS3_PORTABLE_SCENE)
        if(p==path.end()||*p!=*d)return false;
#else
        if(p==path.end())return false;auto a=p->wstring(),b=d->wstring();
        std::transform(a.begin(),a.end(),a.begin(),[](wchar_t c){return wchar_t(std::towlower(c));});
        std::transform(b.begin(),b.end(),b.begin(),[](wchar_t c){return wchar_t(std::towlower(c));});
        if(a!=b)return false;
#endif
    }return true;
}
void dimensions(int width,int height){if(width<320||height<240||width>4096||height>4096)throw std::invalid_argument("Unity target must be320x240 through4096x4096");}
void sceneDimensions(int width,int height){if(width<1||height<1||width>8192||height>8192||std::uint64_t(width)*height>67108864)throw std::invalid_argument("Unity scene output must be1x1 through8192x8192");}
int enqueue(UnityCommand command){
    auto& r=unityRuntime();std::lock_guard lock(r.queueMutex);
    if(r.commands.size()>=32)throw std::runtime_error("Unity render event queue full: issue and drain queued tokens");
    if(r.nextToken==std::numeric_limits<int>::max())throw std::runtime_error("Unity event token exhausted");
    const int token=r.nextToken++;r.commands.emplace(token,std::move(command));return token;
}
void publish(UnityRuntime& r,int eventId){
    std::lock_guard lock(r.statusMutex);auto& s=r.status;
    s.lastEventId=std::uint32_t(eventId);s.renderedFrames=r.frames;s.simulationTicks=r.ticks;s.textureGeneration=r.textureGeneration;
    if(!r.app){s.state=0;s.flags=0;s.width=s.height=0;r.texture=nullptr;return;}
    const auto& app=*r.app;s.state=1;s.width=app.renderer.width;s.height=app.renderer.height;
    s.frontendStage=int(app.frontend.stage);s.attractChild=int(app.frontend.attractChild());s.course=app.courseIndex;s.car=app.frontend.car;
    s.racePhase=int(app.race.phase);s.flags=(app.menu?1u:0)|(app.paused?2u:0)|(app.active?4u:0)|(app.running?8u:0)|(app.originalHandling?16u:0)|(app.legendVisitActive?32u:0)|(app.legendVisitActive&&app.legendVisit.choiceVisible()?64u:0)|(app.multiplayer.active?128u:0)|(app.multiplayer.active&&app.multiplayer.waiting?256u:0)|(app.preRaceDialogueActive?512u:0)|(app.loadingActive?1024u:0)|(app.multiplayerDisconnected()?2048u:0)|(app.extraModeVisitActive()?4096u:0)|(app.canRetireLegendRace()?8192u:0);
    if(app.resultVisit.initialized)s.flags|=262144u;
    if(app.importedCourse)s.flags|=importedCourseDefinition(app.importedCourse->id).sceneFlags|16384u|(app.reverse?32768u:0u)|(app.night?65536u:0u)|(app.wet?131072u:0u);
    s.speedMetresPerSecond=app.vehicle.speed;s.rpm=app.vehicle.rpm;
    s.reserved=r.sceneMode?1u:0u;
    if(r.sceneMode){s.textureGeneration=0;r.texture=nullptr;return;}
#if !defined(IDAS3_PORTABLE_SCENE)
    const auto generation=app.renderer.sharedTextureGeneration();
    if(generation!=r.rendererGeneration){
        // Renderer starts its local generation over at Init; the bridge keeps
        // a process-monotonic generation across Editor stop/start sessions.
        r.rendererGeneration=generation;++r.textureGeneration;s.textureGeneration=r.textureGeneration;
        r.exportedTextures.emplace_back(app.renderer.sharedTexture());
    }
    r.texture=app.renderer.sharedTexture();
#endif
}
void destroyApp(UnityRuntime& r,bool save){
    r.diagnosticCourseDriver=false;r.diagnosticTimerGrace=false;r.diagnosticExtensions=0;r.diagnosticSteer=0;
    if(r.sceneMode)idas3::resetUnityAudioOutput();
    std::exception_ptr saveError;
    if(r.app&&save&&!r.app->replayPlaybackActive)try{r.app->saveSettings();}catch(...){saveError=std::current_exception();}
    // Stop native audio and wait for all its buffers in EngineAudio's original
    // destructor before the renderer releases the borrowed Unity device.
    r.hudEditor.reset();r.hudCar={};r.hudCarTextures={};r.hudCarRanges.clear();r.hudCarImages.clear();r.app.reset();
#if !defined(IDAS3_PORTABLE_SCENE)
    r.exportedTextures.clear();
#endif
    r.rendererGeneration=0;
    if(r.sceneMode)Idas3UiEnable(0);
    {std::lock_guard lock(r.statusMutex);r.texture=nullptr;}
    if(saveError)std::rethrow_exception(saveError);
}
#if defined(IDAS3_PORTABLE_SCENE)
void initialize(UnityRuntime&,const UnityCommand&){throw std::runtime_error("Android requires the Unity scene API");}
#else
void initialize(UnityRuntime& r,const UnityCommand& command){
    destroyApp(r,true);r.sceneMode=false;r.frames=r.ticks=0;
    ComPtr<ID3D11Device> device;command.unityTexture->GetDevice(&device);
    if(!device)throw std::runtime_error("Unity D3D11 texture has no device");
    auto app=std::make_unique<App>();app->root=command.assets;app->saveRoot=command.saves;
    app->settings();app->originalCamera=OriginalChaseCamera::load(app->root);
    app->bumperCamera=OriginalChaseCamera::load(app->root,OriginalDrivingView::Bumper);
    app->frontend.initialize(app->root,true);app->loadStartPresentation();app->hud.loadOriginal(app->root);
    app->frontend.course=app->courseIndex;app->frontend.reverse=app->reverse;app->frontend.wet=app->wet;
    app->frontend.night=app->night;app->frontend.automatic=app->automatic;
    {std::ifstream saved(app->userdataRoot()/"native_selection.txt");int make,car;
        if(saved>>make>>car){app->frontend.make=std::clamp(make,0,6);app->frontend.car=std::clamp(car,0,34);}}
    app->load();app->audio.configure(app->root);
    if(!app->renderer.initializeSharedDevice(device.Get(),command.width,command.height))throw std::runtime_error(app->renderer.error);
    if(command.outputAudio&&!app->audio.open())app->status("Audio output unavailable; driving remains available");
    if(!app->render(0))throw std::runtime_error(app->renderer.error);
    r.app=std::move(app);
    {std::lock_guard lock(r.statusMutex);r.error.clear();}
}
#endif
void assignInput(HostInput& dst,const Idas3UnityInput& src,bool focused){
    for(unsigned key=0;key<256;++key){const bool next=focused&&((src.keys[key/32]>>(key%32))&1u);dst.pressed[key]=next&&!dst.down[key];dst.down[key]=next;}
    dst.connected=focused&&src.padConnected!=0;XINPUT_STATE pad{};
    if(dst.connected){auto& p=pad.Gamepad;p.wButtons=WORD(src.padButtons);
        p.sThumbLX=SHORT(std::clamp(src.thumbLX,-32768,32767));p.sThumbLY=SHORT(std::clamp(src.thumbLY,-32768,32767));
        p.sThumbRX=SHORT(std::clamp(src.thumbRX,-32768,32767));p.sThumbRY=SHORT(std::clamp(src.thumbRY,-32768,32767));
        p.bLeftTrigger=BYTE(std::min(src.leftTrigger,255u));p.bRightTrigger=BYTE(std::min(src.rightTrigger,255u));}
    dst.pad=pad;dst.pressedButtons=pad.Gamepad.wButtons&~dst.oldButtons;dst.oldButtons=pad.Gamepad.wButtons;
}
void frame(UnityRuntime& r,const Idas3UnityInput& input){
    if(!r.app)throw std::logic_error("Unity game is not initialized");auto& app=*r.app;
    const bool focused=(input.flags&1)!=0||app.challengerNotice;
    const double dt=app.challengerNotice?0:input.deltaSeconds;
    if(app.multiplayer.active)app.paused=false;
    else if(app.active&&!focused&&!app.menu&&!app.extraModeVisitActive()&&!app.legendVisitActive&&!app.preRaceDialogueActive&&!app.loadingActive&&app.race.phase!=RacePhase::Finished){app.paused=true;app.clock.reset();}
    app.active=focused;assignInput(app.input,input,focused);
    app.setHostDrivingControlsBlocked((input.flags&2u)!=0);
    // Exactly the standalone outer-loop order: input/commands, focus gate,
    // resize, fixed60Hz solver ticks, render(owner updates), then sound service.
    app.commands(dt);
    if(!app.running||(!focused&&!app.multiplayer.active)){
        app.clock.reset();app.updateAudioScene(true);
        if(r.sceneMode)idas3::submitUnityAudioOutput(app.audio,dt,idas3::UnityAudioFrameState{0,0,0,0,false,true,false});
        else app.audio.update(0,0,0,0,false);
        return;
    }
    if(input.width>0&&input.height>0&&(input.width!=app.renderer.width||input.height!=app.renderer.height)){
#if !defined(IDAS3_PORTABLE_SCENE)
        if(r.exportedTextures.size()>=16)throw std::runtime_error("Unity target resized16times: restart session to release retained external textures");
#endif
        if(!app.renderer.resize(input.width,input.height))throw std::runtime_error(app.renderer.error);
    }
    if(!app.menu&&!app.paused&&!app.loadingActive&&!app.extraModeVisitActive()&&!app.legendVisitActive&&!app.preRaceDialogueActive&&!app.vsActive&&!(app.multiplayer.active&&app.multiplayer.waiting))app.advanceHostClock(dt,[&]{
        auto driver=app.driver();
        if(r.diagnosticCourseDriver&&app.race.phase==RacePhase::Running){
            if(r.diagnosticTimerGrace&&app.originalRace.state().remaining.value<60000u){
                auto state=app.originalRace.state();state.remaining.value+=360000u;
                app.originalRace.restoreNumericalState(state);++r.diagnosticExtensions;
            }
            const auto p=app.projectRacePosition(app.vehicle.position);
            const float speed=std::max(0.f,app.vehicle.speed),look=std::max(6.f,speed*.35f);
            float target=58.f;
            for(float offset:{8.f,18.f,32.f,50.f}){
                const float curvature=std::abs(app.sampleRaceDistance(p.sample.distance+offset).curvature);
                target=std::min(target,std::sqrt(80.f/std::max(.001f,curvature)));
            }
            target=std::clamp(target,32.f,58.f);
            const Vec3 aim=app.sampleRaceDistance(p.sample.distance+look).center-app.vehicle.position;
            const float angle=wrapAngle(std::atan2(aim.x,aim.z)-app.vehicle.yaw);
            const float demand=std::clamp(3.5f*std::atan2(2*app.config.wheelbase*std::sin(angle),look)/recoveredSteeringLimit,-1.f,1.f);
            r.diagnosticSteer+=std::clamp(demand-r.diagnosticSteer,-.07f,.07f);
            driver={};driver.automatic=true;driver.steer=-r.diagnosticSteer;
            driver.throttle=std::clamp((target-speed)*.6f,0.f,1.f);driver.brake=std::clamp((speed-target)*.3f,0.f,.8f);
        }
        app.simulate(driver);++r.ticks;
    });else app.clock.reset();
    if(app.messageSeconds>0){app.messageSeconds-=float(dt);if(app.messageSeconds<=0)app.message.clear();}
    if(dt>0)app.renderFps+=(float(1/dt)-app.renderFps)*.025f;
    if(r.sceneMode)Idas3UiBeginFrame(app.renderer.width,app.renderer.height);
    if(!app.render(dt))throw std::runtime_error(app.renderer.error);++r.frames;
    app.updateAudioScene();
    if(r.sceneMode){
        idas3::submitUnityAudioOutput(app.audio,input.deltaSeconds,idas3::UnityAudioFrameState{
            app.vehicle.rpm,app.vehicle.throttle,app.vehicle.speed,app.originalHandling?0.f:app.vehicle.slip,
            !app.menu&&!app.paused&&app.race.phase!=RacePhase::Finished,app.paused||app.multiplayerDisconnected(),app.audio.enabled});
    }else app.audio.update(app.vehicle.rpm,app.vehicle.throttle,app.vehicle.speed,app.originalHandling?0.f:app.vehicle.slip,!app.menu&&!app.paused&&app.race.phase!=RacePhase::Finished);
}
void IDAS3_UNITY_EVENT renderEvent(int eventId)noexcept{
    auto& r=unityRuntime();std::lock_guard renderLock(r.renderMutex);
    UnityCommand command;
    {std::lock_guard lock(r.queueMutex);const auto found=r.commands.find(eventId);if(found==r.commands.end())return;
        command=std::move(found->second);r.commands.erase(found);}
    try{
        switch(command.operation){
        case UnityOperation::Initialize:initialize(r,command);break;
        case UnityOperation::Frame:frame(r,command.input);break;
        case UnityOperation::Shutdown:
            destroyApp(r,true);
            {std::lock_guard lock(r.queueMutex);for(auto i=r.commands.begin();i!=r.commands.end()&&i->first<eventId;)i=r.commands.erase(i);}
            break;
        }
        publish(r,eventId);
    }catch(const std::exception& e){
        // A failed owner cannot keep advancing or producing audio. Never save
        // a partially initialized/failed frame during this cleanup.
        try{destroyApp(r,false);publish(r,eventId);}catch(...){}
        unityError(e.what(),true);
    }catch(...){try{destroyApp(r,false);publish(r,eventId);}catch(...){}unityError("Unknown exception in native Unity render event",true);}
    {std::lock_guard lock(r.queueMutex);r.completedToken=std::max(r.completedToken,eventId);}r.completed.notify_all();
}
}
extern "C" {
uint32_t IDAS3_UNITY_CALL Idas3UnityVersion(){return 1;}
int IDAS3_UNITY_CALL Idas3SceneReadPresence(char* output,int capacity){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    if(!r.sceneMode||!r.app)return 0;
    try{
        const auto& a=*r.app;const bool menu=a.menu;
        std::ostringstream json;
        json<<"{\"condition\":"<<(menu?a.frontend.course*2+int(a.frontend.reverse):a.courseIndex*2+int(a.reverse))
            <<",\"night\":"<<int(menu?a.frontend.night:a.night)<<",\"weather\":"<<int(menu?a.frontend.wet:a.wet)
            <<",\"car\":"<<a.frontend.car<<",\"mode\":"<<(a.multiplayer.active?1:a.frontend.gameMode==original::OriginalGameMode::TimeAttack?0:a.bunta?3:2)
            <<",\"ticks6000\":"<<a.race.elapsed6000<<",\"opponentName\":"
            <<std::quoted(a.multiplayer.active?a.multiplayer.remoteName:a.battle?OriginalVsBanner::rivalDisplayName(a.battleProfile.u(24)):std::string())<<"}";
        const auto text=json.str();if(!output||capacity<=int(text.size()))return -1;
        std::memcpy(output,text.c_str(),text.size()+1);return int(text.size());
    }catch(...){return 0;}
}
int IDAS3_UNITY_CALL Idas3SharedReadFinish(char* output,int capacity){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    if(!r.sceneMode||!r.app||r.app->sharedFinishJson.empty())return 0;
    const auto& text=r.app->sharedFinishJson;if(!output||capacity<=int(text.size()))return -1;
    std::memcpy(output,text.c_str(),text.size()+1);return int(text.size());
}
int IDAS3_UNITY_CALL Idas3SharedReadReplay(uint8_t* output,int capacity){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    if(!r.sceneMode||!r.app||r.app->sharedFinishJson.empty())return 0;
    const auto& bytes=r.app->sharedFinishReplay;
    if(!output)return int(bytes.size());if(capacity<int(bytes.size()))return -1;
    std::memcpy(output,bytes.data(),bytes.size());return int(bytes.size());
}
void IDAS3_UNITY_CALL Idas3SharedAckFinish(){auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);if(r.app){r.app->sharedFinishJson.clear();r.app->sharedFinishReplay.clear();}}
int IDAS3_UNITY_CALL Idas3ReplayRecordingOptions(uint32_t flags){auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);if(!r.app||(flags&~15u))return 0;r.app->replayRecordingFlags=flags;if((flags&8)&&!r.app->battle&&!r.app->multiplayer.active&&!r.app->replayPlaybackActive)r.app->archiveThisRace=true;return 1;}
int IDAS3_UNITY_CALL Idas3LocalReplayRead(int part,uint8_t* output,int capacity){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);if(!r.app||r.app->localReplayJson.empty())return 0;
    const auto& a=*r.app;if(part<0||part>2)return -1;
    const auto size=part==0?a.localReplayJson.size():part==1?a.localReplayPlayer.size():a.localReplayRival.size();
    if(!output)return int(size);if(capacity<int(size))return -1;
    if(size)std::memcpy(output,part==0?static_cast<const void*>(a.localReplayJson.data()):part==1?static_cast<const void*>(a.localReplayPlayer.data()):static_cast<const void*>(a.localReplayRival.data()),size);return int(size);
}
void IDAS3_UNITY_CALL Idas3LocalReplayAck(){auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);if(r.app){r.app->localReplayJson.clear();r.app->localReplayPlayer.clear();r.app->localReplayRival.clear();}}
int IDAS3_UNITY_CALL Idas3SharedReadPersonalImport(char* output,int capacity){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    if(!r.sceneMode||!r.app)return -1;
    try{const auto text=sharedPersonalImportJson(r.app->userdataRoot());
        if(!output||capacity<=int(text.size()))return -1;
        std::memcpy(output,text.c_str(),text.size()+1);return int(text.size());
    }catch(...){return -1;}
}
int IDAS3_UNITY_CALL Idas3SharedSetRecords(const int32_t* values,int count,int enabled){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    if(!r.sceneMode||!r.app||count<0||count>2112||(count&&!values))return 0;
    try{
        TimeAttackRecords next;
        for(int i=0;i<count;++i){const auto* p=values+i*14;TimeAttackEntry e;
            if(p[0]<0||p[1]<0||p[2]<0||p[3]<=0||p[9]<-1||p[9]>1||p[10]<-1||p[10]>1)return 0;
            e.condition=unsigned(p[0]);e.weather=unsigned(p[1]);e.car=unsigned(p[2]);e.ticks6000=unsigned(p[3]);
            for(int n=0;n<5;++n){if(p[4+n]<0||p[4+n]>221)return 0;e.nameGlyphs[n]=std::uint8_t(p[4+n]);}
            e.manual=p[9]==1;e.night=p[10]==1;e.metadataUnknown=p[9]<0||p[10]<0;
            for(int n=0;n<3;++n){if(p[11+n]<0)return 0;e.intermediate6000[n]=unsigned(p[11+n]);}
            next.record(e);
        }
        r.app->setSharedRecords(std::move(next),enabled!=0);return 1;
    }catch(...){return 0;}
}
// Available only in an explicitly isolated diagnostic process; never changes
// the ordinary driver directory or active multiplayer session.
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneModeFlowFixture(int scene){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app||(r.app->multiplayer.active&&(scene<200||scene>206))||r.app->saveRoot.filename()!="userdata"||
           !fs::is_regular_file(r.app->saveRoot.parent_path()/"ISOLATED_MODE_FLOW_TEST.txt"))
            throw std::logic_error("Mode-flow fixtures require a private diagnostic directory");
        if(scene==-1)return runModeFlowAppTests(*r.app)==0?1:0;
        if(scene==-2)return runTimeAttackCompletionAppTests(*r.app)==0?1:0;
        if(scene==-3)return runFinishMusicAppTests(*r.app)==0?1:0;
        if(scene==-4)return runImportedCarLightingAppTests(*r.app)==0?1:0;
        if(scene==-11)return runDrivingEffectsAppTests(*r.app)==0?1:0;
        if(scene==-5)return runSharedTimeAppTests(*r.app)==0?1:0;
        if(scene==-6)return runSharedImportAppTests(*r.app)==0?1:0;
        if(scene==-7)return runPerformanceOptionsAppTests(*r.app)==0?1:0;
        if(scene==-8)return runPlayerReplayAppTests(*r.app)==0?1:0;
        if(scene==-9)return runSharedRecordResetAppTests(*r.app)==0?1:0;
        if(scene==-10){
            auto& a=*r.app;const bool oldValidation=a.validationMode;const int oldDifficulty=a.aiDifficulty;
            struct Restore{App& a;bool validation;int difficulty;~Restore(){a.validationMode=validation;a.aiDifficulty=difficulty;}}restore{a,oldValidation,oldDifficulty};
            a.validationMode=true;a.multiplayer.active=false;a.paused=false;a.loadingActive=false;a.preRaceDialogueActive=a.legendVisitActive=false;
            unsigned cases=0;
            for(unsigned course:{0u,3u})for(unsigned level:{0u,15u}){
                std::array<float,4> baseline{};
                for(unsigned difficulty=0;difficulty<3;++difficulty){
                    a.aiDifficulty=int(difficulty);a.frontend.gameMode=idas3::original::OriginalGameMode::BuntaChallenge;
                    a.frontend.car=0;auto& p=a.frontend.battleProfile;p=idas3::original::makeOriginalFreshBattleProfile();
                    p.setu(0,2);p.setu(16,0);p.setu(72,1000000);
                    for(unsigned i=0;i<8;++i)p.setu(1080+i*4,level);
                    idas3::original::selectOriginalBuntaCourse(p,course);a.start();
                    if(!a.bunta||!a.battle||a.originalSession.rivalPaceInputs().aiDifficulty!=0)throw std::runtime_error("Bunta received a custom AI pace");
                    for(unsigned frame=0;frame<600;++frame)a.simulate({});
                    const auto& rival=a.originalSession.rivalActor();const std::array<float,4> sample{rival.f(68),rival.f(200),rival.f(204),rival.f(208)};
                    if(difficulty==0)baseline=sample;else if(sample!=baseline)throw std::runtime_error("Difficulty changed Bunta speed or movement");
                    ++cases;
                }
            }
            std::ofstream log(a.saveRoot.parent_path()/"bunta-difficulty.txt");
            log<<"PASS "<<cases<<" Bunta races: Normal, Hard and Expert produce identical speed and movement at two challenge levels on two courses. Original AI pace retained.\n";
            return 1;
        }
        prepareModeFlowFixture(*r.app,unsigned(scene));
        Idas3UiBeginFrame(r.app->renderer.width,r.app->renderer.height);
        if(!r.app->render(0))throw std::runtime_error(r.app->renderer.error);
        if(scene>=250&&scene<=253){std::ofstream out(r.app->saveRoot.parent_path()/("effect-ranges-"+std::to_string(scene)+".txt"));
            out<<"base "<<r.app->smokeTextureBase<<" coverage corner "<<std::hex<<r.app->smokeTextures.at(4).argb[0]<<std::dec<<'\n';
            const auto& frame=r.app->renderer.sceneCapture()->frame();
            const auto& smoke=frame.textures[r.app->smokeTextureBase+4];
            const auto& expected=r.app->smokeTextures.at(4);
            if(smoke.width!=expected.width||smoke.height!=expected.height||std::memcmp(smoke.argb,expected.argb.data(),expected.argb.size()*4))throw std::runtime_error("Smoke texture binding mismatch");
            out<<"smoke binding checked "<<smoke.width<<' '<<smoke.height<<'\n';
            const auto& ranges=r.app->raceMesh.ranges;for(unsigned i=unsigned(ranges.size()>4?ranges.size()-4:0);i<ranges.size();++i){const auto& q=ranges[i];out<<q.first<<' '<<q.count<<' '<<q.texture<<' '<<std::hex<<q.tsp<<' '<<q.pcw<<' '<<q.gmp<<std::dec<<'\n';}
        }
        publish(r,0);return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
}
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneReplayCaptureDiagnostic(int enabled){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    if(!r.app||r.app->saveRoot.filename()!="userdata"||!fs::is_regular_file(r.app->saveRoot.parent_path()/"ISOLATED_SCENE_TEST.txt"))return 0;
    r.app->diagnosticCaptureOff=enabled==0;return 1;
}
// Isolated performance driving only: controls go through the original solver.
// Never teleports, disables collisions or touches user saves. Mode 2 explicitly
// allows timer grace for coverage sweeps and reports every diagnostic extension.
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneCourseDriveDiagnostic(int enabled,float* values,int count){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    if(!r.sceneMode||!r.app||r.app->multiplayer.active||r.app->saveRoot.filename()!="userdata"||
       !fs::is_regular_file(r.app->saveRoot.parent_path()/"ISOLATED_SCENE_TEST.txt"))return 0;
    if(enabled < -1 || enabled > 2 || (values?count!=12:count!=0))return 0;
    if(enabled>=0){r.diagnosticCourseDriver=enabled!=0;r.diagnosticTimerGrace=enabled==2;r.diagnosticSteer=0;if(enabled)r.diagnosticExtensions=0;}
    if(values&&count==12){const auto& a=*r.app;
        values[0]=a.progress;values[1]=a.course.length;values[2]=a.race.progress;
        values[3]=a.race.furthest;values[4]=a.vehicle.wallContact?1.f:0.f;
        values[5]=a.race.phase==RacePhase::Finished?1.f:0.f;values[6]=a.race.timeUp?1.f:0.f;
        values[7]=float(a.segment);values[8]=a.reverse?1.f:0.f;values[9]=a.vehicle.travel;
        values[10]=float(r.diagnosticExtensions);values[11]=r.diagnosticTimerGrace?1.f:0.f;
    }
    return 1;
}
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneModeFlowValue(int field){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    if(!r.sceneMode||!r.app)return -1;const auto& a=*r.app;
    switch(field){
    case 0:return a.extraModeVisitActive();case 1:return int(a.timeAttackVisit.stage());
    case 2:return int(a.audio.originalMusicCue());case 3:return int(a.battleProfile.u(1092));
    case 4:return a.menu;case 5:return int(a.frontend.stage);
    case 6:return int(a.timeAttackVisit.selectedIndex());case 7:return int(a.buntaVisit.dialogueState().kind);
    case 8:return a.buntaVisit.beforeRace();case 9:return int(a.timeAttackVisit.setup().analysis.kind);
    case 10:return int(a.timeAttackVisit.setup().analysis.adviceIndex);
    case 11:return int(a.frontend.rankingState().page.detailMode);case 12:return int(a.frontend.rankingState().page.detailPage);
    case 13:return int(a.frontend.attractChild());case 14:return int(a.frontend.rankingState().page.conditionIndex*2+a.frontend.rankingState().page.wet);
    case 15:return int(a.finishBannerTicks);case 16:return a.finishBannerDone;
    case 17:return int(a.timeSummaryTicks);case 18:return int(a.results.recordFlags);
    case 19:return a.audio.raceMusicFinished();case 20:return a.audio.raceTimingStatistics().scene;
    case 21:return int(a.audio.raceTimingStatistics().streamFrame);case 22:return a.finishAudioSkipped;
    case 23:return a.fullTuneActive;case 24:return int(a.resultVisit.tuning.kind);
    case 25:return int(a.battleProfile.u(72));case 26:return int(a.resultVisit.child.phase);
    case 27:return a.frontend.inputReady();case 28:return a.activeSaveSlot;case 29:return a.fullTuneSelecting;
    case 41:return unsigned(a.frontend.reverse)|(unsigned(a.frontend.wet)<<1)|(unsigned(a.frontend.night)<<2);
    case 40:return int(a.smokeTextureBase);
    case 38:return int(a.drivingEffects.markCount());
    case 39:return int(a.drivingEffects.smokeCount());
    case 36:return a.playerProjectedHeadlight.enabled();
    case 37:return a.carPresentation.headlightState().visible;
    case 34:return a.aiDifficulty;
    case 35:return a.originalSession.ready()&&a.originalSession.rivalActive()?int(a.originalSession.rivalPaceInputs().aiDifficulty):0;
    case 32:return int(a.recording.frames.size());case 33:return int(a.rivalRecording.frames.size());
    case 30:return a.frontend.course;case 31:return unsigned(a.wet)|(unsigned(a.night)<<1);
    default:return -1;}
}

int IDAS3_UNITY_CALL Idas3SceneInitialize(const char* assets,const char* saves,int width,int height){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        sceneDimensions(width,height);const auto assetPath=unicodePath(assets),savePath=unicodePath(saves);
        if(!fs::is_directory(assetPath/"data"))throw std::invalid_argument("Asset root must contain the complete data directory");
        if(savePath==assetPath||within(savePath,fs::weakly_canonical(assetPath/"data"))||within(savePath,fs::weakly_canonical(assetPath/"userdata")))
            throw std::invalid_argument("Unity saves must be isolated from original assets and userdata");
        destroyApp(r,true);r.sceneMode=true;r.frames=r.ticks=0;
        Idas3UiEnable(1);idas3::resetUnityAudioOutput();
        auto app=std::make_unique<App>();app->root=assetPath;app->saveRoot=savePath;
        app->settings();app->originalCamera=OriginalChaseCamera::load(app->root);
        app->bumperCamera=OriginalChaseCamera::load(app->root,OriginalDrivingView::Bumper);
        app->frontend.initialize(app->root,true);app->loadStartPresentation();app->hud.loadOriginal(app->root);
        app->frontend.course=app->courseIndex;app->frontend.reverse=app->reverse;app->frontend.wet=app->wet;
        app->frontend.night=app->night;app->frontend.automatic=app->automatic;
        {std::ifstream saved(app->userdataRoot()/"native_selection.txt");int make,car;
            if(saved>>make>>car){app->frontend.make=std::clamp(make,0,6);app->frontend.car=std::clamp(car,0,34);}}
        app->load();app->audio.configure(app->root);
        if(!app->renderer.initializeSceneCapture(width,height))throw std::runtime_error("Unity scene capture initialization failed");
        Idas3UiBeginFrame(width,height);
        if(!app->render(0))throw std::runtime_error(app->renderer.error);
        r.app=std::move(app);
        {std::lock_guard statusLock(r.statusMutex);r.error.clear();}
        publish(r,0);return 1;
    }catch(const std::exception& e){try{destroyApp(r,false);publish(r,0);}catch(...){}unityError(e.what(),true);return 0;}
    catch(...){try{destroyApp(r,false);}catch(...){}unityError("Unknown Unity scene initialization error",true);return 0;}
}
#include "../tests/hakone_time_attack_app_tests.inl"
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneHakoneTimeAttackTest(const char* root){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{if(!r.app||!r.sceneMode)throw std::logic_error("Scene unavailable");runHakoneTimeAttackTests(*r.app,unicodePath(root));
        Idas3UiBeginFrame(r.app->renderer.width,r.app->renderer.height);if(!r.app->render(0))throw std::runtime_error(r.app->renderer.error);publish(r,0);return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
}
int IDAS3_UNITY_CALL Idas3SceneRegisterImportedCourse(const char* root){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app)throw std::logic_error("Course registration requires an initialized scene");
        auto path=unicodePath(root);
        const auto id=ImportedCourse::courseId(path);r.app->frontend.enableHakoneCourse(path,id);r.app->importedRoot(id)=path;
        return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
}
int IDAS3_UNITY_CALL Idas3SceneShowImportedCourseMenu(const char* root){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app||r.app->multiplayer.active)throw std::logic_error("Hakone menu requires an offline scene");
        auto path=unicodePath(root);auto& a=*r.app;
        const auto id=ImportedCourse::courseId(path);a.frontend.enableHakoneCourse(path,id);a.importedRoot(id)=path;
        a.frontend.gameMode=original::OriginalGameMode::TimeAttack;a.frontend.course=id;a.courseIndex=3;
        a.returnToCourseSelection(true);a.message.clear();
        Idas3UiBeginFrame(a.renderer.width,a.renderer.height);
        if(!a.render(0))throw std::runtime_error(a.renderer.error);publish(r,0);return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
}
int IDAS3_UNITY_CALL Idas3SceneStartImportedCourse(const char* root,int reverse){
    return Idas3SceneStartImportedCourseConditions(root,reverse,0,0);
}
int IDAS3_UNITY_CALL Idas3SceneStartImportedCourseConditions(const char* root,int reverse,int night,int wet){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app||r.app->multiplayer.active||reverse<0||reverse>1||night<0||night>1||wet<0||wet>1)throw std::logic_error("Imported course requires an offline scene and valid conditions");
        auto data=ImportedCourse::load(unicodePath(root));auto& a=*r.app;
        a.importedRoot(data.id)=data.root;a.frontend.enableHakoneCourse(data.root,data.id);
        a.importedCourse=std::move(data);
        a.frontend.gameMode=original::OriginalGameMode::TimeAttack;
        // Retain Akina presentation tables; ImportedCourse selects per-course handling.
        a.courseIndex=3;a.frontend.course=int(a.importedCourse->id);a.reverse=a.frontend.reverse=reverse!=0;a.wet=a.frontend.wet=wet!=0;a.night=a.frontend.night=night!=0;
        a.renderer.farClip=12000;a.start();a.message.clear();
        Idas3UiBeginFrame(a.renderer.width,a.renderer.height);
        if(!a.render(0))throw std::runtime_error(a.renderer.error);publish(r,0);return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
}
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3ReplayStart(int condition,int weather,int night,int car,int manual){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.app||!r.sceneMode||r.app->saveRoot.filename()!="replay-viewer-session")throw std::logic_error("Replay playback requires isolated viewer storage");
        if(condition<0||condition>=int(supportedConditionCount)||weather<0||weather>1||night<0||night>1||car<0||car>34||manual<0||manual>1)throw std::invalid_argument("Invalid replay selection");
        auto& a=*r.app;a.replayPlaybackActive=true;a.validationMode=true;
        a.frontend.gameMode=original::OriginalGameMode::TimeAttack;
        a.frontend.course=condition/2;a.courseIndex=condition/2;
        a.reverse=a.frontend.reverse=(condition%2)!=0;a.wet=a.frontend.wet=weather!=0;a.night=a.frontend.night=night!=0;
        a.frontend.car=car;a.automatic=a.frontend.automatic=manual==0;
        a.frontend.battleProfile=original::makeOriginalFreshBattleProfile();a.start();a.vsActive=false;a.loadingActive=false;a.preRaceDialogueActive=false;
        a.replayLastTick=-1;a.replayInitialRemaining=a.race.remaining6000;a.replayDetailed=false;a.replayLights=night!=0;a.replayOpponent=false;a.rivalVisible=false;
        a.menu=false;a.paused=false;a.race.phase=RacePhase::Running;
        a.message.clear();return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
}
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3ReplayAppearance(const uint32_t* values,int count){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.app||!r.sceneMode||!r.app->replayPlaybackActive||!values||count!=15)throw std::invalid_argument("Invalid replay appearance");
        if(values[0]>1||values[1]>=original::originalCarColorCounts.at(unsigned(r.app->frontend.car))||values[7]>5)throw std::invalid_argument("Invalid replay paint or name");
        for(unsigned i=2;i<7;++i)if(values[i]>221)throw std::invalid_argument("Invalid replay name");
        auto& a=*r.app;
        for(unsigned i=0;i<12;++i)a.frontend.battleProfile.setu(replayProfileOffsets[i],values[i]);
        a.frontend.battleProfile.setu(16,unsigned(a.frontend.car));a.frontend.driverProfileLoaded();a.loadedCar=-1;a.loadSelectedCar();
        for(unsigned i=0;i<3;++i)a.results.bestTimes6000[i]=values[12+i];a.results.modelBestAvailable=true;a.results.edgeAnchored=true;a.results.suppliedRecordTargets=true;
        a.replayDetailed=true;return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
}
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3ReplayDetailFrame(const uint8_t* values,int count){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.app||!r.app->replayPlaybackActive||!r.app->replayDetailed||!values||count!=132)throw std::invalid_argument("Invalid detailed replay frame");
        ReplayDetail d;std::memcpy(&d,values,sizeof(d));
        if(!std::isfinite(d.rpm)||!std::isfinite(d.bodyPosition.x)||!std::isfinite(d.bodyPosition.y)||!std::isfinite(d.bodyPosition.z)||!std::isfinite(d.pitch)||!std::isfinite(d.roll)||!std::isfinite(d.steering)||!std::isfinite(d.throttle)||!std::isfinite(d.brake)||!std::isfinite(d.lightFraction)||!std::isfinite(d.progress)||d.sector>4||d.capacity<1||d.capacity>4||d.lightVisible>1||d.lights>1||d.elapsed>=10800000)throw std::invalid_argument("Invalid replay state");
        for(unsigned i=0;i<4;++i)if(!std::isfinite(d.suspension[i])||!std::isfinite(d.rotation[i]))throw std::invalid_argument("Invalid replay wheel state");
        r.app->replayFrameData=d;return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
}
int IDAS3_UNITY_CALL Idas3ReplayOpponentStart(int car,int enemy,const uint32_t* values,int count){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.app||!r.app->replayPlaybackActive||!values||count!=15||car<0||car>34||enemy< -1||enemy>99)throw std::invalid_argument("Invalid replay opponent");
        auto& a=*r.app;
        if(enemy>=0)a.loadRivalCar(unsigned(car),unsigned(enemy));
        else{
            if(values[0]>1||values[1]>=original::originalCarColorCounts.at(unsigned(car))||values[7]>5)throw std::invalid_argument("Invalid opponent appearance");
            auto profile=original::makeOriginalFreshBattleProfile();for(unsigned i=0;i<12;++i)profile.setu(replayProfileOffsets[i],values[i]);profile.setu(16,unsigned(car));
            const auto folder=std::string(originalCarFolders.at(unsigned(car)));
            a.rivalModel=NativeModel::load(a.root/"data/original_models"/folder/(folder+".idasmesh"));
            a.rivalTextures=NativeTextureBank::load(a.root/"data/original_assets/cars"/folder/"textures/textures.idastex");
            a.rivalPresentation=CarPresentation::loadPlayerProfile(a.root,profile);a.rivalPresentation.applyMaterials(a.rivalModel);
            a.rivalPlate=OriginalNumberPlate::load(a.root,unsigned(car));a.rivalPlate.setPlayerProfile(profile);a.loadedRivalCar=car;a.loadedRivalEnemy=-2;
        }
        a.replayOpponent=true;a.rivalVisible=true;a.texturesPending=true;return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
}
int IDAS3_UNITY_CALL Idas3ReplayOpponentFrame(const uint8_t* frame,int count){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.app||!r.app->replayPlaybackActive||!r.app->replayOpponent||!frame||count!=160)throw std::invalid_argument("Invalid opponent sample");
        const auto scalar=[&](unsigned n){float x;std::memcpy(&x,frame+4*n,4);if(!std::isfinite(x))throw std::invalid_argument("Invalid opponent pose");return x;};
        auto& a=*r.app;ReplayDetail d;std::memcpy(&d,frame+28,132);
        for(unsigned n:{1u,2u,3u,4u,5u,7u,8u,9u,10u,11u,12u,13u,14u,15u,16u,17u,18u,19u,20u,21u,22u,23u,36u,38u})scalar(n);
        a.rivalVehicle.position={scalar(1),scalar(2),scalar(3)};a.rivalVehicle.yaw=scalar(4);a.rivalVehicle.speed=scalar(5);a.rivalVehicle.rpm=d.rpm;a.rivalVehicle.brake=d.brake;
        a.rivalBodyWorld=d.bodyPosition;a.rivalPitch=d.pitch;a.rivalRoll=d.roll;a.rivalWheels.steeringRadians=d.steering;a.rivalWheels.suspensionY=d.suspension;a.rivalWheels.rotationRadians=d.rotation;
        a.previousRival=a.rivalVehicle;a.previousRivalBodyWorld=a.rivalBodyWorld;a.previousRivalPitch=d.pitch;a.previousRivalRoll=d.roll;a.previousRivalWheels=a.rivalWheels;
        a.rivalPresentation.restoreHeadlightState({d.lightCounter,d.lightMaximum,d.lightPhase,d.lightVisible!=0,d.lightFraction});a.replayRivalLights=d.lights!=0;a.replayAdvantage=d.progress;return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
}
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3ReplayHud(int elapsed6000,int finish6000,const int* splits,int count,const int* glyphs,int glyphCount){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.app||!r.sceneMode||!r.app->replayPlaybackActive)throw std::logic_error("No replay is open");
        if(elapsed6000<0||finish6000<60000||finish6000>=10800000||elapsed6000>finish6000||count<0||count>4||(count&&!splits)||glyphCount<0||glyphCount>5||(glyphCount&&!glyphs))throw std::invalid_argument("Invalid replay HUD metadata");
        for(int i=0;i<count;++i)if(splits[i]<=0||splits[i]>finish6000||(i&&splits[i]<=splits[i-1]))throw std::invalid_argument("Invalid replay checkpoint");
        for(int i=0;i<glyphCount;++i)if(glyphs[i]<0||glyphs[i]>221)throw std::invalid_argument("Invalid replay driver glyph");
        auto& a=*r.app;auto& race=a.race;
        race.elapsed6000=unsigned(elapsed6000);race.sector=0;race.sectionTimes6000={};
        // Cumulative crossings are rebuilt on each seek, including backwards.
        for(int i=0;i<count;++i)if(splits[i]<finish6000&&splits[i]<=elapsed6000&&unsigned(race.sector)<race.sectionCapacity-1)
            race.sectionTimes6000[race.sector++]=unsigned(splits[i]);
        race.remaining6000=0; // Not present in legacy recordings.
        for(int i=0;i<5;++i)a.frontend.battleProfile.setu(44+i*4,i<glyphCount&&glyphs[i]<=220?unsigned(glyphs[i]):220u);
        a.frontend.battleProfile.setu(76,unsigned(glyphCount));
        return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
}
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3ReplayPose(double tick,float x,float y,float z,float yaw,float speed,int gear,float pitch,int cameraMode,float orbit,int width,int height){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.app||!r.sceneMode||!r.app->replayPlaybackActive)throw std::logic_error("No replay is open");
        sceneDimensions(width,height);
        if(!std::isfinite(tick)||tick<0||tick>108000||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||!std::isfinite(yaw)||!std::isfinite(speed)||!std::isfinite(pitch)||!std::isfinite(orbit)||gear<0||gear>6||cameraMode<0||cameraMode>3)throw std::invalid_argument("Invalid replay pose");
        auto& a=*r.app;
        if((a.renderer.width!=width||a.renderer.height!=height)&&!a.renderer.resize(width,height))throw std::runtime_error(a.renderer.error);
        a.vehicle.position={x,y,z};a.vehicle.yaw=yaw;a.vehicle.speed=speed;a.vehicle.gear=gear;a.previous=a.vehicle;
        // Old IDR1 recordings have no RPM; never invent one for moderation.
        a.vehicle.rpm=0;
        a.previousPlayerBodyWorld=a.playerBodyWorld=a.vehicle.position+Vec3{0,originalCarRideHeight(unsigned(a.frontend.car)),0};
        a.previousPitch=a.bodyPitch=pitch;a.previousRoll=a.bodyRoll=0;
        a.wheelPose={};a.previousWheelPose=a.wheelPose;
        const auto p=a.course.project(a.vehicle.position);a.segment=p.segment;a.progress=p.sample.distance;
        if(a.originalHandling&&!a.importedCourse){
            // Playback does not run the race owner that normally publishes
            // this cell. Rebuild it from the viewed car, including arbitrary
            // seeks and opponent POV (whose recorded progress is a battle gap).
            a.originalCoordinate.index=int(p.segment);
            a.originalCoordinate.fraction=0;
            a.originalPath.project({x,y,z},a.originalCoordinate,true);
            a.courseLightPathIndex=a.originalCoordinate.index;
        }
        a.race.ticks=std::uint64_t(tick);
        if(a.replayDetailed){
            const auto& d=a.replayFrameData;a.vehicle.rpm=d.rpm;a.vehicle.throttle=d.throttle;a.vehicle.brake=d.brake;
            a.previousPlayerBodyWorld=a.playerBodyWorld=d.bodyPosition;a.previousPitch=a.bodyPitch=d.pitch;a.previousRoll=a.bodyRoll=d.roll;
            a.wheelPose.steeringRadians=d.steering;a.wheelPose.suspensionY=d.suspension;a.wheelPose.rotationRadians=d.rotation;a.previousWheelPose=a.wheelPose;
            a.race.elapsed6000=d.elapsed;a.race.remaining6000=d.remaining;a.race.sectionTimes6000=d.sections;a.race.sector=int(d.sector);a.race.sectionCapacity=d.capacity;
            a.race.progress=d.progress;a.raceFeedback.extensionTicks=d.extension;a.replayLights=d.lights!=0;
            a.carPresentation.restoreHeadlightState({d.lightCounter,d.lightMaximum,d.lightPhase,d.lightVisible!=0,d.lightFraction});
        }
        a.replayCameraMode=cameraMode;a.replayOrbit=orbit;
        a.drivingView=cameraMode==1?OriginalDrivingView::Bumper:OriginalDrivingView::Chase;
        Idas3UiBeginFrame(width,height);
        double dt=(tick-a.replayLastTick)/60.;
        if(a.replayLastTick<0||dt<0||dt>.25){a.wetWeather.reset();dt=1./60.;}
        a.replayLastTick=tick;
        if(!a.render(dt,false))throw std::runtime_error(a.renderer.error);
        ++r.frames;publish(r,0);return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
}
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3ReplayAudio(double dt,int playing,int reset,float master,float engine,float effects){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.app||!r.sceneMode||!r.app->replayPlaybackActive||!std::isfinite(dt)||dt<0||dt>.25||playing<0||playing>1||reset<0||reset>1)
            throw std::invalid_argument("Invalid replay audio state");
        auto& a=*r.app;
        a.audio.setOutputGains({master,0,engine,effects,a.audio.outputGains().tires});
        const bool audible=playing&&a.replayDetailed;
        if(reset){a.audio.resetRaceEffects();a.audio.selectOriginalEngine(a.root,a.frontend.battleProfile);idas3::resetUnityAudioOutput();}
        if(!audible)idas3::resetUnityAudioOutput();
        original::OriginalEngineControlInput sample;
        sample.rpm=std::clamp(a.vehicle.rpm,0.f,30000.f);sample.throttle=std::clamp(a.vehicle.throttle,0.f,1.f);
        sample.gear=a.vehicle.gear;sample.suppressShiftRelease=reset!=0;
        a.audio.setReplayEngine(sample);
        a.audio.scene(false,false,!audible);
        idas3::submitUnityAudioOutput(a.audio,audible?dt:0,idas3::UnityAudioFrameState{sample.rpm,sample.throttle,a.vehicle.speed,0,audible,!audible,audible});
        return 1;
    }catch(const std::exception& e){idas3::resetUnityAudioOutput();unityError(e.what());return 0;}
}
int IDAS3_UNITY_CALL Idas3SceneStep(const Idas3UnityInput* input){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app)throw std::logic_error("Unity scene simulation is not initialized");
        if(!input||input->size!=sizeof(*input))throw std::invalid_argument("Unity input ABI size must be88bytes");
        if(!std::isfinite(input->deltaSeconds)||input->deltaSeconds<0||input->deltaSeconds>.25)
            throw std::invalid_argument("Unity scene delta must be finite in0..0.25seconds");
        if(input->width||input->height)sceneDimensions(input->width,input->height);
        frame(r,*input);publish(r,0);return 1;
    }catch(const std::exception& e){try{destroyApp(r,false);publish(r,0);}catch(...){}unityError(e.what(),true);return 0;}
    catch(...){try{destroyApp(r,false);publish(r,0);}catch(...){}unityError("Unknown Unity scene step error",true);return 0;}
}
int IDAS3_UNITY_CALL Idas3SceneShutdown(){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{if(r.sceneMode){destroyApp(r,true);publish(r,0);r.sceneMode=false;}return 1;}
    catch(const std::exception& e){unityError(e.what());return 0;}
    catch(...){unityError("Unknown Unity scene shutdown error");return 0;}
}
int IDAS3_UNITY_CALL Idas3SceneGetOptions(Idas3Options* options){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app)throw std::logic_error("Options require initialized Unity scene mode");
        if(!options||options->size!=sizeof(*options))throw std::invalid_argument("Options ABI must be40bytes");
        const auto& app=*r.app;const auto gains=app.audio.outputGains();
        *options={sizeof(*options),1,gains.master,gains.music,gains.engine,gains.effects,
            app.drivingView==OriginalDrivingView::Bumper?0u:1u,app.paused?1u:0u,app.managedPauseOverlay?1u:0u,0};return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
    catch(...){unityError("Unknown options read error");return 0;}
}
int IDAS3_UNITY_CALL Idas3SceneGetPreRaceStatus(Idas3PreRaceStatus* status){
    static_assert(sizeof(Idas3PreRaceStatus)==104);
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app)throw std::logic_error("Pre-race status requires initialized scene mode");
        if(!status||status->size!=sizeof(*status))throw std::invalid_argument("Pre-race status ABI must be104bytes");
        const auto& app=*r.app;*status={};status->size=sizeof(*status);status->version=1;
        status->phase=app.menu||app.loadingActive||app.preRaceDialogueActive||app.legendVisitActive||app.extraModeVisitActive()?0u:
            app.vsActive?app.vsPhase:app.multiplayer.active&&app.multiplayer.waiting?0u:
            app.race.phase==RacePhase::Countdown?4u:app.race.phase==RacePhase::Running?5u:
            app.race.phase==RacePhase::Finished?6u:0u;
        status->presentationFrame=app.vsFrame;status->shot=app.vsShot;
        status->reserved=app.vsActive?app.vsBanner.sourceTick():0u;
        status->countdownRemaining=app.originalRaceStart.remaining();status->countdownDigit=app.race.originalStartDigit;
        status->playerCar=unsigned(app.frontend.car);status->opponentCar=app.rivalVisible?unsigned(app.loadedRivalCar):~0u;
        status->opponentId=app.battle?app.battleProfile.u(24):~0u;
        status->cameraView=app.drivingView==OriginalDrivingView::Bumper?0u:1u;
        status->simulationTicks=r.ticks;status->ownerTicks=app.originalRaceOwnerFrame;
        status->condition=unsigned(app.courseIndex*2+int(app.reverse));
        if(const auto capture=app.renderer.sceneCapture()){
            const auto& frame=capture->frame();const auto& camera=frame.cameras[0];
            status->eyeX=camera.eye[0];status->eyeY=camera.eye[1];status->eyeZ=camera.eye[2];
            status->targetX=camera.target[0];status->targetY=camera.target[1];status->targetZ=camera.target[2];
            status->verticalFov=camera.verticalFov;
            for(unsigned i=0;i<frame.rangeCount;++i){
                if(frame.ranges[i].lightScope==2)++status->playerRanges;
                if(frame.ranges[i].lightScope==3)++status->rivalRanges;
            }
        }
        return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
    catch(...){unityError("Unknown pre-race status error");return 0;}
}
int IDAS3_UNITY_CALL Idas3SceneCopyPreRaceName(int side,char* destination,int capacity){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app||side<0||side>1||capacity<0||(!destination&&capacity))
            throw std::invalid_argument("Invalid pre-race name request");
        const auto name=r.app->vsBanner.displayName(unsigned(side));
        if(capacity){const auto count=std::min(name.size(),std::size_t(capacity-1));
            std::memcpy(destination,name.data(),count);destination[count]=0;}
        return int(name.size());
    }catch(const std::exception& e){unityError(e.what());return -1;}
    catch(...){unityError("Unknown pre-race name error");return -1;}
}
int IDAS3_UNITY_CALL Idas3SceneCopyPreRaceBattleRecord(int side,char* destination,int capacity){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app||side<0||side>1||capacity<0||(!destination&&capacity))
            throw std::invalid_argument("Invalid pre-race battle record request");
        const auto text=r.app->vsBanner.displayBattleRecord(unsigned(side));
        if(capacity){const auto count=std::min(text.size(),std::size_t(capacity-1));std::memcpy(destination,text.data(),count);destination[count]=0;}
        return int(text.size());
    }catch(const std::exception& e){unityError(e.what());return -1;}
    catch(...){unityError("Unknown pre-race battle record error");return -1;}
}
int IDAS3_UNITY_CALL Idas3MultiplayerNextBattleRecord(const Idas3BattleRecord* before,int won,const Idas3BattleRecord* opponent,uint32_t experience,Idas3BattleRecord* after,uint32_t* nextExperience){
    try{
        if(!before||!opponent||!after||!nextExperience||(won!=0&&won!=1))throw std::invalid_argument("Invalid battle progression request");
        const auto result=advanceBattleRecord(*before,won!=0,*opponent,experience);
        *after=result.record;*nextExperience=result.experience;return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
    catch(...){unityError("Unknown battle progression error");return 0;}
}
int IDAS3_UNITY_CALL Idas3MultiplayerSetBattleRecords(const Idas3BattleRecord* local,const Idas3BattleRecord* remote){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app||!r.app->multiplayer.active||!r.app->multiplayer.waiting||r.app->vsActive)
            throw std::logic_error("Set online records after loading and before releasing the start");
        const auto validate=[](const Idas3BattleRecord* p){
            if(!p||!validBattleRecord(*p))
                throw std::invalid_argument("Invalid online battle record");
        };
        validate(local);validate(remote);r.app->multiplayer.records={*local,*remote};
        r.app->multiplayerAura[0].configure(local->level,local->streak,false);
        r.app->multiplayerAura[1].configure(remote->level,remote->streak,true);return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
    catch(...){unityError("Unknown online record setup error");return 0;}
}
int IDAS3_UNITY_CALL Idas3MultiplayerGetAuraStatus(int side,Idas3MultiplayerAuraStatus* status){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app||!r.app->multiplayer.active||side<0||side>1||!status||status->size!=sizeof(*status))
            throw std::invalid_argument("Invalid online aura status request");
        const auto& record=r.app->multiplayer.records[unsigned(side)];
        const auto& aura=r.app->multiplayerAura[unsigned(side)];std::uint32_t color=0;
        if(aura.visible())for(const auto& v:aura.model().chunks[0].batches[0].vertices)if((v.color0>>24)>(color>>24))color=v.color0;
        *status={sizeof(*status),1,record.level,record.streak,aura.style().visible?1u:0u,
            r.app->multiplayer.auraRanges[unsigned(side)],color,aura.colorFrame()};return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
    catch(...){unityError("Unknown online aura status error");return 0;}
}
int IDAS3_UNITY_CALL Idas3MultiplayerGetHudStatus(Idas3MultiplayerHudStatus* status){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app||!r.app->multiplayer.active||!status||status->size!=sizeof(*status))
            throw std::invalid_argument("Invalid online HUD status request");
        const auto& app=*r.app;const auto& mp=app.multiplayer;const auto& painted=app.hud.lastBattlePresentation();
        *status={};status->size=sizeof(*status);status->version=1;
        status->active=painted.online&&!app.menu&&!app.vsActive;
        status->cameraView=std::uint32_t(app.drivingView);
        status->localCar=mp.config.localCar;status->remoteCar=mp.config.remoteCar;
        status->sourceProfileMode=painted.profileMode;status->game2dCommands=painted.game2dCommands;
        status->portraitCommands=painted.portraitCommands;status->playerGlyphs=painted.playerGlyphs;status->rivalGlyphs=painted.rivalGlyphs;
        status->mirrorEnabled=painted.mirrorEnabled;status->playerMapMarkers=painted.localMapMarkers;status->rivalMapMarkers=painted.opponentMapMarkers;
        status->sectionCount=painted.sectionCount;status->sectionCapacity=painted.sectionCapacity;status->elapsed6000=painted.elapsed6000;
        status->localProgressMetres=mp.hudLocalMetres;status->remoteProgressMetres=mp.hudRemoteMetres;
        status->signedAdvantage=painted.signedAdvantage;
        std::copy(painted.cumulativeSections.begin(),painted.cumulativeSections.end(),status->cumulativeSections);
        std::copy(painted.renderedSectionDurations.begin(),painted.renderedSectionDurations.end(),status->renderedSectionDurations);
        return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
    catch(...){unityError("Unknown online HUD status error");return 0;}
}
int IDAS3_UNITY_CALL Idas3MultiplayerCopyHudText(int side,char* destination,int capacity){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app||!r.app->multiplayer.active||side<0||side>1||capacity<0||(!destination&&capacity))
            throw std::invalid_argument("Invalid online HUD text request");
        const auto& painted=r.app->hud.lastBattlePresentation();
        const auto text=side==0?painted.playerName+" ["+painted.playerCarCode+"]":painted.rivalName+" ["+painted.rivalCarCode+"]";
        if(capacity){const auto count=std::min(text.size(),std::size_t(capacity-1));std::memcpy(destination,text.data(),count);destination[count]=0;}
        return int(text.size());
    }catch(const std::exception& e){unityError(e.what());return -1;}
    catch(...){unityError("Unknown online HUD text error");return -1;}
}
int IDAS3_UNITY_CALL Idas3MultiplayerDiagnosticFinish(uint64_t ticks60){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app||r.app->saveRoot.filename()!="userdata"||
           !fs::is_regular_file(r.app->saveRoot.parent_path()/"ISOLATED_MULTIPLAYER_TEST.txt")||
           !fs::is_regular_file(r.app->saveRoot.parent_path()/"ISOLATED_MULTIPLAYER_PRESENTATION_TEST.txt"))
            throw std::logic_error("Synthetic finishes require an isolated multiplayer presentation test");
        auto& app=*r.app;
        if(!app.multiplayer.active||app.multiplayer.disconnected||app.multiplayer.waiting||app.vsActive||
           app.race.phase!=RacePhase::Running||app.multiplayer.finishWinner!=-2||ticks60<60||ticks60>216000)
            throw std::logic_error("Synthetic finish requires an active private race");
        app.race.phase=RacePhase::Finished;app.race.timeUp=false;app.race.ticks=ticks60;
        app.race.elapsed6000=unsigned(ticks60*100);app.clock.reset();app.updateAudioScene();publish(r,0);return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
    catch(...){unityError("Unknown synthetic finish error");return 0;}
}
int IDAS3_UNITY_CALL Idas3MultiplayerGetBattleRecord(int side,Idas3BattleRecord* record){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app||!r.app->multiplayer.active||side<0||side>1||!record)
            throw std::invalid_argument("Invalid online battle record request");
        *record=r.app->multiplayer.records[unsigned(side)];return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
    catch(...){unityError("Unknown online record request error");return 0;}
}
int IDAS3_UNITY_CALL Idas3SceneSetPreRaceNames(const char* localUtf8,const char* opponentUtf8){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app||(!r.app->replayPlaybackActive&&(!r.app->multiplayer.active||!r.app->multiplayer.waiting||r.app->vsActive)))
            throw std::logic_error("Set online names after loading and before releasing the start");
        const auto parse=[](const char* text){
            if(!text)throw std::invalid_argument("A driver name is required");
            const auto length=strnlen_s(text,129);
            if(!length||length>128||MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,int(length),nullptr,0)<=0)
                throw std::invalid_argument("Driver names must be valid UTF8 up to128bytes");
            for(std::size_t i=0;i<length;++i)if(static_cast<unsigned char>(text[i])<32)
                throw std::invalid_argument("Driver names cannot contain control characters");
            return std::string(text,length);
        };
        auto local=parse(localUtf8),opponent=parse(opponentUtf8);
        r.app->multiplayer.localName=std::move(local);r.app->multiplayer.remoteName=std::move(opponent);return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
    catch(...){unityError("Unknown pre-race name setup error");return 0;}
}
int IDAS3_UNITY_CALL Idas3SceneGetRaceMusicState(Idas3RaceMusicState* state){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app)throw std::logic_error("Race music requires initialized Unity scene mode");
        if(!state||state->size!=sizeof(*state))throw std::invalid_argument("Race music state must be32bytes");
        const auto& app=*r.app;const bool eligible=app.raceMusicOpponentEligible();
        *state={sizeof(*state),1,eligible?1u:0u,unsigned(raceMusicCatalog.size()),
            app.selectedRaceMusic,app.audio.musicTrack,eligible?app.frontend.battleProfile.u(24):~0u,0};
        return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
    catch(...){unityError("Unknown race music state error");return 0;}
}
int IDAS3_UNITY_CALL Idas3SceneGetRaceAudioStatus(Idas3RaceAudioStatus* status){
    static_assert(sizeof(Idas3RaceAudioStatus)==56);
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app)throw std::logic_error("Audio status requires initialized Unity scene mode");
        if(!status||status->size!=sizeof(*status))throw std::invalid_argument("Audio status must be56bytes");
        const auto s=r.app->audio.raceTimingStatistics();
        *status={sizeof(*status),1,s.flags,s.scene,s.track,0,s.streamFrame,s.idlePcmFrames,s.idleControlFrames,s.drivingControlFrames};
        return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
    catch(...){unityError("Unknown race audio status error");return 0;}
}
int IDAS3_UNITY_CALL Idas3SceneCopyRaceMusicText(int index,int field,char* destination,int capacity){
    try{
        if(!validRaceMusicMetadata(index)||field<0||field>2||capacity<0||(!destination&&capacity))
            throw std::invalid_argument("Invalid race music text request");
        const auto& track=raceMusicCatalog[index<0?effectiveRaceMusicSelection(index):index];
        const std::string_view text=field==0?(index<0?"default":track.id):
            field==1?(index<0?"Game default \xe2\x80\x94 Speedy Speed Boy":track.title):track.artist;
        if(capacity){
            auto count=std::min(text.size(),std::size_t(capacity-1));
            // A short UTF8 destination still receives a valid string.
            while(count&&count<text.size()&&(static_cast<unsigned char>(text[count])&0xc0)==0x80)--count;
            std::memcpy(destination,text.data(),count);destination[count]=0;
        }
        return int(text.size());
    }catch(const std::exception& e){unityError(e.what());return -1;}
    catch(...){unityError("Unknown race music text error");return -1;}
}
int IDAS3_UNITY_CALL Idas3SceneGetRaceMusicStage(int index){
    return validRaceMusicMetadata(index)?raceMusicCatalog[index<0?effectiveRaceMusicSelection(index):index].stage:-1;
}
int IDAS3_UNITY_CALL Idas3SceneSetRaceMusicTrack(int index,int context){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app)throw std::logic_error("Race music requires initialized Unity scene mode");
        r.app->selectRaceMusic(index,context);return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
    catch(...){unityError("Unknown race music selection error");return 0;}
}
int IDAS3_UNITY_CALL Idas3SceneApplyOptions(const Idas3Options* options){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app)throw std::logic_error("Options require initialized Unity scene mode");
        if(!options||options->size!=sizeof(*options)||options->version!=1||options->cameraView>1||
            options->paused>1||options->managedPauseOverlay>1||options->reserved)
            throw std::invalid_argument("Invalid options v1");
        auto& app=*r.app;
        // The setter validates every gain before changing anything. All later
        // assignments are bounded/nonthrowing; rejected options are atomic.
        app.audio.setOutputGains({options->masterGain,options->musicGain,options->engineGain,options->effectsGain,app.audio.outputGains().tires});
        app.drivingView=options->cameraView==0?OriginalDrivingView::Bumper:OriginalDrivingView::Chase;
        app.managedPauseOverlay=options->managedPauseOverlay!=0;
        publish(r,0);return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
    catch(...){unityError("Unknown options apply error");return 0;}
}
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneSetTireVolume(float value){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app)throw std::logic_error("Tire volume requires initialized scene mode");
        auto gains=r.app->audio.outputGains();gains.tires=value;r.app->audio.setOutputGains(gains);return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
}
IDAS3_UNITY_EXPORT float IDAS3_UNITY_CALL Idas3SceneGetTireVolume(){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    return r.sceneMode&&r.app?r.app->audio.outputGains().tires:-1.f;
}
int IDAS3_UNITY_CALL Idas3SceneSetControllerResponse(int response){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    if(!r.sceneMode||!r.app){unityError("Controller response requires initialized Unity scene mode");return 0;}
    if(response<0||response>2){unityError("Unknown controller response");return 0;}
    if(r.app->controllerResponse!=static_cast<original::ControllerResponse>(response))r.app->resetSteeringSmoothing();
    r.app->controllerResponse=static_cast<original::ControllerResponse>(response);return 1;
}
int IDAS3_UNITY_CALL Idas3SceneGetControllerResponse(){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    if(!r.sceneMode||!r.app){unityError("Controller response requires initialized Unity scene mode");return -1;}
    return static_cast<int>(r.app->controllerResponse);
}
int IDAS3_UNITY_CALL Idas3SceneSetSteeringDeadzone(float value){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    if(!r.sceneMode||!r.app){unityError("Steering options require initialized Unity scene mode");return 0;}
    if(!std::isfinite(value)||value<0||value>.3f){unityError("Steering deadzone must be0..0.3");return 0;}
    if(r.app->steeringDeadzone!=value)r.app->resetSteeringSmoothing();
    r.app->steeringDeadzone=value;return 1;
}
float IDAS3_UNITY_CALL Idas3SceneGetSteeringDeadzone(){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    return r.sceneMode&&r.app?r.app->steeringDeadzone:-1.f;
}
struct Idas3HudCarFrame {uint32_t size,vertices,ranges,textures;const void* vertexData;const void* rangeData;const void* textureData;};
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneHudCar(Idas3HudCarFrame* frame){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app||!frame||frame->size!=sizeof(*frame))throw std::invalid_argument("Invalid editor car request");
        const auto folder=std::string(originalCarFolders.at(r.app->frontend.car));const auto root=r.app->root;
        auto profile=r.app->frontend.battleProfile;profile.setu(16,unsigned(r.app->frontend.car));profile.setu(64,r.app->frontend.selectedColor());
        auto model=idas3::NativeModel::load(root/"data/original_models"/folder/(folder+".idasmesh"));
        auto presentation=idas3::CarPresentation::loadPlayerProfile(root,profile);presentation.applyMaterials(model);
        r.hudCar={};r.hudCar.originalCar(model,presentation.pose({},false,false),{0,0,0},0);
        r.hudCarTextures=idas3::NativeTextureBank::load(root/"data/original_assets/cars"/folder/"textures/textures.idastex");
        r.hudCarRanges.clear();for(const auto& range:r.hudCar.ranges)r.hudCarRanges.push_back({range.first,range.count,range.texture});
        r.hudCarImages.clear();for(unsigned i=0;i<r.hudCarTextures.size();++i){const auto& image=r.hudCarTextures.at(i);r.hudCarImages.push_back({image.width,image.height,image.argb.size(),image.argb.data()});}
        *frame={sizeof(*frame),unsigned(r.hudCar.vertices.size()),unsigned(r.hudCarRanges.size()),unsigned(r.hudCarImages.size()),r.hudCar.vertices.data(),r.hudCarRanges.data(),r.hudCarImages.data()};return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
}
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneHudPreview(int mode,int width,int height,int mapSize,int mapZoom,float seconds,int messages){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app||mode<0||mode>2||width<320||height<240||width>8192||height>8192||!std::isfinite(seconds))throw std::invalid_argument("Invalid HUD preview");
        if(!r.hudEditor){auto hud=std::make_unique<idas3::Hud>();hud->loadOriginal(r.app->root);r.hudEditor=std::move(hud);}
        auto& hud=*r.hudEditor;hud.resize(width,height);hud.setMapSize(mapSize);hud.setMapZoom(mapZoom);
        idas3::Course course;course.name="HUD editor";course.length=1000;course.points={{0,0,0},{0,0,1000}};course.left={{-5,0,0},{-5,0,1000}};course.right={{5,0,0},{5,0,1000}};course.cumulative={0,1000};
        idas3::VehicleState car;car.speed=36.f+std::sin(seconds)*6.f;car.rpm=6000.f+std::sin(seconds)*1200.f;car.gear=4;car.position={0,0,400};
        auto rival=car;rival.position.z+=18.9f;
        idas3::RaceClock race;race.phase=idas3::RacePhase::Running;race.originalTiming=true;race.ticks=300;race.elapsed6000=270000+unsigned(std::fmod(std::max(0.f,seconds),60.f)*6000);race.remaining6000=438000;race.sectionCapacity=4;race.sector=1;race.sectionTimes6000={220000,0,0,0};
        if(messages){race.originalStartDigit=0;race.originalStartElapsed=180;}
        idas3::OriginalResultsState records;records.livePanel=true;records.edgeAnchored=true;records.suppliedRecordTargets=true;records.bestTimes6000={1000000,1030000,1100000};records.modelBestAvailable=true;
        idas3::UiState state;state.menu=false;state.course=&course;state.car=&car;state.rival=&rival;state.race=&race;state.hudIntroFrame=240;state.battleHudFrame=240;state.battleEnemy=13;state.battleProfileMode=0;state.battleAdvantage=18.9f;state.battleRivalPositionFraction=.2f;state.rearView=true;state.timeExtended=true;state.battle=mode==1;state.results=mode==0?&records:nullptr;
        state.onlineBattleHud.active=mode==2;state.onlineBattleHud.frame=240;state.onlineBattleHud.playerName="PLAYER";state.onlineBattleHud.rivalName="OPPONENT";state.onlineBattleHud.playerCar=0;state.onlineBattleHud.rivalCar=8;state.onlineBattleHud.advantage=18.9f;
        Idas3UiBeginFrame(width,height);auto pixels=hud.paint(state);
        if(mode==0){const idas3::UnityUiHudScope group(4);hud.originalBank().paintAuthoredChunk(std::span<std::uint32_t>(const_cast<std::uint32_t*>(pixels),std::size_t(width)*height),width,height,186);idas3::unityUiMarkHud(pixels);}
        idas3::unityUiSubmit(pixels,width,height,false,false,false);return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
}
int IDAS3_UNITY_CALL Idas3SceneSetAiDifficulty(int difficulty){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    if(!r.sceneMode||!r.app||difficulty<0||difficulty>2){unityError("Invalid AI difficulty");return 0;}
    r.app->aiDifficulty=difficulty;return 1;
}
int IDAS3_UNITY_CALL Idas3SceneSetMapZoom(int zoom){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    if(!r.sceneMode||!r.app||zoom<0||zoom>2){unityError("Invalid minimap zoom");return 0;}
    r.app->hud.setMapZoom(zoom);return 1;
}
int IDAS3_UNITY_CALL Idas3SceneSetMapSize(int size){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    if(!r.sceneMode||!r.app||size<0||size>2){unityError("Invalid minimap size");return 0;}
    r.app->hud.setMapSize(size);return 1;
}
int IDAS3_UNITY_CALL Idas3SceneSetPerformance(int rainDetail){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    if(!r.sceneMode||!r.app||rainDetail<0||rainDetail>1){unityError("Invalid presentation quality options");return 0;}
    r.app->performanceRainDetail=rainDetail;return 1;
}
int IDAS3_UNITY_CALL Idas3SceneSetCustomRaceMusic(const short* samples,int count,int rate,int channels,int context){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app)throw std::logic_error("Custom music requires initialized scene mode");
        if(!samples||count<1||count>32*1024*1024||(channels!=1&&channels!=2)||count%channels||rate<8000||rate>48000||count/channels>rate*600)
            throw std::invalid_argument("Custom music must be mono/stereo PCM, at most ten minutes and64MiB");
        auto clip=std::make_shared<OriginalAudioClip>();clip->sampleRate=rate;clip->channels=channels;clip->looping=true;clip->samples.assign(samples,samples+count);
        auto& app=*r.app;
        if(context==2){if(!app.menu||app.loadingActive||app.multiplayer.active)throw std::logic_error("Custom music restore requires menus");}
        else app.selectRaceMusic(-1,context);
        app.audio.customRaceMusic=std::move(clip);app.selectedRaceMusic=-2;return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
    catch(...){unityError("Unknown custom music error");return 0;}
}
int IDAS3_UNITY_CALL Idas3SceneSetSteeringSmoothing(float value){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    if(!r.sceneMode||!r.app){unityError("Steering options require initialized Unity scene mode");return 0;}
    if(!std::isfinite(value)||value<0.f||value>1.f){unityError("Steering smoothing must be 0..1");return 0;}
    if(r.app->steeringSmoothing.amount()!=value){r.app->steering=0;r.app->steeringSmoothing.reset();}
    r.app->steeringSmoothing.setAmount(value);return 1;
}
float IDAS3_UNITY_CALL Idas3SceneGetSteeringSmoothing(){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    return r.sceneMode&&r.app?r.app->steeringSmoothing.amount():-1.f;
}
int IDAS3_UNITY_CALL Idas3SceneImportedSourceNode(){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    if(!r.sceneMode||!r.app||!r.app->importedCourse)return -1;
    const auto& app=*r.app;
    return int(app.importedCourse->source.project(app.vehicle.position).segment);
}
int IDAS3_UNITY_CALL Idas3SceneGetWheelState(Idas3WheelState* out){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    if(!r.sceneMode||!r.app||!out||out->size!=sizeof(*out)){unityError("Invalid wheel state request");return 0;}
    *out={sizeof(*out),1};const auto& app=*r.app;
    out->simulationTicks=app.vehicle.tick;
    if(!app.originalHandling||app.menu||app.paused||!app.active||app.legendVisitActive||app.preRaceDialogueActive||
        app.loadingActive||app.extraModeVisitActive()||app.race.phase!=RacePhase::Running||app.multiplayerDisconnected())return 1;
    const auto& d=app.presentedSession().vehicle().drive;
    out->speed=app.vehicle.speed;out->steering=-d.f(0x1CC);
    out->headingError=std::remainder(d.f(0x108)-d.f(0x10),2.f*pi);
    out->wallLateral=d.f(0x258)*std::cos(app.vehicle.yaw)-d.f(0x25C)*std::sin(app.vehicle.yaw);
    out->impact=app.vehicle.wallImpactSpeed;out->flags=1|(app.vehicle.wallContact?2:0);return 1;
}
int IDAS3_UNITY_CALL Idas3SceneSetPaused(int paused){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app)throw std::logic_error("Pause requires initialized Unity scene mode");
        if(paused!=0&&paused!=1)throw std::invalid_argument("Pause must be0or1");
        auto& app=*r.app;
        if(paused&&app.multiplayer.active)throw std::logic_error("Online races cannot pause");
        if(paused&&(app.menu||app.legendVisitActive||app.preRaceDialogueActive||app.extraModeVisitActive()||app.loadingActive||app.race.phase==RacePhase::Finished))
            throw std::logic_error("Pause requires an active race owner");
        if(app.paused!=(paused!=0)){
            app.paused=paused!=0;app.clock.reset();idas3::resetUnityAudioOutput();
            app.updateAudioScene();
        }
        publish(r,0);return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
    catch(...){unityError("Unknown pause error");return 0;}
}
int IDAS3_UNITY_CALL Idas3SceneRestart(){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app)throw std::logic_error("Restart requires initialized Unity scene mode");
        auto& app=*r.app;
        if(app.multiplayer.active||app.menu||app.legendVisitActive||app.preRaceDialogueActive||app.extraModeVisitActive()||app.loadingActive||app.race.phase==RacePhase::Finished)
            throw std::logic_error("Restart requires an offline race owner");
        app.start();Idas3UiBeginFrame(app.renderer.width,app.renderer.height);
        if(!app.render(0))throw std::runtime_error(app.renderer.error);
        app.updateAudioScene();publish(r,0);return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
    catch(...){unityError("Unknown restart error");return 0;}
}
int IDAS3_UNITY_CALL Idas3SceneRetire(){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app||!r.app->retireLegendRace())throw std::logic_error("Retire requires an offline Legend race");
        auto& app=*r.app;Idas3UiBeginFrame(app.renderer.width,app.renderer.height);
        if(!app.render(0))throw std::runtime_error(app.renderer.error);
        idas3::resetUnityAudioOutput();app.updateAudioScene();publish(r,0);return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
    catch(...){unityError("Unknown retire error");return 0;}
}
int IDAS3_UNITY_CALL Idas3SceneChallenger(int action){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app||r.app->multiplayer.active)throw std::logic_error("Challenger requires an offline scene");
        auto& app=*r.app;
        if(action==0){app.challengerNotice=false;return 1;}
        if(action==1){app.challengerNotice=true;app.paused=false;app.clock.reset();app.audio.playChallengerCue();return 1;}
        if(action!=2)throw std::invalid_argument("Unknown challenger action");
        app.challengerNotice=false;
        app.returnToCourseSelection(true);idas3::resetUnityAudioOutput();
        Idas3UiBeginFrame(app.renderer.width,app.renderer.height);
        if(!app.render(0))throw std::runtime_error(app.renderer.error);
        app.updateAudioScene();publish(r,0);return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
}
int IDAS3_UNITY_CALL Idas3SceneReturnToCourse(){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app)throw std::logic_error("Course return requires initialized Unity scene mode");
        auto& app=*r.app;
        if(app.multiplayer.active||app.menu||app.legendVisitActive||app.preRaceDialogueActive||app.extraModeVisitActive()||app.loadingActive||app.race.phase==RacePhase::Finished)
            throw std::logic_error("Course return requires an offline race owner");
        app.returnToCourseSelection();idas3::resetUnityAudioOutput();
        Idas3UiBeginFrame(app.renderer.width,app.renderer.height);
        if(!app.render(0))throw std::runtime_error(app.renderer.error);
        app.updateAudioScene();publish(r,0);return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
    catch(...){unityError("Unknown course return error");return 0;}
}
int IDAS3_UNITY_CALL Idas3SceneCanFullTune(){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    return r.sceneMode&&r.app&&r.app->canFullTune()?1:0;
}
int IDAS3_UNITY_CALL Idas3SceneFullTune(){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app)throw std::logic_error("Full Tune requires an initialized scene");
        r.app->beginFullTune();idas3::resetUnityAudioOutput();
        Idas3UiBeginFrame(r.app->renderer.width,r.app->renderer.height);
        if(!r.app->render(0))throw std::runtime_error(r.app->renderer.error);
        r.app->updateAudioScene();publish(r,0);return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
}
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneGetFrame(Idas3SceneFrame* destination){
    auto& r=unityRuntime();
    if(!destination||destination->size!=sizeof(*destination)||!r.sceneMode||!r.app||!r.app->renderer.sceneCapture())return 0;
    *destination=r.app->renderer.sceneCapture()->frame();return 1;
}
const uint64_t* IDAS3_UNITY_CALL Idas3SceneGetGeometryIds(uint64_t generation,uint32_t rangeCount){
    auto& r=unityRuntime();
    if(!r.sceneMode||!r.app||!r.app->renderer.sceneCapture())return nullptr;
    const auto& capture=*r.app->renderer.sceneCapture();
    const auto& frame=capture.frame();
    return frame.frameGeneration==generation&&frame.rangeCount==rangeCount?capture.geometryIds():nullptr;
}
int IDAS3_UNITY_CALL Idas3MultiplayerCurrentGear(){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    return r.app&&r.app->multiplayer.active?r.app->vehicle.gear:-1;
}
int IDAS3_UNITY_CALL Idas3MultiplayerCurrentCar(){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    if(!r.app)return -1;
    if(r.app->activeSaveSlot<0)return 5+r.app->frontend.car;
    const int slot=r.app->activeSaveSlot,car=r.app->frontend.car;
    const LocalSaveSlots slots(r.app->userdataRoot()/"saves");
    return slots.at(unsigned(slot)).used&&slots.at(unsigned(slot)).car==unsigned(car)
        ?slot:onlineSlotCarSelection(slot,car);
}
int IDAS3_UNITY_CALL Idas3MultiplayerReadCar(int selection,uint32_t* words,uint32_t count){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app||!words||count!=307)throw std::invalid_argument("Invalid saved car query");
        original::OriginalBattleProfile profile;const int found=r.app->onlineCarProfile(selection,profile);
        if(found)std::copy(profile.words.begin(),profile.words.end(),words);return found;
    }catch(const std::exception& e){unityError(e.what());return -1;}
}
int IDAS3_UNITY_CALL Idas3MultiplayerReadRaceCar(int side,uint32_t* words,uint32_t count){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    if(!r.app||!r.app->multiplayer.active||!words||count!=307||side<0||side>1)return 0;
    const auto& profile=side?r.app->multiplayer.remoteProfile:r.app->battleProfile;
    std::copy(profile.words.begin(),profile.words.end(),words);return 1;
}
int IDAS3_UNITY_CALL Idas3MultiplayerStartSaved(const Idas3MultiplayerConfig* config,int selection,const uint32_t* local,const uint32_t* remote,uint32_t count){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app||!config||!local||!remote||count!=307)throw std::invalid_argument("Invalid saved race cars");
        validateMultiplayerConfig(*config);
        original::OriginalBattleProfile player;
        if(!r.app->onlineCarProfile(selection,player)||player.u(16)!=config->localCar||
           !std::equal(player.words.begin(),player.words.end(),local))
            throw std::invalid_argument("Saved car changed; reopen the online menu and select it again");
        // Only appearance/name data are accepted from the peer. Its tuning,
        // balances and progress never enter the local simulation or saves.
        auto opponent=original::makeOriginalFreshBattleProfile();
        for(unsigned offset:{16u,44u,48u,52u,56u,60u,64u,76u,156u,160u,164u})opponent.setu(offset,remote[offset/4]);
        if(config->version==2){
            opponent.setu(24,remote[24/4]);opponent.setu(152,remote[152/4]);
            if(opponent.u(24)>31||opponent.byte(164)>=76)throw std::invalid_argument("Invalid remote physics profile");
        }
        if(opponent.u(16)!=config->remoteCar||opponent.u(76)>5||opponent.u(64)>=original::originalCarColorCounts.at(config->remoteCar))
            throw std::invalid_argument("Invalid remote saved car appearance");
        for(unsigned offset=44;offset<=60;offset+=4)if(opponent.u(offset)>220)throw std::invalid_argument("Invalid remote driver glyph");
        for(unsigned offset=156;offset<164;++offset)if(opponent.byte(offset)>6)throw std::invalid_argument("Invalid remote body part");
        if(opponent.byte(165)>3||opponent.byte(166)>3)throw std::invalid_argument("Invalid remote appearance flags");
        auto request=*config; // Lobby transmission choice is race-local; saved profile is unchanged.
        r.app->startMultiplayer(request,&player,&opponent);
        Idas3UiBeginFrame(r.app->renderer.width,r.app->renderer.height);
        if(!r.app->render(0))throw std::runtime_error(r.app->renderer.error);
        publish(r,0);return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
    catch(...){unityError("Unknown saved car start error");return 0;}
}
int IDAS3_UNITY_CALL Idas3MultiplayerStart(const Idas3MultiplayerConfig* config){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app)throw std::logic_error("Multiplayer requires initialized Unity scene mode");
        if(!config)throw std::invalid_argument("Null multiplayer configuration");
        validateMultiplayerConfig(*config);r.app->startMultiplayer(*config);
        Idas3UiBeginFrame(r.app->renderer.width,r.app->renderer.height);
        if(!r.app->render(0))throw std::runtime_error(r.app->renderer.error);
        publish(r,0);return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
    catch(...){unityError("Unknown multiplayer start error");return 0;}
}
int IDAS3_UNITY_CALL Idas3MultiplayerEnableAuthority(uint64_t race,int remoteAutomatic,int boost){
    return Idas3MultiplayerEnableAuthorityRules(race,remoteAutomatic,boost,1);
}
int IDAS3_UNITY_CALL Idas3MultiplayerEnableAuthorityRules(uint64_t race,int remoteAutomatic,int boost,int collisions){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{if(!r.sceneMode||!r.app||remoteAutomatic<0||remoteAutomatic>1||boost<0||boost>1||collisions<0||collisions>1)throw std::invalid_argument("Invalid authority initialization");
        r.app->enableAuthority(race,remoteAutomatic!=0,boost!=0,collisions!=0);return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
}
int IDAS3_UNITY_CALL Idas3MultiplayerAuthorityPacket(uint8_t* bytes,uint32_t capacity){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{if(!r.app||!r.app->authorityLink||!bytes)throw std::logic_error("Authority race unavailable");
        auto packet=r.app->authorityLink->packet();if(packet.size()>capacity)throw std::invalid_argument("Authority output buffer too small");
        std::copy(packet.begin(),packet.end(),bytes);return int(packet.size());
    }catch(const std::exception& e){unityError(e.what());return -1;}
}
int IDAS3_UNITY_CALL Idas3MultiplayerAuthorityReceive(const uint8_t* bytes,uint32_t count){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{if(!r.app||r.app->multiplayerDisconnected()||!bytes||count>1064)throw std::invalid_argument("Invalid authority input");
        r.app->receiveAuthority({bytes,count});return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
}
// Read-only, opt-in presentation diagnostic. No simulation or save mutation.
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3MultiplayerMotionSample(double* values,uint32_t count){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    if(!r.sceneMode||!r.app||!r.app->authorityRace||!values||count!=26)return 0;
    const auto& a=*r.app;const unsigned remote=1-a.multiplayer.config.localSlot;
    const float alpha=a.menu||a.paused||a.race.phase==RacePhase::Finished?1.f:a.clock.alpha();
    const auto pose=lerp(a.previousRival.position,a.rivalVehicle.position,alpha);
    const auto body=lerp(a.previousRivalBodyWorld,a.rivalBodyWorld,alpha);
    const auto& actor=a.authorityRace->car(remote).actor();
    const auto offset=a.authorityVisualOffset[remote];const auto prior=a.previousRival.position,current=a.rivalVehicle.position;
    const auto local=lerp(a.previous.position,a.vehicle.position,alpha);
    const auto& metrics=a.authorityLink->timeline().metrics();
    const double sample[]{double(a.authorityRace->frame()),alpha,double(metrics.rollbacks),double(metrics.replayedFrames),
        pose.x,pose.y,pose.z,body.x,body.y,body.z,actor.f(0),actor.f(4),actor.f(8),offset.x,offset.y,offset.z,
        prior.x,prior.y,prior.z,current.x,current.y,current.z,local.x,local.y,local.z,double(r.frames)};
    std::copy(std::begin(sample),std::end(sample),values);return 1;
}
int IDAS3_UNITY_CALL Idas3MultiplayerAuthorityStatus(Idas3AuthorityStatus* status){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{if(!r.app||!r.app->authorityLink||!status||status->size!=sizeof(*status))throw std::invalid_argument("Invalid authority status request");
        auto& app=*r.app;const auto& timeline=app.authorityLink->timeline();const auto& metrics=timeline.metrics();
        *status={sizeof(*status),1,timeline.frame(),timeline.confirmed(),app.authorityLink->verifiedPeerFrames(),app.authorityRace->contactFrames(),
            metrics.rollbacks,metrics.replayedFrames,metrics.maxDepth,app.authorityStalled?1u:0u,float(metrics.maxCorrection),float(metrics.maxReplayMs),
            app.authorityWinner(),0,app.authorityFinishTicks[0],app.authorityFinishTicks[1]};return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
}
int IDAS3_UNITY_CALL Idas3MultiplayerGetRemoteSnapshot(Idas3MultiplayerSnapshot* snapshot){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{if(!r.app||!r.app->multiplayer.active||!snapshot||snapshot->size!=sizeof(*snapshot))throw std::invalid_argument("Invalid remote snapshot request");
        *snapshot=r.app->multiplayer.remote;return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
}
int IDAS3_UNITY_CALL Idas3MultiplayerSetGo(int released){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app)throw std::logic_error("Multiplayer scene is not initialized");
        if(released!=0&&released!=1)throw std::invalid_argument("Multiplayer GO must be0or1");
        r.app->setMultiplayerGo(released!=0);publish(r,0);return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
    catch(...){unityError("Unknown multiplayer GO error");return 0;}
}
int IDAS3_UNITY_CALL Idas3MultiplayerSetResult(int winner){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app)throw std::logic_error("Multiplayer scene is not initialized");
        r.app->setMultiplayerResult(winner);publish(r,0);return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
    catch(...){unityError("Unknown multiplayer result error");return 0;}
}
int IDAS3_UNITY_CALL Idas3MultiplayerGetLocalSnapshot(Idas3MultiplayerSnapshot* snapshot){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app)throw std::logic_error("Multiplayer scene is not initialized");
        if(!snapshot||snapshot->size!=sizeof(*snapshot))throw std::invalid_argument("Multiplayer snapshot ABI must be128bytes");
        *snapshot=r.app->multiplayerSnapshot();return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
    catch(...){unityError("Unknown multiplayer snapshot error");return 0;}
}
int IDAS3_UNITY_CALL Idas3MultiplayerApplyRemoteSnapshot(const Idas3MultiplayerSnapshot* snapshot){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app)throw std::logic_error("Multiplayer scene is not initialized");
        if(!snapshot)throw std::invalid_argument("Null multiplayer snapshot");
        r.app->applyMultiplayerSnapshot(*snapshot);return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
    catch(...){unityError("Unknown remote multiplayer snapshot error");return 0;}
}
int IDAS3_UNITY_CALL Idas3MultiplayerDisconnect(){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app)throw std::logic_error("Multiplayer scene is not initialized");
        r.app->disconnectMultiplayer();idas3::resetUnityAudioOutput();
        Idas3UiBeginFrame(r.app->renderer.width,r.app->renderer.height);
        if(!r.app->render(0))throw std::runtime_error(r.app->renderer.error);
        publish(r,0);return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
    catch(...){unityError("Unknown multiplayer disconnect error");return 0;}
}
int IDAS3_UNITY_CALL Idas3MultiplayerLeave(){
    auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
    try{
        if(!r.sceneMode||!r.app)throw std::logic_error("Multiplayer scene is not initialized");
        r.app->leaveMultiplayer();Idas3UiBeginFrame(r.app->renderer.width,r.app->renderer.height);
        if(!r.app->render(0))throw std::runtime_error(r.app->renderer.error);
        publish(r,0);return 1;
    }catch(const std::exception& e){unityError(e.what());return 0;}
    catch(...){unityError("Unknown multiplayer leave error");return 0;}
}
int IDAS3_UNITY_CALL Idas3SceneGetRivalStatus(Idas3RivalStatus* status){
    try{
        if(!status||status->size!=sizeof(*status))return 0;
        auto& r=unityRuntime();std::lock_guard lock(r.renderMutex);
        if(!r.sceneMode||!r.app)return 0;
        const auto& app=*r.app;
        Idas3RivalStatus out{};out.size=sizeof(out);out.version=1;
        out.preRaceActive=app.preRaceDialogueActive;out.legendActive=app.legendVisitActive;
        if(out.preRaceActive||out.legendActive){
            const auto& dialog=out.preRaceActive?app.preRaceDialogue.state():app.legendVisit.dialogueState();
            out.dialogEnemy=out.preRaceActive?app.frontend.battleProfile.u(24):app.battleProfile.u(24);
            out.dialogKind=dialog.kind;out.dialogPhase=dialog.phase;
        }
        out.choiceVisible=out.legendActive&&app.legendVisit.choiceVisible();
        if(out.choiceVisible){out.choiceKind=unsigned(app.legendVisit.choiceKind());out.selectedIndex=app.legendVisit.selectedIndex();}
        out.musicCue=app.audio.originalMusicCue();out.musicPlaying=app.audio.selectionPlaying();
        out.musicSamplePosition=app.audio.selectionSamplePosition();
        *status=out;return 1;
    }catch(...){return 0;}
}
#if defined(IDAS3_PORTABLE_SCENE)
int IDAS3_UNITY_CALL Idas3UnityQueueInitialize(const char*,const char*,void*,int,int,int){unityError("Android requires Idas3SceneInitialize");return 0;}
#else
int IDAS3_UNITY_CALL Idas3UnityQueueInitialize(const char* assets,const char* saves,void* texture,int width,int height,int output){
    try{
        dimensions(width,height);if(!texture)throw std::invalid_argument("Unity D3D11 texture pointer is null");
        UnityCommand command;command.operation=UnityOperation::Initialize;command.assets=unicodePath(assets);command.saves=unicodePath(saves);
        if(!fs::is_directory(command.assets/"data"))throw std::invalid_argument("Asset root must contain the complete data directory");
        if(command.saves==command.assets||within(command.saves,fs::weakly_canonical(command.assets/"data"))||within(command.saves,fs::weakly_canonical(command.assets/"userdata")))
            throw std::invalid_argument("Unity saves must be isolated from original assets and userdata");
        command.unityTexture=static_cast<ID3D11Texture2D*>(texture);command.width=width;command.height=height;command.outputAudio=output!=0;
        return enqueue(std::move(command));
    }catch(const std::exception& e){unityError(e.what());return 0;}catch(...){unityError("Cannot queue Unity initialization");return 0;}
}
#endif
int IDAS3_UNITY_CALL Idas3UnityQueueFrame(const Idas3UnityInput* input){
    try{
        if(!input||input->size!=sizeof(Idas3UnityInput))throw std::invalid_argument("Unity input ABI size must be88bytes");
        if(!std::isfinite(input->deltaSeconds)||input->deltaSeconds<0)throw std::invalid_argument("Unity delta must be finite and nonnegative");
        if(input->width||input->height)dimensions(input->width,input->height);
        UnityCommand command;command.input=*input;return enqueue(std::move(command));
    }catch(const std::exception& e){unityError(e.what());return 0;}catch(...){unityError("Cannot queue Unity frame");return 0;}
}
int IDAS3_UNITY_CALL Idas3UnityQueueShutdown(){try{UnityCommand command;command.operation=UnityOperation::Shutdown;return enqueue(std::move(command));}catch(...){unityError("Cannot queue Unity shutdown");return 0;}}
Idas3UnityRenderEvent IDAS3_UNITY_CALL Idas3UnityGetRenderEventFunc(){return renderEvent;}
void* IDAS3_UNITY_CALL Idas3UnityGetTexture(){try{auto& r=unityRuntime();std::lock_guard lock(r.statusMutex);return r.texture;}catch(...){return nullptr;}}
int IDAS3_UNITY_CALL Idas3UnityGetStatus(Idas3UnityStatus* status){try{if(!status||status->size!=sizeof(*status))return 0;auto& r=unityRuntime();std::lock_guard lock(r.statusMutex);*status=r.status;return 1;}catch(...){return 0;}}
int IDAS3_UNITY_CALL Idas3UnityCopyError(char* destination,int capacity){
    try{auto& r=unityRuntime();std::lock_guard lock(r.statusMutex);const auto length=r.error.size();if(destination&&capacity>0){const auto n=std::min(length,std::size_t(capacity-1));std::memcpy(destination,r.error.data(),n);destination[n]=0;}return int(std::min(length,std::size_t(INT_MAX)));}catch(...){if(destination&&capacity>0)*destination=0;return 0;}
}
int IDAS3_UNITY_CALL Idas3UnityWaitForEvent(int token,int timeout){
    try{auto& r=unityRuntime();std::unique_lock lock(r.queueMutex);if(token<=0||token>=r.nextToken||timeout<0)return -1;
        return r.completed.wait_for(lock,std::chrono::milliseconds(std::min(timeout,10000)),[&]{return r.completedToken>=token;})?1:0;
    }catch(...){return -1;}
}
}
