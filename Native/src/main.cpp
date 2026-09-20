#include "renderer.h"
#include "wet_weather.h"
#include "online_car_selection.h"
#include "unity_ui_capture.h"
#include "imported_course.h"
#include "native_multiplayer.h"
#include "multiplayer_battle_record.h"
#include "ui.h"
#include "audio.h"
#include "race_music_selection.h"
#include "frontend.h"
#include "original_legend_menu.h"
#include "car_catalog.h"
#include "original_car_color_catalog.h"
#include "akina_scene.h"
#include "akina_background.h"
#include "original_driving_session.h"
#include "online_race_link.h"
#include "original_host_input.h"
#include "host_steering_smoothing.h"
#include "original_start_camera.h"
#include "original_start_grid.h"
#include "original_race_start.h"
#include "chase_camera.h"
#include "original_chase_camera.h"
#include "car_shadow.h"
#include "car_presentation.h"
#include "car_pose_interpolation.h"
#include "original_number_plate.h"
#include "original_car_dimensions.h"
#include "original_car_body_position.h"
#include "original_battle_profile.h"
#include "original_course_oil.h"
#include "original_battle_metrics.h"
#include "original_legend_progress.h"
#include "original_bunta_setup.h"
#include "original_bunta_results.h"
#include "original_bunta_visit.h"
#include "original_time_attack_visit.h"
#include "original_battle_result_animation.h"
#include "original_tuning.h"
#include "original_result_tuning_visit.h"
#include "original_legend_visit.h"
#include "original_time_attack_points.h"
#include "original_tuning_presentation.h"
#include "original_tuning_preview.h"
#include "original_tuning_candidate.h"
#include "local_driver_profiles.h"
#include "local_driver_setup.h"
#include "local_save_slots.h"
#include "original_loading_screen.h"
#include "original_vs_banner.h"
#include "original_aura.h"
#include "car_catalog.h"
#include <cctype>
#include "original_name_entry.h"
#include "original_driver_entry_route.h"
#include "course_scene_catalog.h"
#include "original_course_crows.h"
#include "original_course_objects.h"
#include "original_course_animation.h"
#include "original_course_billboards.h"
#include "original_course_light_path.h"
#include "original_headlight_presentation.h"
#include "original_rival_light_request.h"
#include "original_car_lighting.h"
#include "original_car_light_gain.h"
#include "original_race_rules.h"
#include "original_race_feedback.h"
#include "original_race_path.h"
#include "original_record_rules.h"
#include "time_attack_records.h"
#include "original_showroom.h"
#include "original_showroom_details.h"
#include "original_showroom_lighting.h"
#include "original_demo_presentation.h"
#include "original_ranking_presentation.h"
#include "host_platform.h"
#if !defined(IDAS3_PORTABLE_SCENE)
#include <shellapi.h>
#include <windowsx.h>
#endif
#include <chrono>
#include <set>
#include <filesystem>
#include <fstream>
#include <map>
#include "unity_ui_capture.h"
#include <array>
#include <optional>
#include <stdexcept>
#include <sstream>

namespace fs=std::filesystem;
using namespace idas3;
namespace {
const std::array<const char*,9> courseIds={"k_ez","s_nm","h_hd","k_df","s_vh","s_uh","n_sy","k_tu","k_df"};
const std::array<const char*,9> courseNames={"Myogi","Usui","Akagi","Akina","Happogahara","Irohazaka","Shomaru","Tsuchisaka","Akina Snow"};
struct HostInput {
    std::array<bool,256> down{},pressed{};
    XINPUT_STATE pad{};WORD oldButtons=0,pressedButtons=0;bool connected=false;
#if !defined(IDAS3_PORTABLE_SCENE)
    void poll(bool active){
        for(int i=0;i<256;i++){bool next=active&&(GetAsyncKeyState(i)&0x8000)!=0;pressed[i]=next&&!down[i];down[i]=next;}
        XINPUT_STATE next{};connected=active&&XInputGetState(0,&next)==ERROR_SUCCESS;
        pad=connected?next:XINPUT_STATE{};pressedButtons=pad.Gamepad.wButtons&~oldButtons;oldButtons=pad.Gamepad.wButtons;
    }
#endif
    bool key(int code)const{return pressed[code];}
    bool button(WORD b)const{return (pressedButtons&b)!=0;}
};
struct App {
    std::filesystem::path importedCourseRoot,sadamineCourseRoot;
    std::filesystem::path& importedRoot(int id){return id==Frontend::sadamineCourse?sadamineCourseRoot:importedCourseRoot;}
    std::optional<ImportedCourse> importedCourse;
    struct MultiplayerState {
        std::string localName="PLAYER",remoteName="OPPONENT";
        bool active=false,waiting=false,received=false,disconnected=false;
        int finishWinner=-2; // -2 pending; protocol -1 both time-up,0/1 winning slot,2 draw
        Idas3MultiplayerConfig config{};
        std::array<Idas3BattleRecord,2> records{{{0,0,1,0},{0,0,1,0}}};
        std::array<std::uint32_t,2> auraRanges{};
        double auraSeconds=0;
        float hudLocalMetres=0,hudRemoteMetres=0;
        Idas3MultiplayerSnapshot remote{};
        std::array<std::uint32_t,42> remoteActor{};
        std::uint64_t sequence=0;
        original::OriginalBattleProfile savedProfile{},savedBattleProfile{},remoteProfile{};
        original::OriginalGameMode savedMode=original::OriginalGameMode::TimeAttack;
        int savedCar=0,savedMake=0,savedCourse=0,savedFrontendCourse=0,savedProfileCar=-1,savedProfileKind=0;
        bool savedReverse=false,savedWet=false,savedNight=false,savedAutomatic=true;
    } multiplayer;
    WetWeather wetWeather;
    int performanceRainDetail=0;
    NativeTextureBank rainTextures,rainmarkTextures;
    std::uint32_t rainTextureBase=0,rainmarkTextureBase=0;
    fs::path root;
    // Unity supplies an explicit isolated save directory. The standalone
    // executable keeps its original root/userdata default.
    fs::path saveRoot;
    fs::path userdataRoot()const{return saveRoot.empty()?root/"userdata":saveRoot;}
    int selectedRaceMusic=-1;
    bool extraModeVisitActive()const{return buntaVisitActive||timeAttackVisitActive;}
    bool raceMusicOpponentEligible()const{
        const bool selection=frontend.stage==FrontendStage::Rival||
            (frontend.stage==FrontendStage::Course&&frontend.gameMode!=original::OriginalGameMode::LegendOfTheStreets);
        return menu&&!multiplayer.active&&!loadingActive&&!preRaceDialogueActive&&!legendVisitActive&&!extraModeVisitActive()&&!vsActive&&
            selection&&!frontend.confirmationInProgress();
    }
    void selectRaceMusic(int index,int context){
        if(!validRaceMusicSelection(index)||context<0||context>1)
            throw std::invalid_argument("Invalid race music selection");
        if(multiplayer.active||loadingActive||preRaceDialogueActive||legendVisitActive||extraModeVisitActive()||vsActive||
            (context==0&&!raceMusicOpponentEligible())||
            (menu&&frontend.confirmationInProgress()))
            throw std::logic_error("Choose race music before starting the race");
        // Context1 is a managed idle lobby, which may cover an offline race.
        // Queue only: changing the active track here would restart its stream.
        saveRaceMusicSelection(userdataRoot(),index);
        selectedRaceMusic=index;
    }
    Frontend frontend;NativeModel originalModel;NativeAssembly originalAssembly;NativeTextureBank originalTextures;
    CarPresentation carPresentation;CarWheelPose wheelPose,previousWheelPose;
    OriginalNumberPlate numberPlate;
    original::OriginalBattleProfile battleProfile=original::makeOriginalFreshBattleProfile();
    NativeModel rivalModel;NativeTextureBank rivalTextures;CarPresentation rivalPresentation;
    OriginalNumberPlate rivalPlate;int loadedRivalCar=-1,loadedRivalEnemy=-1;bool rivalVisible=false;
    VehicleState rivalVehicle{},previousRival{};CarWheelPose rivalWheels{},previousRivalWheels{};
    float rivalPitch=0,rivalRoll=0,previousRivalPitch=0,previousRivalRoll=0;
    OriginalCarBodyPosition playerBody,rivalBody;
    Vec3 playerBodyWorld{},previousPlayerBodyWorld{},rivalBodyWorld{},previousRivalBodyWorld{};
    original::OriginalDrivingSession originalSession;
#include "online_race_app.inl"
    original::OriginalRaceRules originalRace;
    original::OriginalRaceStart originalRaceStart;
    std::uint32_t originalRaceOwnerFrame=0;
    OriginalRaceFeedback raceFeedback;
    OriginalRearViewFrame rearCameraFrame{},previousRearCameraFrame{};
    original::OriginalRacePath originalPath;
    original::OriginalPathCoordinate originalCoordinate;
    original::OriginalPathCoordinate rivalCoordinate;
    original::OriginalBattleMetrics battleMetrics;
    bool battle=false,bunta=false;
    LocalDriverProfiles profiles{fs::path{}};
    LocalDriverSetup driverSetup{fs::path{}};
    // Five save files. The chosen one scopes the per-car profile and setup
    // stores, so a file's car, parts, balance and progress belong to it alone.
    LocalSaveSlots saveSlots{fs::path{}};
    int activeSaveSlot=-1;
    bool saveFilesShown=false;
    double saveSlotSeconds=0;
    original::OriginalNameEntryTables nameTables;
    bool nameTablesLoaded=false;
    std::array<std::optional<original::OriginalBattleProfile>,35> pendingProfiles;
    std::array<bool,35> pendingSetupCompletion{};
    int loadedProfileCar=-1;bool battleProgressApplied=false;
    original::OriginalLegendResult battleResult=original::OriginalLegendResult::NotLegend;
    original::OriginalLegendPoints battlePoints{};float settledBattleAdvantage=0;
    original::OriginalBuntaPoints buntaPoints{};
    original::OriginalTimeAttackPoints timeAttackPoints{};
    OriginalBattleResultsState battleResults;FixedClock battleResultsClock;
    original::OriginalResultTuningVisit resultVisit;
    std::optional<original::OriginalBattleResultAnimationSetup> pendingResultSetup;
    std::optional<original::OriginalTuningData> tuningTables;
    std::unique_ptr<OriginalTuningPresentation> tuningPresentation;
    std::unique_ptr<original::OriginalTuningPreviewPresentation> tuningPreview;
    std::vector<std::uint32_t> tuningPixels;
    bool tuningTexturesLoaded=false;
    std::uint64_t loadedTuningTextureRevision=0;
    float resultSelectionAxis=.5f;
    original::OriginalBattleResultAnimationFrame resultAnimationFrame;
    bool resultConfirmPending=false;
    bool fullTuneActive=false,fullTuneSelecting=false;
    std::uint32_t fullTuneRandomSeed=1;
    std::vector<std::uint8_t> fullTuneOffers;
    // Source A_VISIT post-result owner: rival dialogue, continue and next-rival
    // choices, then the destination it hands back.
    original::OriginalLegendVisit legendVisit;
    // The screen between a chosen course and the race. The course load runs
    // behind it rather than in front of it, which is the whole point of it.
    OriginalLoadingScreen loadingScreen;
    std::vector<std::uint32_t> loadingPixels;
    double loadingSeconds=0;
    std::uint32_t loadingSequence=0;
    bool loadingActive=false,loadingStarted=false;
    static constexpr double loadingMinimumSeconds=3.0;
    static constexpr double loadingFadeSeconds=.5,loadingBlackSeconds=2.0;
    // The car showcase owns presentation time while the initialized race waits.
    OriginalVsBanner vsBanner;
    std::array<original::OriginalAura,2> multiplayerAura;
    std::vector<std::uint32_t> vsPixels;
    bool vsBannerLoaded=false,vsActive=false;
    double vsSeconds=0;
    std::uint32_t vsPhase=0,vsFrame=0,vsShot=0;
    original::OriginalStartShowcaseCamera startShowcaseCamera;
    void loadStartPresentation(){
        if(OriginalLoadingScreen::available(root))loadingScreen.load(root);
        if(OriginalVsBanner::available(root)){vsBanner.load(root);vsBannerLoaded=true;}
        for(auto& aura:multiplayerAura)aura.load(root);
    }
    FixedClock legendVisitClock;
    std::vector<std::uint32_t> legendVisitPixels;
    bool legendVisitLoaded=false,legendVisitActive=false,legendVisitSmoke=false;
    bool legendConfirmPending=false,legendPreviousPending=false,legendNextPending=false;
    // The rival's challenge before a race chosen from the menu. Continuing to
    // the next opponent already plays this through the Legend owner.
    original::OriginalRivalDialogScene preRaceDialogue;
    FixedClock preRaceDialogueClock;
    std::vector<std::uint32_t> preRaceDialoguePixels;
    bool preRaceDialogueLoaded=false,preRaceDialogueActive=false;
    bool preRaceDialogueSmoke=false;
    original::OriginalBuntaVisit buntaVisit;
    original::OriginalTimeAttackVisit timeAttackVisit;
    original::OriginalTimeAttackDrivingTrace timeAttackTrace;
    original::OriginalTimeAttackTelemetry timeAttackTelemetry;
    original::OriginalTimeAttackTelemetrySnapshot timeAttackSnapshot;
    original::OriginalTimeAttackAnalysisInput timeAttackAnalysisInput;
    original::OriginalTimeAttackAnalysis timeAttackAnalysis;
    bool timeAttackAnalysisPrepared=false;
    std::unique_ptr<original::OriginalTuningPreviewPresentation> timeAttackRankingPreview;
    bool timeAttackRankingTexturesLoaded=false;
    original::OriginalTimeAttackVisit::Stage timeAttackBackdropStage=original::OriginalTimeAttackVisit::Stage::Inactive;
    FixedClock modeVisitClock,timeSummaryClock;
    std::vector<std::uint32_t> modeVisitPixels;
    bool buntaVisitActive=false,timeAttackVisitActive=false,timeAttackLectureDone=false,timeSummaryDone=false;
    bool timeAttackCourseRankingQualified=false;
    bool timeAttackPersonalRegistered=false;
    unsigned timeSummaryTicks=0;
    int modeVisitSoundSet=1;
    // A completed solo Time Attack switches directly from FINISH to its
    // record panel after two seconds. Battle/time-up outcomes retain their
    // separate announcement and outgoing transition.
    unsigned finishBannerTicks=0,finishFadeTicks=0;bool finishBannerDone=false,finishAudioSkipped=false;
    static constexpr unsigned finishBannerSwap=120,finishBannerOutcome=180;
    // The start button the dialogue advertises: held, it skips the scripted pages.
    bool legendSkipHeld=false;
    // Set once a run has beaten the last rival and the owner reported Ending.
    bool legendRunCompleted=false;
    TimeAttackRecords records,importedPersonalRecords,sharedRecords;
    std::array<std::optional<original::OriginalBattleProfile>,35> personalRecordProfiles;
    mutable TimeAttackRecords personalDisplayRecords;
    mutable bool personalRecordsDirty=true;
    mutable TimeAttackRecords combinedDisplayRecords;
    mutable bool combinedRecordsDirty=true;
    bool sharedRecordsEnabled=false;
    // Playback publishes recorded poses without stepping physics, rewards or saves.
    bool replayPlaybackActive=false;
    bool diagnosticCaptureOff=false;
    bool replayDetailed=false,replayLights=false;
    ReplayDetail replayFrameData{};
    int replayInitialRemaining=0;
    int replayCameraMode=0;
    float replayOrbit=0;
    double replayLastTick=-1;
    std::string sharedFinishJson;
    std::vector<std::uint8_t> sharedFinishReplay;
    unsigned replayRecordingFlags=9; // Time Attack + sharing; optional battle recording is opt-in.
    bool archiveThisRace=false,archivePublished=false,replayOpponent=false,replayRivalLights=false;
    int archiveMode=0,archiveRivalCar=-1,archiveRivalEnemy=-1;
    bool archiveOpponentAutomatic=false;
    float replayAdvantage=0;
    Replay rivalRecording;
    std::string localReplayJson;
    std::vector<std::uint8_t> localReplayPlayer,localReplayRival;
    void setSharedRecords(TimeAttackRecords next,bool enabled){
        sharedRecords=std::move(next);sharedRecordsEnabled=enabled;combinedRecordsDirty=true;
    }
    const TimeAttackRecords& displayedTimeAttackRecords()const{
        if(personalRecordsDirty){
            personalDisplayRecords={};
            // Only this save's cards are authoritative offline. The old cabinet
            // CSV has no save ownership and must never seed these displays.
            for(unsigned car=0;car<personalRecordProfiles.size();++car)if(personalRecordProfiles[car]){
                const auto& p=*personalRecordProfiles[car];
                for(unsigned condition=0;condition<18;++condition)for(unsigned weather=0;weather<2;++weather){
                    if(condition>=16&&!weather)continue;
                    const auto saved=original::originalPersonalTimeAttackRecord(p,original::originalRecordPartition(condition,weather!=0));
                    if(!saved.ticks6000||saved.ticks6000>=10800000||saved.night>1)continue;
                    TimeAttackEntry e{condition,weather,car,saved.ticks6000};e.night=saved.night!=0;e.metadataUnknown=true;
                    for(unsigned n=0;n<5;++n){const auto glyph=p.u(44+n*4);e.nameGlyphs[n]=glyph<=221?std::uint8_t(glyph):221;}
                    personalDisplayRecords.record(e);
                }
            }
            for(const auto& e:importedPersonalRecords.entries())if(e.condition>=18)personalDisplayRecords.record(e);
            personalRecordsDirty=false;
            combinedRecordsDirty=true;
        }
        if(sharedRecordsEnabled){
            if(combinedRecordsDirty){
                combinedDisplayRecords=sharedRecords;
                // A published personal best is one ranking row. Prefer the
                // remote copy's complete metadata when both sources contain it.
                for(const auto& own:personalDisplayRecords.entries()){
                    const bool published=std::any_of(sharedRecords.entries().begin(),sharedRecords.entries().end(),[&](const auto& remote){
                        return own.condition==remote.condition&&own.weather==remote.weather&&own.car==remote.car&&
                            own.ticks6000==remote.ticks6000&&own.nameGlyphs==remote.nameGlyphs;
                    });
                    if(!published)combinedDisplayRecords.record(own);
                }
                combinedRecordsDirty=false;
            }
            return combinedDisplayRecords;
        }
        return personalDisplayRecords;
    }
    TimeAttackEntry importedPreviousBest,importedCoursePreviousBest;
    fs::path importedPersonalPath;OriginalResultsState results;bool resultsReady=false;
    original::OriginalHostInputState originalInput;
    bool originalHandling=false;
    int loadedCar=-1,loadedColor=-1;bool texturesPending=false,menuTexturesLoaded=false;
    std::uint32_t loadedAppearanceWord=0,loadedProfileCondition=0;
    std::array<std::uint32_t,6> loadedDriverName{};
    NativeModel originalCourseModel;NativeTextureBank originalCourseTextures;std::array<NativeAssembly,30> originalCourseSectors;
    OriginalCourseScene courseScene;
    original::OriginalCourseFog raceFog=original::originalBootstrapFog();
    std::optional<original::OriginalCourseLighting> raceLighting;
    original::OriginalCarLighting playerCarLight,rivalCarLight;
    original::OriginalLightVector raceCarAmbient{};
    std::optional<original::OriginalRaceLightingSets> raceLightSets;
    original::OriginalCarLightGain carLightGain;
    original::OriginalPathCoordinate playerLightCoordinate,rivalLightCoordinate;
    std::optional<original::OriginalFscaTable> courseLightFsca;
    int courseLightPathIndex=0;
    OriginalHeadlightPresentation playerProjectedHeadlight,rivalProjectedHeadlight;
    NativeTextureBank projectedHeadlightTextures;
    std::uint32_t projectedHeadlightTextureBase=0;
    original::OriginalRivalLightState rivalLightState;
    float projectedLightPriorAdvantage=0;
    CourseMeshCache courseMeshCache;
    std::optional<OriginalCourseCrows> courseCrows;
    std::optional<OriginalCourseObjects> courseObjects;
    std::optional<OriginalCourseBillboards> courseBillboards;
    OriginalCourseAnimation courseAnimation;
    std::uint32_t crowTextureBase=0;
    Mesh raceMesh;
    NativeModel originalBackgroundModel;NativeTextureBank originalBackgroundTextures;
    bool courseModelLoaded=false,hasOriginalScenery=false,loadedNight=false,loadedReverse=false,loadedWet=false,catalogScenery=false,sceneWet=false;
    int loadedCourse=-1;
    Course course;VehicleConfig config;VehicleState vehicle{},previous{};
    RaceClock race;Replay recording,best;FixedClock clock;Renderer renderer;Hud hud;EngineAudio audio;HostInput input;
    int courseIndex=3,profile=0;bool reverse=false,wet=false,night=false,automatic=true;
    bool menu=true,paused=false,active=true,debug=false,showControls=false,running=true,finishedSaved=false,validationMode=false;
    bool challengerNotice=false;
    bool managedPauseOverlay=false;
    original::ControllerResponse controllerResponse=original::ControllerResponse::FlycastGamepad;
    float steeringDeadzone=-1.f;
    HostSteeringSmoothing steeringSmoothing;
    bool steeringControllerConnected=false,hostDrivingControlsBlocked=false;
    bool resizePending=false,mouseStart=false;int pendingWidth=1280,pendingHeight=720;
    int menuDirection=0;double menuRepeatWait=0;
    float steering=0,progress=0,trackStart=2.f,trackFinish=0,renderFps=60.f;
    std::size_t segment=0;double bestTime=0;std::string message;float messageSeconds=0;
    Vec3 camera{};bool cameraReady=false;
    ChaseCamera chaseCamera;
    OriginalChaseCamera originalCamera;
    OriginalChaseFrame previousCameraFrame;
    OriginalChaseCamera bumperCamera;
    OriginalChaseFrame previousBumperFrame;
    OriginalDrivingView drivingView=OriginalDrivingView::Bumper;
    OriginalShowroom showroom;FixedClock menuClock;FrontendStage showroomStage=FrontendStage::Title;
    std::array<std::vector<std::uint32_t>,6> nameLayers;
    std::uint32_t namePaintFrame=~0u;int namePaintWidth=0,namePaintHeight=0;
    std::vector<std::uint32_t> tuningCourseBackground;
    std::vector<OriginalTuningCourseOverlay> tuningCourseLayers;
    std::uint32_t tuningCoursePaintFrame=~0u;int tuningCoursePaintWidth=0,tuningCoursePaintHeight=0;
    NativeModel tuningCoursePreviewModel;
    NativeTextureBank tuningCoursePreviewEnvironment;
    bool tuningCoursePreviewEnvironmentLoaded=false;
    CarPresentation tuningCoursePreviewCar;
    std::optional<original::OriginalCarAppearanceConfig> tuningCoursePreviewAppearance;
    int tuningCoursePreviewCarId=-1,tuningCoursePreviewPackage=-1;
    std::uint32_t tuningCoursePreviewCondition=0,tuningCoursePreviewCarState=0;
    std::array<std::uint32_t,6> tuningCoursePreviewName{};
    std::uint64_t tuningCoursePreviewRevision=0;
    bool tuningCoursePreviewActive=false;
    NativeModel driverEntryPreviewModel;
    CarPresentation driverEntryPreviewCar;
    std::optional<original::OriginalCarAppearanceConfig> driverEntryPreviewAppearance;
    std::array<std::uint32_t,6> driverEntryPreviewName{};
    std::uint32_t driverEntryPreviewCondition=0;
    std::uint64_t driverEntryPreviewRevision=0;
    bool driverEntryPreviewActive=false;
    NativeModel showroomShadow;original::OriginalFscaTable showroomFsca;bool showroomDetailsLoaded=false;
    OriginalShowroomLighting showroomLighting;
    std::future<std::unique_ptr<OriginalDemoPresentation>> demoPreparation;
    std::unique_ptr<OriginalDemoPresentation> demoPresentation;
    original::OriginalRankingPresentation rankingPresentation;
    bool demoPreparationStarted=false,rankingPresentationLoaded=false;
    unsigned lastAttractRendered=~0u;
    std::uint64_t rankingTexturesRevision=~0ull;
    std::vector<std::uint32_t> rankingOverlay;
    float bodyPitch=0,bodyRoll=0,previousPitch=0,previousRoll=0;
    std::vector<std::pair<Vec3,Vec3>> skids;
    HWND window=nullptr;
    bool supportsOriginalHandling()const{return courseIndex>=0&&courseIndex<9;}
    std::string sceneId()const{return courseIndex==8?"k_df3":course.id;}
    std::string runKey()const {return std::string(supportsOriginalHandling()?"original_race_v3_":"development_v01_")+sceneId()+(reverse?"_rev":"_fwd")+(wet?"_wet":"_dry")+"_car"+std::to_string(frontend.car)+(automatic?"_at":"_mt");}
    CourseProjection projectRacePosition(Vec3 position)const{
        // Renderer/diagnostic distance only. Race timing uses original096200.
        if(originalHandling&&course.closed&&(segment<100||segment+100>=course.points.size()-1))return course.project(position);
        return course.project(position,segment);
    }
    original::OriginalPathCoordinate pathCoordinate(const CourseProjection& projection)const{
        const auto i=projection.segment;const float fraction=(projection.sample.distance-course.cumulative[i])/(course.cumulative[i+1]-course.cumulative[i]);
        return {std::int32_t(i),std::clamp(fraction,0.f,1.f)};
    }
    CourseSample sampleRaceDistance(float distance)const{
        if(course.closed){distance=std::fmod(distance,course.length);if(distance<0)distance+=course.length;}
        return course.sample(distance);
    }
    fs::path ghostPath()const{return userdataRoot()/(runKey()+".csv");}
    void status(std::string s){message=std::move(s);messageSeconds=5;}
    void configureCar(){
        config=VehicleConfig{};
        if(profile==1){config.name="Research FF";config.drive=DriveLayout::Front;config.mass=1020;config.maxTorque=185;config.frontWeight=.61f;config.wheelbase=2.57f;config.yawInertia=1520;}
        if(profile==2){config.name="Research 4WD";config.drive=DriveLayout::All;config.mass=1380;config.maxTorque=330;config.frontWeight=.59f;config.wheelbase=2.51f;config.yawInertia=2010;config.redlineRpm=7600;config.gearCount=6;}
    }
    void loadSelectedCar(){
        if(!replayPlaybackActive)loadSelectedProfile();
        const auto color=frontend.selectedColor();
        auto appearanceProfile=frontend.battleProfile;appearanceProfile.setu(16,unsigned(frontend.car));appearanceProfile.setu(64,color);
        const auto appearance=original::originalPlayerAppearanceConfig(appearanceProfile);
        std::array<std::uint32_t,6> name{appearanceProfile.u(76)};for(unsigned i=0;i<5;++i)name[i+1]=appearanceProfile.u(44+4*i);
        if(loadedCar==frontend.car&&loadedColor==int(color)&&loadedAppearanceWord==appearance.word&&loadedProfileCondition==appearanceProfile.u(32)&&loadedDriverName==name)return;
        const bool differentCar=loadedCar!=frontend.car;
        auto folder=std::string(originalCarFolders.at(frontend.car));auto modelDir=root/"data/original_models"/folder;
        originalModel=NativeModel::load(modelDir/(folder+".idasmesh"));
        originalAssembly=NativeAssembly::load(modelDir/"assembly"/(folder+"_default.idasasm"),originalModel.chunks.size());
        carPresentation=CarPresentation::loadPlayerProfile(root,appearanceProfile);
        carPresentation.applyMaterials(originalModel);
        numberPlate=OriginalNumberPlate::load(root,unsigned(frontend.car),color);
        numberPlate.setPlayerProfile(appearanceProfile);
        if(differentCar){originalTextures=NativeTextureBank::load(root/"data/original_assets/cars"/folder/"textures/textures.idastex");texturesPending=true;menuTexturesLoaded=false;}
        loadedCar=frontend.car;loadedColor=int(color);loadedAppearanceWord=appearance.word;loadedProfileCondition=appearanceProfile.u(32);loadedDriverName=name;
    }
    // Points the per-car stores at one save file. -1 is the no-file state used
    // before a file is chosen, which keeps the legacy shared directory so an
    // existing install still reads its old profiles.
    // The panel box on the file screen, in source-canvas pixels. frontend.cpp
    // draws its frame at the same place.
    static constexpr int savePanelBox[4]={505,173,107,86};
    int browsedSaveSlot=-2;
    bool legacyDriversChecked=false;
    bool browsingSaveFiles=false;
    void useSaveSlot(int slot){
        activeSaveSlot=slot;saveSlotSeconds=0;
        const auto directory=slot<0?userdataRoot()/"driver_profiles_v1"
                                   :saveSlots.profileDirectory(unsigned(slot));
        importedPersonalPath=directory/"hakone_personal_v1.csv";
        importedPersonalRecords=TimeAttackRecords{};importedPersonalRecords.load(importedPersonalPath);
        profiles=LocalDriverProfiles(directory);
        driverSetup=LocalDriverSetup(directory);
        pendingProfiles={};loadedProfileCar=-1;
        personalRecordProfiles={};personalRecordsDirty=true;
        for(unsigned car=0;car<personalRecordProfiles.size();++car){
            const auto saved=profiles.load(car);
            if(saved.origin==LocalDriverProfiles::Origin::Saved||saved.origin==LocalDriverProfiles::Origin::Backup)personalRecordProfiles[car]=saved.profile;
        }
    }
    static int originalCarMake(unsigned car){
        for(int make=0;make<7;++make){
            const auto roster=Frontend::carsForMake(make);
            if(std::find(roster.begin(),roster.end(),int(car))!=roster.end())return make;
        }
        return 0;
    }
    static std::string originalCarName(unsigned car){
        return std::string(originalCarNames.at(car));
    }
    static std::string originalCarGrade(unsigned car){
        return std::string(originalCarGrades.at(car));
    }
    // An install made before save files existed keeps its drivers in one shared
    // directory. Give each finished driver there its own file, by COPYING: the
    // shared directory is left exactly as it was, so nothing a driver saved is
    // moved, rewritten or removed by this.
    void adoptLegacyDrivers(){
        if(legacyDriversChecked)return;
        legacyDriversChecked=true;
        const auto legacy=userdataRoot()/"driver_profiles_v1";
        std::error_code ec;
        if(saveSlots.used()||!fs::exists(legacy,ec))return;
        const LocalDriverProfiles old(legacy);
        const LocalDriverSetup oldSetup(legacy);
        unsigned slot=0,adopted=0;
        for(unsigned car=0;car<35&&slot<LocalSaveSlots::count;++car){
            if(oldSetup.load(car).status!=LocalDriverSetup::Status::Complete)continue;
            const auto loaded=old.load(car);
            if(loaded.origin!=LocalDriverProfiles::Origin::Saved&&
               loaded.origin!=LocalDriverProfiles::Origin::Backup)continue;
            const auto destination=saveSlots.profileDirectory(slot);
            fs::create_directories(destination,ec);
            if(ec)return;
            const LocalDriverProfiles copied(destination);
            const LocalDriverSetup copiedSetup(destination);
            bool ok=true;
            for(const auto& [from,to]:{std::pair{old.path(car),copied.path(car)},
                                      {oldSetup.path(car),copiedSetup.path(car)}}){
                fs::copy_file(from,to,fs::copy_options::overwrite_existing,ec);
                if(ec){ok=false;break;}
            }
            if(!ok)continue;
            auto profile=loaded.profile;profile.setu(16,car);
            if(!saveSlots.adopt(slot,profile))continue;
            ++slot;++adopted;
        }
        if(adopted)status(std::to_string(adopted)+(adopted==1?" existing driver was":" existing drivers were")+
                          " copied into save files; the original save is untouched.");
    }
    void refreshSaveFiles(){
        if(!nameTablesLoaded){nameTables=original::OriginalNameEntryTables::load(root);nameTablesLoaded=true;}
        adoptLegacyDrivers();
        saveSlots.reload();
        for(unsigned i=0;i<LocalSaveSlots::count;++i){
            const auto& slot=saveSlots.at(i);
            auto& summary=frontend.saveFiles[i];
            summary={};
            summary.used=slot.used;
            if(!slot.used)continue;
            summary.playedSeconds=slot.playedSeconds;summary.wins=slot.wins;summary.lastPlayed=slot.lastPlayed;
            summary.name=nameTables.encodedName(slot.nameGlyphs,slot.nameLength);
            if(summary.name.empty())summary.name="NO NAME";
            summary.car=originalCarName(slot.car);
            summary.grade=originalCarGrade(slot.car);
        }
    }
    // A chosen file either starts a driver's setup or resumes one already made.
    void openSaveFile(int slot){
        browsingSaveFiles=false;useSaveSlot(slot);
        const auto& file=saveSlots.at(unsigned(slot));
        if(!file.used){
            frontend.stage=FrontendStage::Make;
            return;
        }
        frontend.car=int(file.car);
        frontend.make=originalCarMake(file.car);
        loadSelectedProfile();
        frontend.stage=fullTuneSelecting?FrontendStage::Make:FrontendStage::Mode;
    }
    void loadSelectedProfile(){
        if(multiplayer.active||validationMode||loadedProfileCar==frontend.car)return;
        const auto car=unsigned(frontend.car);
        bool hasStoredProfile=false;
        if(pendingProfiles.at(car)){frontend.battleProfile=*pendingProfiles[car];hasStoredProfile=true;}
        else {
            const auto saved=profiles.load(car);frontend.battleProfile=saved.profile;
            hasStoredProfile=saved.origin==LocalDriverProfiles::Origin::Saved||saved.origin==LocalDriverProfiles::Origin::Backup;
            if(saved.origin==LocalDriverProfiles::Origin::Backup)status("Driver profile recovered from its previous save.");
            else if(saved.origin==LocalDriverProfiles::Origin::Unreadable)status("Driver save unreadable; preserved for recovery. Using a fresh profile.");
        }
        loadedProfileCar=frontend.car;
        const auto loadedWords=frontend.battleProfile.words;
        // A native per-car driver profile replaces an accepted physical card.
        // Preserve the source flag, earned balance and profile contents.
        if(!(frontend.battleProfile.u(1180)&1u)){
            original::applyOriginalAcceptedCardFlag(frontend.battleProfile);
        }
        // Setup readiness is explicit native metadata. Old autosaved profiles
        // still visit setup once; a nonempty name chooses the source import
        // editor only, and never substitutes for a completion marker.
        const auto setup=driverSetup.load(car);
        if(hasStoredProfile&&setup.status==LocalDriverSetup::Status::Complete&&!original::originalDriverSetupRequested(frontend.battleProfile)){
            original::finishOriginalDriverSetupFlag(frontend.battleProfile);
            frontend.battleProfile.setByte(1192,0);
        }else{
            original::requestOriginalDriverSetup(frontend.battleProfile);
            const auto length=frontend.battleProfile.u(76);bool validName=length>0&&length<=5;
            if(validName)for(unsigned i=0;i<length;++i)validName=validName&&frontend.battleProfile.u(44+4*i)<=220;
            frontend.battleProfile.setByte(1192,validName?2:0);
        }
        // Persist an unfinished replacement before a stale completion marker
        // can be observed on another launch. A marker alone cannot make a
        // missing/corrupt driver's fresh fallback into an established driver.
        if(frontend.battleProfile.words!=loadedWords){pendingProfiles[car]=frontend.battleProfile;flushProfiles();}
        frontend.driverProfileLoaded();
    }
    void loadDriverEntryEnvironment(){
        if(!tuningCoursePreviewEnvironmentLoaded){
            tuningCoursePreviewEnvironment=NativeTextureBank::load(root/"data/original_assets/tuning/environment/textures.idastex");
            tuningCoursePreviewEnvironmentLoaded=true;menuTexturesLoaded=false;
        }
    }
    void loadDriverEntryPreview(){
        if(!driverEntryPreviewActive)menuTexturesLoaded=false;
        loadDriverEntryEnvironment();
        auto profile=frontend.battleProfile;profile.setu(16,unsigned(frontend.car));profile.setu(64,frontend.selectedColor());
        const auto appearance=original::originalPlayerAppearanceConfig(profile,frontend.stage==FrontendStage::Car?1u:0u);
        std::array<std::uint32_t,6> name{profile.u(76)};for(unsigned i=0;i<5;++i)name[i+1]=profile.u(44+4*i);
        if(driverEntryPreviewActive&&driverEntryPreviewAppearance&&driverEntryPreviewAppearance->car==appearance.car&&
            driverEntryPreviewAppearance->word==appearance.word&&driverEntryPreviewAppearance->materialVariant==appearance.materialVariant&&
            driverEntryPreviewCondition==profile.u(32)&&driverEntryPreviewName==name)return;
        const auto folder=std::string(originalCarFolders.at(frontend.car));
        auto model=NativeModel::load(root/"data/original_models"/folder/(folder+".idasmesh"));
        //11D720/12E4A0 use selector1 + material variant1 in Car selection.
        //11E260 uses selector3 + variant0 in Transmission/Name. Both selectors
        // alias j_env_select128_b; both retain layer2 on the borrowed car.
        auto presentation=CarPresentation::loadConfiguredAppearance(root,profile,appearance,2,
            unsigned(originalTextures.size()+numberPlate.textures.size()));
        presentation.applyMaterials(model);
        driverEntryPreviewModel=std::move(model);driverEntryPreviewCar=std::move(presentation);
        driverEntryPreviewAppearance=appearance;driverEntryPreviewName=name;driverEntryPreviewCondition=profile.u(32);
        driverEntryPreviewActive=true;++driverEntryPreviewRevision;
    }
    void loadTuningCoursePreview(){
        const auto& menuState=frontend.tuningCourseState();
        if(!tuningCoursePreviewActive)menuTexturesLoaded=false;
        if(!tuningTables)tuningTables=original::OriginalTuningData::load(root);
        loadDriverEntryEnvironment();
        auto profile=frontend.battleProfile;profile.setu(16,unsigned(frontend.car));profile.setu(64,frontend.selectedColor());
        auto appearance=original::originalPlayerAppearanceConfig(profile);
        const auto candidate=original::originalTuningCandidateAppearance(*tuningTables,unsigned(frontend.car),menuState.selected496);
        original::applyOriginalTuningCandidateAppearance(appearance,candidate);
        std::array<std::uint32_t,6> name{profile.u(76)};for(unsigned i=0;i<5;++i)name[i+1]=profile.u(44+4*i);
        if(tuningCoursePreviewActive&&tuningCoursePreviewCarId==frontend.car&&tuningCoursePreviewPackage==int(menuState.selected496)&&
            tuningCoursePreviewAppearance&&tuningCoursePreviewAppearance->word==appearance.word&&
            tuningCoursePreviewAppearance->materialVariant==appearance.materialVariant&&tuningCoursePreviewCondition==profile.u(32)&&tuningCoursePreviewName==name)return;
        const auto folder=std::string(originalCarFolders.at(frontend.car));
        auto model=NativeModel::load(root/"data/original_models"/folder/(folder+".idasmesh"));
        //11E4FC..11E532 gives this borrowed showroom car selector3's menu
        // environment and layer mask2;12B520 preserves those resource bindings.
        auto presentation=CarPresentation::loadConfiguredAppearance(root,profile,appearance,candidate.carState224,
            unsigned(originalTextures.size()+numberPlate.textures.size()));
        presentation.applyMaterials(model);
        tuningCoursePreviewModel=std::move(model);tuningCoursePreviewCar=std::move(presentation);
        tuningCoursePreviewAppearance=appearance;tuningCoursePreviewCarId=frontend.car;tuningCoursePreviewPackage=int(menuState.selected496);
        tuningCoursePreviewCondition=profile.u(32);tuningCoursePreviewName=name;tuningCoursePreviewCarState=candidate.carState224;
        tuningCoursePreviewActive=true;++tuningCoursePreviewRevision;
    }
    void flushProfiles(){
        if(multiplayer.active||validationMode)return;
        for(unsigned car=0;car<pendingProfiles.size();++car){
            if(pendingProfiles[car]){
                personalRecordProfiles[car]=*pendingProfiles[car];personalRecordsDirty=true;
                if(profiles.save(car,*pendingProfiles[car]))pendingProfiles[car].reset();
                else status("Driver progress could not be saved. It is retained for another save attempt.");
            }
            if(pendingSetupCompletion[car]&&!pendingProfiles[car]){
                if(driverSetup.markComplete(car))pendingSetupCompletion[car]=false;
                else status("Driver setup could not be saved. It is retained for another save attempt.");
            }
        }
    }
    // Read an exact slot/car profile without changing offline selection or saves.
    // Primary-slot aliases0..4 and legacy5..39 remain supported;40..214
    // enumerate every car stored inside each of the five save directories.
    int onlineCarProfile(int selection,original::OriginalBattleProfile& result){
        const auto selected=decodeOnlineCarSelection(selection);
        unsigned car;fs::path directory;bool current=false;
        if(selected.slot>=0){
            const LocalSaveSlots slots(userdataRoot()/"saves");
            if(!slots.at(unsigned(selected.slot)).used)return 0;
            car=selected.car<0?slots.at(unsigned(selected.slot)).car:unsigned(selected.car);
            directory=slots.profileDirectory(unsigned(selected.slot));
            current=activeSaveSlot==selected.slot;
        }else{car=unsigned(selected.car);directory=userdataRoot()/"driver_profiles_v1";current=activeSaveSlot<0;}
        if(current&&loadedProfileCar==int(car)){
            result=multiplayer.active?multiplayer.savedProfile:frontend.battleProfile;return 1;
        }
        if(current&&pendingProfiles.at(car)){result=*pendingProfiles[car];return 1;}
        const auto saved=LocalDriverProfiles(directory).load(car);result=saved.profile;
        if(saved.origin==LocalDriverProfiles::Origin::Saved||saved.origin==LocalDriverProfiles::Origin::Backup)return 1;
        if(selected.slot>=0||saved.origin==LocalDriverProfiles::Origin::Unreadable)return 0;
        return 2;
    }
    void startMultiplayer(const Idas3MultiplayerConfig& request,
                          const original::OriginalBattleProfile* local=nullptr,
                          const original::OriginalBattleProfile* remote=nullptr){
        validateMultiplayerConfig(request);
        if(Frontend::isImportedCourse(request.course)&&importedRoot(request.course).empty())throw std::logic_error("Hakone course pack is not installed");
        if(importedCourse)returnToCourseSelection(true);
        if(multiplayer.active)leaveMultiplayer();
        multiplayer={};auto& mp=multiplayer;
        mp.savedProfile=frontend.battleProfile;mp.savedBattleProfile=battleProfile;mp.savedMode=frontend.gameMode;
        mp.savedCar=frontend.car;mp.savedMake=frontend.make;mp.savedCourse=courseIndex;mp.savedProfileCar=loadedProfileCar;mp.savedProfileKind=profile;
        mp.savedFrontendCourse=frontend.course;
        mp.savedReverse=reverse;mp.savedWet=wet;mp.savedNight=night;mp.savedAutomatic=automatic;
        mp.config=request;mp.active=true;mp.waiting=true;
        for(auto& aura:multiplayerAura)aura.configure(1,0);
        // Race-owned copies retain saved tuning/paint/parts without writing
        // online race results into either driver's offline profile.
        frontend.battleProfile=local?*local:original::makeOriginalFreshBattleProfile();
        mp.remoteProfile=remote?*remote:original::makeOriginalFreshBattleProfile();
        mp.remoteProfile.setu(16,request.remoteCar);
        frontend.car=int(request.localCar);frontend.make=originalCarMake(request.localCar);
        frontend.stage=FrontendStage::Course;frontend.gameMode=original::OriginalGameMode::TimeAttack;
        frontend.course=courseIndex=int(request.course);frontend.reverse=reverse=request.reverse!=0;
        frontend.wet=wet=request.wet!=0;frontend.night=night=request.night!=0;frontend.automatic=automatic=request.automatic!=0;
        loadingActive=preRaceDialogueActive=legendVisitActive=vsActive=buntaVisitActive=timeAttackVisitActive=false;
        input={};steering=0;clock.reset();
        try{
            start(true);
            if(!originalHandling||presentedSession().rivalActive())throw std::logic_error("Multiplayer requires source local physics without AI");
            clock.reset();status("Waiting for the other driver");
        }catch(...){leaveMultiplayer();throw;}
    }
    void initializeMultiplayerRemote(unsigned condition){
        const unsigned car=multiplayer.config.remoteCar;
        auto profile=multiplayer.remoteProfile;profile.setu(16,car);profile.setu(32,wet?1u:0u);
        const auto folder=std::string(originalCarFolders.at(car));
        rivalModel=NativeModel::load(root/"data/original_models"/folder/(folder+".idasmesh"));
        rivalTextures=NativeTextureBank::load(root/"data/original_assets/cars"/folder/"textures/textures.idastex");
        rivalPresentation=CarPresentation::loadPlayerProfile(root,profile);rivalPresentation.applyMaterials(rivalModel);
        rivalPlate=OriginalNumberPlate::load(root,car);rivalPlate.setPlayerProfile(profile);
        loadedRivalCar=int(car);loadedRivalEnemy=-2;texturesPending=true;
        const auto spawn=importedCourse?importedCourse->onlineSpawn(reverse,1u-multiplayer.config.localSlot):original::originalStartPose(condition,1u-multiplayer.config.localSlot);
        auto& s=multiplayer.remote;s={sizeof(s),1};s.flags=Idas3MpActive|Idas3MpWaiting|(night?Idas3MpHeadlights:0);s.car=car;
        for(unsigned i=0;i<3;++i)s.actorPosition[i]=spawn.position[i];
        const auto body=rivalBody.update(presentedSession().collision(),car,{s.actorPosition[0],s.actorPosition[1],s.actorPosition[2]});
        s.bodyPosition[0]=body.x;s.bodyPosition[1]=body.y;s.bodyPosition[2]=body.z;
        s.yaw=wrapAngle(spawn.angles[1]+pi);s.pitch=-spawn.angles[0];s.roll=-spawn.angles[2];s.headlightCounter=-1;
    }
    void projectMultiplayerRemote(){
        const auto& s=multiplayer.remote;rivalVisible=true;
        rivalVehicle.position={s.actorPosition[0],s.actorPosition[1],s.actorPosition[2]};rivalVehicle.yaw=s.yaw;
        rivalVehicle.speed=s.speed;rivalVehicle.rpm=s.rpm;rivalVehicle.brake=(s.flags&Idas3MpBrake)?1.f:0.f;
        rivalPitch=s.pitch;rivalRoll=s.roll;rivalWheels.steeringRadians=s.steering;
        for(unsigned i=0;i<4;++i){rivalWheels.suspensionY[i]=s.suspension[i];rivalWheels.rotationRadians[i]=s.wheelRotation[i];}
        // The transmitted body is authoritative. This private read-only query
        // only supplies the remote projected lamp's road surface; no RNG/AI.
        rivalBody.update(presentedSession().collision(),s.car,rivalVehicle.position);
        rivalBodyWorld={s.bodyPosition[0],s.bodyPosition[1],s.bodyPosition[2]};
        auto& actor=multiplayer.remoteActor;actor={};
        const auto set=[&](unsigned offset,float value){actor[offset/4]=std::bit_cast<std::uint32_t>(value);};
        for(unsigned i=0;i<3;++i)set(i*4,s.actorPosition[i]);
        set(24,-s.pitch);set(28,wrapAngle(s.yaw-pi));set(32,-s.roll);set(60,s.steering);
        for(unsigned i=0;i<4;++i){set(64+i*4,s.suspension[i]);set(96+i*4,s.wheelRotation[i]);}
        actor[92/4]=(s.flags&Idas3MpBrake)?1u:0u;
        auto light=rivalPresentation.headlightState();
        light.counter=s.headlightCounter;light.phase=std::min(s.headlightPhase,light.maximumPhase);
        light.visible=s.headlightVisible!=0;light.fraction=std::max(0,light.counter)/40.f;rivalPresentation.restoreHeadlightState(light);
        // C# supplies the interpolated network presentation. Do not introduce
        // another local-physics interpolation delay or advance remote wheels.
        previousRival=rivalVehicle;previousRivalBodyWorld=rivalBodyWorld;previousRivalWheels=rivalWheels;
        previousRivalPitch=rivalPitch;previousRivalRoll=rivalRoll;
    }
    const std::array<std::uint32_t,42>& renderedRivalActor()const{
        return multiplayer.active?multiplayer.remoteActor:presentedSession().publishedActors().secondary0C8FF430;
    }
    void applyMultiplayerSnapshot(const Idas3MultiplayerSnapshot& incoming){
        if(multiplayer.disconnected)throw std::logic_error("The disconnected race is finished");
        if(!multiplayer.active)throw std::logic_error("No multiplayer race");
        auto next=sanitizeMultiplayerSnapshot(incoming,multiplayer.config.remoteCar);
        if(multiplayer.received&&next.sequence<multiplayer.remote.sequence)return;
        multiplayer.remote=next;multiplayer.received=true;projectMultiplayerRemote();
    }
    Idas3MultiplayerSnapshot multiplayerSnapshot(){
        if(!multiplayer.active||!presentedSession().ready())throw std::logic_error("No multiplayer race");
        Idas3MultiplayerSnapshot s{sizeof(s),1};s.sequence=++multiplayer.sequence;s.raceTicks=race.ticks;s.car=unsigned(frontend.car);
        s.flags=Idas3MpActive|(multiplayer.waiting?Idas3MpWaiting:0)|(paused?Idas3MpPaused:0)|
            (race.phase==RacePhase::Finished?Idas3MpFinished:0)|(race.timeUp?Idas3MpTimeUp:0)|
            (playerProjectedHeadlight.enabled()?Idas3MpHeadlights:0)|(vehicle.brake>.05f?Idas3MpBrake:0);
        s.bodyPosition[0]=playerBodyWorld.x;s.bodyPosition[1]=playerBodyWorld.y;s.bodyPosition[2]=playerBodyWorld.z;
        s.actorPosition[0]=vehicle.position.x;s.actorPosition[1]=vehicle.position.y;s.actorPosition[2]=vehicle.position.z;
        s.yaw=vehicle.yaw;s.pitch=bodyPitch;s.roll=bodyRoll;s.steering=wheelPose.steeringRadians;
        for(unsigned i=0;i<4;++i){s.suspension[i]=wheelPose.suspensionY[i];s.wheelRotation[i]=wheelPose.rotationRadians[i];}
        s.speed=vehicle.speed;s.rpm=vehicle.rpm;s.progress=race.progress;
        const auto& light=carPresentation.headlightState();s.headlightPhase=light.phase;s.headlightCounter=light.counter;s.headlightVisible=light.visible?1:0;
        return s;
    }
    void setMultiplayerGo(bool released){
        if(multiplayer.disconnected)throw std::logic_error("The disconnected race is finished");
        if(!multiplayer.active)throw std::logic_error("No multiplayer race");
        if(multiplayer.waiting==!released)return;
        multiplayer.waiting=!released;clock.reset();
        if(released){paused=false;message.clear();messageSeconds=0;beginVsBanner();}
    }
    bool multiplayerDisconnected()const{return multiplayer.active&&multiplayer.disconnected;}
    void setMultiplayerResult(int winner){
        if(winner < -1||winner > 2)throw std::invalid_argument("Invalid multiplayer result");
        if(!multiplayer.active||multiplayer.disconnected||race.phase!=RacePhase::Finished)
            throw std::logic_error("A connected finished race is required for its result");
        if(multiplayer.finishWinner!=-2){
            if(multiplayer.finishWinner!=winner)throw std::logic_error("Multiplayer result is already settled");
            return;
        }
        multiplayer.finishWinner=winner;updateAudioScene();
    }
    void updateAudioScene(bool forcePause=false){
        if(multiplayerDisconnected())audio.scene(true,false,true,false);
        else {
            // render() ends the VS hold after this frame's physics opportunity.
            // Release the selected stream on the first source countdown tick,
            // when the source 3 is visible, rather than that preceding frame.
            const bool held=!menu&&(loadingActive||vsActive||(multiplayer.active&&multiplayer.waiting)||
                (originalHandling&&race.phase==RacePhase::Countdown&&originalRaceOwnerFrame==0));
            const bool carsHidden=menu||loadingActive||preRaceDialogueActive||legendVisitActive||extraModeVisitActive();
            auto outcome=battle&&battleResult!=original::OriginalLegendResult::Win?
                EngineAudio::FinishOutcome::Loss:EngineAudio::FinishOutcome::Win;
            if(multiplayer.active){
                outcome=multiplayer.finishWinner==-2?EngineAudio::FinishOutcome::Pending:
                    multiplayer.finishWinner==2?EngineAudio::FinishOutcome::Draw:
                    multiplayer.finishWinner==int(multiplayer.config.localSlot)?EngineAudio::FinishOutcome::Win:EngineAudio::FinishOutcome::Loss;
            }
            audio.scene(menu&&!legendVisitActive&&!preRaceDialogueActive&&!extraModeVisitActive(),race.phase==RacePhase::Finished,paused||forcePause,race.timeUp,held,carsHidden,outcome);
        }
    }
    void disconnectMultiplayer(){
        if(!multiplayer.active)throw std::logic_error("No multiplayer race");
        if(multiplayer.disconnected)return;
        // Terminal presentation only: do not tick finish rules, score points,
        // publish a win/loss, or release the multiplayer save barrier.
        multiplayer.disconnected=true;multiplayer.waiting=false;
        input={};paused=false;clock.reset();
        loadingActive=preRaceDialogueActive=legendVisitActive=vsActive=buntaVisitActive=timeAttackVisitActive=false;
        vsSeconds=0;vsPhase=vsFrame=vsShot=0;
        race.phase=RacePhase::Finished;race.timeUp=false;
        race.originalStartDigit=-1;raceFeedback.extensionTicks=0;
        finishBannerTicks=finishFadeTicks=0;finishBannerDone=finishAudioSkipped=false;
        resultVisit={};pendingResultSetup.reset();resultsReady=false;
        battleProgressApplied=false;battle=bunta=false;
        originalSession.setEngineOutput({});
        audio.resetRaceEffects();audio.endResultMusic();updateAudioScene();
        message.clear();messageSeconds=0;renderer.screenFadeArgb=0;
    }
    void leaveMultiplayer(){
        publishLocalReplay();
        if(!multiplayer.active)return;
        clearAuthority();
        // Restore while the write barrier remains active. load() can load
        // native menu assets, but cannot flush or migrate the user's profile.
        const auto saved=multiplayer;
        audio.resetRaceEffects();audio.endResultMusic();audio.useDevelopmentEngine();
        originalSession.setEngineOutput({});rivalVisible=false;loadedRivalCar=loadedRivalEnemy=-1;
        playerProjectedHeadlight.reset();rivalProjectedHeadlight.reset();
        resultVisit={};pendingResultSetup.reset();battleProgressApplied=false;resultsReady=false;battle=bunta=false;
        legendVisitActive=loadingActive=preRaceDialogueActive=vsActive=buntaVisitActive=timeAttackVisitActive=false;
        frontend.battleProfile=saved.savedProfile;battleProfile=saved.savedBattleProfile;frontend.gameMode=saved.savedMode;
        frontend.car=saved.savedCar;frontend.make=saved.savedMake;loadedProfileCar=saved.savedProfileCar;profile=saved.savedProfileKind;
        importedCourse.reset();renderer.farClip=5000;
        frontend.course=saved.savedFrontendCourse;courseIndex=saved.savedCourse;frontend.reverse=reverse=saved.savedReverse;frontend.wet=wet=saved.savedWet;
        frontend.night=night=saved.savedNight;frontend.automatic=automatic=saved.savedAutomatic;
        frontend.stage=FrontendStage::Course;menu=true;paused=false;input={};clock.reset();
        raceFog=original::originalBootstrapFog();raceLighting.reset();raceLightSets.reset();
        renderer.courseFog=nullptr;renderer.courseLighting=nullptr;renderer.playerLighting=nullptr;renderer.rivalLighting=nullptr;
        renderer.screenFadeArgb=0;menuTexturesLoaded=false;texturesPending=true;tuningPreview.reset();tuningTexturesLoaded=false;
        if(tuningPresentation)tuningPresentation->clear();
        try{load();}catch(...){multiplayer={};throw;}
        multiplayer={};message.clear();messageSeconds=0;
    }
    void loadRivalCar(unsigned carId,unsigned enemy){
        if(loadedRivalCar==int(carId)&&loadedRivalEnemy==int(enemy))return;
        const auto folder=std::string(originalCarFolders.at(carId));
        rivalModel=NativeModel::load(root/"data/original_models"/folder/(folder+".idasmesh"));
        rivalTextures=NativeTextureBank::load(root/"data/original_assets/cars"/folder/"textures/textures.idastex");
        rivalPresentation=CarPresentation::loadRival(root,carId,enemy,rivalModel.chunks.size());
        rivalPresentation.applyMaterials(rivalModel);
        rivalPlate=OriginalNumberPlate::loadRival(root,carId,enemy);
        loadedRivalCar=int(carId);loadedRivalEnemy=int(enemy);texturesPending=true;
    }
    void projectRivalPose(bool advance){
        if(multiplayer.active){projectMultiplayerRemote();return;}
        rivalVisible=originalSession.rivalInitialized()&&presentedSession().rivalActive();
        if(!rivalVisible)return;
        const auto& published=presentedSession().publishedActors().secondary0C8FF430;
        const auto field=[&](unsigned offset){return std::bit_cast<float>(published.at(offset/4));};
        if(advance){previousRival=rivalVehicle;previousRivalPitch=rivalPitch;previousRivalRoll=rivalRoll;previousRivalWheels=rivalWheels;previousRivalBodyWorld=rivalBodyWorld;}
        rivalVehicle.position={field(0),field(4),field(8)};rivalVehicle.yaw=wrapAngle(field(28)+pi);
        rivalPitch=-field(24);rivalRoll=-field(32);rivalWheels.steeringRadians=field(60);
        for(unsigned i=0;i<4;++i){rivalWheels.suspensionY[i]=field(64+i*4);rivalWheels.rotationRadians[i]=field(96+i*4);}
        if(!advance){
            //15AE00 seeds the internal grid pose but intentionally leaves the
            // public angle/wheel fields until15B0A0. The native countdown
            // precedes that first simulation frame; display the seeded pose.
            const auto& actor=presentedSession().rivalActor();
            rivalVehicle.position={actor.f(200),actor.f(204),actor.f(208)};
            rivalVehicle.yaw=wrapAngle(actor.f(228)+pi);rivalPitch=-actor.f(224);rivalRoll=-actor.f(232);rivalWheels={};
        }
        rivalBodyWorld=rivalBody.update(presentedSession().collision(),unsigned(loadedRivalCar),rivalVehicle.position);
        if(!advance){previousRival=rivalVehicle;previousRivalPitch=rivalPitch;previousRivalRoll=rivalRoll;previousRivalWheels=rivalWheels;previousRivalBodyWorld=rivalBodyWorld;}
    }
    void load(){
        if(importedCourse){
            course=Course::load(importedCourse->root,importedCourse->slug,importedCourse->name,reverse);
            hasOriginalScenery=catalogScenery=false;sceneWet=wet;courseObjects.reset();courseBillboards.reset();courseCrows.reset();
            trackStart=2;trackFinish=course.length;bodyPitch=bodyRoll=previousPitch=previousRoll=0;
            chaseCamera.reset();originalCamera.reset();bumperCamera.reset();configureCar();
            auto spawn=course.sample(trackStart);reset(vehicle,config,spawn.center,std::atan2(spawn.tangent.x,spawn.tangent.z));
            previous=vehicle;segment=0;progress=trackStart;steering=0;cameraReady=false;clock.reset();best=Replay{};bestTime=0;
            loadSelectedCar();texturesPending=true;return;
        }
        if(courseIndex==8)wet=true;
        if(OriginalCourseCrows::availableFor(unsigned(courseIndex),night,wet)){
            if(!courseCrows){courseCrows=OriginalCourseCrows::load(root);texturesPending=true;}
        }else if(courseCrows){courseCrows.reset();texturesPending=true;}
        course=Course::load(root/"data"/"courses",courseIds[courseIndex],courseNames[courseIndex],reverse);
        const bool wantWetScene=wet&&OriginalCourseScene::available(root,sceneId(),night,reverse,true);
        const bool wantCatalog=OriginalCourseScene::available(root,sceneId(),night,reverse,wantWetScene);
        const bool wantOriginalScenery=(course.id=="k_df"&&courseIndex!=8)||wantCatalog;
        if(hasOriginalScenery!=wantOriginalScenery){hasOriginalScenery=wantOriginalScenery;texturesPending=true;}
        if(hasOriginalScenery&&(!courseModelLoaded||loadedCourse!=courseIndex||loadedNight!=night||loadedReverse!=reverse||loadedWet!=wet||catalogScenery!=wantCatalog||sceneWet!=wantWetScene)){
          courseMeshCache.invalidate();
          if(wantCatalog){
            courseScene=OriginalCourseScene::load(root,sceneId(),night,reverse,wantWetScene);
            originalCourseModel=std::move(courseScene.model);originalCourseTextures=std::move(courseScene.textures);
            originalBackgroundModel=std::move(courseScene.backgroundModel);originalBackgroundTextures=std::move(courseScene.backgroundTextures);
          }else{
            const auto directory=root/"data/original_models/courses/k_df";
            originalCourseModel=NativeModel::load(directory/(night?"night/k_df_night.idasmesh":"k_df.idasmesh"));
            originalCourseTextures=NativeTextureBank::load(root/"data/original_assets/courses/k_df/textures/textures.idastex");
            originalBackgroundModel=NativeModel::load(night?directory/"night/background.idasmesh":root/originalAkinaBackgroundModel);
            originalBackgroundTextures=NativeTextureBank::load(night?root/"data/original_assets/courses/k_df/night/background/textures.idastex":root/originalAkinaBackgroundTextures);
            for(int i=0;i<30;i++){std::string name="sector_"+(i<10?std::string("0"):std::string())+std::to_string(i)+".idasasm";originalCourseSectors[i]=NativeAssembly::load(directory/"assembly"/name,originalCourseModel.chunks.size());}
          }
            courseObjects.reset();
            courseBillboards.reset();
            if(wantCatalog&&OriginalCourseObjects::available(root,sceneId(),night,reverse,wantWetScene))
                courseObjects=OriginalCourseObjects::load(root,sceneId(),night,reverse,wantWetScene,originalCourseModel.chunks.size());
            if(courseObjects)courseObjects->setMinimumDrawDistance(600.f);
            if(wantCatalog&&OriginalCourseBillboards::available(root,sceneId(),night,reverse,wantWetScene))
                courseBillboards=OriginalCourseBillboards::load(root,sceneId(),night,reverse,wantWetScene,originalCourseModel.chunks.size());
            courseModelLoaded=true;loadedCourse=courseIndex;loadedNight=night;loadedReverse=reverse;loadedWet=wet;catalogScenery=wantCatalog;sceneWet=wantWetScene;texturesPending=true;
        }
        courseAnimation.reset(unsigned(courseIndex),night,wantWetScene);
        trackStart=2.f;trackFinish=course.length;bodyPitch=bodyRoll=previousPitch=previousRoll=0;chaseCamera.reset();originalCamera.reset();bumperCamera.reset();
        configureCar();auto spawn=course.sample(trackStart);reset(vehicle,config,spawn.center,std::atan2(spawn.tangent.x,spawn.tangent.z));previous=vehicle;segment=spawn.segmentIndex;progress=trackStart;steering=0;cameraReady=false;clock.reset();
        best=Replay{};bestTime=0;if(!multiplayer.active&&best.load(ghostPath().string())&&!best.frames.empty())bestTime=best.finishTicks6000?double(best.finishTicks6000)/6000:double(best.frames.back().tick)/60;
        loadSelectedCar();
    }
    void projectOriginalPose(bool advance){
        const auto& source=presentedSession().vehicle();const auto& d=source.drive;
        const auto oldPosition=vehicle.position;
        vehicle.position={d.f(0),d.f(4),d.f(8)};
        if(authorityRace)vehicle.position=vehicle.position+authorityVisualOffset[multiplayer.config.localSlot];
        if(advance)previousPlayerBodyWorld=playerBodyWorld;
        playerBodyWorld=playerBody.update(presentedSession().collision(),unsigned(frontend.car),vehicle.position);
        if(!advance)previousPlayerBodyWorld=playerBodyWorld;
        // Original motion is(-sin(yaw),-cos(yaw)); host camera/car forward is
        // (+sin(yaw),+cos(yaw)). Apply the half-turn only at presentation.
        vehicle.yaw=wrapAngle(d.f(0x10)+pi+(authorityRace?authorityYawOffset[multiplayer.config.localSlot]:0.f));
        bodyPitch=-d.f(0x0C);bodyRoll=-d.f(0x14);
        const auto& actor=presentedSession().actor();wheelPose.steeringRadians=actor.f(0x3C);
        for(std::size_t i=0;i<4;++i){wheelPose.suspensionY[i]=actor.f(0x40+i*4);wheelPose.rotationRadians[i]=actor.f(0x60+i*4);}
        vehicle.velocity=advance?(vehicle.position-oldPosition)/physicsDt:Vec3{};
        vehicle.speed=d.f(0x238);vehicle.previousSpeed=d.f(0x23C);vehicle.velocityDelta=d.f(0x234);
        vehicle.rpm=source.transmission.tach1c;vehicle.previousGear=vehicle.gear;vehicle.gear=int(source.transmission.gear00);
        vehicle.steering=d.f(0x1CC);vehicle.throttle=d.f(0x1B8);vehicle.brake=d.f(0x1C4);
        vehicle.slip=d.f(0x27C);vehicle.yawRate=d.f(0xDC)/physicsDt;
        vehicle.yawContribution=d.f(0xD8);vehicle.yawFrameVelocity=d.f(0xDC);vehicle.yawUnwrapped=d.f(0x110)+pi;
        vehicle.accelProxy=d.f(0x24C);vehicle.steeringDelta=d.f(0x1E8);vehicle.previousSteering=d.f(0x1EC);
        for(std::size_t i=0;i<6;++i)vehicle.steeringBasis[i]=d.f(0x1CC+i*4);
        vehicle.wallContact=d.u(0x150)!=0;vehicle.wallImpactSpeed=presentedSession().roadContact().impact0C900E60;
        if(advance){vehicle.travel+=length(vehicle.position-oldPosition);++vehicle.tick;vehicle.simulatedSeconds+=physicsDt;}
    }
    void advanceOriginalCamera(){
        const auto& d=presentedSession().vehicle().drive;
        const Vec3 cameraOffset=authorityRace?authorityVisualOffset[multiplayer.config.localSlot]:Vec3{};
        const float cameraYaw=authorityRace?authorityYawOffset[multiplayer.config.localSlot]:0;
        const Vec3 cameraPosition=Vec3{d.f(0),d.f(4),d.f(8)}+cameraOffset;
        const Vec3 cameraAngles{d.f(0x0C),d.f(0x10)+cameraYaw,d.f(0x14)};
        const bool initialized=originalCamera.ready();
        if(initialized)previousRearCameraFrame=rearCameraFrame;
        rearCameraFrame=originalCamera.rearView(playerBodyWorld,cameraAngles);
        if(!initialized)previousRearCameraFrame=rearCameraFrame;
        if(initialized)previousCameraFrame=originalCamera.frame();
        const auto& frame=originalCamera.update(cameraPosition,cameraAngles);
        if(!initialized)previousCameraFrame=frame;
        const bool bumperInitialized=bumperCamera.ready();
        if(bumperInitialized)previousBumperFrame=bumperCamera.frame();
        const auto& bumper=bumperCamera.update(cameraPosition,cameraAngles);
        if(!bumperInitialized)previousBumperFrame=bumper;
    }
    void start(bool networkStart=false){
        fullTuneActive=fullTuneSelecting=false;fullTuneOffers.clear();
        if(multiplayer.active&&!networkStart)throw std::logic_error("Online races cannot restart locally");
        if(Frontend::isImportedCourse(frontend.course)&&frontend.gameMode==original::OriginalGameMode::TimeAttack){
            if(importedRoot(frontend.course).empty())throw std::logic_error("Hakone course pack is not registered");
            if(!importedCourse||importedCourse->id!=unsigned(frontend.course))importedCourse=ImportedCourse::load(importedRoot(frontend.course));
            courseIndex=3;
        }else importedCourse.reset();
        steeringSmoothing.reset();
        buntaVisitActive=timeAttackVisitActive=timeAttackLectureDone=timeSummaryDone=false;
        timeSummaryTicks=0;timeAttackCourseRankingQualified=false;timeAttackPersonalRegistered=false;timeSummaryClock.reset();modeVisitClock.reset();timeAttackTrace.clear();
        timeAttackTelemetry.reset();timeAttackSnapshot={};timeAttackAnalysisInput={};timeAttackAnalysis={};timeAttackAnalysisPrepared=false;
        timeAttackRankingPreview.reset();timeAttackRankingTexturesLoaded=false;
        timeAttackBackdropStage=original::OriginalTimeAttackVisit::Stage::Inactive;
        vsActive=false;vsSeconds=0;vsPhase=vsFrame=vsShot=0;
        loadSelectedProfile();battleProgressApplied=false;battleResult=original::OriginalLegendResult::NotLegend;battlePoints={};buntaPoints={};timeAttackPoints={};settledBattleAdvantage=0;battleResults={};battleResultsClock.reset();
        resultVisit={};pendingResultSetup.reset();resultAnimationFrame={};resultConfirmPending=false;resultSelectionAxis=.5f;
        finishBannerTicks=finishFadeTicks=0;finishBannerDone=finishAudioSkipped=false;
        tuningTexturesLoaded=false;
        if(tuningPresentation)tuningPresentation->clear();tuningPreview.reset();
        frontend.battleProfile.setu(16,unsigned(frontend.car));frontend.battleProfile.setu(68,frontend.automatic?0u:1u);
        if(frontend.gameMode==original::OriginalGameMode::TimeAttack){
            // Keep the native quick-start path and the ordinary selection path
            // on the same source profile mode and chosen race conditions.
            frontend.battleProfile.setu(0,1);frontend.battleProfile.setu(4,unsigned(courseIndex));
            frontend.battleProfile.setu(8,unsigned(night));frontend.battleProfile.setu(12,unsigned(reverse));
            frontend.battleProfile.setu(32,unsigned(wet||courseIndex==8));
        }
        battleProfile=frontend.battleProfile;
        if(!multiplayer.active&&!validationMode){pendingProfiles.at(unsigned(frontend.car))=frontend.battleProfile;flushProfiles();}
        bunta=frontend.gameMode==original::OriginalGameMode::BuntaChallenge;
        battle=frontend.gameMode==original::OriginalGameMode::LegendOfTheStreets||bunta;
        if(battle){
            battleProfile=frontend.battleProfile;
            const auto selected=original::originalBattleSelection(battleProfile);
            courseIndex=int(selected.course);reverse=selected.direction!=0;wet=selected.weather!=0;night=selected.night!=0;
        }
        // Dialogue music owns the same resource slot as selection music.
        // Release it before PACK23 is registered for the race.
        audio.endResultMusic();
        audio.musicTrack=effectiveRaceMusicSelection(selectedRaceMusic);
        // Release the outgoing score before race setup reuses its resource
        // slot2 for PACK23. A late menu unload would remove the new race bank.
        frontend.endSelectionMusic();
        for(const auto& command:frontend.takeSelectionMusicCommands())audio.selection(command);
        load();originalHandling=supportsOriginalHandling();originalInput={};raceLightSets.reset();rivalVisible=false;playerBody.reset();rivalBody.reset();carPresentation.resetHeadlights();
        // Course activation owns these registers. Menu asset preloads must
        // preserve the retained fog table; Happo changes only density/color.
        if(courseIndex==4)original::applyHappoFogRegisters(raceFog,night,wet);
        else raceFog=original::originalCourseFog(unsigned(courseIndex),night,wet);
        raceLighting=original::originalCourseLighting(unsigned(courseIndex),night,wet);
        if(!courseLightFsca)courseLightFsca=original::OriginalFscaTable::load(root/"data/original_physics/fsca_table.bin");
        courseLightPathIndex=0;
        if(originalHandling){
            original::OriginalDrivingSelection selection;
            selection.physics=original::makeOriginalFreshTimeAttackSelection(std::uint32_t(frontend.car),importedCourse?importedCourse->handlingCondition(reverse):std::uint32_t(courseIndex*2+int(reverse)),wet?original::OriginalWeather::Wet:original::OriginalWeather::Dry);
            //159720 reads tuning from the loaded driver profile. The chosen
            //weather is a race-owner input, including a quick-run override.
            auto physicsProfile=battle?battleProfile:frontend.battleProfile;
            physicsProfile.setu(16,unsigned(frontend.car));physicsProfile.setu(32,wet?1u:0u);
            original::applyOriginalProfilePhysicsSelection(selection.physics,physicsProfile);
            selection.collisionVariant=reverse?1u:0u;
            if(battle){
                original::OriginalDrivingRivalSetup rival;
                if(bunta){
                    const auto setup=original::makeOriginalBuntaRaceSetup(battleProfile,2);
                    rival.control=setup.ordinaryRivalControl;rival.geometryCar0C9015F8=setup.geometryCar0C9015F8;
                    rival.profileMode0C901648=setup.profileMode0C901648;rival.enemyId0C9015E0=setup.enemyId0C9015E0;rival.level0C9015D0=setup.level0C9015D0;
                    rival.opponentProgress0C901644=setup.opponentProgress0C901644;rival.progress0C901604=setup.progress0C901604;
                }else{
                    rival.control=std::bit_cast<std::int32_t>(battleProfile.u(20));rival.geometryCar0C9015F8=battleProfile.u(20);
                    rival.profileMode0C901648=battleProfile.u(0);rival.enemyId0C9015E0=battleProfile.u(24);rival.level0C9015D0=battleProfile.u(148);
                    rival.opponentProgress0C901644=battleProfile.byte(116+battleProfile.u(24));
                    for(unsigned i=0;i<8;++i)rival.progress0C901604[i]=battleProfile.u(1080+i*4);
                }
                const auto pose=original::originalStartPose(selection.physics.conditionCode,1);
                rival.position=pose.position;rival.angles=pose.angles;selection.rival=rival;
                loadRivalCar(battleProfile.u(20),battleProfile.u(24));
                rivalPresentation.resetHeadlights();
            }
            const unsigned numericMode=battle?0:2;
            const auto condition=std::uint32_t(courseIndex*2+int(reverse));
            // ACar binds the authored path before its first pose publication.
            // Driving rules use the same immutable path after startup.
            originalPath=importedCourse?importedCourse->racePath(reverse):original::OriginalRacePath::load(root,condition);
            const auto spawn=importedCourse?(multiplayer.active?importedCourse->onlineSpawn(reverse,multiplayer.config.localSlot):importedCourse->spawn(reverse)):original::originalStartPose(selection.physics.conditionCode,multiplayer.active?multiplayer.config.localSlot:original::originalSoloStartGridSlot(numericMode));
            // Original course loading creates the flock before player setup.
            // Reconstruct its preceding nineteen RNG calls by value, ending
            // at the existing driving entry seed. No driving state is changed.
            if(courseCrows)courseCrows->resetBeforeDrivingSeed(presentedSession().ready()?presentedSession().contactCompletion().randomSeed0C37C778:1u);
            auto importedRoad=importedCourse?std::optional(importedCourse->drivingRoad(reverse)):std::nullopt;
            const auto resetEffects=originalSession.reset(root,selection,spawn.position,spawn.angles,importedRoad?&*importedRoad:nullptr);
            if(multiplayer.active)initializeMultiplayerRemote(condition);
            originalRaceStart.reset(numericMode);originalRaceOwnerFrame=0;
            if(resetEffects.resetPlatformDigitalInput)originalInput={};
            const auto held=driver(false);
            const original::OriginalHostControls frozenControls{held.steer,held.throttle,held.brake,held.shiftDown,held.shiftUp};
            const auto frozenInputs=original::adaptOriginalHostInput(originalInput,frozenControls,frontend.automatic,false,0);
            originalSession.setProgressCorrection(0.f,0);
            projectOriginalPose(false);projectRivalPose(false);
            initializeRaceCarLighting(condition);
            initializeProjectedHeadlights();
            original::warmupOriginalRaceSession(originalSession,frozenInputs,originalSession.platformFrame(),0,
                [this](const auto&){
                    projectOriginalPose(false);projectRivalPose(true);
                    //062100 invokes063CE0 during warmup, but no scene draw.
                    //Its HUD gap and accumulated progress are both exact0.
                    publishProjectedHeadlights(0.f,0);
                });
            audio.selectOriginalEngine(root,physicsProfile);
            originalSession.setEngineOutput([this](const original::OriginalContactCompletionEffects& effect,std::uint32_t& seed){
                original::OriginalEngineControlInput input;input.rpm=effect.engineValue;input.throttle=effect.throttle;
                input.gear=std::bit_cast<std::int32_t>(effect.gear);input.suppressShiftRelease=originalSession.raceAutomaticBrakeByte()!=0;
                for(unsigned i=0;i<4;++i)input.wheelSurface[i]=std::uint8_t(presentedSession().actor().u(116)>>(8*i));
                audio.stepOriginalEngine(input,seed);
            });
            previousRival=rivalVehicle;previousRivalPitch=rivalPitch;previousRivalRoll=rivalRoll;previousRivalWheels=rivalWheels;previousRivalBodyWorld=rivalBodyWorld;
            config.gearCount=int(presentedSession().parameters().transmission.maximumGear);
            previous=vehicle;previousPitch=bodyPitch;previousRoll=bodyRoll;previousWheelPose=wheelPose;
            advanceOriginalCamera();
            const auto startProjection=course.project(vehicle.position);trackStart=startProjection.sample.distance;
            segment=startProjection.segment;progress=trackStart;
            originalCoordinate={importedCourse?importedCourse->rules(reverse).startIndex:original::originalRaceRuleRow(original::originalRaceRuleRowIndex(condition,numericMode)).startIndex,0};
            rivalCoordinate=originalCoordinate;
            if(battle){
                if(bunta)originalRace.resetBunta(root,{condition,2,wet?1u:0u},originalCoordinate,spawn.position);
                else originalRace.resetLegend(root,{condition,battleProfile.u(24),2,battleProfile.byte(116+battleProfile.u(24))},originalCoordinate,spawn.position);
            }
            else if(importedCourse)importedCourse->resetRules(originalRace,reverse,spawn.position);
            else originalRace.reset(root,{condition,2,wet?1u:0u},originalCoordinate,spawn.position);
            if(battle||multiplayer.active)battleMetrics=importedCourse?original::OriginalBattleMetrics(std::span(importedCourse->center).subspan(importedCourse->checkpoints[0],importedCourse->rules(reverse).goalIndex+1),reverse):original::OriginalBattleMetrics::load(root,condition);
        }
        if(!originalHandling)audio.useDevelopmentEngine();
        texturesPending=true;
        menu=false;paused=false;wetWeather.reset();audio.resetRaceEffects();race.start(originalHandling?float(originalRace.rules().goalIndex):trackFinish-trackStart);
        race.originalTiming=originalHandling;if(originalHandling){race.remaining6000=std::bit_cast<std::int32_t>(originalRace.state().remaining.value);
            race.sectionCapacity=1;for(auto index:originalRace.rules().sectionIndices)if(index>=0)++race.sectionCapacity;race.sectionCapacity=std::min(race.sectionCapacity,4u);}
        raceFeedback.reset(race.remaining6000);
        recording.beginCapture(originalHandling);finishedSaved=false;skids.clear();resultsReady=false;results={};
        archiveMode=multiplayer.active?1:battle?2:0;
        archiveThisRace=!replayPlaybackActive&&((archiveMode==0&&(replayRecordingFlags&9))||(archiveMode==1&&(replayRecordingFlags&2))||(archiveMode==2&&!bunta&&(replayRecordingFlags&4)));
        archivePublished=false;
        if(archiveThisRace&&archiveMode!=0)rivalRecording.beginCapture(true);
        else rivalRecording=Replay{};
        archiveRivalCar=(battle||multiplayer.active)?loadedRivalCar:-1;
        archiveRivalEnemy=battle?int(battleProfile.u(24)):-1;
        if(multiplayer.active)for(unsigned i=0;i<replayProfileOffsets.size();++i)rivalRecording.profile[i]=multiplayer.remoteProfile.u(replayProfileOffsets[i]);
        recording.detailed=originalHandling;
        for(unsigned i=0;i<replayProfileOffsets.size();++i)recording.profile[i]=frontend.battleProfile.u(replayProfileOffsets[i]);
        if(originalHandling&&!battle&&!multiplayer.active){
            results.carId=unsigned(frontend.car);results.condition=unsigned((importedCourse?int(importedCourse->id):courseIndex)*2+reverse);
            const auto saved=displayedTimeAttackRecords().best(results.condition,unsigned(wet),results.carId);
            if(importedCourse){
                importedPreviousBest=importedPersonalRecords.personalBest(results.condition,unsigned(wet),results.carId);
                importedCoursePreviousBest={};for(const auto& entry:displayedTimeAttackRecords().entries())if(entry.condition==results.condition&&entry.weather==unsigned(wet)&&entry.ticks6000==saved.course){importedCoursePreviousBest=entry;break;}
                results.bestTimes6000={saved.course,saved.model,importedPreviousBest.ticks6000};
            }else{
            const auto personal=original::originalPersonalTimeAttackRecord(battleProfile,original::originalRecordPartition(results.condition,wet));
            results.bestTimes6000={saved.course,saved.model,personal.ticks6000};
            }
            results.modelBestAvailable=true;results.edgeAnchored=true;results.suppliedRecordTargets=true;
        }
        recording.priorRecords=results.bestTimes6000;
        if(!validationMode&&!originalHandling)status("Development handling - original contact not available for this selection");
        if(!loadingActive&&!multiplayer.active&&!importedCourse)beginVsBanner();
        updateAudioScene();
    }
    void saveSettings(){if(multiplayer.active)return;flushProfiles();std::ofstream out(userdataRoot()/"settings.txt");out<<courseIndex<<' '<<profile<<' '<<reverse<<' '<<wet<<' '<<night<<' '<<automatic<<' '<<audio.enabled<<' '<<audio.musicTrack<<'\n';std::ofstream native(userdataRoot()/"native_selection.txt");native<<frontend.make<<' '<<frontend.car<<'\n';}
    void settings(){fs::create_directories(userdataRoot());saveSlots=LocalSaveSlots(userdataRoot()/"saves");useSaveSlot(-1);records.load(userdataRoot()/"time_attack_records_v1.csv");frontend.timeAttackBest=[this](unsigned condition,unsigned weather,unsigned car){return displayedTimeAttackRecords().best(condition,weather,car);};frontend.importedPersonalBest=[this](unsigned condition,unsigned weather,unsigned car){return importedPersonalRecords.personalBest(condition,weather,car);};std::ifstream in(userdataRoot()/"settings.txt");int ci,p,r,w,n,a,s,music;if(in>>ci>>p>>r>>w>>n>>a>>s){courseIndex=std::clamp(ci,0,8);profile=std::clamp(p,0,2);reverse=r==1;wet=w==1||courseIndex==8;night=n==1||courseIndex==8;automatic=a==1;audio.enabled=s==1;if(in>>music)audio.musicTrack=clampMusicTrack(music);}
        selectedRaceMusic=loadRaceMusicSelection(userdataRoot(),audio.musicTrack);
    }
    void commands(double dt){
        updateSteeringSmoothingLifecycle();
        if(multiplayerDisconnected())return;
        // A peer race has no local pause state, even if a stale host flag or
        // unfocused window tries to leave one behind. Options remain local.
        if(multiplayer.active)paused=false;
        // These screen owners consume their own input. In particular, Start
        // skips a rival intro without also backing out of the hidden menu.
        if(legendVisitActive||preRaceDialogueActive||loadingActive||extraModeVisitActive())return;
        if(multiplayer.active){
            // Network host owns leave/restart/room input; native commands may
            // not launch a single-player race or a post-result owner here.
            if(input.key('C')||input.button(XINPUT_GAMEPAD_Y))drivingView=drivingView==OriginalDrivingView::Bumper?OriginalDrivingView::Chase:OriginalDrivingView::Bumper;
            if(input.key(VK_F2))audio.enabled=!audio.enabled;
            return;
        }
        if(input.key(VK_F1)){if(menu)showControls=!showControls;else debug=!debug;}
        if(input.key(VK_F2)){audio.enabled=!audio.enabled;status(audio.enabled?"Audio enabled":"Audio muted");}
        if(menu){
            // Attract model pages use the source gear signal; VIEW CHANGE
            // advances the ranking condition. These edges do not act as START.
            if(frontend.stage==FrontendStage::Title&&frontend.attractChild()==12)
                frontend.queueRankingInput({input.key('Q')||input.key('E')||input.button(XINPUT_GAMEPAD_LEFT_SHOULDER)||input.button(XINPUT_GAMEPAD_RIGHT_SHOULDER),
                    input.key('C')||input.button(XINPUT_GAMEPAD_Y)});
            // The original gear-up/down signals cycle factory paint in Car.
            // Keyboard and controller buttons are the native device mapping.
            if(input.key('E')||input.key(VK_UP)||input.button(XINPUT_GAMEPAD_Y)||input.button(XINPUT_GAMEPAD_DPAD_UP))frontend.changeColor(1);
            if(input.key('Q')||input.key(VK_DOWN)||input.button(XINPUT_GAMEPAD_X)||input.button(XINPUT_GAMEPAD_DPAD_DOWN))frontend.changeColor(-1);
            auto change=[&](int direction){
                auto selection=[&]{return std::array<int,10>{frontend.make,frontend.car,frontend.course,int(frontend.automatic),int(frontend.reverse),int(frontend.wet),int(frontend.night),int(frontend.gameMode),int(frontend.stage),frontend.rivalChoice};};
                const auto before=selection();frontend.change(direction);
                if(selection()!=before)audio.playMenuCue(OriginalMenuCue::Change);
            };
            const bool left=input.down[VK_LEFT]||input.down['A']||(input.pad.Gamepad.wButtons&XINPUT_GAMEPAD_DPAD_LEFT)||input.pad.Gamepad.sThumbLX< -16000;
            const bool right=input.down[VK_RIGHT]||input.down['D']||(input.pad.Gamepad.wButtons&XINPUT_GAMEPAD_DPAD_RIGHT)||input.pad.Gamepad.sThumbLX>16000;
            const int direction=int(right)-int(left);
            if(frontend.stage==FrontendStage::Name){
                const float stick=input.connected?std::clamp(input.pad.Gamepad.sThumbLX/32767.f,-1.f,1.f):0.f;
                frontend.setNameSteering(direction?float(direction):std::abs(stick)>.13f?stick:0.f);
                menuDirection=0;menuRepeatWait=0;
            }else if(!direction){menuDirection=0;menuRepeatWait=0;}
            else if(direction!=menuDirection){change(direction);menuDirection=direction;menuRepeatWait=.32;}
            else {menuRepeatWait-=std::min(dt,.1);if(menuRepeatWait<=0){change(direction);menuRepeatWait+=.12;}}
            if((input.key(VK_ESCAPE)||input.button(XINPUT_GAMEPAD_B)||(frontend.stage==FrontendStage::Name&&input.key(VK_BACK)))&&!frontend.confirmationInProgress()){
                const bool sourceEntry=frontend.stage==FrontendStage::Name||frontend.stage==FrontendStage::TuningCourse;
                if(!frontend.back())running=false;else if(!sourceEntry)audio.playMenuCue(OriginalMenuCue::Back);
            }
            bool launch=false;
            if(mouseStart||input.key(VK_RETURN)||input.button(XINPUT_GAMEPAD_A)||input.button(XINPUT_GAMEPAD_START)){
                mouseStart=false;
                if(frontend.inputReady()&&frontend.stage!=FrontendStage::Name&&frontend.stage!=FrontendStage::TuningCourse)audio.playMenuCue(frontend.unsupportedModeSelected()?OriginalMenuCue::Back:OriginalMenuCue::Confirm);
                launch=frontend.confirm();menuRepeatWait=.32;
            }
            if(input.key(VK_F5)){audio.playMenuCue(OriginalMenuCue::Confirm);frontend.gameMode=original::OriginalGameMode::TimeAttack;frontend.course=3;frontend.reverse=false;frontend.wet=false;launch=true;}
            courseIndex=Frontend::isImportedCourse(frontend.course)?3:frontend.course;automatic=frontend.automatic;reverse=frontend.reverse;wet=frontend.wet;night=frontend.night;
            if(launch)start();
        }else{
            const bool tuneLeft=input.down[VK_LEFT]||input.down['A']||(input.pad.Gamepad.wButtons&XINPUT_GAMEPAD_DPAD_LEFT);
            const bool tuneRight=input.down[VK_RIGHT]||input.down['D']||(input.pad.Gamepad.wButtons&XINPUT_GAMEPAD_DPAD_RIGHT);
            const float tuneStick=input.connected?input.pad.Gamepad.sThumbLX/32767.f:0.f;
            resultSelectionAxis=tuneLeft!=tuneRight?(tuneLeft?0.f:1.f):std::abs(tuneStick)>.13f?std::clamp((tuneStick+1.f)*.5f,0.f,1.f):.5f;
            // Full Tune is also entered with buttons. Keep the selected choice
            // after releasing Left/Right so a separate Confirm press selects it.
            if(fullTuneActive&&resultVisit.tuning.kind==original::OriginalTuningChildKind::optionalPart&&
                !tuneLeft&&!tuneRight&&std::abs(tuneStick)<=.13f)
                resultSelectionAxis=resultVisit.child.choice?1.f:0.f;
            if(input.key('C')||input.button(XINPUT_GAMEPAD_Y))drivingView=drivingView==OriginalDrivingView::Bumper?OriginalDrivingView::Chase:OriginalDrivingView::Bumper;
            if(input.key('R')&&!fullTuneActive)start();
            if(fullTuneActive&&resultVisit.tuning.kind==original::OriginalTuningChildKind::optionalPart&&resultVisit.child.phase==0&&
                (input.key(VK_ESCAPE)||input.button(XINPUT_GAMEPAD_B))){finishFullTune();return;}
            if((input.key(VK_ESCAPE)||input.button(XINPUT_GAMEPAD_START))&&race.phase==RacePhase::Finished&&!resultVisit.initialized&&pendingResultSetup){
                // Start is the explicit opt-out from the entire finish audio
                // hold, including a record banner that has already appeared.
                finishAudioSkipped=true;finishBannerDone=true;resultConfirmPending=false;
            }
            else if((input.key(VK_ESCAPE)||input.button(XINPUT_GAMEPAD_START))&&race.phase==RacePhase::Finished)resultConfirmPending=true;
            else if(input.key(VK_ESCAPE)||input.button(XINPUT_GAMEPAD_START)){paused=!paused;clock.reset();}
            if(paused&&input.key(VK_BACK))returnToCourseSelection();
            else if(race.phase==RacePhase::Finished&&!paused&&(input.key(VK_RETURN)||input.button(XINPUT_GAMEPAD_A))){
                if(resultVisit.initialized)resultConfirmPending=true;
                else if(!pendingResultSetup)returnToCourseSelection();
            }
        }
    }
    // The source post-result owner runs between a settled Legend result and the
    // next destination. Without its artwork the existing shortcut is kept.
    bool beginLegendVisit(){
        if(!battle||bunta||(validationMode&&!legendVisitSmoke))return false;
        if(battleProfile.u(0)!=0||battleProfile.u(4)>8||battleProfile.u(24)>30)return false;
        if(!legendVisitLoaded){
            if(!original::OriginalLegendVisit::available(root))return false;
            legendVisit.load(root);
            legendVisitLoaded=true;
        }
        original::OriginalLegendVisit::Setup setup;
        setup.resultStatus=battleResults.resultStatus;
        setup.playerCar=unsigned(frontend.car);
        setup.weather=wet?1u:0u;
        // 0F8B00 draws the spectator variant from the shared game RNG (1F9E60).
        if(presentedSession().ready())originalSession.withSharedRandom([&](std::uint32_t& seed){
            seed=seed*0x41c64e6du+12345u;
            setup.cheer=(((seed>>16)&0x7fffu)%1000u)%7u;
        });
        legendVisit.begin(battleProfile,setup);
        frontend.battleProfile=battleProfile;
        legendVisitActive=true;legendVisitClock.reset();
        legendConfirmPending=legendPreviousPending=legendNextPending=false;legendSkipHeld=false;
        audio.endResultMusic();
        return true;
    }
    void finishLegendVisit(){
        const auto destination=legendVisit.destination();
        legendVisitActive=false;
        frontend.battleProfile=battleProfile;
        saveResultProfile();
        // The owner already advanced the profile to the next rival and its
        // course, so a course load races that rival straight away. Ending and
        // EjectCard are session ends; both leave the player at the ordinary
        // selection the native shell already owns.
        if(destination==original::OriginalLegendReturnDestination::CourseLoad){
            frontend.gameMode=original::OriginalGameMode::LegendOfTheStreets;
            start();
            return;
        }
        returnToCourseSelection();
        // A completed run is the end of the driver's session, so the shell goes
        // back to its start rather than to the middle of a selection.
        if(destination==original::OriginalLegendReturnDestination::Ending){
            frontend.stage=FrontendStage::Title;
            frontend.advance(0);
            legendRunCompleted=true;
        }
    }
    bool canRetireLegendRace()const{
        return originalHandling&&battle&&!bunta&&!multiplayer.active&&!menu&&!loadingActive&&
            !legendVisitActive&&!preRaceDialogueActive&&!buntaVisitActive&&!timeAttackVisitActive&&
            (race.phase==RacePhase::Running||race.phase==RacePhase::Countdown);
    }
    bool retireLegendRace(){
        if(!canRetireLegendRace()||!originalRace.retire())return false;
        paused=false;vsActive=false;vsSeconds=0;vsPhase=vsFrame=vsShot=0;clock.reset();
        race.phase=RacePhase::Finished;race.timeUp=true;race.remaining6000=0;
        race.elapsed6000=originalRace.displayedElapsed();
        originalSession.setRaceAutomaticBrake(true);
        finishBannerTicks=finishFadeTicks=0;finishBannerDone=finishAudioSkipped=false;resultConfirmPending=false;
        settleBattleResult(false);
        return true;
    }
    void settleBattleResult(bool finished){
        if(!battle||battleProgressApplied||race.phase!=RacePhase::Finished)return;

        if(bunta)battleResult=original::recordOriginalBuntaResult(battleProfile,finished?1u:0u,originalRace.state().battleResult.outcomeCode)?original::OriginalLegendResult::Win:original::OriginalLegendResult::Loss;
        else battleResult=original::recordOriginalLegendResult(battleProfile,finished?1u:0u,originalRace.state().battleResult.outcomeCode);
        original::updateOriginalPostRaceRank(battleProfile);
        settledBattleAdvantage=battleMetrics.advantage(originalRace.state().progress,originalRace.state().rivalProgress);
        if(bunta)buntaPoints=original::awardOriginalBuntaPoints(battleProfile,battleResult==original::OriginalLegendResult::Win?0u:1u,settledBattleAdvantage);
        else battlePoints=original::awardOriginalLegendPoints(battleProfile,battleResult==original::OriginalLegendResult::Win?0u:1u,settledBattleAdvantage);
        battleResults.resultStatus=race.timeUp?2u:battleResult==original::OriginalLegendResult::Win?0u:1u;
        // A win goes on the open file, so the list can count them.
        if(!battleResults.resultStatus&&activeSaveSlot>=0&&!validationMode)
            saveSlots.addWin(unsigned(activeSaveSlot));
        battleResults.profileMode=battleProfile.u(0);battleResults.totalTicks6000=race.elapsed6000;
        battleResults.sectionTimes6000=race.sectionTimes6000;battleResults.sectionCount=unsigned(race.sector);battleResults.sectionCapacity=race.sectionCapacity;
        battleResults.signedAdvantage=settledBattleAdvantage;
        battleResults.circuitLayout=courseIndex==0||courseIndex==1;
        battleResults.deduction=bunta&&buntaPoints.deduction;
        if(bunta)battleResults.points={buntaPoints.participation,buntaPoints.win,buntaPoints.advantage,buntaPoints.total,battleProfile.u(72)};
        else battleResults.points={battlePoints.participation,battlePoints.win,battlePoints.advantage,battlePoints.total,battleProfile.u(72)};
        // The scorer commits once before the original visible count.
        // Its uncapped result preserves the source's starting balance.
        pendingResultSetup=original::OriginalBattleResultAnimationSetup{battleResults.points[3],
            bunta?buntaPoints.balanceBeforeCap:battlePoints.balanceBeforeCap,battleResults.deduction};
        battleProgressApplied=true;frontend.battleProfile=battleProfile;
        if(!validationMode){pendingProfiles.at(unsigned(frontend.car))=battleProfile;flushProfiles();}
    }
    void returnToCourseSelection(bool challengerInterrupt=false){
        if(importedCourse){importedCourse.reset();renderer.farClip=5000;}
        // Leaving an unfinished Legend battle is the same retirement action.
        // Completed dialogue/results still return directly to selection.
        if(!challengerInterrupt&&canRetireLegendRace()){retireLegendRace();return;}
        if(multiplayer.active){leaveMultiplayer();return;}
        if(challengerInterrupt){
            // A network challenger interrupts play, never awards a time-out
            // loss or writes an unfinished result to the driver's card.
            loadingActive=preRaceDialogueActive=legendVisitActive=false;
            resultVisit={};pendingResultSetup.reset();battleProgressApplied=false;resultsReady=false;
            battle=bunta=false;input={};steering=0;
            audio.resetRaceEffects();audio.useDevelopmentEngine();
            if(originalSession.ready())originalSession.setEngineOutput({});rivalVisible=false;
        }
        // Returning during either showcase shot must release its music/input
        // ownership too. The presentation no longer advances in menu mode.
        vsActive=false;vsSeconds=0;vsPhase=vsFrame=vsShot=0;clock.reset();
        buntaVisitActive=timeAttackVisitActive=false;
        raceFog=original::originalBootstrapFog();raceLighting.reset();raceLightSets.reset();
        renderer.courseFog=nullptr;renderer.courseLighting=nullptr;renderer.playerLighting=nullptr;renderer.rivalLighting=nullptr;
        bool persistResult=resultVisit.initialized;
        if(battleProgressApplied&&!bunta&&battleResult==original::OriginalLegendResult::Win){
            original::refreshOriginalLegendCourseProgress(battleProfile);frontend.battleProfile=battleProfile;
            persistResult=true;
        }
        if(persistResult){
            frontend.battleProfile=battleProfile;
            saveResultProfile();
        }
        audio.endResultMusic();tuningPreview.reset();tuningTexturesLoaded=false;
        if(tuningPresentation)tuningPresentation->clear();
        menuTexturesLoaded=false;texturesPending=true;
        menu=true;paused=false;frontend.stage=FrontendStage::Course;renderer.screenFadeArgb=0;load();
    }
    void resetSteeringSmoothing(){
        steeringSmoothing.reset();
        // With the preference disabled, preserve the existing conditioner.
        if(steeringSmoothing.amount()>0.f)steering=0;
    }
    void setHostDrivingControlsBlocked(bool blocked){
        hostDrivingControlsBlocked=blocked;
        if(blocked){steering=0;steeringSmoothing.reset();}
    }
    void updateSteeringSmoothingLifecycle(){
        if(!active||hostDrivingControlsBlocked||paused||menu||loadingActive||vsActive||
            extraModeVisitActive()||legendVisitActive||preRaceDialogueActive||
            (multiplayer.active&&multiplayer.waiting)||(steeringControllerConnected&&!input.connected))
            resetSteeringSmoothing();
        steeringControllerConnected=input.connected;
    }
    DriverInput driver(bool advanceSmoothing=true){
        DriverInput d;d.automatic=automatic;
        if(hostDrivingControlsBlocked||(multiplayer.active&&!active)){
            steering=0;steeringSmoothing.reset();return d;
        }
        const float keyboard=float(input.down['D']||input.down[VK_RIGHT])-float(input.down['A']||input.down[VK_LEFT]);
        const auto pad=input.connected?original::controllerHostControls(input.pad.Gamepad.sThumbLX,
            input.pad.Gamepad.sThumbLY,input.pad.Gamepad.bRightTrigger,input.pad.Gamepad.bLeftTrigger,
            controllerResponse,steeringDeadzone):original::OriginalHostControls{};
        const float analog=pad.steering;
        const bool useStick=std::abs(analog)>.01f;
        float target=useStick?std::clamp(analog,-1.f,1.f):keyboard;
        if(originalHandling&&input.connected&&(useStick||keyboard==0))steering=target;
        else if(!useStick)steering=advanceHostKeyboardSteering(steering,target,physicsDt);
        else steering+=std::clamp(target-steering,-12.f*physicsDt,12.f*physicsDt);
        // Called once per physics tick. The race-start input snapshot observes
        // the neutral filter without advancing a tick before driving begins.
        d.steer=steeringSmoothing.advance(steering,advanceSmoothing?physicsDt:0.f);
        d.throttle=std::max(float(input.down['W']||input.down[VK_UP]),pad.throttle);
        d.brake=std::max(float(input.down['S']||input.down[VK_DOWN]||input.down[VK_SPACE]),pad.brake);
        d.shiftUp=input.down['E']||(input.pad.Gamepad.wButtons&XINPUT_GAMEPAD_B);d.shiftDown=input.down['Q']||(input.pad.Gamepad.wButtons&XINPUT_GAMEPAD_X);return d;
    }
    template<class F> void advanceHostClock(double elapsed,F&& step){
        if(multiplayerDisconnected()){clock.reset();return;}
        if(!multiplayer.active){clock.advance(elapsed,step);return;}
        // Keep the original60Hz solver intact. Online background frames can
        // represent up to250ms, rather than silently losing time at100ms.
        double remaining=std::clamp(elapsed,0.0,.25);
        while(remaining>0){const double slice=std::min(remaining,.1);clock.advance(slice,step);remaining-=slice;}
    }
    original::OriginalMatrix actorVisualMatrix(const std::array<std::uint32_t,42>& actor)const{
        const auto value=[&](unsigned offset){return std::bit_cast<float>(actor.at(offset/4));};
        auto matrix=original::originalActorMatrix(
            {value(0),value(4)-std::bit_cast<float>(0x3ca3d70au),value(8)},
            {value(24),value(28),value(32)},*courseLightFsca);
        original::rotateOriginalMatrixPhase(matrix,1,0x8000,*courseLightFsca);return matrix;
    }
    bool projectedRearViewActive()const{
        return (battle||multiplayer.active)&&originalHandling&&originalCamera.ready()&&
            (multiplayer.active||drivingView==OriginalDrivingView::Bumper)&&race.phase!=RacePhase::Finished;
    }
    original::OriginalCarLightingSetup carLightingSetup()const{
        // Visual scopes contain two cars for multiplayer, while all physics
        // and finish rules retain numeric Time Attack mode2.
        return {unsigned(courseIndex),(battle||multiplayer.active)?0u:2u,night,wet};
    }
    void updateCarAmbient(bool rival,float advantage,bool priorRivalLights){
        original::updateOriginalCarAmbient(rival?rivalCarLight:playerCarLight,
            {raceCarAmbient,unsigned(courseIndex),wet,night,rival,priorRivalLights,advantage});
    }
    void publishCarLighting(bool rival){
        const auto& actors=presentedSession().publishedActors();
        const auto& actor=rival?renderedRivalActor():actors.player0C8FF388;
        const original::OriginalRacePoint position{std::bit_cast<float>(actor[0]),std::bit_cast<float>(actor[1]),std::bit_cast<float>(actor[2])};
        //034AC2 uses current actor XYZ, with independently retained path words.
        originalPath.project(position,rival?rivalLightCoordinate:playerLightCoordinate);
        original::publishOriginalCarLight(rival?rivalCarLight:playerCarLight,actorVisualMatrix(actor).elements);
    }
    void initializeRaceCarLighting(unsigned condition){
        raceLightSets.reset();
        playerCarLight=original::originalCarLighting();rivalCarLight=original::originalCarLighting();
        raceCarAmbient=original::originalCourseCarAmbient(carLightingSetup());
        // Imported roads have no matching D3 shadow-intensity stream. Their
        // path indices cannot address the presentation donor's Akina table.
        // Keep neutral shadow gain, retaining ambient, spots and headlights.
        carLightGain=importedCourse?original::OriginalCarLightGain{}:
            original::OriginalCarLightGain::load(root,unsigned(courseIndex),night,wet,reverse);
        playerLightCoordinate={importedCourse?importedCourse->rules(reverse).startIndex:
            original::originalRaceRuleRow(original::originalRaceRuleRowIndex(condition,battle?0u:2u)).startIndex,0};
        rivalLightCoordinate=playerLightCoordinate;
        publishCarLighting(false);
        // Initial063CE0 precedes the HUD allocation and initial LightON.
        // The supplied BIOS value atNULL+100 closes this initial gap input.
        const float initialGap=std::bit_cast<float>(0xa05f7480u);
        updateCarAmbient(false,initialGap,false);
        if(rivalVisible){
            publishCarLighting(true);
            updateCarAmbient(true,initialGap,false);
        }
    }
    void composeRaceCarLights(){
        if(!originalHandling||!raceLighting){raceLightSets.reset();return;}
        original::setOriginalCarLightEnabled(playerCarLight,playerProjectedHeadlight.enabled());
        original::setOriginalCarLightEnabled(rivalCarLight,rivalProjectedHeadlight.enabled());
        raceLightSets=original::composeOriginalRaceLighting(*raceLighting,playerCarLight,rivalCarLight,carLightingSetup());
    }
    void advanceCarLightGain(){
        if(!originalHandling||night||!raceLightSets)return;
        //069020 follows the current pose/ambient callbacks. Startup's sixty
        //solver/pose warmups do not run this owner; night retains prior gain.
        playerCarLight.gain=carLightGain.evaluate(playerLightCoordinate.index,playerLightCoordinate.fraction);
        if(rivalVisible)rivalCarLight.gain=carLightGain.evaluate(rivalLightCoordinate.index,rivalLightCoordinate.fraction);
        raceLightSets=original::composeOriginalRaceLighting(*raceLighting,playerCarLight,rivalCarLight,carLightingSetup());
    }
    void initializeProjectedHeadlights(){
        if(!originalHandling)return;
        if(!playerProjectedHeadlight.loaded()){
            playerProjectedHeadlight.load(root);rivalProjectedHeadlight.load(root);
            projectedHeadlightTextures=NativeTextureBank::load(root/"data/original_assets/headlight_projection/textures/textures.idastex");
        }
        playerProjectedHeadlight.reset();rivalProjectedHeadlight.reset();
        rivalLightState={};projectedLightPriorAdvantage=0;
        //061C08 performs the initial car pose before night-only063D20/LightON.
        //Bind the initial query, but do not invent a projection during setup.
        const auto& actor=presentedSession().actor();
        playerProjectedHeadlight.request(night,presentedSession().collision(),playerBody.query(),{actor.f(0),actor.f(4),actor.f(8)});
        if(rivalVisible){const auto& p=renderedRivalActor();
            //The initial enemy29 tail requests On before HUD allocation.
            //Source address64 reads Naomi2 BIOS wordA05F7480, inside the
            //dwell interval. This initial publication contributes one tick.
            const bool initialRivalLights=multiplayer.active?(multiplayer.remote.flags&Idas3MpHeadlights)!=0:night||(battleProfile.u(0)==0&&battleProfile.u(24)==29);
            if(!multiplayer.active&&battleProfile.u(0)==0&&battleProfile.u(24)==29)rivalLightState.frames1732=1;
            rivalProjectedHeadlight.request(initialRivalLights,presentedSession().collision(),rivalBody.query(),
                {std::bit_cast<float>(p[0]),std::bit_cast<float>(p[1]),std::bit_cast<float>(p[2])});}
        composeRaceCarLights();
    }
    void publishProjectedHeadlights(float priorAdvantage,std::int32_t accumulatedProgress){
        if(!originalHandling)return;
        //034840 publishes last frame's working records.034E60 then draws
        //that immutable model and updates working records using current pose.
        const auto& actors=presentedSession().publishedActors();
        publishCarLighting(false);
        playerProjectedHeadlight.publish();
        updateCarAmbient(false,priorAdvantage,false);
        if(rivalVisible){
            publishCarLighting(true);
            rivalProjectedHeadlight.publish();
            // Source ambient/gain consumes prior byte81 before the light request.
            updateCarAmbient(true,priorAdvantage,rivalProjectedHeadlight.enabled());
            //0638C0 changes outer light81 after034840 publication. This
            //includes enemy29's original temporary headlight shutoff.
            const auto request=multiplayer.active
                ? ((multiplayer.remote.flags&Idas3MpHeadlights)?original::OriginalRivalLightRequest::On:original::OriginalRivalLightRequest::Off)
                : original::advanceOriginalRivalLightRequest(rivalLightState,
                    {battle?0u:2u,battleProfile.u(0),battleProfile.u(24),actors.secondary0C8FF430[80/4],
                     priorAdvantage,accumulatedProgress});
            if(request!=original::OriginalRivalLightRequest::Hold){const auto& p=renderedRivalActor();
                rivalProjectedHeadlight.request(request==original::OriginalRivalLightRequest::On,
                    presentedSession().collision(),rivalBody.query(),
                    {std::bit_cast<float>(p[0]),std::bit_cast<float>(p[1]),std::bit_cast<float>(p[2])});
            }
        }
        composeRaceCarLights();
    }
    void advanceProjectedHeadlights(){
        if(!originalHandling)return;
        publishProjectedHeadlights(projectedLightPriorAdvantage,originalRace.state().progress.index);
        advanceCarLightGain();
        const auto& actors=presentedSession().publishedActors();
        playerProjectedHeadlight.advance(presentedSession().collision(),actorVisualMatrix(actors.player0C8FF388).elements);
        if(rivalVisible){const auto matrix=actorVisualMatrix(renderedRivalActor());
            rivalProjectedHeadlight.advance(presentedSession().collision(),matrix.elements);
            //The rear callback draws the same published model, then updates
            //the rival's working query state again. Render never advances it.
            if(projectedRearViewActive())rivalProjectedHeadlight.advance(presentedSession().collision(),matrix.elements);
        }
    }
    void appendProjectedHeadlights(Mesh& mesh){
        const auto append=[&](const OriginalHeadlightPresentation& light,unsigned viewMask){
            if(!light.enabled())return;
            const auto begin=mesh.ranges.size();
            mesh.originalWorldChunk(light.model(),0,projectedHeadlightTextureBase);
            for(auto i=begin;i<mesh.ranges.size();++i){mesh.ranges[i].emissive=true;mesh.ranges[i].viewMask=viewMask;}
        };
        append(playerProjectedHeadlight,1);if(rivalVisible)append(rivalProjectedHeadlight,3);
    }
    void advanceCourseLighting(){
        if(!raceLighting||!originalHandling||!presentedSession().ready())return;
        //05FE1A..46 supplies the prior ACar visual matrix to course+80.
        //034840 builds it from the actor, before this frame's solver, using
        // source FSCA angles and its final visual half-turn.
        const auto& actor=presentedSession().actor();
        auto matrix=original::originalActorMatrix(
            {actor.f(0),actor.f(4)-std::bit_cast<float>(0x3ca3d70au),actor.f(8)},
            {actor.f(24),actor.f(28),actor.f(32)},*courseLightFsca);
        original::rotateOriginalMatrixPhase(matrix,1,0x8000,*courseLightFsca);
        const auto point=originalCourseLightReference(course.points,courseLightPathIndex);
        original::updateOriginalCourseLighting(*raceLighting,matrix.elements,{point.x,point.y,point.z});
        //03A2E0 copies the existing race history index after light update.
        //099460 alone handles reversal; Course::load already orients points.
        //The current GO/rules projection occurs later in simulate().
        courseLightPathIndex=originalCoordinate.index;
    }
    void simulate(const DriverInput& d){
        // Full Tune owns the result children without a race or physics actor.
        if(fullTuneActive)return;
        if(race.phase==RacePhase::Finished)publishLocalReplay();
        if(multiplayer.active&&(multiplayer.waiting||multiplayer.disconnected))return;
        // The original primary draw continues animating while its course is
        // visible behind the finish banner. Mirrors/repaints never tick it.
        if(race.phase==RacePhase::Countdown||race.phase==RacePhase::Running||race.phase==RacePhase::Finished)courseAnimation.advance();
        // The race-end announcement runs on every update once the race is
        // over, ahead of the guard that returns while only racing.
        if(race.phase==RacePhase::Finished&&!finishBannerDone){
            ++finishBannerTicks;
            // The outcome stays up for its own minimum and then for as long
            // as the win, loss or time-up stream keeps playing. A silent device
            // reports the stream as over at once, so the minimum is what
            // keeps the wording readable there.
            if(!battle&&!multiplayer.active&&!race.timeUp){
                // Show a genuine record immediately after FINISH, but retain
                // its audio through the record panel. Without a record, keep
                // FINISH visible until the stream ends instead of an empty HUD.
                const bool hasRecord=resultsReady&&(results.recordFlags&OriginalResultsState::newRecord);
                finishBannerDone=finishBannerTicks>=finishBannerSwap&&
                    (hasRecord||finishAudioSkipped||audio.raceMusicFinished());
            }else{
                const bool announcementOver=finishBannerTicks>=finishBannerSwap+finishBannerOutcome&&
                    (finishAudioSkipped||audio.raceMusicFinished());
                if(announcementOver&&++finishFadeTicks>=15)finishBannerDone=true;
            }
        }
        if(race.phase==RacePhase::Countdown||race.phase==RacePhase::Running){
            //05FE66 updates the course before countdown/rules/solver. The
            // sixty solver-only startup warmups never call this owner.
            advanceCourseLighting();
            if(courseCrows)courseCrows->advance();
            carPresentation.advanceOriginalFrame(originalHandling?playerProjectedHeadlight.enabled():night);
            if(rivalVisible&&(!multiplayer.active||authorityRace))rivalPresentation.advanceOriginalFrame(originalHandling?rivalProjectedHeadlight.enabled():night);
        }
        if(authorityLink){simulateAuthority(d);if(!authorityStalled&&(race.phase==RacePhase::Running||race.phase==RacePhase::Finished))captureReplayFrame();return;}
        previous=vehicle;
        previousPitch=bodyPitch;previousRoll=bodyRoll;
        previousWheelPose=wheelPose;
        if(race.phase==RacePhase::Countdown&&!originalHandling){
            race.tick(0);
            return;
        }
        if(race.phase==RacePhase::Finished&&originalHandling){
            originalSession.setRaceAutomaticBrake(true);
            const original::OriginalHostControls braking{d.steer,0,1,false,false};
            auto inputs=original::adaptOriginalHostInput(originalInput,braking,d.automatic,false,0);
            inputs.suppressRawThrottle0C2F4BC8=1;
            const auto effects=originalSession.tick(inputs);
            if(effects.invalidScalarDiagnostics)throw std::runtime_error("Post-race braking produced invalid physics");
            projectOriginalPose(true);advanceOriginalCamera();projectRivalPose(true);advanceProjectedHeadlights();
            originalSession.finishSoundFrame([&](auto& seed){audio.finishSoundFrame(seed);});
            return;
        }
        if(race.phase!=RacePhase::Running&&race.phase!=RacePhase::Countdown)return;
        original::OriginalRaceEvents event;
        if(originalHandling){
            const auto startFrame=originalRaceStart.step();++originalRaceOwnerFrame;
            projectedLightPriorAdvantage=battle?battleMetrics.advantage(originalRace.state().progress,originalRace.state().rivalProgress):0.f;
            race.originalStartDigit=startFrame.countdownDigit;race.originalStartElapsed=240-startFrame.countdownRemaining;
            race.countdown=int(startFrame.countdownRemaining>60?startFrame.countdownRemaining-60:0);
            if(startFrame.cue2)audio.playRaceCue(4,2);
            if(startFrame.go){originalSession.enableRaceStart(originalRaceStart.mode());originalRace.start();race.phase=RacePhase::Running;}
            if(startFrame.cue3)audio.playRaceCue(4,3);
            //05C040 updates projection, checkpoints, outcome and timers on
            // the prior published pose BEFORE this frame's062DE0 solver.
            if(startFrame.runRules){
                const original::OriginalRacePoint position{vehicle.position.x,vehicle.position.y,vehicle.position.z};
                originalPath.project(position,originalCoordinate);
                if(battle){originalPath.project({rivalVehicle.position.x,rivalVehicle.position.y,rivalVehicle.position.z},rivalCoordinate);event=originalRace.tickBattle(originalCoordinate,position,rivalCoordinate,originalSession.stoppedForRace());}
                else event=originalRace.tick(originalCoordinate,position,originalSession.stoppedForRace());
                originalSession.setRaceAutomaticBrake(originalRace.state().automaticBrake);
            }
            original::OriginalHostControls controls{d.steer,d.throttle,d.brake,d.shiftDown,d.shiftUp};
            const bool justFinished=event.finished||event.timeUp;
            if(justFinished){controls={d.steer,0,1,false,false};originalSession.setRaceAutomaticBrake(true);}
            auto inputs=original::adaptOriginalHostInput(originalInput,controls,d.automatic,startFrame.gearEnabled&&!justFinished,0);
            if(justFinished)inputs.suppressRawThrottle0C2F4BC8=1;
            //062DE0 feeds06A420's accumulated path-distance difference to
            // the local battle solver; solo modes receive exact +0. Local
            // race initialization leaves the linked-race override at0.
            const auto& timing=originalRace.state();
            originalSession.setProgressCorrection(battle?battleMetrics.advantage(timing.progress,timing.rivalProgress):0.f,0);
            const auto effects=originalSession.tick(inputs);
            if(!battle&&!multiplayer.active&&startFrame.runRules){
                const auto recordIndex=importedCourse?timeAttackTelemetry.recordImportedProgress(timing.progress.index,originalRace.displayedElapsed()):timeAttackTelemetry.recordProgress(unsigned(courseIndex),unsigned(reverse),timing.progress.index,
                    original::originalTimeAttackRecordingLength(unsigned(courseIndex),unsigned(reverse)),originalRace.displayedElapsed());
                timeAttackTelemetry.recordDrivingPoint({vehicle.position.x,vehicle.position.y,vehicle.position.z},recordIndex,d.throttle,d.brake);
                timeAttackTelemetry.record(effects.newImpactRecords,recordIndex);
                timeAttackTelemetry.recordDitches(presentedSession().contactCompletion(),recordIndex);
                if(event.finished||event.timeUp){
                    const auto& sourceVehicle=presentedSession().vehicle();
                    timeAttackSnapshot=timeAttackTelemetry.snapshot(unsigned(courseIndex),sourceVehicle.drive,sourceVehicle.tail,
                        presentedSession().contactCompletion(),0,!importedCourse);
                }
            }
            audio.requestOriginalTire(effects.vehicle.feedback);
            if(effects.invalidScalarDiagnostics)throw std::runtime_error("Original contact reported invalid numerical state");
            for(const auto cue:effects.feedback142460)audio.playRaceCue(2,cue);
            if(effects.completion.requestCue4)audio.playRaceCue(2,4);
            projectOriginalPose(true);
            advanceOriginalCamera();
            projectRivalPose(true);
            advanceProjectedHeadlights();
            if(!std::isfinite(vehicle.position.x)||!std::isfinite(vehicle.position.y)||!std::isfinite(vehicle.position.z)||!std::isfinite(vehicle.yaw)||!std::isfinite(vehicle.speed))
                throw std::runtime_error("Original player simulation produced non-finite state");
            if(!startFrame.runRules){
                originalSession.finishSoundFrame([&](auto& seed){audio.finishSoundFrame(seed);});
                return;
            }
        }else{
            auto projection=course.project(vehicle.position,segment);segment=projection.segment;
            auto& sample=projection.sample;RoadContact road;road.centre=sample.center;road.heading=std::atan2(sample.tangent.x,sample.tangent.z);road.curvature=sample.curvature;road.grade=sample.grade;road.halfWidth=sample.width*.5f;road.lateralOffset=projection.lateral;road.surfaceGrip=wet?.78f:1.f;
            auto legacyInput=d;legacyInput.steer=-d.steer;
            step(vehicle,legacyInput,config,road);
        }
        auto next=projectRacePosition(vehicle.position);segment=next.segment;progress=next.sample.distance;
        if(!originalHandling)vehicle.position.y=next.sample.center.y;
        if(originalHandling){
            const auto& originalState=originalRace.state();
            ++race.ticks;race.elapsed6000=originalRace.displayedElapsed();race.remaining6000=std::bit_cast<std::int32_t>(originalState.remaining.value);
            race.progress=float(originalState.progress.index)+originalState.progress.fraction;race.furthest=std::max(race.furthest,race.progress);
            race.sector=int(originalState.times.sectionCount);race.sectionTimes6000=originalState.times.sectionTimes;
            for(std::size_t i=0;i<4;++i)race.splits[i]=race.sectionTimes6000[i]/100;
            if(event.finished||event.timeUp){race.phase=RacePhase::Finished;race.timeUp=event.timeUp;}
            settleBattleResult(event.finished);
            if(event.timeExtension)raceFeedback.extend(race.remaining6000-event.secondsAdded*6000);
            if(raceFeedback.tick(race.remaining6000))audio.playRaceCue(4,1);
        }else race.tick(progress-trackStart);
        if(originalHandling)originalSession.finishSoundFrame([&](auto& seed){audio.finishSoundFrame(seed);});
        // Optional replay files are separate from multiplayer progression/saves.
        if(multiplayer.active&&!archiveThisRace)return;
        captureReplayFrame();
        if(multiplayer.active)return; // Never award multiplayer parts/points or write a driver save.
        if(originalHandling&&!battle)timeAttackTrace.recordFrame(vehicle.speed*3.6f,vehicle.travel,d.throttle,d.brake,vehicle.wallContact,race.elapsed6000);
        if(race.originalTiming&&!battle&&race.phase==RacePhase::Finished&&!race.timeUp&&!resultsReady){
            resultsReady=true;results.totalTicks6000=race.elapsed6000;results.remainingTicks6000=race.remaining6000;
            results.sectionTimes6000=race.sectionTimes6000;results.sectionCount=unsigned(race.sector);results.sectionCapacity=race.sectionCapacity;
            if((!results.bestTimes6000[0])||original::originalModelRecordImproved(results.bestTimes6000[0],race.elapsed6000))results.recordFlags|=OriginalResultsState::courseRecord;
            if((!results.bestTimes6000[1])||original::originalModelRecordImproved(results.bestTimes6000[1],race.elapsed6000))results.recordFlags|=OriginalResultsState::modelRecord;
            if(original::originalPersonalRecordImproved(results.bestTimes6000[2],race.elapsed6000))results.recordFlags|=OriginalResultsState::personalBest;
            if(results.recordFlags)results.recordFlags|=OriginalResultsState::newRecord;
            // Stable local insertion ranks ties after earlier rows. Capture
            // before persistence: a top-ten run need not be a new best.
            const auto& rankingRecords=displayedTimeAttackRecords().entries();
            const auto earlier=std::count_if(rankingRecords.begin(),rankingRecords.end(),[&](const auto& e){
                return e.condition==results.condition&&e.weather==unsigned(wet)&&e.ticks6000<=race.elapsed6000;});
            timeAttackCourseRankingQualified=earlier<10;
        }
        if(race.originalTiming&&!battle&&race.phase==RacePhase::Finished&&!pendingResultSetup&&!resultVisit.initialized){
            // The source AResultTA producer reads the pre-insertion clocks.
            // In particular, a first personal record is different from a tie.
            // Queue on the actual finish/time-up event: later Finished ticks
            // return before this block. Rendering waits for the announcement
            // before entering the result owner, as it does for battles.
            timeAttackPoints=original::calculateOriginalTimeAttackPoints(battleProfile,
                {race.timeUp?2:0,race.elapsed6000,!results.bestTimes6000[0]?UINT32_MAX:results.bestTimes6000[0],!results.bestTimes6000[1]?UINT32_MAX:results.bestTimes6000[1],results.bestTimes6000[2]});
            battleResults.resultStatus=race.timeUp?2u:0u;battleResults.profileMode=1;
            battleResults.totalTicks6000=race.elapsed6000;battleResults.sectionTimes6000=race.sectionTimes6000;
            battleResults.sectionCount=unsigned(race.sector);battleResults.sectionCapacity=race.sectionCapacity;
            completeOriginalResultSections(battleResults);
            battleResults.circuitLayout=courseIndex==0||courseIndex==1;
            battleResults.points={timeAttackPoints.participation,timeAttackPoints.finish,timeAttackPoints.recordBonus,timeAttackPoints.total,battleProfile.u(72)};
            pendingResultSetup=original::OriginalBattleResultAnimationSetup{timeAttackPoints.total,timeAttackPoints.balanceBeforeCap,false};
        }
        if(!originalHandling&&vehicle.slip>.18f&&vehicle.speed>7&&race.ticks%3==0){auto r=right(vehicle.yaw)*.68f;skids.push_back({vehicle.position-r+Vec3{0,.035f,0},vehicle.position+r+Vec3{0,.035f,0}});if(skids.size()>2000)skids.erase(skids.begin(),skids.begin()+500);}
        if(race.phase==RacePhase::Finished&&!finishedSaved&&!validationMode){
            finishedSaved=true;
            if(resultsReady&&!battle&&!multiplayer.active&&!race.timeUp&&race.originalTiming){
                // Only a newly completed production Time Attack can produce
                // an upload. Historical CSV rows and saves are never scraped.
                std::ostringstream out;out<<"{\"condition\":"<<results.condition<<",\"weather\":"<<unsigned(wet)<<",\"car\":"<<results.carId<<",\"ticks6000\":"<<race.elapsed6000<<",\"nameGlyphs\":[";
                for(unsigned i=0;i<5;++i){if(i)out<<',';out<<battleProfile.u(44+4*i);}out<<"],\"splits\":[";
                // The original race stores intermediate crossings separately
                // from its finish. Reuse the completed result owner's splits.
                for(unsigned i=0;i<4;++i){if(i)out<<',';out<<battleResults.sectionTimes6000[i];}
                out<<"],\"manual\":"<<unsigned(!automatic)<<",\"night\":"<<unsigned(night)<<",\"points\":"<<std::min(battleProfile.u(72),999999u)<<'}';
                sharedFinishReplay=recording.sharedBytes(race.elapsed6000);sharedFinishJson=out.str();
            }
            if(race.originalTiming&&!race.timeUp)recording.finishTicks6000=race.elapsed6000;
            if(resultsReady){TimeAttackEntry entry{results.condition,unsigned(wet),results.carId,race.elapsed6000};for(unsigned i=0;i<5;++i)entry.nameGlyphs[i]=std::uint8_t(battleProfile.u(44+4*i));entry.manual=!automatic;entry.night=night;if(importedCourse)std::copy_n(race.sectionTimes6000.begin(),3,entry.intermediate6000.begin());records.record(entry);if(!records.save(userdataRoot()/"time_attack_records_v1.csv"))status("Could not save Time Attack records");}
            if(!recording.save((userdataRoot()/"last_run.csv").string()))status("Could not save complete run telemetry");
            if(!battle&&!race.timeUp&&(bestTime<=0||race.seconds()<bestTime)&&!recording.truncated){if(recording.save(ghostPath().string())){best=recording;bestTime=race.seconds();status("Best run saved with replay telemetry");}else status("Could not save best-run replay");}
        }
    }
    void captureReplayFrame(){
        if(diagnosticCaptureOff||replayPlaybackActive||archivePublished||(multiplayer.active&&!archiveThisRace))return;
        const auto replayTick=multiplayer.active?std::uint64_t(recording.frames.size()+1):race.ticks;
        ReplayDetail detail;
        detail.rpm=vehicle.rpm;detail.bodyPosition=playerBodyWorld;detail.pitch=bodyPitch;detail.roll=bodyRoll;detail.steering=wheelPose.steeringRadians;
        detail.suspension=wheelPose.suspensionY;detail.rotation=wheelPose.rotationRadians;detail.throttle=vehicle.throttle;detail.brake=vehicle.brake;
        detail.elapsed=race.elapsed6000;detail.remaining=raceFeedback.displayedRemaining;detail.sections=race.sectionTimes6000;detail.sector=unsigned(race.sector);detail.capacity=race.sectionCapacity;
        const auto& lamps=carPresentation.headlightState();detail.lightCounter=lamps.counter;detail.lightMaximum=lamps.maximumPhase;detail.lightPhase=lamps.phase;detail.lightVisible=unsigned(lamps.visible);detail.lightFraction=lamps.fraction;
        detail.lights=unsigned(playerProjectedHeadlight.enabled());detail.progress=race.progress;detail.extension=raceFeedback.extensionTicks;
        recording.record(replayTick,vehicle.position,vehicle.yaw,vehicle.speed,vehicle.gear,originalHandling?&detail:nullptr);
        if(archiveThisRace&&archiveMode!=0&&rivalVisible){
            archiveRivalCar=loadedRivalCar;
            ReplayDetail other=detail;other.rpm=rivalVehicle.rpm;other.bodyPosition=rivalBodyWorld;
            other.pitch=rivalPitch;other.roll=rivalRoll;other.steering=rivalWheels.steeringRadians;
            other.suspension=rivalWheels.suspensionY;other.rotation=rivalWheels.rotationRadians;
            other.throttle=rivalVehicle.throttle;other.brake=rivalVehicle.brake;
            if(authorityRace){const auto& remote=authorityRace->rules(1-multiplayer.config.localSlot);const auto& state=remote.state();other.elapsed=remote.displayedElapsed();other.remaining=std::bit_cast<std::int32_t>(state.remaining.value);other.sections=state.times.sectionTimes;other.sector=state.times.sectionCount;other.extension=0;}
            const auto& lamp=rivalPresentation.headlightState();other.lightCounter=lamp.counter;other.lightMaximum=lamp.maximumPhase;other.lightPhase=lamp.phase;other.lightVisible=unsigned(lamp.visible);other.lightFraction=lamp.fraction;
            other.lights=unsigned(multiplayer.active?(multiplayer.remote.flags&Idas3MpHeadlights)!=0:rivalProjectedHeadlight.enabled());
            // In an opponent stream the progress slot preserves the displayed
            // battle advantage, rather than reconstructing it from transforms.
            if(multiplayer.active){const auto p=multiplayer.remote.progress;other.progress=multiplayer.received?battleMetrics.distance(originalRace.state().progress)-battleMetrics.distance({std::int32_t(std::floor(p)),p-std::floor(p)}):0.f;}
            else {other.progress=battleProgressApplied?settledBattleAdvantage:battleMetrics.advantage(originalRace.state().progress,originalRace.state().rivalProgress);other.brake=(renderedRivalActor()[92/4]&1)?1.f:0.f;}
            rivalRecording.record(replayTick,rivalVehicle.position,rivalVehicle.yaw,rivalVehicle.speed,rivalVehicle.gear,&other);
        }
        if(archiveThisRace&&race.phase==RacePhase::Finished)publishLocalReplay();
    }
    void publishLocalReplay(){
        if(archivePublished||!archiveThisRace||recording.frames.size()<2||validationMode)return;
        archivePublished=true;
        const auto duration=archiveMode==0&&!race.timeUp?race.elapsed6000:unsigned(recording.frames.back().tick*100);
        auto bytes=recording.sharedBytes(duration);if(bytes.empty())return;
        auto other=archiveMode!=0?rivalRecording.sharedBytes(duration):std::vector<std::uint8_t>{};
        if(archiveMode!=0&&(other.empty()||rivalRecording.frames.size()!=recording.frames.size())){status("Replay unavailable: incomplete opponent recording");return;}
        std::ostringstream meta;meta<<"{\"mode\":"<<archiveMode<<",\"opponentCar\":"<<archiveRivalCar<<",\"opponentEnemy\":"<<archiveRivalEnemy<<",\"condition\":"<<((archiveMode==2?courseIndex:frontend.course)*2+unsigned(reverse))<<",\"weather\":"<<unsigned(wet)<<",\"night\":"<<unsigned(night)<<",\"car\":"<<frontend.car<<",\"manual\":"<<unsigned(!automatic)<<",\"ticks6000\":"<<duration<<",\"outcome\":"<<(race.phase!=RacePhase::Finished?2:race.timeUp?1:0)<<",\"nameGlyphs\":[";
        for(unsigned i=0;i<5;++i){if(i)meta<<',';meta<<recording.profile[2+i];}meta<<"]";
        if(archiveMode==1)meta<<",\"playerName\":"<<std::quoted(multiplayer.localName)<<",\"opponentName\":"<<std::quoted(multiplayer.remoteName);
        else {meta<<",\"playerName\":"<<std::quoted(vsBanner.profileDisplayName(battleProfile));if(archiveMode==2)meta<<",\"opponentName\":"<<std::quoted(OriginalVsBanner::rivalDisplayName(unsigned(archiveRivalEnemy)));}
        meta<<",\"opponentManual\":"<<unsigned(!archiveOpponentAutomatic)<<",\"opponentTelemetry\":"<<unsigned(bool(authorityRace));
        meta<<'}';
        localReplayJson=meta.str();localReplayPlayer=std::move(bytes);localReplayRival=std::move(other);
    }
    void saveResultProfile(){
        if(multiplayer.active||validationMode)return;
        const auto car=unsigned(frontend.car);const auto saved=profiles.load(car);
        // A result event can also finish the owner. Do not rewrite an already
        // saved state or replace the previous distinct recovery snapshot.
        if(saved.origin==LocalDriverProfiles::Origin::Saved&&saved.profile.words==battleProfile.words){
            pendingProfiles.at(car).reset();return;
        }
        pendingProfiles.at(car)=battleProfile;flushProfiles();
    }
    void beginResultVisit(const original::OriginalBattleResultAnimationSetup& setup){
        //06FA2C resets graphics before the result owner initializes.
        raceFog=original::originalBootstrapFog();
        renderer.courseFog=nullptr;renderer.courseLighting=nullptr;renderer.playerLighting=nullptr;renderer.rivalLighting=nullptr;
        if(!tuningTables)tuningTables=original::OriginalTuningData::load(root);
        original::OriginalResultTuningVisitBegin begun;
        const auto begin=[&](auto& seed){begun=original::beginOriginalResultTuningVisit(resultVisit,battleProfile,*tuningTables,seed,setup,fullTuneActive?&fullTuneOffers:nullptr);};
        // A menu tuning visit can precede the first race; it has no driving
        // session whose RNG can be borrowed. Ordinary results retain theirs.
        if(fullTuneActive)begin(fullTuneRandomSeed);else originalSession.withSharedRandom(begin);
        resultAnimationFrame={};resultAnimationFrame.displayedBalance=resultVisit.animation.displayedBalance;resultAnimationFrame.fadeAlpha=255;
        if(fullTuneActive){resultAnimationFrame.fadeAlpha=0;resultAnimationFrame.showUpgradeChild=resultVisit.animation.showUpgradeChild;}
        battleResults.points[4]=resultVisit.animation.displayedBalance;
        if(resultVisit.tuning.kind!=original::OriginalTuningChildKind::none){
            if(!tuningPresentation)tuningPresentation=std::make_unique<OriginalTuningPresentation>(root);
            tuningPresentation->begin(resultVisit.child,*tuningTables);
        }
        tuningPreview=std::make_unique<original::OriginalTuningPreviewPresentation>();
        tuningPreview->load(root,battleProfile);tuningTexturesLoaded=false;
        frontend.battleProfile=battleProfile;
        if(begun.profileChanged)saveResultProfile();
        audio.beginResultMusic(begun.requestResultSoundSet);
    }
    bool canFullTune()const{
        if(fullTuneActive||multiplayer.active||loadingActive||preRaceDialogueActive||legendVisitActive||extraModeVisitActive())return false;
        // Returning to the frontend can retain the completed result owner or
        // an unfinished Full Tune car selection. Neither owns an ordinary
        // menu anymore; beginFullTune safely restarts the save/car selection.
        if(menu)return true;
        return !fullTuneSelecting&&!resultVisit.initialized&&race.phase!=RacePhase::Finished;
    }
    void beginFullTune(){
        if(!canFullTune())throw std::logic_error("Leave online play and finish the current screen before using Full Tune");
        flushProfiles();returnToCourseSelection(true);
        fullTuneSelecting=true;frontend.stage=FrontendStage::SaveSelect;
        frontend.saveSelected=activeSaveSlot>=0?activeSaveSlot:0;
        saveFilesShown=false;browsedSaveSlot=-2;menuTexturesLoaded=false;
    }
    void beginSelectedFullTune(){
        if(!fullTuneSelecting||activeSaveSlot<0||frontend.stage!=FrontendStage::Mode||
            original::originalDriverSetupRequested(frontend.battleProfile))
            throw std::logic_error("Full Tune requires a selected, configured save");
        fullTuneSelecting=false;
        // A file can contain several per-car profiles. Remember the car just
        // chosen, while keeping the other cars' profiles in the same file.
        if(!validationMode)saveSlots.adopt(unsigned(activeSaveSlot),frontend.battleProfile);
        // Reuse the interruption cleanup, without awarding a fictitious race.
        returnToCourseSelection(true);battleProfile=frontend.battleProfile;
        fullTuneActive=true;fullTuneOffers.clear();battleProfile.setu(72,999999);frontend.battleProfile=battleProfile;
        saveResultProfile();menu=false;paused=false;race.phase=RacePhase::Finished;
        battle=bunta=false;finishBannerDone=timeSummaryDone=timeAttackLectureDone=true;
        battleResults={};battleResultsClock.reset();resultConfirmPending=false;resultSelectionAxis=.5f;
        beginResultVisit({0,999999,false});
        if(resultVisit.tuning.kind==original::OriginalTuningChildKind::none)finishFullTune();
    }
    void finishFullTune(){
        saveResultProfile();fullTuneActive=false;fullTuneOffers.clear();
        returnToCourseSelection(true);frontend.stage=FrontendStage::Mode;menuTexturesLoaded=false;
    }
    void advanceResultVisit(){
        if(resultAnimationFrame.finished)return;
        const auto frame=original::advanceOriginalResultTuningVisit(resultVisit,battleProfile,*tuningTables,{resultConfirmPending,resultSelectionAxis});
        resultConfirmPending=false;resultAnimationFrame=frame.result;++battleResults.frame60;
        battleResults.points[4]=resultAnimationFrame.displayedBalance;
        battleResults.balanceHighlighted=resultAnimationFrame.highlightBalance;battleResults.balanceVisible=resultAnimationFrame.balanceVisible;
        if(frame.childAdvanced&&tuningPresentation)tuningPresentation->consume(frame.child);
        if(tuningPreview)tuningPreview->consume(frame.previewEvents,battleProfile,*tuningTables,resultVisit.tuning.kind);
        for(const auto cue:frame.cueIds){
            if(cue==14)audio.playMenuCue(OriginalMenuCue::UpgradeNotice);
            else audio.playTuningCue(cue);
        }
        audio.tickResultMusic();
        // Preview commands5/6 never alter the saved appearance. Persist the
        // original award/purchase/candidate mutation once at its owner event;
        // the per-frame optional countdown is saved only when the visit ends.
        frontend.battleProfile=battleProfile;
        if(frame.mutation.profileChanged)saveResultProfile();
    }
    void scenery(Mesh& mesh,float at){
        if(importedCourse)return;
        if(hasOriginalScenery){
            const auto routeIndex=course.sample(at).segmentIndex;
            auto sourceIndex=routeIndex;
            if(course.reversed)sourceIndex=course.points.size()-1-sourceIndex;
            Vec3 objectReference=course.points.at(routeIndex);
            if(!menu&&originalHandling){
                // The source course owner receives a driving-cell index once
                // per fixed tick. Its reverse sector cell is N-2-i, whereas
                // 099460 samples the reverse point N-1-i for spatial culling.
                const int period=int(course.points.size()-1);
                const int ownerIndex=(courseLightPathIndex%period+period)%period;
                sourceIndex=std::size_t(course.reversed?period-1-ownerIndex:ownerIndex);
                objectReference=originalCourseLightReference(course.points,courseLightPathIndex);
            }
            const auto& assembly=catalogScenery?courseScene.assemblyForPathIndex(sourceIndex):originalCourseSectors.at(akinaSectorForPathIndex(sourceIndex));
            std::vector<NativeAssemblyInsertion> insertions;
            if(courseBillboards)insertions=courseBillboards->insertionsForPathIndex(sourceIndex,assembly.instances.size());
            if(courseObjects){
                auto trees=courseObjects->insertionsForPathIndex(sourceIndex,objectReference,assembly.instances.size());
                insertions.insert(insertions.end(),std::make_move_iterator(trees.begin()),std::make_move_iterator(trees.end()));
            }
            const unsigned oilOpponent=originalHandling&&presentedSession().ready()?presentedSession().parameters().road.mode0C9015E0:
                bunta?31u:battle?battleProfile.u(24):0u;
            if(auto oil=originalCourseOilInsertion(assembly,unsigned(courseIndex),night,wet,oilOpponent))
                insertions.insert(insertions.begin(),std::move(*oil));
            if(courseAnimation.active()&&courseLightFsca){
                for(std::size_t i=0;i<assembly.instances.size();++i)if(assembly.instances[i].chunk==courseAnimation.chunk()){
                    NativeAssemblyInsertion replacement;replacement.before=i;replacement.replaceCount=1;
                    replacement.assembly.instances.push_back(assembly.instances[i]);courseAnimation.apply(replacement.assembly,*courseLightFsca);
                    insertions.push_back(std::move(replacement));
                }
            }
            std::stable_sort(insertions.begin(),insertions.end(),[](const auto& a,const auto& b){return a.before==b.before?a.replaceCount<b.replaceCount:a.before<b.before;});
            courseMeshCache.appendWithInsertions(mesh,originalCourseModel,assembly,insertions);
            return;
        }
        if(at<80){ // Development staging apron, outside the authored route.
            auto s=course.sample(0);Vec3 f=normalized(s.tangent),r=normalized(s.right-s.left);
            mesh.quad(s.left-f*40,s.left,s.right,s.right-f*40,wet?Color{.13f,.16f,.18f}:Color{.20f,.21f,.22f});
            mesh.quad(s.left-r*17-f*40,s.left-r*17,s.left,s.left-f*40,{.19f,.23f,.15f});
            mesh.quad(s.right,s.right+r*17,s.right+r*17-f*40,s.right-f*40,{.19f,.23f,.15f});
        }
        const float begin=std::max(0.f,std::floor((at-65)/4)*4),end=std::min(course.length,at+525);
        for(float d=begin;d<end;d+=4){
            auto a=course.sample(d),b=course.sample(std::min(d+4,course.length));Vec3 up{0,.025f,0};
            auto ra=normalized(Vec3{a.tangent.z,0,-a.tangent.x}),rb=normalized(Vec3{b.tangent.z,0,-b.tangent.x});
            const Color asphalt=wet?Color{.13f,.16f,.18f}:Color{.20f,.21f,.22f};
            mesh.quad(a.left,b.left,b.right,a.right,asphalt);
            mesh.quad(a.left+up,a.left+ra*.12f+up,b.left+rb*.12f+up,b.left+up,{.75f,.75f,.67f});
            mesh.quad(a.right+up,b.right+up,b.right-rb*.12f+up,a.right-ra*.12f+up,{.75f,.75f,.67f});
            if(int(d/4)%3!=0)mesh.quad(a.center-ra*.075f+up,b.center-rb*.075f+up,b.center+rb*.075f+up,a.center+ra*.075f+up,{.94f,.64f,.16f});
            for(float side:{-1.f,1.f}){
                Vec3 ea=side<0?a.left:a.right,eb=side<0?b.left:b.right;
                mesh.quad(ea,eb,eb+rb*(side*17)+Vec3{0,-.5f,0},ea+ra*(side*17)+Vec3{0,-.5f,0},{.19f,.23f,.15f});
                if(side<0)mesh.quad(ea+ra*(side*17)+Vec3{0,-.5f,0},eb+rb*(side*17)+Vec3{0,-.5f,0},eb+rb*(side*63)+Vec3{0,36,0},ea+ra*(side*63)+Vec3{0,36,0},{.18f,.23f,.20f});
                mesh.quad(ea+ra*(side*.45f)+Vec3{0,.59f,0},eb+rb*(side*.45f)+Vec3{0,.59f,0},eb+rb*(side*.45f)+Vec3{0,.84f,0},ea+ra*(side*.45f)+Vec3{0,.84f,0},{.56f,.59f,.57f});
                if(int(d/4)%2==0)mesh.box(ea+ra*(side*.45f)+Vec3{0,.42f,0},{.1f,.88f,.1f},0,{.44f,.46f,.44f});
                if(int(d/4)%5==0){unsigned seed=unsigned(d)*1664525u+1013904223u;float offset=3.f+float((seed>>12)%50)*.1f;mesh.tree(ea+ra*(side*offset),6.f+float(seed%30)*.15f);}
            }
        }
        // Start/finish markings use authoring endpoints until original gates are decoded.
        for(float gate:{trackStart,course.length-1.f})if(std::abs(gate-at)<500){auto a=course.sample(gate),b=course.sample(std::min(course.length,gate+1.2f));for(int stripe=0;stripe<16;stripe++){float t=stripe/16.f,u=(stripe+1)/16.f;mesh.quad(lerp(a.left,a.right,t)+Vec3{0,.04f,0},lerp(b.left,b.right,t)+Vec3{0,.04f,0},lerp(b.left,b.right,u)+Vec3{0,.04f,0},lerp(a.left,a.right,u)+Vec3{0,.04f,0},stripe%2?Color{.94f,.94f,.87f}:Color{.06f,.065f,.06f});}}
        for(std::size_t i=1;i<skids.size();i++)if(length(skids[i].first-vehicle.position)<180&&length(skids[i].first-skids[i-1].first)<8){for(bool rightWheel:{false,true}){Vec3 a=rightWheel?skids[i-1].second:skids[i-1].first,b=rightWheel?skids[i].second:skids[i].first;Vec3 r=normalized(cross(b-a,{0,1,0}))*.08f;mesh.quad(a-r,b-r,b+r,a+r,{.07f,.075f,.076f});}}
    }
    // The file screen is a flat canvas with one hole in it: the panel on the
    // right shows the highlighted file's own car, drawn through that aperture
    // so it is the live model with that file's parts, not a stored picture.
    bool renderSaveSelect(double dt=0){
        const auto& file=frontend.saveFiles[std::size_t(std::clamp(frontend.saveSelected,0,4))];
        renderer.screenFadeArgb=frontend.screenFadeArgb();
        renderer.cameraUp={0,1,0};renderer.nearClip=1.f;renderer.farClip=5000;
        renderer.overrideClearColor=true;renderer.clearColor={0,0,0,1};
        renderer.vehicleLights=false;renderer.opponentLights=false;renderer.courseLampPositions.clear();
        hud.resize(renderer.width,renderer.height);
        Mesh mesh;
        if(file.used){
            showroomStage=frontend.stage;
            menuClock.advance(dt,[&]{showroom.advanceTicks();});
            frontend.liveCarPreview=true;
            loadSelectedCar();
            if(!showroomDetailsLoaded){
                showroomShadow=NativeModel::load(root/"data/original_models/showroom/selcrs.idasmesh");
                showroomFsca=original::OriginalFscaTable::load(root/"data/original_physics/fsca_table.bin");
                showroomDetailsLoaded=true;
            }
            if(!menuTexturesLoaded){
                if(!renderer.loadTextures(originalTextures)||!renderer.loadTextures(numberPlate.textures,true))return false;
                menuTexturesLoaded=true;texturesPending=true;
            }
            const auto pose=showroom.frame(unsigned(frontend.car));
            mesh.originalCar(originalModel,carPresentation.pose({},false,false),pose.carPosition,pose.carYaw,0.f,0.f,0,{},true);
            mesh.originalCar(numberPlate.model,carPresentation.profilePlateAssembly(),pose.carPosition,pose.carYaw,0.f,0.f,
                std::uint32_t(originalTextures.size()),{},true);
            frontend.saveFileCarLive=true;
            // SAVE_PANEL is the box the canvas leaves clear for it.
            renderer.sceneViewport={float(savePanelBox[0]),float(savePanelBox[1]),
                                    float(savePanelBox[2]),float(savePanelBox[3])};
            const float aspect=float(savePanelBox[2])/float(savePanelBox[3]);
            renderer.projectionAspect=aspect;
            renderer.verticalFieldOfView=pose.verticalFieldOfView;
            // The showroom sets its car off to one side to leave room for that
            // screen's captions. The panel is a small box, so it looks straight
            // at the car and closes in until the body fills the frame.
            const Vec3 focus{pose.carPosition.x,pose.carPosition.y+.28f,pose.carPosition.z};
            Vec3 back{pose.eye.x-focus.x,pose.eye.y-focus.y,pose.eye.z-focus.z};
            const float span=std::sqrt(back.x*back.x+back.y*back.y+back.z*back.z);
            const float reach=2.62f/(std::tan(pose.verticalFieldOfView*.5f)*aspect);
            back={back.x/span*reach,back.y/span*reach,back.z/span*reach};
            const Vec3 eye{focus.x+back.x,focus.y+back.y,focus.z+back.z};
            const auto& canvas=frontend.paint(renderer.width,renderer.height);
            const bool drawn=renderer.draw(mesh,eye,focus,false,false,canvas.data(),true,&showroomLighting);
            renderer.sceneViewport={};renderer.projectionAspect=0;
            return drawn;
        }
        frontend.saveFileCarLive=false;showroomStage=frontend.stage;
        renderer.sceneViewport={};renderer.projectionAspect=0;renderer.verticalFieldOfView=.95f;
        return renderer.draw(mesh,{0,0,0},{0,0,-1},false,false,frontend.paint(renderer.width,renderer.height).data());
    }
    // One frame of the pre-race loading screen. The first frame is presented
    // before the course is loaded, so the painting is already on screen while
    // the load blocks.
    bool renderLoading(double dt){
        loadingSeconds+=std::clamp(dt,0.,.25);
        // Keep the completed load behind its artwork, fade to black, then
        // hold full black for two seconds before revealing the race.
        if(loadingStarted&&loadingSeconds+1e-9>=loadingMinimumSeconds+loadingFadeSeconds+loadingBlackSeconds){
            loadingActive=false;beginVsBanner();
            return render(0);
        }
        if(renderer.width>0&&renderer.height>0){
            loadingPixels.assign(std::size_t(renderer.width)*renderer.height,0xff000000u);
            unityUiClear(loadingPixels.data(),renderer.width,renderer.height,0xff000000u);
            loadingScreen.paint(loadingPixels,renderer.width,renderer.height);
        }
        renderer.nearClip=1.f;renderer.farClip=5000;renderer.fitOriginalViewport=false;
        renderer.overrideClearColor=true;renderer.clearColor={0,0,0,1};
        renderer.vehicleLights=false;renderer.opponentLights=false;renderer.courseLampPositions.clear();
        renderer.courseFog=nullptr;renderer.courseLighting=nullptr;
        renderer.playerLighting=nullptr;renderer.rivalLighting=nullptr;
        renderer.screenFadeArgb=std::uint32_t(std::lround(255.*std::clamp((loadingSeconds-loadingMinimumSeconds)/loadingFadeSeconds,0.,1.)))<<24;
        renderer.sceneViewport={};renderer.projectionAspect=0;
        renderer.verticalFieldOfView=1.f;renderer.cameraUp={0,1,0};
        hud.resize(renderer.width,renderer.height);
        const Mesh empty;
        const bool drawn=renderer.draw(empty,{0,0,0},{0,0,-1},false,false,loadingPixels.data());
        if(!drawn)return false;
        // The load happens after the first frame is on screen.
        if(!loadingStarted){loadingStarted=true;start();}
        return true;
    }
    bool renderMenu(double dt){
        if(!demoPreparationStarted){
            demoPreparationStarted=true;
            // Load source scene/cars and evaluate recorded path/body queries
            // while the original logo/caution owners are on screen.
            demoPreparation=std::async(std::launch::async,[directory=root]{
                auto result=std::make_unique<OriginalDemoPresentation>();
                const auto data=original::OriginalDemoData::load(directory);
                result->load(directory,data);return result;
            });
        }
        const auto previousColor=frontend.selectedColor();
        const auto previousStage=frontend.stage;
        frontend.advance(dt);
        // Keep the normal make/car confirmation animation. Established cars
        // can then bypass setup; fresh cars retain transmission/package/name.
        if(fullTuneSelecting&&previousStage==FrontendStage::Car&&frontend.stage==FrontendStage::Transmission&&
            !original::originalDriverSetupRequested(frontend.battleProfile))frontend.stage=FrontendStage::Mode;
        renderer.courseFog=nullptr;renderer.courseLighting=nullptr;renderer.playerLighting=nullptr;renderer.rivalLighting=nullptr;
        if(frontend.stage!=FrontendStage::TuningCourse)tuningCoursePreviewActive=false;
        if(frontend.stage!=FrontendStage::Car&&frontend.stage!=FrontendStage::Transmission&&frontend.stage!=FrontendStage::Name)driverEntryPreviewActive=false;
        for(const auto cue:frontend.takeMenuCueIds())audio.playOriginalMenuCue(cue);
        if(frontend.stage==FrontendStage::SaveSelect&&!validationMode&&!saveFilesShown){
            refreshSaveFiles();saveFilesShown=true;
        }
        if(frontend.stage!=FrontendStage::SaveSelect){
            saveFilesShown=false;browsedSaveSlot=-2;
            // Browsing opened a file only to show its car. Leaving without
            // choosing must not leave that file open, or play time would be
            // banked against a file the player never picked.
            if(browsingSaveFiles){browsingSaveFiles=false;useSaveSlot(-1);}
        }
        if(frontend.stage==FrontendStage::SaveSelect&&!validationMode&&
           browsedSaveSlot!=frontend.saveSelected){
            browsedSaveSlot=frontend.saveSelected;
            browsingSaveFiles=true;
            const auto& browsed=saveSlots.at(unsigned(frontend.saveSelected));
            if(browsed.used){
                // The parts and paint live in that file's own directory, so the
                // panel has to read from it rather than from whatever was open.
                useSaveSlot(frontend.saveSelected);
                frontend.car=int(browsed.car);frontend.make=originalCarMake(browsed.car);
                loadSelectedProfile();
            }else useSaveSlot(-1);
        }
        if(frontend.takeSaveFileChosen()){
            // Choosing a file only rebinds where the per-file stores read from
            // and moves the stage on; nothing is written here, so a diagnostic
            // walks the same path a player does.
            openSaveFile(frontend.saveSelected);
            if(!validationMode)audio.playMenuCue(OriginalMenuCue::Confirm);
        }
        // Time on a file counts while it is open, whatever the player is doing.
        if(activeSaveSlot>=0&&!validationMode&&frontend.stage!=FrontendStage::SaveSelect){
            saveSlotSeconds+=dt;
            if(saveSlotSeconds>=30){
                const auto whole=std::uint64_t(saveSlotSeconds);
                if(saveSlots.addPlayTime(unsigned(activeSaveSlot),whole))saveSlotSeconds-=double(whole);
            }
        }
        const bool profileCommitted=frontend.takeDriverProfileCommit();
        const bool setupCompleted=frontend.takeDriverSetupCompleted();
        if((profileCommitted||setupCompleted)&&!validationMode){
            const auto car=unsigned(frontend.car);pendingProfiles.at(car)=frontend.battleProfile;
            pendingSetupCompletion[car]=pendingSetupCompletion[car]||setupCompleted;flushProfiles();
            if(setupCompleted&&activeSaveSlot>=0)saveSlots.adopt(unsigned(activeSaveSlot),frontend.battleProfile);
        }
        // The ordinary file/car/color/transmission/name/package owners have
        // completed. Enter upgrades here before Choose a Mode accepts input.
        if(fullTuneSelecting&&frontend.stage==FrontendStage::Title)fullTuneSelecting=false;
        if(fullTuneSelecting&&frontend.stage==FrontendStage::Mode){beginSelectedFullTune();return render(0);}
        for(const auto& event:frontend.takeAttractAudioEvents())audio.attract(event.child,event.sourceFrame,event.finished);
        for(const auto& command:frontend.takeSelectionMusicCommands())audio.selection(command);
        renderer.nearClip=1.f;renderer.farClip=5000;renderer.fitOriginalViewport=false;renderer.overrideClearColor=false;
        if(frontend.stage==FrontendStage::Car&&frontend.selectedColor()!=previousColor)audio.playMenuCue(OriginalMenuCue::Change);
        if(frontend.takeStartRequest()){
            courseIndex=Frontend::isImportedCourse(frontend.course)?3:frontend.course;automatic=frontend.automatic;reverse=frontend.reverse;wet=frontend.wet;night=frontend.night;
            // The rival's challenge comes before the load. Continuing to the
            // next opponent already played it, because that path runs through
            // the Legend owner; choosing the race from the menu did not.
            if(beginBuntaVisit(true))return renderBuntaVisit(0);
            if(beginPreRaceDialogue())return renderPreRaceDialogue(0);
            return enterRaceAfterPrompt();
        }
        const unsigned attract=frontend.stage==FrontendStage::Title?frontend.attractChild():~0u;
        const bool changed=attract!=lastAttractRendered;lastAttractRendered=attract;
        if(attract==7){
            if(!demoPresentation)demoPresentation=demoPreparation.get();
            if(changed&&!demoPresentation->upload(renderer))return false;
            texturesPending=true;menuTexturesLoaded=false;
            const auto cursor=frontend.demoCursor();const auto& frame=frontend.demoData().camera(cursor.frame);const auto& m=frame.world;
            const auto cameraOffset=demoPresentation->cameraOffset(cursor.frame);
            const Vec3 eye=Vec3{m[12],m[13],m[14]}+cameraOffset,look=eye-Vec3{m[8],m[9],m[10]};
            renderer.verticalFieldOfView=frame.verticalFovRadians();renderer.cameraUp={m[4],m[5],m[6]};renderer.projectionAspect=0;
            renderer.nearClip=.01f;renderer.farClip=10000;renderer.overrideClearColor=true;renderer.clearColor={0,0,0,1};
            renderer.screenFadeArgb=frontend.screenFadeArgb();
            const auto timeline=frontend.attractFrame()?frontend.attractFrame()-1:0;
            const auto& first=frontend.demoData().actor(cursor.frame,0);const auto& second=frontend.demoData().actor(cursor.frame,1);
            renderer.vehiclePosition={first.f(0),first.f(4),first.f(8)};renderer.vehicleForward=forward(first.f(28)+pi);
            renderer.opponentPosition={second.f(0),second.f(4),second.f(8)};renderer.opponentForward=forward(second.f(28)+pi);
            renderer.vehicleLights=original::OriginalDemoData::lightEffectsOn(timeline,0);
            renderer.opponentLights=original::OriginalDemoData::lightEffectsOn(timeline,1);
            //03F620 creates these four original night-course emitters.
            // They use the current native surface-light falloff at rendering.
            renderer.courseLampPositions={{190,1142,-349},{166,1142,-288},{1782,681,-1999},{1816,681,-1995}};
            const auto& mesh=demoPresentation->mesh(frontend.demoData(),cursor,timeline);
            return renderer.draw(mesh,eye,look,true,false,frontend.paint(renderer.width,renderer.height).data());
        }
        if(attract==12){
            if(!rankingPresentationLoaded){rankingPresentation.load(root);rankingPresentationLoaded=true;}
            const auto& playback=frontend.rankingState();const auto& mesh=rankingPresentation.mesh(playback);
            const auto revision=rankingPresentation.textureRevision();
            if(changed||revision!=rankingTexturesRevision){
                if(!renderer.loadTextures(rankingPresentation.groundTextures())||!renderer.loadTextures(rankingPresentation.carTextures(),true)||!renderer.loadTextures(rankingPresentation.plateTextures(),true)||!renderer.loadTextures(rankingPresentation.environmentTextures(),true))return false;
                rankingTexturesRevision=revision;
            }
            texturesPending=true;menuTexturesLoaded=false;
            const auto& rankingSource=rankingPresentation.paint(renderer.width,renderer.height,playback,frontend.rankingData());
            rankingOverlay=rankingSource;
            unityUiCopy(rankingOverlay.data(),rankingSource.data(),renderer.width,renderer.height);
            frontend.paintAttractPrompts(rankingOverlay,renderer.width,renderer.height);
            renderer.verticalFieldOfView=rankingPresentation.verticalFieldOfView;renderer.projectionAspect=rankingPresentation.aspect;
            renderer.cameraUp={0,1,0};renderer.nearClip=rankingPresentation.nearClip;renderer.farClip=rankingPresentation.farClip;
            renderer.fitOriginalViewport=true;renderer.overrideClearColor=true;renderer.clearColor={0,0,0,1};
            renderer.screenFadeArgb=frontend.screenFadeArgb();
            return renderer.draw(mesh,rankingPresentation.eye,rankingPresentation.target,false,false,rankingOverlay.data(),false,&rankingPresentation.lighting);
        }
        if(frontend.stage==FrontendStage::SaveSelect)return renderSaveSelect(dt);
        const bool nameEntry=frontend.stage==FrontendStage::Name;
        const bool tuningCourseEntry=frontend.stage==FrontendStage::TuningCourse;
        const bool ordinaryDriverEntry=frontend.stage==FrontendStage::Car||frontend.stage==FrontendStage::Transmission||nameEntry;
        const bool showCar=frontend.stage==FrontendStage::Car||frontend.stage==FrontendStage::Transmission||nameEntry||tuningCourseEntry;
        if(showCar&&frontend.stage!=showroomStage){showroom.reset();menuClock.reset();namePaintFrame=~0u;tuningCoursePaintFrame=~0u;}
        if(showCar&&!nameEntry&&!tuningCourseEntry)menuClock.advance(dt,[&]{showroom.advanceTicks();});
        showroomStage=frontend.stage;
        frontend.liveCarPreview=true;
        const auto pose=nameEntry?frontend.nameEntryPresentation().showroomFrame(unsigned(frontend.car)):
            tuningCourseEntry?frontend.tuningCoursePresentation().showroomFrame(unsigned(frontend.car)):
            frontend.stage==FrontendStage::Transmission?showroom.transmissionFrame(unsigned(frontend.car)):showroom.frame(unsigned(frontend.car));
        Mesh mesh;
        if(showCar){
            loadSelectedCar();
            if(tuningCourseEntry)loadTuningCoursePreview();
            else if(ordinaryDriverEntry)loadDriverEntryPreview();
            if(!showroomDetailsLoaded){
                showroomShadow=NativeModel::load(root/"data/original_models/showroom/selcrs.idasmesh");
                showroomFsca=original::OriginalFscaTable::load(root/"data/original_physics/fsca_table.bin");
                showroomDetailsLoaded=true;
            }
            if(!menuTexturesLoaded){if(!renderer.loadTextures(originalTextures)||!renderer.loadTextures(numberPlate.textures,true)||
                ((tuningCourseEntry||ordinaryDriverEntry)&&!renderer.loadTextures(tuningCoursePreviewEnvironment,true)))return false;menuTexturesLoaded=true;texturesPending=true;}
            mesh.originalCar(showroomShadow,OriginalShowroomShadow::assembly(unsigned(frontend.car),pose.carYaw,showroomFsca),{0,0,0},0);
            auto& displayedCar=tuningCourseEntry?tuningCoursePreviewCar:ordinaryDriverEntry?driverEntryPreviewCar:carPresentation;
            const auto& displayedModel=tuningCourseEntry?tuningCoursePreviewModel:ordinaryDriverEntry?driverEntryPreviewModel:originalModel;
            const bool ordinaryCarFaces=true;
            mesh.originalCar(displayedModel,displayedCar.pose({},false,false),pose.carPosition,pose.carYaw,0.f,0.f,0,{},ordinaryCarFaces);
            mesh.originalCar(numberPlate.model,displayedCar.profilePlateAssembly(),pose.carPosition,pose.carYaw,0.f,0.f,std::uint32_t(originalTextures.size()),{},ordinaryCarFaces);
        }
        UiState state;state.menu=true;state.course=&course;state.car=&vehicle;state.race=&race;
        state.frontend=&frontend;state.musicName=audio.musicName();state.message=message;state.gamepad=input.connected;state.showControls=showControls;
        hud.resize(renderer.width,renderer.height);renderer.verticalFieldOfView=pose.verticalFieldOfView;renderer.cameraUp={0,1,0};renderer.projectionAspect=0;
        renderer.screenFadeArgb=frontend.screenFadeArgb();
        if(tuningCourseEntry){
            const auto& entry=frontend.tuningCourseState();const auto& presentation=frontend.tuningCoursePresentation();
            if(tuningCoursePaintWidth!=renderer.width||tuningCoursePaintHeight!=renderer.height||tuningCourseBackground.empty()){
                tuningCoursePaintWidth=renderer.width;tuningCoursePaintHeight=renderer.height;tuningCoursePaintFrame=~0u;
                tuningCourseBackground.assign(std::size_t(renderer.width)*renderer.height,0xff000000u);
                unityUiClear(tuningCourseBackground.data(),renderer.width,renderer.height,0xff000000u);
                presentation.paintBackground(tuningCourseBackground,renderer.width,renderer.height);
            }
            if(tuningCoursePaintFrame!=entry.frame492){
                tuningCourseLayers=presentation.overlays(640,480,entry,frontend.battleProfile.u(1176));
                tuningCoursePaintFrame=entry.frame492;
            }
            std::vector<OverlayPass> passes;passes.reserve(tuningCourseLayers.size());
            for(const auto& layer:tuningCourseLayers)passes.push_back({layer.pixels.data(),layer.additive,true});
            return renderer.draw(mesh,pose.eye,pose.target,false,false,tuningCourseBackground.data(),true,&showroomLighting,nullptr,passes);
        }
        if(nameEntry){
            const auto& state=frontend.nameEntryState();const auto& presentation=frontend.nameEntryPresentation();
            if(namePaintWidth!=renderer.width||namePaintHeight!=renderer.height||nameLayers[0].empty()){
                namePaintWidth=renderer.width;namePaintHeight=renderer.height;namePaintFrame=~0u;
                nameLayers[0].assign(std::size_t(renderer.width)*renderer.height,0xff000000u);
                unityUiClear(nameLayers[0].data(),renderer.width,renderer.height,0xff000000u);
                presentation.paintBackground(nameLayers[0],renderer.width,renderer.height);
            }
            if(namePaintFrame!=state.frame572){
                for(unsigned layer=1;layer<nameLayers.size();++layer){nameLayers[layer].assign(640*480,0);unityUiClear(nameLayers[layer].data(),640,480);}
                presentation.paintNameBacking(nameLayers[1],640,480);
                presentation.paintGlow(nameLayers[2],640,480,state);
                presentation.paint(nameLayers[3],640,480,state,state.sharedCountdown1176);
                presentation.paintCursor(nameLayers[4],640,480,state);
                presentation.paintLegacy(nameLayers[5],640,480,state);namePaintFrame=state.frame572;
            }
            const std::array<OverlayPass,5> layers{{{nameLayers[1].data(),false,true},{nameLayers[2].data(),true,true},
                {nameLayers[3].data(),false,true},{nameLayers[4].data(),true,true},{nameLayers[5].data(),false,true}}};
            return renderer.draw(mesh,pose.eye,pose.target,false,false,nameLayers[0].data(),true,&showroomLighting,nullptr,layers);
        }
        return renderer.draw(mesh,pose.eye,pose.target,false,false,hud.paint(state),showCar,showCar?&showroomLighting:nullptr);
    }
    // One source owner update of the post-result Legend visit.
    void advanceLegendVisit(double dt){
        if(!paused)legendVisitClock.advance(dt,[&]{
            audio.tickLegendStream();
            original::OriginalLegendVisit::Input in;
            in.confirm=legendConfirmPending;in.previous=legendPreviousPending;in.next=legendNextPending;
            in.skip=legendSkipHeld;
            legendConfirmPending=legendPreviousPending=legendNextPending=false;
            legendVisit.advance(battleProfile,in);
            for(const auto& event:legendVisit.takeEvents()){
                audio.applyLegendStreamCommand(event);
                using C=original::OriginalLegendReturnCommand;
                // 141EC0 names the cue this scene plays: the rival's own theme
                // for the next-rival step, an after-race track for the result.
                if(event.command==C::MusicRequest)audio.beginOriginalMusicCue(event.a);
                else if(event.command==C::MusicFade)audio.fadeOriginalMusicCue();
                else if(event.command==C::Cue)audio.playOriginalMenuCue(event.a);
            }
            audio.tickResultMusic();
        });
    }
    bool renderLegendVisit(double dt){
        if(input.key(VK_RETURN)||input.button(XINPUT_GAMEPAD_A))legendConfirmPending=true;
        // The prompt this owner draws says "Steering SELECT", so the wheel and
        // the pad have to move the selection. Confirm already accepted the pad
        // while these two did not, which left a player on a controller able to
        // confirm the default and never reach NO or REFUSE. The stick is read
        // as a level rather than an edge because the owner's selection is a
        // plain set to 0 or 1, so repeating it changes nothing.
        const auto stick=input.connected?input.pad.Gamepad.sThumbLX:0;
        if(input.key(VK_LEFT)||input.key(VK_UP)||input.button(XINPUT_GAMEPAD_DPAD_LEFT)
            ||input.button(XINPUT_GAMEPAD_DPAD_UP)||stick<-16000)legendPreviousPending=true;
        if(input.key(VK_RIGHT)||input.key(VK_DOWN)||input.button(XINPUT_GAMEPAD_DPAD_RIGHT)
            ||input.button(XINPUT_GAMEPAD_DPAD_DOWN)||stick>16000)legendNextPending=true;
        // "PRESS THE START BUTTON TO SKIP" is what this owner draws; the same
        // button therefore skips here rather than pausing.
        legendSkipHeld=input.down[VK_ESCAPE]||(input.pad.Gamepad.wButtons&XINPUT_GAMEPAD_START)!=0;
        advanceLegendVisit(dt);
        // A course load hands straight back to the race the owner selected.
        if(legendVisit.finished()){finishLegendVisit();return menu?renderMenu(0):render(0);}
        const bool conquered=legendVisit.courseClearRunning();
        const int canvasWidth=conquered?640:renderer.width,canvasHeight=conquered?480:renderer.height;
        legendVisitPixels.assign(std::size_t(canvasWidth)*canvasHeight,0xff000000u);
        unityUiClear(legendVisitPixels.data(),canvasWidth,canvasHeight,0xff000000u);
        legendVisit.paint(legendVisitPixels,canvasWidth,canvasHeight);
        renderer.nearClip=1.f;renderer.farClip=5000;renderer.fitOriginalViewport=false;
        renderer.overrideClearColor=true;renderer.clearColor={0,0,0,1};
        renderer.vehicleLights=false;renderer.opponentLights=false;renderer.courseLampPositions.clear();
        renderer.courseFog=nullptr;renderer.courseLighting=nullptr;
        renderer.playerLighting=nullptr;renderer.rivalLighting=nullptr;
        renderer.screenFadeArgb=legendVisit.courseClearRunning()
            ?legendVisit.courseClearFadeArgb():legendVisit.fadeArgb();
        renderer.verticalFieldOfView=1.f;renderer.projectionAspect=0;renderer.cameraUp={0,1,0};
        hud.resize(renderer.width,renderer.height);
        const Mesh empty;
        if(conquered){
            const std::array<OverlayPass,1> layers{{{legendVisitPixels.data(),false,true}}};
            return renderer.draw(empty,{0,0,0},{0,0,-1},false,false,nullptr,false,nullptr,nullptr,layers);
        }
        return renderer.draw(empty,{0,0,0},{0,0,-1},false,false,legendVisitPixels.data(),false,nullptr);
    }
    void beginVsBanner(){
        vsActive=false;
        if(!vsBannerLoaded||validationMode||!courseLightFsca)return;
        const auto condition=std::uint32_t(courseIndex)*2u+(reverse?1u:0u);
        if(condition>=18)return;
        const auto playerGrid=importedCourse?(multiplayer.active?importedCourse->onlineSpawn(reverse,0):importedCourse->spawn(reverse)):original::originalStartPose(condition,(!multiplayer.active&&!battle)?1u:0u);
        const auto rivalGrid=importedCourse?(multiplayer.active?importedCourse->onlineSpawn(reverse,1):playerGrid):original::originalStartPose(condition,1);
        std::array<float,3> midpoint{};
        for(unsigned axis=0;axis<3;++axis)midpoint[axis]=(playerGrid.position[axis]+rivalGrid.position[axis])*.5f;
        startShowcaseCamera.reset(midpoint,playerGrid.angles[1],unsigned(courseIndex),reverse,*courseLightFsca);
        OriginalVsBannerSetup setup;
        setup.course=std::uint32_t(courseIndex);
        setup.compactHeader=true;
        static constexpr unsigned directions[9][2]={{5,4},{5,4},{1,0},{1,0},{2,3},{1,6},{2,3},{2,3},{1,0}};
        setup.drawDirection=true;setup.direction=directions[courseIndex][reverse?1:0];
        if(importedCourse){setup.customCourseName=importedCourse->name;setup.drawDirection=true;setup.direction=reverse?0u:1u;}
        if(battle&&!bunta){
            const auto metadata=original::originalLegendStartMetadata(battleProfile.u(24));
            setup.drawDirection=true;setup.direction=metadata.direction;
            setup.race=metadata.race;setup.extra=metadata.extra;
        }
        // The cars are behind this, so the bank's full-screen chunk stays off.
        setup.drawBackdrop=false;
        setup.night=night;setup.wet=wet;setup.snow=courseIndex==8;
        setup.profile=frontend.battleProfile;setup.enemy=battle?battleProfile.u(24):0u;
        setup.showVersus=battle||multiplayer.active;
        if(multiplayer.active){
            setup.localNameUtf8=multiplayer.localName;setup.opponentNameUtf8=multiplayer.remoteName;
            setup.showBattleRecords=true;
            for(unsigned side=0;side<2;++side){setup.battles[side]=multiplayer.records[side].battles;setup.wins[side]=multiplayer.records[side].wins;}
            // The shared source showcase frames grid0 on the left and grid1
            // on the right. Labels follow those cars on both clients.
            if(multiplayer.config.localSlot==1){
                std::swap(setup.localNameUtf8,setup.opponentNameUtf8);
                std::swap(setup.battles[0],setup.battles[1]);std::swap(setup.wins[0],setup.wins[1]);
            }
        }
        vsBanner.begin(setup);
        vsSeconds=0;vsPhase=1;vsFrame=vsShot=0;vsActive=true;
        clock.reset();
    }
    void advanceStartPresentation(double dt){
        if(!vsActive||paused||dt<=0)return;
        // ARaceStandBy::update (0C05B760): switch shots with the counter at
        // 120, complete when it exceeds239, then increment. Names animate
        // during both shots; there is no separate post-shot name hold.
        vsSeconds+=std::min(dt,.25);
        while(vsActive&&vsSeconds+1e-9>=1.0/60.0){
            vsSeconds-=1.0/60.0;
            const auto sourceFrame=vsBanner.sourceTick();
            vsBanner.tick();
            if(sourceFrame>=240u){
                vsActive=false;vsPhase=0;vsSeconds=0;clock.reset();
                break;
            }
            if(sourceFrame==120u){vsPhase=2;vsFrame=0;vsShot=1;startShowcaseCamera.selectShot(1);}
            ++vsFrame;
            startShowcaseCamera.advance();
        }
    }
    // The challenge the rival makes before the race. Continuing to the next
    // opponent plays it because that path runs through the Legend owner, which
    // configures the same scene; choosing the race from the menu never did.
    //
    // The kind is that owner's own rule (original_legend_return.cpp): a rival
    // you have already beaten challenges you differently from one who has beaten
    // you, and one you have never met differently again. The record byte is the
    // profile's, read the way it reads it.
    void applyModeVisitEvents(const std::vector<original::OriginalLegendReturnEvent>& events){
        using C=original::OriginalLegendReturnCommand;
        for(const auto& event:events){
            if(event.command==C::SoundSet)modeVisitSoundSet=int(event.a);
            else if(event.command==C::MusicRequest)audio.beginOriginalMusicCue(event.a,true,modeVisitSoundSet);
            else if(event.command==C::MusicFade)audio.fadeOriginalMusicCue();
            else if(event.command==C::Cue)audio.playOriginalMenuCue(event.a);
        }
    }
    bool paintModeVisit(std::uint32_t fade){
        renderer.nearClip=1.f;renderer.farClip=5000;renderer.fitOriginalViewport=false;
        renderer.overrideClearColor=true;renderer.clearColor={0,0,0,1};
        renderer.vehicleLights=false;renderer.opponentLights=false;renderer.courseLampPositions.clear();
        renderer.courseFog=nullptr;renderer.courseLighting=nullptr;
        renderer.playerLighting=nullptr;renderer.rivalLighting=nullptr;
        renderer.screenFadeArgb=fade;renderer.verticalFieldOfView=1.f;
        renderer.projectionAspect=0;renderer.cameraUp={0,1,0};hud.resize(renderer.width,renderer.height);
        const Mesh empty;
        return renderer.draw(empty,{0,0,0},{0,0,-1},false,false,modeVisitPixels.data(),false,nullptr);
    }
    bool beginBuntaVisit(bool beforeRace){
        if(multiplayer.active||frontend.gameMode!=original::OriginalGameMode::BuntaChallenge)return false;
        if(!buntaVisit.loaded()){
            if(!original::OriginalBuntaVisit::available(root))throw std::runtime_error("Bunta dialogue assets are missing");
            buntaVisit.load(root);
        }
        if(beforeRace){
            frontend.endSelectionMusic();
            for(const auto& command:frontend.takeSelectionMusicCommands())audio.selection(command);
            battleProfile=frontend.battleProfile;
        }else audio.endResultMusic();
        buntaVisit.begin(battleProfile,{beforeRace,beforeRace?0u:battleResults.resultStatus,unsigned(frontend.car)});
        frontend.battleProfile=battleProfile;
        // Source win Init clamps the temporary level16 sentinel to15. Persist
        // after that Init so a completed course cannot reload at level16.
        if(!beforeRace)saveResultProfile();
        buntaVisitActive=true;paused=false;modeVisitClock.reset();
        applyModeVisitEvents(buntaVisit.takeEvents());
        return true;
    }
    bool renderBuntaVisit(double dt){
        const bool skip=input.down[VK_ESCAPE]||(input.pad.Gamepad.wButtons&XINPUT_GAMEPAD_START)!=0;
        modeVisitClock.advance(dt,[&]{
            buntaVisit.advance(battleProfile,{skip});
            applyModeVisitEvents(buntaVisit.takeEvents());audio.tickResultMusic();
        });
        frontend.battleProfile=battleProfile;
        if(buntaVisit.finished()){
            const bool before=buntaVisit.beforeRace();buntaVisitActive=false;
            if(before)return enterRaceAfterPrompt();
            saveResultProfile();returnToCourseSelection();return renderMenu(0);
        }
        modeVisitPixels.assign(std::size_t(renderer.width)*renderer.height,0xff000000u);
        unityUiClear(modeVisitPixels.data(),renderer.width,renderer.height,0xff000000u);
        buntaVisit.paint(modeVisitPixels,renderer.width,renderer.height);
        return paintModeVisit(buntaVisit.fadeArgb());
    }
    original::OriginalTimeAttackVisit::Setup timeAttackVisitSetup()const{
        original::OriginalTimeAttackVisit::Setup setup;
        setup.condition=results.condition;setup.weather=unsigned(wet);setup.car=unsigned(frontend.car);
        setup.resultStatus=race.timeUp?2u:0u;setup.ticks6000=race.elapsed6000;
        setup.recordFlags=results.recordFlags;setup.courseRankingQualified=timeAttackCourseRankingQualified;setup.oldBestTimes6000=results.bestTimes6000;
        for(unsigned i=0;i<5;++i)setup.nameGlyphs[i]=std::uint8_t(battleProfile.u(44+4*i));
        setup.manual=!automatic;setup.night=night;setup.suppressLecture=(battleProfile.u(1180)&0x20000u)!=0;
        setup.localRecords=displayedTimeAttackRecords().entries();
        // The current completed run appears in its result table while its
        // background upload is pending; it is not inserted into the cache.
        if(!race.timeUp){TimeAttackEntry e{results.condition,unsigned(wet),results.carId,race.elapsed6000};e.nameGlyphs=setup.nameGlyphs;e.manual=!automatic;e.night=night;if(std::none_of(setup.localRecords.begin(),setup.localRecords.end(),[&](const auto& row){return row.condition==e.condition&&row.weather==e.weather&&row.car==e.car&&row.ticks6000==e.ticks6000&&row.nameGlyphs==e.nameGlyphs;}))setup.localRecords.push_back(e);}
        setup.trace=timeAttackTrace;
        setup.sourceAnalysisAvailable=timeAttackAnalysisPrepared;setup.analysis=timeAttackAnalysis;setup.telemetry=timeAttackSnapshot;
        setup.analysisInput=timeAttackAnalysisInput;
        if(importedCourse){setup.customCourseName=importedCourse->name;setup.customMaps=importedCourse->analysisMaps(timeAttackSnapshot,reverse);}
        return setup;
    }
    void prepareTimeAttackAnalysis(){
        if(timeAttackAnalysisPrepared||battle||multiplayer.active||(battleProfile.u(1180)&0x20000u))return;
        if(!timeAttackSnapshot.valid)return;
        auto prior=original::originalPersonalTimeAttackRecord(battleProfile,
            original::originalRecordPartition(unsigned(courseIndex)*2u+unsigned(reverse),wet));
        if(importedCourse){prior.ticks6000=importedPreviousBest.ticks6000;prior.intermediate6000=importedPreviousBest.intermediate6000;
            timeAttackSnapshot.convertedEventCount=0;for(const auto& event:timeAttackSnapshot.wallEvents)if(event.magnitude>original::originalTimeAttackImpactThreshold(3))++timeAttackSnapshot.convertedEventCount;
        }
        auto& input=timeAttackAnalysisInput;
        input={};input.course=unsigned(courseIndex);input.car=unsigned(frontend.car);input.manual=!automatic;
        input.convertedEventCount=timeAttackSnapshot.convertedEventCount;
        input.acceleratorFraction=timeAttackSnapshot.acceleratorFraction;input.brakeFraction=timeAttackSnapshot.brakeFraction;
        input.maxSteeringDelta=timeAttackSnapshot.maxSteeringDelta;input.wallCount=timeAttackSnapshot.wallCount;
        input.ditchCount=timeAttackSnapshot.ditchCount;input.maxGearUsed=timeAttackSnapshot.maxGearUsed;
        original::populateOriginalTimeAttackAnalysisTiming(input.course,originalRace.state().times,race.timeUp?2u:0u,
            prior.ticks6000,prior.intermediate6000,input);
        originalSession.withSharedRandom([&](auto& seed){timeAttackAnalysis=original::analyzeOriginalTimeAttack(input,seed);});
        timeAttackAnalysisPrepared=true;
    }
    bool registerTimeAttackPersonalResult(){
        if(timeAttackPersonalRegistered||multiplayer.active||battle||race.phase!=RacePhase::Finished||race.timeUp)return false;
        timeAttackPersonalRegistered=true;
        const auto& times=originalRace.state().times;
        if(importedCourse){
            if(importedPreviousBest.ticks6000&&race.elapsed6000>=importedPreviousBest.ticks6000)return false;
            TimeAttackEntry entry{results.condition,unsigned(wet),unsigned(frontend.car),race.elapsed6000};
            for(unsigned i=0;i<5;++i)entry.nameGlyphs[i]=std::uint8_t(battleProfile.u(44+i*4));entry.manual=!automatic;entry.night=night;
            std::copy_n(times.sectionTimes.begin(),3,entry.intermediate6000.begin());
            importedPersonalRecords.record(entry);
            personalRecordsDirty=true;
            if(!validationMode){fs::create_directories(importedPersonalPath.parent_path());if(!importedPersonalRecords.save(importedPersonalPath))status("Could not save Hakone personal record");}
            return true;
        }
        const auto count=std::min(times.sectionCount,3u);
        const bool changed=original::registerOriginalPersonalTimeAttackRecord(battleProfile,
            original::originalRecordPartition(unsigned(courseIndex)*2u+unsigned(reverse),wet),race.elapsed6000,unsigned(night),
            {times.sectionTimes.data(),count});
        if(changed){frontend.battleProfile=battleProfile;saveResultProfile();}
        return changed;
    }
    void prepareTimeAttackBackdrop(){
        using Stage=original::OriginalTimeAttackVisit::Stage;
        const auto stage=timeAttackVisit.stage();
        if(stage==timeAttackBackdropStage)return;
        if(stage==Stage::Ranking||stage==Stage::Continue){
            timeAttackRankingPreview=std::make_unique<original::OriginalTuningPreviewPresentation>();
            timeAttackRankingPreview->load(root,battleProfile,stage==Stage::Ranking,stage==Stage::Continue);
            timeAttackRankingTexturesLoaded=false;timeAttackBackdropStage=stage;
        }
    }
    bool beginTimeAttackVisit(bool lecture){
        if(multiplayer.active||battle)return false;
        if(!timeAttackVisit.loaded()){
            if(!original::OriginalTimeAttackVisit::available(root))throw std::runtime_error("Time Attack result assets are missing");
            timeAttackVisit.load(root);
        }
        // Ranking/Continue inherit the common result music. Stopping that
        // owner here would incorrectly replay WIN.bin on the next audio frame.
        if(lecture)audio.endResultMusic();
        if(lecture){prepareTimeAttackAnalysis();timeAttackVisit.beginLecture(timeAttackVisitSetup());}
        else {
            registerTimeAttackPersonalResult();timeAttackVisit.beginAfterResults(timeAttackVisitSetup());
            prepareTimeAttackBackdrop();
        }
        timeAttackVisitActive=true;paused=false;modeVisitClock.reset();
        resultConfirmPending=false;applyModeVisitEvents(timeAttackVisit.takeEvents());return true;
    }
    bool renderTimeAttackVisit(double dt){
        const auto stick=input.connected?input.pad.Gamepad.sThumbLX:0;
        original::OriginalTimeAttackVisit::Input choice{
            input.key(VK_RETURN)||input.button(XINPUT_GAMEPAD_A),
            input.key(VK_LEFT)||input.button(XINPUT_GAMEPAD_DPAD_LEFT)||stick<-16000,
            input.key(VK_RIGHT)||input.button(XINPUT_GAMEPAD_DPAD_RIGHT)||stick>16000,
            input.down[VK_ESCAPE]||(input.pad.Gamepad.wButtons&XINPUT_GAMEPAD_START)!=0,
            input.key('C')||input.button(XINPUT_GAMEPAD_Y)};
        modeVisitClock.advance(dt,[&]{
            if(timeAttackRankingPreview)timeAttackRankingPreview->consume({},battleProfile,*tuningTables,original::OriginalTuningChildKind::none);
            timeAttackVisit.advance(choice);choice.confirm=choice.nextMap=false;
            prepareTimeAttackBackdrop();
            applyModeVisitEvents(timeAttackVisit.takeEvents());audio.tickResultMusic();
        });
        if(timeAttackVisit.finished()){
            const auto route=timeAttackVisit.route();timeAttackVisitActive=false;
            if(route==original::OriginalTimeAttackVisit::Route::Points){timeAttackLectureDone=true;return render(0);}
            returnToCourseSelection();
            if(route==original::OriginalTimeAttackVisit::Route::Exit){frontend.stage=FrontendStage::Title;frontend.advance(0);}
            return renderMenu(0);
        }
        const auto background=timeAttackRankingPreview?0u:0xff000000u;
        modeVisitPixels.assign(std::size_t(renderer.width)*renderer.height,background);
        unityUiClear(modeVisitPixels.data(),renderer.width,renderer.height,background);
        timeAttackVisit.paint(modeVisitPixels,renderer.width,renderer.height);
        if(timeAttackRankingPreview){
            if(!timeAttackRankingTexturesLoaded){
                if(!timeAttackRankingPreview->upload(renderer))return false;
                timeAttackRankingTexturesLoaded=true;texturesPending=true;menuTexturesLoaded=false;
            }
            renderer.screenFadeArgb=timeAttackVisit.fadeArgb();renderer.fitOriginalViewport=true;
            renderer.sceneViewport={0,0,640,480};
            renderer.overrideClearColor=true;renderer.clearColor={0,0,0,1};
            renderer.verticalFieldOfView=timeAttackRankingPreview->verticalFieldOfView;
            renderer.projectionAspect=timeAttackRankingPreview->aspect;renderer.cameraUp={0,1,0};
            renderer.nearClip=timeAttackRankingPreview->nearClip;renderer.farClip=timeAttackRankingPreview->farClip;
            renderer.vehicleLights=false;renderer.opponentLights=false;renderer.courseLampPositions.clear();
            renderer.courseFog=nullptr;renderer.courseLighting=nullptr;renderer.playerLighting=nullptr;renderer.rivalLighting=nullptr;
            return renderer.draw(timeAttackRankingPreview->mesh(),timeAttackRankingPreview->eye,timeAttackRankingPreview->target,
                false,false,modeVisitPixels.data(),false,&timeAttackRankingPreview->lighting);
        }
        return paintModeVisit(timeAttackVisit.fadeArgb());
    }
    bool beginPreRaceDialogue(){
        preRaceDialogueActive=false;
        if(validationMode&&!preRaceDialogueSmoke)return false;
        // start() has not run yet, so battle/bunta/battleProfile still describe
        // the previous race. The frontend is what has been chosen.
        if(frontend.gameMode!=original::OriginalGameMode::LegendOfTheStreets)return false;
        const auto& profile=frontend.battleProfile;
        if(profile.u(0)!=0||profile.u(4)>8||profile.u(24)>30)return false;
        if(!preRaceDialogueLoaded){
            if(!original::OriginalRivalDialogScene::available(root))return false;
            preRaceDialogue.load(root);preRaceDialogueLoaded=true;
        }
        const unsigned enemy=profile.u(24);
        const unsigned record=profile.byte(116+enemy);
        const auto selected=original::originalBattleSelection(profile);
        original::OriginalRivalDialogSetup setup;
        setup.enemy=enemy;
        setup.kind=(record>>4)?14u:(record&15)?7u:0u;
        // start() takes the battle's own weather, not the menu toggles.
        setup.weather=selected.weather!=0?1u:0u;
        setup.playerCar=unsigned(frontend.car);
        // 0F8B00 draws the spectator variant from the shared game RNG, as the
        // Legend path does.
        if(presentedSession().ready())originalSession.withSharedRandom([&](std::uint32_t& seed){
            seed=seed*0x41c64e6du+12345u;
            setup.cheer=(((seed>>16)&0x7fffu)%1000u)%7u;
        });
        setup.playerName=original::originalRivalDialogPlayerName(profile);
        preRaceDialogue.begin(profile,setup);
        // End the menu manager before the rival theme takes its bank slot.
        // The original next-rival owner requests the same enemy + 3 cue.
        frontend.endSelectionMusic();
        for(const auto& command:frontend.takeSelectionMusicCommands())audio.selection(command);
        audio.beginOriginalMusicCue(enemy+3);
        preRaceDialogueActive=true;
        preRaceDialogueClock.reset();
        return true;
    }
    bool renderPreRaceDialogue(double dt){
        // The scene advertises "PRESS THE START BUTTON TO SKIP" and nothing
        // else, so the pages turn themselves once each one has finished; a
        // per-page button would strand the player on the first one.
        // Held Start must survive render frames with no source-clock step and
        // continue through each authored page, like the post-race owner.
        const bool skip=input.down[VK_ESCAPE]||(input.pad.Gamepad.wButtons&XINPUT_GAMEPAD_START)!=0;
        if(!paused)preRaceDialogueClock.advance(dt,[&]{
            preRaceDialogue.step(frontend.battleProfile);
            if(skip)original::skipOriginalRivalDialog(preRaceDialogue.state());
            if(preRaceDialogue.ready()){
                // The Legend owner advances while the next kind is authored and
                // closes when it is not; the same rule ends this one.
                if(original::originalRivalDialogKindAvailable(preRaceDialogue.state().kind+1))
                    original::advanceOriginalRivalDialogPage(preRaceDialogue.state(),
                        preRaceDialogue.data(),frontend.battleProfile,1);
                else{
                    original::closeOriginalRivalDialog(preRaceDialogue.state());
                    audio.fadeOriginalMusicCue();
                }
            }
            audio.tickResultMusic();
        });
        if(preRaceDialogue.closed()){preRaceDialogueActive=false;return enterRaceAfterPrompt();}
        preRaceDialoguePixels.assign(std::size_t(renderer.width)*renderer.height,0xff000000u);
        unityUiClear(preRaceDialoguePixels.data(),renderer.width,renderer.height,0xff000000u);
        preRaceDialogue.paint(preRaceDialoguePixels,renderer.width,renderer.height);
        renderer.nearClip=1.f;renderer.farClip=5000;renderer.fitOriginalViewport=false;
        renderer.overrideClearColor=true;renderer.clearColor={0,0,0,1};
        renderer.vehicleLights=false;renderer.opponentLights=false;renderer.courseLampPositions.clear();
        renderer.courseFog=nullptr;renderer.courseLighting=nullptr;
        renderer.playerLighting=nullptr;renderer.rivalLighting=nullptr;
        renderer.screenFadeArgb=preRaceDialogue.fadeArgb();
        renderer.verticalFieldOfView=1.f;renderer.projectionAspect=0;renderer.cameraUp={0,1,0};
        hud.resize(renderer.width,renderer.height);
        const Mesh empty;
        return renderer.draw(empty,{0,0,0},{0,0,-1},false,false,preRaceDialoguePixels.data(),false,nullptr);
    }
    // What a start request does once anything that precedes the race is done.
    bool enterRaceAfterPrompt(){
        if(loadingScreen.loaded()&&!validationMode){
            loadingActive=true;loadingStarted=false;loadingSeconds=0;
            loadingScreen.begin(loadingSequence++);
            return renderLoading(0);
        }
        start();
        return menu?renderMenu(0):render(0);
    }
    bool render(double dt,bool drawRearView=true){
        renderer.supplementalCourseLampLighting=false;
        if(loadingActive)return renderLoading(dt);
        if(buntaVisitActive)return renderBuntaVisit(dt);
        if(timeAttackVisitActive)return renderTimeAttackVisit(dt);
        if(preRaceDialogueActive)return renderPreRaceDialogue(dt);
        if(menu)return renderMenu(dt);
        if(legendVisitActive)return renderLegendVisit(dt);
        if(!replayPlaybackActive)advanceStartPresentation(dt);
        if(multiplayer.active){multiplayer.auraRanges={};multiplayer.auraSeconds+=std::clamp(dt,0.,.25);}
        // Auras belong to the car showcase. Clear both cars' submissions on
        // its final frame, before the countdown and normal driving begin.
        const bool showMultiplayerAura=multiplayer.active&&!multiplayer.disconnected&&vsActive;
        // Common result Init follows the final driving/sound update. Its
        // candidate selection consumes the same shared RNG at this boundary.
        if(pendingResultSetup&&finishBannerDone&&!paused){
            if(!battle&&!timeAttackLectureDone){
                // Keep the record's reading time and let the finish stream
                // complete before analysis replaces it with result music.
                // Only Start can bypass this; accelerator/confirm cannot.
                if(!timeSummaryDone&&!race.timeUp&&(results.recordFlags&OriginalResultsState::newRecord)){
                    timeSummaryClock.advance(dt,[&]{++timeSummaryTicks;});
                    if(finishAudioSkipped||(timeSummaryTicks>=180&&audio.raceMusicFinished()))timeSummaryDone=true;
                    resultConfirmPending=false;
                }else timeSummaryDone=true;
                if(timeSummaryDone&&beginTimeAttackVisit(true))return renderTimeAttackVisit(0);
            }
            if(battle||timeAttackLectureDone){beginResultVisit(*pendingResultSetup);pendingResultSetup.reset();}
        }
        renderer.nearClip=1.f;renderer.farClip=importedCourse?12000.f:5000.f;renderer.fitOriginalViewport=false;renderer.overrideClearColor=false;renderer.vehicleLights=true;lastAttractRendered=~0u;
        if(resultVisit.initialized&&!paused)battleResultsClock.advance(dt,[&]{advanceResultVisit();});
        if(resultVisit.initialized&&resultAnimationFrame.finished){
            if(fullTuneActive){
                saveResultProfile();beginResultVisit({0,battleProfile.u(72),false});
                if(resultVisit.tuning.kind==original::OriginalTuningChildKind::none)finishFullTune();
                return render(0);
            }
            if(bunta&&beginBuntaVisit(false))return renderBuntaVisit(0);
            if(!battle&&beginTimeAttackVisit(false))return renderTimeAttackVisit(0);
            if(beginLegendVisit())return renderLegendVisit(0);
            returnToCourseSelection();return renderMenu(0);
        }
        renderer.sceneViewport={};
        renderer.screenFadeArgb=resultVisit.initialized?resultAnimationFrame.fadeAlpha<<24:
            race.phase==RacePhase::Finished&&!finishBannerDone?(std::min(finishFadeTicks,15u)*255u/15u)<<24:0;
        if(resultVisit.initialized){
            if(!tuningPreview)throw std::logic_error("Result owner has no car preview");
            if(!tuningTexturesLoaded||loadedTuningTextureRevision!=tuningPreview->textureRevision()){
                if(!tuningPreview->upload(renderer))return false;
                tuningTexturesLoaded=true;loadedTuningTextureRevision=tuningPreview->textureRevision();
                texturesPending=true;menuTexturesLoaded=false;
            }
            std::span<const std::uint32_t> overlay;
            if(resultAnimationFrame.showUpgradeChild&&tuningPresentation){
                tuningPixels.assign(std::size_t(renderer.width)*renderer.height,0u);
                unityUiClear(tuningPixels.data(),renderer.width,renderer.height);
                tuningPresentation->paintOverlay(tuningPixels,renderer.width,renderer.height,
                    resultVisit.child,*tuningTables,battleProfile.u(1176));
                overlay=tuningPixels;
            }
            hud.resize(renderer.width,renderer.height);
            renderer.verticalFieldOfView=tuningPreview->verticalFieldOfView;
            renderer.projectionAspect=tuningPreview->aspect;renderer.cameraUp={0,1,0};
            renderer.nearClip=tuningPreview->nearClip;renderer.farClip=tuningPreview->farClip;
            renderer.fitOriginalViewport=true;renderer.overrideClearColor=true;renderer.clearColor={0,0,0,1};
            renderer.vehicleLights=false;renderer.opponentLights=false;renderer.courseLampPositions.clear();
            return renderer.draw(tuningPreview->mesh(),tuningPreview->eye,tuningPreview->target,false,false,
                hud.paintResult(battleResults,overlay,paused,showControls,managedPauseOverlay),false,&tuningPreview->lighting);
        }
        if(texturesPending){
            if(hasOriginalScenery){if(!renderer.loadTextures(originalCourseTextures))return false;if(!renderer.loadTextures(originalBackgroundTextures,true))return false;}
            if(!renderer.loadTextures(originalTextures,hasOriginalScenery)||!renderer.loadTextures(numberPlate.textures,true))return false;
            if(rivalVisible&&(!renderer.loadTextures(rivalTextures,true)||!renderer.loadTextures(rivalPlate.textures,true)))return false;
            crowTextureBase=(hasOriginalScenery?std::uint32_t(originalCourseTextures.size()+originalBackgroundTextures.size()):0u)+
                std::uint32_t(originalTextures.size()+numberPlate.textures.size())+
                (rivalVisible?std::uint32_t(rivalTextures.size()+rivalPlate.textures.size()):0u);
            if(courseCrows&&!renderer.loadTextures(courseCrows->textures,true))return false;
            projectedHeadlightTextureBase=crowTextureBase+(courseCrows?std::uint32_t(courseCrows->textures.size()):0u);
            if(originalHandling&&!renderer.loadTextures(projectedHeadlightTextures,true))return false;
            rainTextureBase=projectedHeadlightTextureBase+(originalHandling?std::uint32_t(projectedHeadlightTextures.size()):0u);
            if(wet||courseIndex==8){
                if(rainTextures.size()==0)rainTextures=NativeTextureBank::load(root/"data/original_assets/weather/rain/textures/textures.idastex");
                if(!renderer.loadTextures(rainTextures,true))return false;
                if(courseIndex!=8){
                    rainmarkTextureBase=rainTextureBase+std::uint32_t(rainTextures.size());
                    if(rainmarkTextures.size()==0)rainmarkTextures=NativeTextureBank::load(root/"data/original_assets/weather/rainmark/textures/textures.idastex");
                    if(!renderer.loadTextures(rainmarkTextures,true))return false;
                }
            }
            texturesPending=false;menuTexturesLoaded=false;
        }
        const float alpha=clock.alpha();VehicleState drawCar=vehicle;drawCar.position=lerp(previous.position,vehicle.position,alpha);drawCar.yaw=lerpAngle(previous.yaw,vehicle.yaw,alpha);
        if(menu||paused||race.phase==RacePhase::Finished)drawCar=vehicle;
        Vec3 target;
        if(menu){
            const Vec3 desired=drawCar.position-forward(drawCar.yaw)*6.3f+right(drawCar.yaw)*7.2f+Vec3{0,3.1f,0};
            target=drawCar.position+Vec3{0,.72f,0};renderer.verticalFieldOfView=.95f;
            if(!cameraReady){camera=desired;cameraReady=true;}else camera=lerp(camera,desired,float(1-std::exp(-std::min(dt,.1)*9)));
        }else if(replayPlaybackActive){
            const auto heading=forward(drawCar.yaw);
            const auto center=drawCar.position+Vec3{0,1.0f,0};
            if(replayCameraMode==1){camera=center+heading*1.5f;target=camera+heading*30.f;}
            else if(replayCameraMode==2){camera=center-heading*3.f+Vec3{0,24.f,0};target=center;}
            else if(replayCameraMode==3){camera=center-forward(drawCar.yaw+replayOrbit)*8.f+Vec3{0,3.f,0};target=center;}
            else{camera=center-heading*7.f+Vec3{0,2.f,0};target=center+heading*9.f;}
            renderer.cameraUp={0,1,0};renderer.verticalFieldOfView=.95f;renderer.projectionAspect=0;renderer.nearClip=.15f;
        }else if(originalHandling&&originalCamera.ready()){
            const bool bumper=drivingView==OriginalDrivingView::Bumper;
            const auto& frame=bumper?bumperCamera.frame():originalCamera.frame();
            const auto& prior=bumper?previousBumperFrame:previousCameraFrame;
            const float cameraAlpha=paused||race.phase==RacePhase::Finished?1.f:alpha;
            camera=lerp(prior.eye,frame.eye,cameraAlpha);
            target=lerp(prior.target,frame.target,cameraAlpha);
            renderer.cameraUp=normalized(lerp(prior.up,frame.up,cameraAlpha));
            renderer.verticalFieldOfView=frame.verticalFieldOfView;
            // Preserve the original vertical FOV and follow transform while
            // matching the window aspect, so wider views reveal more road
            // instead of stretching cars and course geometry horizontally.
            renderer.projectionAspect=0;
        }else{
            if(drivingView==OriginalDrivingView::Bumper){camera=drawCar.position+Vec3{0,.98f,0}+forward(drawCar.yaw);target=camera+forward(drawCar.yaw);renderer.verticalFieldOfView=OriginalChaseCamera::sourceBumperFieldOfView;}
            else{const auto view=chaseCamera.update(drawCar.position,drawCar.yaw,paused?0:dt);camera=view.eye;target=view.target;renderer.verticalFieldOfView=ChaseCamera::verticalFieldOfView;}
            renderer.cameraUp={0,1,0};renderer.projectionAspect=0;
        }
        if(vsActive){
            const auto& frame=startShowcaseCamera.frame();
            camera={frame.eye[0],frame.eye[1],frame.eye[2]};
            target={frame.target[0],frame.target[1],frame.target[2]};
            renderer.verticalFieldOfView=frame.verticalFieldOfView;
            renderer.cameraUp={frame.up[0],frame.up[1],frame.up[2]};renderer.projectionAspect=0;
            // Imported starts can be much steeper than the D3 showcase grids.
            // Keep the authored camera motion, but retain nearby road geometry
            // when the low shot is less than a metre above an uphill surface.
            if(importedCourse)renderer.nearClip=.1f;
        }
        auto& mesh=raceMesh;mesh.vertices.clear();mesh.ranges.clear();mesh.vertices.reserve(140000);
        if(originalHandling&&!replayPlaybackActive)appendProjectedHeadlights(mesh);
        const auto courseRangeBegin=mesh.ranges.size();
        if(hasOriginalScenery){
            NativeAssembly background;if(catalogScenery)background=courseScene.backgroundAssembly(camera);else background.instances.push_back(originalAkinaBackgroundInstance(camera));
            mesh.originalCar(originalBackgroundModel,background,{0,0,0},0,std::uint32_t(originalCourseTextures.size()));
        }
        scenery(mesh,menu?trackStart:progress);
        if(courseCrows)mesh.originalCar(courseCrows->model,courseCrows->assembly(),{0,0,0},0,crowTextureBase);
        if(hasOriginalScenery)for(auto i=courseRangeBegin;i<mesh.ranges.size();++i)mesh.ranges[i].courseLighting=true;
        if(originalHandling&&!replayPlaybackActive&&!menu&&presentedSession().ready()){
            CarShadowFootprint shadow;shadow.widthScale=1.70f;shadow.lengthScale=2.15f;shadow.opacity=.46f;shadow.surfaceLift=.02f;bool valid=true;
            const auto& road=presentedSession().roadContact();
            for(std::size_t i=0;i<4;++i){const auto& query=road.surfaces0CAA9518[i];
                if(std::int32_t(query.u(60))<0){valid=false;break;}
                shadow.roadPoints[i]=Vec3{query.f(12),query.f(16),query.f(20)}+(drawCar.position-vehicle.position);
            }
            if(valid)appendCarContactShadow(mesh,shadow);
        }
        const auto carTextureBase=hasOriginalScenery?std::uint32_t(originalCourseTextures.size()+originalBackgroundTextures.size()):0u;
        const float poseAlpha=menu||paused||race.phase==RacePhase::Finished?1.f:alpha;
        CarWheelPose drawWheels;
        if(originalHandling&&!menu){
            drawWheels=interpolateCarWheels(previousWheelPose,wheelPose,poseAlpha);
        }else if(!menu){drawWheels.steeringRadians=vehicle.steering*.3f;for(auto& rotation:drawWheels.rotationRadians)rotation=std::fmod(vehicle.travel/.3f,2*pi);}
        const auto& carAssembly=carPresentation.pose(drawWheels,replayPlaybackActive?replayLights:originalHandling&&!menu?playerProjectedHeadlight.enabled():night,!menu&&vehicle.brake>.05f);
        const auto bodyAngles=interpolateCarBodyAngles({previousPitch,previousRoll},{bodyPitch,bodyRoll},poseAlpha);
        const float drawPitch=bodyAngles.pitch,drawRoll=bodyAngles.roll;
        const Vec3 bodyPosition=originalHandling?lerp(previousPlayerBodyWorld,playerBodyWorld,poseAlpha):drawCar.position+Vec3{0,originalCarRideHeight(unsigned(frontend.car)),0};
        if(menu||vsActive||drivingView==OriginalDrivingView::Chase){
        const auto playerRangeBegin=mesh.ranges.size();
        mesh.originalCar(originalModel,carAssembly,bodyPosition,drawCar.yaw,
            drawPitch,drawRoll,carTextureBase,carPresentation.illuminatedChunks(),true);
        mesh.originalCar(numberPlate.model,carPresentation.profilePlateAssembly(),bodyPosition,drawCar.yaw,
            drawPitch,drawRoll,carTextureBase+std::uint32_t(originalTextures.size()),{},true);
        if(originalHandling&&!menu)for(auto i=playerRangeBegin;i<mesh.ranges.size();++i)mesh.ranges[i].carLighting=1;
        if(showMultiplayerAura){
            auto& aura=multiplayerAura[0];
            aura.update(std::uint32_t(std::fmod(multiplayer.auraSeconds*60.,4294967296.)),unsigned(frontend.car),drawCar.position,camera);
            if(aura.visible()){const auto begin=mesh.ranges.size();mesh.originalCar(aura.model(),aura.assembly(),{0,0,0},0);multiplayer.auraRanges[0]=std::uint32_t(mesh.ranges.size()-begin);}
        }
        // The source rear camera sits inside the local car. Its body, plate
        // and aura belong only to the primary view; the peer remains visible
        // in both views. This also keeps chase-view mirrors clear.
        if(multiplayer.active)for(auto i=playerRangeBegin;i<mesh.ranges.size();++i)mesh.ranges[i].viewMask=1;
        }
        if(rivalVisible){
            const auto position=lerp(previousRival.position,rivalVehicle.position,poseAlpha);
            const auto yaw=lerpAngle(previousRival.yaw,rivalVehicle.yaw,poseAlpha);
            const auto angles=interpolateCarBodyAngles({previousRivalPitch,previousRivalRoll},{rivalPitch,rivalRoll},poseAlpha);
            const auto pitch=angles.pitch,roll=angles.roll;
            const auto wheels=interpolateCarWheels(previousRivalWheels,rivalWheels,poseAlpha);
            CarShadowFootprint shadow;shadow.widthScale=1.70f;shadow.lengthScale=2.15f;shadow.opacity=.46f;shadow.surfaceLift=.02f;bool valid=true;
            if(multiplayer.active||replayPlaybackActive)valid=false; // Replays have no live AI road contacts.
            else {
                const auto& road=presentedSession().rivalRoadContact();
                for(unsigned i=0;i<4;++i){const auto& query=road.surfaces0CAA9764[i];if(std::int32_t(query.u(60))<0){valid=false;break;}shadow.roadPoints[i]=Vec3{query.f(12),query.f(16),query.f(20)}+(position-rivalVehicle.position);}
            }
            if(valid)appendCarContactShadow(mesh,shadow);
            const auto base=carTextureBase+std::uint32_t(originalTextures.size()+numberPlate.textures.size());
            const auto body=lerp(previousRivalBodyWorld,rivalBodyWorld,poseAlpha);
            // 034ADC..034B24 copies the original published actor+92 bit0
            // into ACar+80. The rival solver already produces this signal.
            const bool braking=replayPlaybackActive?rivalVehicle.brake>.05f:(renderedRivalActor()[92/4]&1)!=0;
            const auto& assembly=rivalPresentation.pose(wheels,replayPlaybackActive?replayRivalLights:originalHandling?rivalProjectedHeadlight.enabled():night,braking);
            const auto rivalRangeBegin=mesh.ranges.size();
            mesh.originalCar(rivalModel,assembly,body,yaw,pitch,roll,base,rivalPresentation.illuminatedChunks(),true);
            mesh.originalCar(rivalPlate.model,(multiplayer.active||(replayPlaybackActive&&loadedRivalEnemy==-2))?rivalPresentation.profilePlateAssembly():rivalPlate.assembly(),body,yaw,pitch,roll,base+std::uint32_t(rivalTextures.size()),{},true);
            if(originalHandling)for(auto i=rivalRangeBegin;i<mesh.ranges.size();++i)mesh.ranges[i].carLighting=2;
            if(showMultiplayerAura){
                auto& aura=multiplayerAura[1];
                aura.update(std::uint32_t(std::fmod(multiplayer.auraSeconds*60.,4294967296.)),multiplayer.config.remoteCar,position,camera);
                if(aura.visible()){const auto begin=mesh.ranges.size();mesh.originalCar(aura.model(),aura.assembly(),{0,0,0},0);multiplayer.auraRanges[1]=std::uint32_t(mesh.ranges.size()-begin);}
            }
        }
        // Best-run telemetry remains available for records; Time Attack has no ghost car.
        // Snow/rain and tire spray are scene geometry, depth-tested against cars/scenery
        // and drawn before the HUD. Their private clock cannot alter physics.
        const bool snowWeather=courseIndex==8;
        const bool weatherVisible=(wet||snowWeather)&&!menu;
        std::array<WetWeather::Car,2> weatherCars{{
            {drawCar.position,drawCar.yaw,std::abs(drawCar.speed),true},
            {lerp(previousRival.position,rivalVehicle.position,poseAlpha),lerpAngle(previousRival.yaw,rivalVehicle.yaw,poseAlpha),std::abs(rivalVehicle.speed),rivalVisible}}};
        if(weatherVisible&&originalHandling&&!replayPlaybackActive&&presentedSession().ready()){
            const auto bindContacts=[&](WetWeather::Car& car,const auto& contacts,Vec3 offset){
                car.contactsValid=true;
                for(unsigned i=0;i<2;++i){const auto& contact=contacts[i+2];
                    if(std::int32_t(contact.u(60))<0)car.contactsValid=false;
                    car.rearContacts[i]=Vec3{contact.f(12),contact.f(16),contact.f(20)}+offset;
                }
            };
            bindContacts(weatherCars[0],presentedSession().roadContact().surfaces0CAA9518,drawCar.position-vehicle.position);
            if(rivalVisible&&!multiplayer.active&&!replayPlaybackActive)bindContacts(weatherCars[1],presentedSession().rivalRoadContact().surfaces0CAA9764,weatherCars[1].position-rivalVehicle.position);
        }
        wetWeather.advance(dt,weatherVisible,paused&&!multiplayer.active,weatherCars,snowWeather);
        wetWeather.build(camera,target,weatherVisible,performanceRainDetail==1?4u:1u);
        for(unsigned i=0;i<wetWeather.count;++i){
            const auto& q=wetWeather.quads[i];
            // Original rain material: translucent list, source alpha blending.
            // Use explicit vertex tint/alpha and unlit world geometry; never
            // enter the opaque course alpha-depth prepass.
            mesh.beginRange((q.waterTrail?rainmarkTextureBase:rainTextureBase)+q.texture,0x941024d2u,0x8a00072cu,0x93000000u,512,true,true);
            const float tint=night?.62f:.88f;const Color color{tint,tint,tint,q.alpha};
            const auto a=q.center-q.across+q.up,b=q.center+q.across+q.up,
                c=q.center+q.across-q.up,d=q.center-q.across-q.up;
            const Vec3 normal=normalized(camera-q.center);
            mesh.vertices.insert(mesh.vertices.end(),{{a,normal,color,0,0},{b,normal,color,1,0},{c,normal,color,1,1},
                {a,normal,color,0,0},{c,normal,color,1,1},{d,normal,color,0,1}});
            mesh.ranges.back().count+=6;
        }
        UiState state;state.menu=menu;state.paused=paused;state.wet=wet;state.night=night;state.automatic=automatic;state.debug=debug;state.gamepad=input.connected;state.carProfile=profile;state.course=&course;state.car=&vehicle;state.race=&race;state.rival=(battle||multiplayer.active)?&rivalVehicle:nullptr;state.bestTime=bestTime;state.progress=progress;state.fps=renderFps;state.message=message;
        state.suppressPauseOverlay=managedPauseOverlay;state.multiplayer=multiplayer.active;
        state.hudIntroFrame=originalHandling&&!replayPlaybackActive?originalRaceOwnerFrame:240u;
        state.frontend=&frontend;state.originalHandling=originalHandling;state.originalWeatherScenery=!wet||sceneWet;state.snow=courseIndex==8;state.musicName=audio.musicName();hud.resize(renderer.width,renderer.height);
        state.extendedCountdown=bool(importedCourse);state.timeExtended=raceFeedback.extensionTicks!=0;state.useDisplayedRemaining=originalHandling;
        state.rearView=drawRearView&&!vsActive&&(battle||multiplayer.active)&&originalHandling&&originalCamera.ready()&&(multiplayer.active||drivingView==OriginalDrivingView::Bumper)&&race.phase!=RacePhase::Finished;
        state.displayedRemaining6000=replayPlaybackActive?race.remaining6000:raceFeedback.displayedRemaining;
        auto displayRecords=results;
        displayRecords.livePanel=originalHandling&&!battle&&!multiplayer.active&&!replayOpponent&&!menu&&race.phase!=RacePhase::Finished;
        if(displayRecords.livePanel){
            displayRecords.recordFlags=0;
            auto prior=original::originalPersonalTimeAttackRecord(battleProfile,original::originalRecordPartition(unsigned(courseIndex*2+reverse),wet));
            if(importedCourse){prior.ticks6000=importedPreviousBest.ticks6000;prior.intermediate6000=importedPreviousBest.intermediate6000;}
            if(importedCourse&&race.sector>0&&race.sector<=3&&importedCoursePreviousBest.intermediate6000[race.sector-1]){
                displayRecords.differenceAvailable[0]=true;
                displayRecords.differences6000[0]=std::int32_t(race.sectionTimes6000[race.sector-1])-std::int32_t(importedCoursePreviousBest.intermediate6000[race.sector-1]);
            }
            if(race.sector>0&&race.sector<=3&&prior.intermediate6000[race.sector-1]){
                displayRecords.differenceAvailable[1]=true;
                displayRecords.differences6000[1]=std::int32_t(race.sectionTimes6000[race.sector-1])-std::int32_t(prior.intermediate6000[race.sector-1]);
            }
        }
        state.results=(resultsReady||displayRecords.livePanel)?&displayRecords:nullptr;
        if(multiplayerDisconnected())state.finishBanner=OriginalHudState::FinishBanner::finish;
        else if(!multiplayer.active&&race.phase==RacePhase::Finished&&!finishBannerDone)
            state.finishBanner=finishBannerTicks<finishBannerSwap?OriginalHudState::FinishBanner::finish
                :race.timeUp?OriginalHudState::FinishBanner::timeUp
                :battle&&battleResult==original::OriginalLegendResult::Loss?OriginalHudState::FinishBanner::lose
                :battle?OriginalHudState::FinishBanner::win:OriginalHudState::FinishBanner::finish;
        state.battleResults=battleProgressApplied?&battleResults:nullptr;
        state.battle=battle;state.battleWon=battle&&battleResult==original::OriginalLegendResult::Win;
        if(battle){
            state.battleEnemy=battleProfile.u(24);state.battleProfileMode=battleProfile.u(0);state.battleRivalCar=battleProfile.u(20);
            state.battleHudFrame=int(originalRaceOwnerFrame);
            state.battleAdvantage=battleProgressApplied?settledBattleAdvantage:battleMetrics.advantage(originalRace.state().progress,originalRace.state().rivalProgress);
            state.battleRivalPositionFraction=battleMetrics.positionFraction(rivalCoordinate);
        }
        if(multiplayer.active){
            auto& online=state.onlineBattleHud;online.active=true;
            online.playerName=multiplayer.localName;online.rivalName=multiplayer.remoteName;
            online.playerCar=multiplayer.config.localCar;online.rivalCar=multiplayer.config.remoteCar;
            online.frame=int(originalRaceOwnerFrame);
            // Network progress is the accumulated source path coordinate,
            // not metres. The original table preserves uneven cells, reverse
            // courses and complete laps in the displayed signed advantage.
            const auto index=std::int32_t(std::floor(multiplayer.remote.progress));
            const original::OriginalPathCoordinate remote{index,multiplayer.remote.progress-float(index)};
            multiplayer.hudLocalMetres=battleMetrics.distance(originalRace.state().progress);
            multiplayer.hudRemoteMetres=multiplayer.received?battleMetrics.distance(remote):multiplayer.hudLocalMetres;
            online.advantage=multiplayer.hudLocalMetres-multiplayer.hudRemoteMetres;
            // Source validity is separate from accumulated progress. A valid
            // packet before the start line must not turn its gap into dashes.
            online.rivalPositionFraction=multiplayer.received?0.f:-1.f;
        }
        if(replayOpponent){
            state.rival=&rivalVehicle;
            if(loadedRivalEnemy>=0){state.battle=true;state.battleEnemy=unsigned(loadedRivalEnemy);state.battleProfileMode=0;state.battleRivalCar=unsigned(loadedRivalCar);state.battleHudFrame=int(race.ticks);state.battleAdvantage=replayAdvantage;state.battleRivalPositionFraction=0;}
            else {auto& online=state.onlineBattleHud;online.active=true;online.playerName=multiplayer.localName;online.rivalName=multiplayer.remoteName;online.playerCar=unsigned(frontend.car);online.rivalCar=unsigned(loadedRivalCar);online.frame=int(race.ticks);online.advantage=replayAdvantage;online.rivalPositionFraction=0;}
        }
        renderer.vehiclePosition=drawCar.position;const auto carForward=forward(drawCar.yaw);
        renderer.vehicleForward={carForward.x*std::cos(drawPitch),-std::sin(drawPitch),carForward.z*std::cos(drawPitch)};
        renderer.opponentLights=night&&rivalVisible;
        //Source projected geometry now supplies race headlights. Preserve
        //native vehicle/street lighting only for the development/attract path.
        if(originalHandling&&!replayPlaybackActive){renderer.vehicleLights=false;renderer.opponentLights=false;}
        renderer.courseLampPositions.clear();
        if(catalogScenery){const auto lamps=courseScene.lampPositions();renderer.courseLampPositions.assign(lamps.begin(),lamps.end());}
        renderer.supplementalCourseLampLighting=bool(importedCourse)&&night&&!menu;
        if(importedCourse)renderer.courseLampPositions=importedCourse->lamps;
        renderer.opponentPosition=lerp(previousRival.position,rivalVehicle.position,paused?1.f:alpha);
        const auto lightPose=interpolateCarBodyAngles({previousRivalPitch,previousRivalRoll},{rivalPitch,rivalRoll},paused?1.f:alpha);
        const auto rivalForward=forward(lerpAngle(previousRival.yaw,rivalVehicle.yaw,paused?1.f:alpha));
        renderer.opponentForward={rivalForward.x*std::cos(lightPose.pitch),-std::sin(lightPose.pitch),rivalForward.z*std::cos(lightPose.pitch)};
        std::optional<OriginalRearViewFrame> rearView;
        if(state.rearView){
            rearView=rearCameraFrame;
            rearView->eye=lerp(previousRearCameraFrame.eye,rearCameraFrame.eye,poseAlpha);
            rearView->target=lerp(previousRearCameraFrame.target,rearCameraFrame.target,poseAlpha);
            rearView->up=normalized(lerp(previousRearCameraFrame.up,rearCameraFrame.up,poseAlpha));
        }
        renderer.courseFog=&raceFog;
        renderer.courseLighting=raceLightSets?&raceLightSets->course:raceLighting?&*raceLighting:nullptr;
        renderer.playerLighting=raceLightSets?&raceLightSets->player:nullptr;
        renderer.rivalLighting=raceLightSets&&raceLightSets->hasRival?&raceLightSets->rival:nullptr;
        const std::uint32_t* overlay=nullptr;
        if(vsActive&&renderer.width>0&&renderer.height>0){
            vsPixels.assign(std::size_t(renderer.width)*renderer.height,0u);
            unityUiClear(vsPixels.data(),renderer.width,renderer.height);
            vsBanner.paint(vsPixels,renderer.width,renderer.height);
            overlay=vsPixels.data();
        }else overlay=hud.paint(state);
        if(replayPlaybackActive)rearView.reset();
        return renderer.draw(mesh,camera,target,night,wet,overlay,false,nullptr,rearView?&*rearView:nullptr);
    }
};
#if !defined(IDAS3_PORTABLE_SCENE)
App* current=nullptr;
LRESULT CALLBACK windowProc(HWND h,UINT msg,WPARAM w,LPARAM l){
    if(!current)return DefWindowProcW(h,msg,w,l);
    if(msg==WM_CLOSE){current->running=false;return 0;}
    if(msg==WM_ACTIVATEAPP){current->active=w!=0;if(current->multiplayer.active)current->paused=false;else if(!w&&!current->menu){current->paused=true;current->clock.reset();}return 0;}
    if(msg==WM_SIZE){if(w!=SIZE_MINIMIZED){current->pendingWidth=LOWORD(l);current->pendingHeight=HIWORD(l);current->resizePending=true;}return 0;}
    if(msg==WM_GETMINMAXINFO){auto* info=reinterpret_cast<MINMAXINFO*>(l);info->ptMinTrackSize={960,580};return 0;}
    if(msg==WM_LBUTTONDOWN&&current->menu){float x=GET_X_LPARAM(l)*1280.f/std::max(1,current->renderer.width),y=GET_Y_LPARAM(l)*720.f/std::max(1,current->renderer.height);if(current->frontend.stage==FrontendStage::Title&&x>=194&&x<=804&&y>=649&&y<=690)current->mouseStart=true;return 0;}
    return DefWindowProcW(h,msg,w,l);
}
int runFactoryPaintPreview(App& app,const fs::path& output){
    fs::create_directories(output);
    std::ofstream report(output/"factory-paints.csv");report<<"car,color,assembly_instances,material_patches,selected_color,committed_color\n";
    for(unsigned car:{0u,9u,30u,33u,34u}){
        app.menu=true;app.frontend.stage=FrontendStage::Title;app.frontend.advance(0);
        for(int make=0;make<7;++make){const auto roster=Frontend::carsForMake(make);if(std::find(roster.begin(),roster.end(),int(car))!=roster.end())app.frontend.make=make;}
        app.frontend.car=int(car);app.frontend.battleProfile=original::makeOriginalFreshBattleProfile();
        app.frontend.battleProfile.setu(16,car);app.frontend.battleProfile.setu(1180,app.frontend.battleProfile.u(1180)|1u);
        app.frontend.stage=FrontendStage::Car;app.frontend.gameMode=original::OriginalGameMode::TimeAttack;
        app.frontend.advance(8./60.);
        const unsigned count=original::originalCarColorCounts[car];
        for(unsigned color=0;color<count;++color){
            if(color)app.frontend.changeColor(1);
            if(!app.render(1./60))throw std::runtime_error(app.renderer.error);
            if(app.loadedCar!=int(car)||app.loadedColor!=int(color)||app.frontend.selectedColor()!=color)throw std::runtime_error("Factory paint input did not reach the live showroom");
            const auto stem="car-"+std::to_string(car)+"-color-"+std::to_string(color);
            if(!app.renderer.saveBitmap((output/(stem+".bmp")).wstring()))throw std::runtime_error("Factory paint capture failed");
            report<<car<<','<<color<<','<<app.carPresentation.assembly().instances.size()<<','<<app.carPresentation.materialPatchCount()<<','<<app.frontend.selectedColor()<<','<<app.frontend.battleProfile.u(64)<<'\n';
        }
        app.frontend.confirm();app.frontend.advance(164./60.);
        if(app.frontend.stage!=FrontendStage::Transmission||app.frontend.battleProfile.u(64)!=count-1)throw std::runtime_error("Car confirmation did not commit selected paint");
        app.frontend.advance(16./60.);app.frontend.change(1);app.frontend.confirm();app.frontend.advance(142./60.);
        if(app.frontend.stage!=FrontendStage::Mode||app.frontend.battleProfile.u(68)!=1)throw std::runtime_error("Transmission owner failed to commit MT");
        app.frontend.advance(16./60.);app.frontend.change(1);app.frontend.advance(1./60.);
        app.courseIndex=3;app.reverse=false;app.wet=false;app.night=true;app.automatic=false;
        app.start();
        if(app.loadedColor!=int(count-1)||app.presentedSession().selection().physics.vehicleIndex!=car)throw std::runtime_error("Race discarded chosen factory appearance");
        for(unsigned frame=0;frame<300;++frame)app.simulate({});
        if(!app.render(1./60)||!app.renderer.saveBitmap((output/("car-"+std::to_string(car)+"-night-race.bmp")).wstring()))throw std::runtime_error("Factory paint night-race capture failed");
    }
    // The app must carry saved performance fields into the solver, even when
    // quick-run weather differs from the profile's last saved weather.
    for(unsigned car:{0u,9u})for(unsigned upgrade:{0u,5u,63u}){
        app.frontend.car=int(car);app.frontend.make=car==0?6:3;
        auto& p=app.frontend.battleProfile;p=original::makeOriginalFreshBattleProfile();p.setu(16,car);p.setByte(164,std::uint8_t(upgrade));p.setByte(152,0);p.setu(32,0);
        app.wet=true;app.start();
        const auto& selected=app.presentedSession().selection().physics;
        if(selected.vehicleIndex!=car||selected.upgradeIndex0C9015F0!=upgrade||selected.overrideMode0C9015F4!=unsigned(car==0&&upgrade>4)||selected.mode0C9015FC!=1)throw std::runtime_error("Saved profile tuning did not reach original race initialization");
    }
    report<<"PASS: showroom input, appearance commit, MT transition, night race and saved performance binding\n";
    return 0;
}
int runHeadless(App& app){
    app.validationMode=true;app.courseIndex=3;app.wet=false;app.profile=0;app.automatic=true;
    fs::create_directories(app.root/"verification");
    std::ofstream report(app.root/"verification"/"integration_smoke.txt");
    report<<"Native original-player integration smoke (CPU only)\nScripted test driver; not a matched arcade lap.\n";
    int failures=0;
    for(bool reverse:{false,true}){
    app.reverse=reverse;app.start();float steer=0;int wallTicks=0;float maxProgress=app.trackStart;
    for(int i=0;i<60*60;i++){
        auto projection=app.course.project(app.vehicle.position,app.segment);float speed=std::max(0.f,app.vehicle.speed);float lookahead=std::max(10.f,speed*.65f);
        Vec3 target=app.course.sample(projection.sample.distance+lookahead).center-app.vehicle.position;
        float angle=wrapAngle(std::atan2(target.x,target.z)-app.vehicle.yaw);
        float targetSteer=std::clamp(std::atan2(2*app.config.wheelbase*std::sin(angle),lookahead)/recoveredSteeringLimit,-.75f,.75f);
        steer+=std::clamp(targetSteer-steer,-.04f,.04f);
        DriverInput d;d.steer=-steer;d.throttle=speed<18?.7f:.05f;d.brake=speed>20?.3f:0;
        app.simulate(d);wallTicks+=app.vehicle.wallContact;maxProgress=std::max(maxProgress,app.progress);
        if(!std::isfinite(app.vehicle.speed)||!std::isfinite(app.vehicle.position.x)||!std::isfinite(app.vehicle.position.y)||!std::isfinite(app.vehicle.position.z)||!std::isfinite(app.vehicle.yaw))throw std::runtime_error("Non-finite headless simulation");
    }
    app.recording.save((app.root/"verification"/(reverse?"candidate_smoke_reverse.csv":"candidate_smoke.csv")).string());
    const float gained=maxProgress-app.trackStart;
    report<<"Akina dry "<<(reverse?"reverse":"forward")<<", car "<<app.frontend.car<<"\nTicks: "<<app.vehicle.tick<<"\nTravel: "<<app.vehicle.travel<<"\nCourse progress gained: "<<gained<<"\nWall contact ticks: "<<wallTicks<<"\nSpeed km/h: "<<app.vehicle.speedKmh()<<"\n";
    if(!app.originalHandling||gained<100.f||app.vehicle.travel<100.f)++failures;
    }
    return failures?2:0;
}
int runOriginalRouteSmoke(App& app){
    app.validationMode=true;app.wet=false;app.profile=0;app.automatic=true;
    fs::create_directories(app.root/"verification");std::ofstream report(app.root/"verification/original_route_smoke.csv");
    report<<"course,reverse,wet,original_handling,original_scenery,wet_scenery,ticks,travel,race_path_progress,wall_ticks\n";
    for(bool wet:{false,true})for(int index=0;index<9;++index)for(bool reverse:{false,true}){
        if(wet&&index==8)continue;
        app.courseIndex=index;app.reverse=reverse;app.wet=wet;app.start();int walls=0;
        for(int tick=0;tick<900;++tick){
            const auto projection=app.projectRacePosition(app.vehicle.position);
            const float look=std::max(10.f,std::max(0.f,app.vehicle.speed)*.65f);
            const auto target=app.sampleRaceDistance(projection.sample.distance+look).center-app.vehicle.position;
            const float angle=wrapAngle(std::atan2(target.x,target.z)-app.vehicle.yaw);
            DriverInput input;input.automatic=true;input.steer=-std::clamp(std::atan2(2*app.config.wheelbase*std::sin(angle),look)/recoveredSteeringLimit,-.75f,.75f);
            input.throttle=app.vehicle.speed<18?.7f:.05f;input.brake=app.vehicle.speed>20?.3f:0;
            app.simulate(input);walls+=app.vehicle.wallContact;
        }
        report<<app.course.name<<','<<reverse<<','<<app.wet<<','<<app.originalHandling<<','<<app.hasOriginalScenery<<','<<app.sceneWet<<','<<app.vehicle.tick<<','<<app.vehicle.travel<<','<<app.race.progress<<','<<walls<<'\n';report.flush();
        if(index==8&&(app.presentedSession().vehicle().drive.u(0x434)!=1||app.presentedSession().vehicle().drive.u(0x438)!=1))throw std::runtime_error("Original Snow must retain both wet and snow flags");
        if(!app.originalHandling||app.vehicle.travel<5)throw std::runtime_error("Original route launch did not advance: "+app.course.name);
    }
    app.courseIndex=3;app.reverse=false;app.wet=false;app.start();
    for(int tick=0;tick<10000&&app.race.phase!=RacePhase::Finished;++tick)app.simulate({});
    if(!app.race.timeUp||!app.originalSession.stoppedForRace())throw std::runtime_error("Original time-up did not stop and finish the race");
    std::ofstream timeout(app.root/"verification/original_timeout_smoke.txt");
    timeout<<"Natural original time-up; no forced timer/result writes\nElapsed6000: "<<app.race.elapsed6000<<"\nRemaining6000: "<<app.race.remaining6000<<"\nActor stopped: "<<app.originalSession.stoppedForRace()<<"\n";
    return 0;
}
// Drives the source post-result owner from a settled Legend result to the
// destination it hands back, exercising the same App entry points play uses.
void captureLegendVisitFrame(App& app,const fs::path& file,int width,int height){
    // The same buffer fill renderLegendVisit performs before it submits.
    app.legendVisitPixels.assign(std::size_t(width)*height,0xff000000u);
    unityUiClear(app.legendVisitPixels.data(),width,height,0xff000000u);
    app.legendVisit.paint(app.legendVisitPixels,width,height);
    if(const auto alpha=app.legendVisit.fadeArgb()>>24){
        for(auto& pixel:app.legendVisitPixels){
            const auto blend=[&](int shift){
                const auto source=(pixel>>shift)&0xffu;
                return std::uint32_t((source*(255u-alpha))/255u)<<shift;
            };
            pixel=0xff000000u|blend(16)|blend(8)|blend(0);
        }
    }
    std::ofstream out(file,std::ios::binary);
    out<<"P6\n"<<width<<" "<<height<<"\n255\n";
    for(const auto pixel:app.legendVisitPixels){
        const char rgb[3]={char((pixel>>16)&0xff),char((pixel>>8)&0xff),char(pixel&0xff)};
        out.write(rgb,3);
    }
}
// A whole Legend of the Streets run: every battle settled through the source
// progression, every post-result owner played, until the owner reports Ending.
// The race outcomes are supplied - each battle is settled as a win through
// recordOriginalLegendResult, the same call a finished race makes - so this
// establishes the mode's progression, the owner and the ending, not the driving.
// Which conditions each Legend rival is raced under, so a night pairing can be
// selected for presentation work.
int runLegendConditions(App& app){
    app.validationMode=true;
    fs::create_directories(app.root/"verification");
    std::ofstream report(app.root/"verification/legend_conditions.csv");
    report<<"rival,course,direction,weather,night\n";
    for(unsigned enemy=0;enemy<31;++enemy){
        auto profile=original::makeOriginalFreshBattleProfile();
        original::selectOriginalRival(profile,enemy);
        const auto s=original::originalBattleSelection(profile);
        report<<enemy<<','<<s.course<<','<<s.direction<<','<<s.weather<<','<<s.night<<"\n";
    }
    return 0;
}
// Paints the race-end announcement on its own so its artwork and authored
// placement can be compared with the cabinet.
int runFinishBannerSmoke(App& app){
    app.validationMode=true;
    fs::create_directories(app.root/"verification/finish-banner");
    const auto hud=OriginalRaceHud::load(app.root);
    constexpr int width=640,height=480;
    // The announcement must sit on the middle of the output at any shape.
    {
        std::ofstream centres(app.root/"verification/finish_banner_centres.csv");
        centres<<"width,height,banner_centre,frame_centre,offset\n";
        for(const auto [w,h]:std::initializer_list<std::pair<int,int>>{{640,480},{1280,720},{1600,700},{2560,1080},{900,1200}}){
            std::vector<std::uint32_t> shot(std::size_t(w)*h,0xff000000u);
            OriginalHudState state;state.finishBanner=OriginalHudState::FinishBanner::finish;state.edgeAnchored=true;
            hud.paint(std::span<std::uint32_t>(shot),w,h,state);
            int lo=w,hi=-1;
            for(int y=0;y<h;++y)for(int x=0;x<w;++x){
                const auto c=shot[std::size_t(y)*w+x];
                const int r=int((c>>16)&0xff),g=int((c>>8)&0xff),b=int(c&0xff);
                if(r>170&&g>60&&b<90&&r-b>90){lo=std::min(lo,x);hi=std::max(hi,x);}
            }
            if(hi<0){centres<<w<<','<<h<<",none\n";continue;}
            const double centre=(lo+hi)/2.0;
            centres<<w<<','<<h<<','<<centre<<','<<w/2.0<<','<<(centre-w/2.0)<<"\n";
        }
    }
    std::vector<std::uint32_t> pixels(std::size_t(width)*height);
    const std::array<std::pair<OriginalHudState::FinishBanner,const char*>,4> banners{{
        {OriginalHudState::FinishBanner::finish,"finish"},
        {OriginalHudState::FinishBanner::win,"win"},
        {OriginalHudState::FinishBanner::lose,"lose"},
        {OriginalHudState::FinishBanner::timeUp,"timeup"}}};
    for(const auto& [banner,name]:banners){
        std::fill(pixels.begin(),pixels.end(),0xff204060u);
        OriginalHudState state;state.finishBanner=banner;
        hud.paint(std::span<std::uint32_t>(pixels),width,height,state);
        std::ofstream out(app.root/"verification/finish-banner"/(std::string(name)+".ppm"),std::ios::binary);
        out<<"P6\n"<<width<<" "<<height<<"\n255\n";
        for(const auto pixel:pixels){
            const char rgb[3]={char((pixel>>16)&0xff),char((pixel>>8)&0xff),char(pixel&0xff)};
            out.write(rgb,3);
        }
    }
    std::ofstream report(app.root/"verification/finish_banner.txt");
    report<<"Painted the four race-end announcements from game2d chunks 182 to 185\n";

    // Drive a real race to its timeout and record which announcement is up.
    app.frontend.gameMode=original::OriginalGameMode::LegendOfTheStreets;
    app.frontend.battleProfile=original::makeOriginalFreshBattleProfile();
    app.frontend.battleProfile.setu(16,unsigned(app.frontend.car));
    original::selectOriginalRival(app.frontend.battleProfile,0);
    app.start();
    unsigned raced=0;
    while(raced<40000u&&app.race.phase!=RacePhase::Finished){app.simulate({});++raced;}
    report<<"race finished after "<<raced<<" updates, timeUp "<<app.race.timeUp<<"\n";
    const auto name=[&]{
        if(app.finishBannerDone)return "none";
        if(app.finishBannerTicks<120u)return "finish";
        if(app.race.timeUp)return "timeup";
        return app.battleResult==original::OriginalLegendResult::Loss?"lose":"win";
    };
    for(unsigned step=0;step<400u;++step){
        if(step==0||step==119||step==121||step==399)
            report<<"  after "<<app.finishBannerTicks<<" announcement updates: "<<name()<<"\n";
        app.simulate({});
    }
    report<<"final announcement ticks "<<app.finishBannerTicks<<", done "<<app.finishBannerDone<<"\n";
    return 0;
}
int runLegendRunSmoke(App& app){
    app.validationMode=true;app.legendVisitSmoke=true;
    app.frontend.gameMode=original::OriginalGameMode::LegendOfTheStreets;
    fs::create_directories(app.root/"verification");
    std::ofstream report(app.root/"verification/legend_run_smoke.txt");
    if(!original::OriginalLegendVisit::available(app.root)){
        report<<"Legend return artwork unavailable; the run was not played\n";
        return 0;
    }
    app.legendVisit.load(app.root);app.legendVisitLoaded=true;
    const fs::path frames=app.root/"verification/legend-run-frames";
    fs::create_directories(frames);
    app.frontend.battleProfile=original::makeOriginalFreshBattleProfile();
    app.frontend.battleProfile.setu(16,unsigned(app.frontend.car));
    original::selectOriginalRival(app.frontend.battleProfile,0);
    report<<"battle,rival,course,owner_updates,destination,course_clear,points,result_music,next_music\n";
    unsigned battles=0,courseClears=0;
    auto destination=original::OriginalLegendReturnDestination::Select;
    while(battles<64u){
        app.start();
        const auto rival=app.battleProfile.u(24),course=app.battleProfile.u(4);
        // Settle the battle as a win through the source progression.
        app.battleResult=original::recordOriginalLegendResult(app.battleProfile,1u,0u);
        if(app.battleResult!=original::OriginalLegendResult::Win)
            throw std::runtime_error("A won Legend battle was not recorded as a win");
        app.battlePoints=original::awardOriginalLegendPoints(app.battleProfile,0u,0.f);
        app.battleProgressApplied=true;
        app.battleResults={};app.battleResults.resultStatus=0;
        app.frontend.battleProfile=app.battleProfile;
        if(!app.beginLegendVisit())throw std::runtime_error("The post-result owner did not start");
        const bool clear=app.legendVisit.courseCleared();
        // The last rival's scene is the one that ends the run; play it whole and
        // keep frames from it rather than skipping through.
        const bool finalRival=rival==30u;
        unsigned ticks=0,resultMusic=0;
        while(!app.legendVisit.finished()&&ticks<60u*600u){
            app.legendSkipHeld=!finalRival&&!app.legendVisit.choiceVisible();
            if(app.legendVisit.choiceVisible()&&app.legendVisit.selectedIndex()==0)
                app.legendConfirmPending=true;
            app.advanceLegendVisit(1.0/60.0);
            ++ticks;
            // The result dialogue asks for its own track first; the
            // next-rival step then replaces it with that rival's theme.
            if(!resultMusic&&app.audio.selectionPlaying())resultMusic=app.audio.originalMusicCue();
            if(finalRival&&(ticks==600u||ticks==1500u||ticks==2400u))
                captureLegendVisitFrame(app,frames/("ending-tick"+std::to_string(ticks)+".ppm"),640,480);
        }
        app.legendSkipHeld=false;
        if(!app.legendVisit.finished())throw std::runtime_error("The owner never reached a destination");
        destination=app.legendVisit.destination();
        ++battles;if(clear)++courseClears;
        const auto nextMusic=app.audio.originalMusicCue();
        report<<battles<<','<<rival<<','<<course<<','<<ticks<<','<<unsigned(destination)
            <<','<<(clear?1:0)<<','<<app.battleProfile.u(72)<<','<<resultMusic<<','<<nextMusic<<"\n";report.flush();
        if(resultMusic<3u||resultMusic>=original::originalMusicCueCount)
            throw std::runtime_error("The result dialogue did not request its own music");
        if(destination==original::OriginalLegendReturnDestination::CourseLoad&&
           nextMusic!=app.battleProfile.u(24)+3u)
            throw std::runtime_error("The next-rival dialogue played the wrong rival theme");
        app.finishLegendVisit();
        if(destination!=original::OriginalLegendReturnDestination::CourseLoad)break;
    }
    unsigned beaten=0;for(unsigned i=0;i<31;++i)if(app.battleProfile.byte(116+i)>>4)++beaten;
    report<<"battles "<<battles<<", rivals beaten "<<beaten<<", course clears "<<courseClears
        <<", final destination "<<unsigned(destination)<<"\n";
    if(destination!=original::OriginalLegendReturnDestination::Ending)
        throw std::runtime_error("A won Legend run did not reach the ending");
    if(beaten!=31u)throw std::runtime_error("The ending was reached without beating every rival");
    if(!app.legendRunCompleted)throw std::runtime_error("The ending did not end the session");
    if(!app.menu||app.frontend.stage!=FrontendStage::Title)
        throw std::runtime_error("The completed run did not return the shell to its start");
    report<<"The run reached the source ending after beating all 31 rivals "
        <<"and returned the shell to the title\n";
    return 0;
}
// A losing run against every rival. The source picks the after-race track from
// its own loss table, so this plays each of the 31 losses twice, once finished
// and once timed out, and renders the first two seconds of each through the
// real mixer: a cue that is merely requested proves nothing about the track.
int runLegendLossRun(App& app){
    app.validationMode=true;app.legendVisitSmoke=true;
    app.frontend.gameMode=original::OriginalGameMode::LegendOfTheStreets;
    fs::create_directories(app.root/"verification");
    std::ofstream report(app.root/"verification/legend_loss_run.txt");
    if(!original::OriginalLegendVisit::available(app.root)){
        report<<"Legend return artwork unavailable; the losing run was not played\n";
        return 0;
    }
    app.legendVisit.load(app.root);app.legendVisitLoaded=true;
    app.audio.enabled=true;
    report<<"rival,status,course,owner_updates,destination,music,notes,peak,rendered\n";
    std::map<unsigned,unsigned> byTrack;unsigned visits=0;
    for(const unsigned status:{1u,2u})for(unsigned rival=0;rival<31u;++rival){
        app.frontend.battleProfile=original::makeOriginalFreshBattleProfile();
        app.frontend.battleProfile.setu(16,unsigned(app.frontend.car));
        original::selectOriginalRival(app.frontend.battleProfile,rival);
        app.start();
        const auto course=app.battleProfile.u(4);
        // Timeout records a loss even when ahead, which is the source rule the
        // result lifecycle applies at 05CFAC.
        app.battleResult=original::recordOriginalLegendResult(app.battleProfile,status==2u?0u:1u,1u);
        if(app.battleResult!=original::OriginalLegendResult::Loss)
            throw std::runtime_error("A lost Legend battle was not recorded as a loss");
        app.battlePoints=original::awardOriginalLegendPoints(app.battleProfile,status,0.f);
        app.battleProgressApplied=true;
        app.battleResults={};app.battleResults.resultStatus=status;
        app.frontend.battleProfile=app.battleProfile;
        if(!app.beginLegendVisit())throw std::runtime_error("The post-result owner did not start");
        const auto before=app.audio.selectionStatistics();
        const auto notesBefore=before.notesStarted;
        unsigned ticks=0,rendered=0,music=0;int peak=0;
        while(!app.legendVisit.finished()&&ticks<60u*600u){
            app.legendSkipHeld=!app.legendVisit.choiceVisible();
            app.advanceLegendVisit(1.0/60.0);
            ++ticks;
            if(!music&&app.audio.selectionPlaying())music=app.audio.originalMusicCue();
            // Two seconds of the track itself, through the mixer the game uses.
            if(music&&rendered<120u){
                ++rendered;
                for(unsigned sample=0;sample<735u;++sample){
                    const auto stereo=app.audio.renderStereo(0,0,0,0,false);
                    peak=std::max(peak,std::max(std::abs(int(stereo[0])),std::abs(int(stereo[1]))));
                }
            }
        }
        app.legendSkipHeld=false;
        if(!app.legendVisit.finished())throw std::runtime_error("The losing owner never reached a destination");
        const auto destination=app.legendVisit.destination();
        const auto notes=app.audio.selectionStatistics().notesStarted-notesBefore;
        report<<rival<<','<<status<<','<<course<<','<<ticks<<','<<unsigned(destination)
            <<','<<music<<','<<notes<<','<<peak<<','<<rendered<<"\n";report.flush();
        if(music!=original::originalLegendLossMusic(rival))
            throw std::runtime_error("The loss dialogue played the wrong after-race track");
        if(!notes||!peak)throw std::runtime_error("The loss after-race track produced no sound");
        ++byTrack[music];++visits;
        app.finishLegendVisit();
        app.audio.endResultMusic();
    }
    report<<"losing visits "<<visits;
    for(const auto& [track,plays]:byTrack)
        report<<", cue "<<track<<" ("<<original::originalMusicCueDescriptor(track).filename<<") "<<plays<<" times";
    report<<"\n";
    if(visits!=62u)throw std::runtime_error("The losing run did not cover every rival twice");
    if(byTrack.size()!=3u)throw std::runtime_error("The losing run did not reach all three after-race tracks");
    report<<"Every rival lost twice, finished and timed out; each after-race track "
          <<"was the one the source loss table names, and each one played."<<"\n";
    return 0;
}
// Drives the pre-race challenge headlessly for each of the three records the
// kind rule distinguishes, so the thing Chris could only check by playing has a
// check of its own.
// Reads the driver profiles this machine actually has and reports whether the
// pre-race challenge would run for each. Read-only: it loads them through the
// game's own loader and writes nothing back.
int runPreRaceGateCheck(App& app){
    app.validationMode=true;
    fs::create_directories(app.root/"verification");
    std::ofstream report(app.root/"verification/pre_race_gate.txt");
    const auto describe=[&](const char* where,int slot){
        app.useSaveSlot(slot);
        unsigned examined=0,eligible=0;
        for(unsigned car=0;car<40;++car){
            const auto stored=app.profiles.load(car);
            // Only profiles this machine actually wrote; a Fresh one is the
            // loader's default for a car that has never been played.
            if(stored.origin!=LocalDriverProfiles::Origin::Saved&&
               stored.origin!=LocalDriverProfiles::Origin::Backup)continue;
            ++examined;
            const auto& profile=stored.profile;
            const auto mode=profile.u(0),course=profile.u(4),enemy=profile.u(24);
            const bool ok=mode==0&&course<=8&&enemy<=30;
            if(ok)++eligible;
            const auto record=enemy<=30?profile.byte(116+enemy):0u;
            report<<where<<" car "<<car<<": mode "<<mode<<", course "<<course
                  <<", enemy "<<enemy<<" -> "<<(ok?"challenge runs":"CHALLENGE SKIPPED");
            if(ok)report<<", kind "<<((record>>4)?14:(record&15)?7:0);
            report<<char(10);
        }
        report<<where<<": "<<eligible<<" of "<<examined<<" profiles would show the challenge"<<char(10);
        return examined;
    };
    unsigned total=describe("default",-1);
    for(int slot=0;slot<5;++slot)total+=describe(("slot"+std::to_string(slot+1)).c_str(),slot);
    if(!total)report<<"no driver profiles on this machine"<<char(10);
    return 0;
}
int runPreRaceDialogueSmoke(App& app){
    app.validationMode=true;app.preRaceDialogueSmoke=true;
    app.frontend.gameMode=original::OriginalGameMode::LegendOfTheStreets;
    fs::create_directories(app.root/"verification");
    std::ofstream report(app.root/"verification/pre_race_dialogue.txt");
    if(!original::OriginalRivalDialogScene::available(app.root)){
        report<<"Rival dialogue artwork unavailable; challenge not exercised"<<char(10);
        return 0;
    }
    const fs::path frames=app.root/"verification/pre-race-dialogue";
    fs::create_directories(frames);
    struct Case {const char* name;std::uint8_t record;unsigned expectedKind;};
    // wins ? 14 : losses ? 7 : 0, as original_legend_return.cpp selects it.
    const Case cases[]{{"unmet",0x00,0},{"beaten-me",0x01,7},{"i-beat-them",0x10,14}};
    unsigned checked=0;
    for(const auto& one:cases){
        app.frontend.battleProfile=original::makeOriginalFreshBattleProfile();
        app.frontend.battleProfile.setu(16,unsigned(app.frontend.car));
        original::selectOriginalRival(app.frontend.battleProfile,0);
        app.frontend.battleProfile.setByte(116,one.record);
        if(!app.beginPreRaceDialogue())
            throw std::runtime_error(std::string("The challenge did not start for ")+one.name);
        const auto kind=app.preRaceDialogue.state().kind;
        if(kind!=one.expectedKind)
            throw std::runtime_error(std::string("The challenge chose kind ")+std::to_string(kind)+
                " for "+one.name+" instead of "+std::to_string(one.expectedKind));
        unsigned ticks=0,lit=0;
        while(!app.preRaceDialogue.closed()&&ticks<60u*60u){
            app.preRaceDialogue.step(app.frontend.battleProfile);
            if(app.preRaceDialogue.ready()){
                if(original::originalRivalDialogKindAvailable(app.preRaceDialogue.state().kind+1))
                    original::advanceOriginalRivalDialogPage(app.preRaceDialogue.state(),
                        app.preRaceDialogue.data(),app.frontend.battleProfile,1);
                else original::closeOriginalRivalDialog(app.preRaceDialogue.state());
            }
            if(ticks==120u){
                std::vector<std::uint32_t> canvas(std::size_t(640)*480,0xff000000u);
                unityUiClear(canvas.data(),640,480,0xff000000u);
                app.preRaceDialogue.paint(canvas,640,480);
                for(const auto pixel:canvas)if((pixel&0xffffffu)!=0)++lit;
                std::ofstream out(frames/(std::string(one.name)+".ppm"),std::ios::binary);
                out<<"P6"<<char(10)<<640<<" "<<480<<char(10)<<"255"<<char(10);
                for(const auto pixel:canvas){
                    const char rgb[3]{char(pixel>>16),char(pixel>>8),char(pixel)};out.write(rgb,3);}
            }
            ++ticks;
        }
        if(!app.preRaceDialogue.closed())
            throw std::runtime_error(std::string("The challenge never closed for ")+one.name);
        if(lit<2000u)
            throw std::runtime_error(std::string("The challenge painted almost nothing for ")+one.name);
        report<<one.name<<": started, kind "<<kind<<", closed after "<<ticks<<" ticks, "
              <<lit<<" pixels lit"<<char(10);
        ++checked;
        app.preRaceDialogueActive=false;
    }
    report<<"PASS the pre-race challenge starts, picks its kind and closes for all "
          <<checked<<" records"<<char(10);
    return 0;
}
int runLegendVisitSmoke(App& app){
    app.validationMode=true;app.legendVisitSmoke=true;
    app.frontend.gameMode=original::OriginalGameMode::LegendOfTheStreets;
    fs::create_directories(app.root/"verification");
    std::ofstream report(app.root/"verification/legend_visit_smoke.txt");
    if(!original::OriginalLegendVisit::available(app.root)){
        report<<"Legend return artwork unavailable; visit not exercised\n";
        return 0;
    }
    const fs::path frames=app.root/"verification/legend-visit-frames";
    fs::create_directories(frames);
    app.frontend.battleProfile=original::makeOriginalFreshBattleProfile();
    app.frontend.battleProfile.setu(16,unsigned(app.frontend.car));
    original::selectOriginalRival(app.frontend.battleProfile,0);app.start();
    app.multiplayer.active=true;
    if(app.retireLegendRace())throw std::runtime_error("Online retirement was accepted");
    app.multiplayer.active=false;app.paused=true;
    if(!app.retireLegendRace()||!app.race.timeUp||app.battleResults.resultStatus!=2||
            app.battleProfile.byte(116)!=1||!app.pendingResultSetup||app.paused||app.vsActive)
        throw std::runtime_error("Retire did not settle exactly one timeout loss");
    const auto retired=app.battleProfile.words;
    if(app.retireLegendRace())throw std::runtime_error("Retirement applied twice");
    app.settleBattleResult(false);
    if(app.battleProfile.words!=retired)throw std::runtime_error("Retirement points repeated");
    if(!app.beginLegendVisit()||app.legendVisit.dialogueState().kind!=21)
        throw std::runtime_error("Retirement did not select the timeout cutscene");
    app.legendVisitActive=false;
    report<<"PASS retirement timeout/loss/once-only/online guards and original kind21 cutscene\n";
    report<<"loading return artwork\n";report.flush();
    app.legendVisit.load(app.root);
    app.legendVisitLoaded=true;
    report<<"artwork loaded\n";report.flush();
    for(const unsigned status:{0u,1u})for(const bool accept:{false,true}){
        app.frontend.battleProfile=original::makeOriginalFreshBattleProfile();
        app.frontend.battleProfile.setu(16,unsigned(app.frontend.car));
        original::selectOriginalRival(app.frontend.battleProfile,0);
        app.battleProfile=app.frontend.battleProfile;
        app.battleProfile.setByte(116,std::uint8_t(status?0x01:0x10));
        app.battle=true;app.bunta=false;app.menu=false;app.paused=false;
        app.battleResults.resultStatus=status;
        if(!app.beginLegendVisit())throw std::runtime_error("Legend visit did not start after a settled result");
        unsigned ticks=0;
        int confirmedKind=-1;
        std::set<unsigned> seen;
        while(!app.legendVisit.finished()&&ticks<60u*240u){
            // One press per prompt, as a player gives it. Holding confirm every
            // frame is not how the owner is driven.
            const bool prompting=app.legendVisit.choiceVisible()&&app.legendVisit.selectedIndex()==0;
            const int kindNow=prompting?int(app.legendVisit.choiceKind()):-1;
            if(accept&&prompting&&kindNow!=confirmedKind){app.legendConfirmPending=true;confirmedKind=kindNow;}
            if(!prompting)confirmedKind=-1;
            app.advanceLegendVisit(1.0/60.0);
            ++ticks;
            // Unconditional stills, so a frame is kept whether or not a prompt
            // happens to be up at that tick.
            if(accept&&(ticks==600u||ticks==1800u||ticks==2600u||ticks==3000u||ticks==5000u||ticks==5900u||ticks==6300u))
                captureLegendVisitFrame(app,frames/("status"+std::to_string(status)
                    +"-t"+std::to_string(ticks)+(app.legendVisit.choiceVisible()?"-prompt":"-scene")+".ppm"),640,480);
            if(!app.legendVisit.choiceVisible())continue;
            // One capture the first time each distinct prompt appears.
            const auto kind=unsigned(app.legendVisit.choiceKind());
            if(!seen.count(kind)){
                seen.insert(kind);
                const char* names[]={"continue-after-loss","continue-after-win","rematch","new-challenger"};
                report<<"  status "<<status<<(accept?" accepted":" declined")<<" prompt "
                    <<(kind<4?names[kind]:"?")<<" at tick "<<ticks<<"\n";report.flush();
                captureLegendVisitFrame(app,frames/("status"+std::to_string(status)
                    +(accept?"-accept-":"-decline-")+(kind<4?names[kind]:"x")+".ppm"),640,480);
                // The same state at the player's resolution, to separate a
                // painter placement fault from a Unity presentation one.
                captureLegendVisitFrame(app,frames/("status"+std::to_string(status)
                    +(accept?"-accept-":"-decline-")+(kind<4?names[kind]:"x")+"-720p.ppm"),1280,720);
            }
        }
        if(!app.legendVisit.finished())throw std::runtime_error("Legend visit never reached a destination");
        const auto destination=app.legendVisit.destination();
        const auto rivalBefore=app.battleProfile.u(24);
        app.finishLegendVisit();
        if(app.legendVisitActive)throw std::runtime_error("Legend visit stayed active after it finished");
        if(destination==original::OriginalLegendReturnDestination::CourseLoad){
            if(app.menu||!app.battle)throw std::runtime_error("Accepted Legend visit did not race the next rival");
            if(app.battleProfile.u(24)!=rivalBefore)throw std::runtime_error("The raced rival is not the one the owner selected");
        }else if(!app.menu||app.frontend.stage!=FrontendStage::Course)
            throw std::runtime_error("Declined Legend visit did not hand control back to selection");
        if(accept&&destination!=original::OriginalLegendReturnDestination::CourseLoad
            &&destination!=original::OriginalLegendReturnDestination::Select)
            throw std::runtime_error("Accepted Legend visit did not continue the session");
        report<<"status "<<status<<(accept?" accepted":" declined")<<": "<<ticks
            <<" owner updates, destination "<<unsigned(destination)
            <<(destination==original::OriginalLegendReturnDestination::CourseLoad
                ?", racing rival "+std::to_string(app.battleProfile.u(24)):std::string(", back at selection"))<<"\n";
        report.flush();
    }
    // The start button the owner advertises must actually shorten the scripted
    // pages rather than only drawing the prompt.
    {
        app.frontend.battleProfile=original::makeOriginalFreshBattleProfile();
        app.frontend.battleProfile.setu(16,unsigned(app.frontend.car));
        original::selectOriginalRival(app.frontend.battleProfile,0);
        app.battleProfile=app.frontend.battleProfile;
        app.battleProfile.setByte(116,0x10);
        app.battle=true;app.bunta=false;app.menu=false;app.paused=false;
        app.battleResults.resultStatus=0;
        if(!app.beginLegendVisit())throw std::runtime_error("Legend visit did not start for the skip route");
        unsigned ticks=0;
        while(!app.legendVisit.finished()&&ticks<60u*240u){
            app.legendSkipHeld=!app.legendVisit.choiceVisible();
            if(app.legendVisit.choiceVisible()&&app.legendVisit.selectedIndex()==0)
                app.legendConfirmPending=true;
            app.advanceLegendVisit(1.0/60.0);
            ++ticks;
        }
        app.legendSkipHeld=false;
        if(!app.legendVisit.finished())throw std::runtime_error("Skipped Legend visit never reached a destination");
        const auto destination=app.legendVisit.destination();
        const auto rivalBefore=app.battleProfile.u(24);
        app.finishLegendVisit();
        if(ticks>=5751u)throw std::runtime_error("The start button did not skip the scripted dialogue");
        if(destination==original::OriginalLegendReturnDestination::CourseLoad
            &&(app.menu||!app.battle||app.battleProfile.u(24)!=rivalBefore))
            throw std::runtime_error("Skipped Legend visit did not race the next rival");
        report<<"skipped: "<<ticks<<" owner updates, destination "<<unsigned(destination)<<"\n";
    }
    report<<"All four post-result routes finished: declining ends the session at selection, ""accepting races the rival the owner selected, and the start button skips the dialogue\n";
    return 0;
}
int runLegendBattleSmoke(App& app){
    app.validationMode=true;app.frontend.gameMode=original::OriginalGameMode::LegendOfTheStreets;
    fs::create_directories(app.root/"verification");std::ofstream report(app.root/"verification/legend_battle_smoke.csv");
    report<<"enemy,model,condition,player_ticks,rival_ticks,player_travel,rival_displacement,remaining6000\n";
    for(unsigned enemy=0;enemy<31;++enemy){
        app.frontend.battleProfile=original::makeOriginalFreshBattleProfile();app.frontend.battleProfile.setu(16,unsigned(app.frontend.car));
        original::selectOriginalRival(app.frontend.battleProfile,enemy);app.start();
        const auto start=app.rivalVehicle.position;
        for(unsigned tick=0;tick<900;++tick){
            const auto projection=app.projectRacePosition(app.vehicle.position);
            const float look=std::max(10.f,std::max(0.f,app.vehicle.speed)*.65f);
            const auto target=app.sampleRaceDistance(projection.sample.distance+look).center-app.vehicle.position;
            const float angle=wrapAngle(std::atan2(target.x,target.z)-app.vehicle.yaw);
            DriverInput input;input.automatic=true;input.steer=-std::clamp(std::atan2(2*app.config.wheelbase*std::sin(angle),look)/recoveredSteeringLimit,-.75f,.75f);
            input.throttle=app.vehicle.speed<18?.7f:.05f;input.brake=app.vehicle.speed>20?.3f:0;
            app.simulate(input);
        }
        const float moved=length(app.rivalVehicle.position-start);
        const float gap=app.battleMetrics.advantage(app.originalRace.state().progress,app.originalRace.state().rivalProgress);
        report<<enemy<<','<<app.loadedRivalCar<<','<<app.presentedSession().selection().physics.conditionCode<<','<<app.vehicle.tick<<','<<app.originalSession.rivalFrameCounter()<<','<<app.vehicle.travel<<','<<moved<<','<<app.race.remaining6000<<'\n';report.flush();
        if(!app.battle||!app.rivalVisible||app.loadedRivalCar!=int(original::originalRival(enemy).car)||app.vehicle.travel<5||!std::isfinite(moved)||moved<5||!std::isfinite(gap))
            throw std::runtime_error("Original Legend battle did not advance enemy "+std::to_string(enemy));
    }
    app.frontend.battleProfile=original::makeOriginalFreshBattleProfile();app.frontend.battleProfile.setu(16,unsigned(app.frontend.car));
    original::selectOriginalRival(app.frontend.battleProfile,0);app.start();
    for(unsigned tick=0;tick<20000&&app.race.phase!=RacePhase::Finished;++tick)app.simulate({});
    if(!app.race.timeUp||!app.battleProgressApplied||app.battleProfile.byte(116)!=1||app.battleProfile.u(72)!=1000||app.battlePoints.total!=1000)throw std::runtime_error("Natural Legend timeout failed to record one loss and participation points");
    const auto settled=app.battleProfile.words;
    for(unsigned tick=0;tick<180;++tick)app.simulate({});
    if(app.battleProfile.words!=settled||app.frontend.battleProfile.words!=settled)throw std::runtime_error("Settled Legend result was applied more than once or lost before menu");
    std::ofstream timeout(app.root/"verification/legend_timeout_progress.txt");
    timeout<<"Natural Legend timeout; no forced timer/result writes\nElapsed6000: "<<app.race.elapsed6000<<"\nEnemy0 result byte: "<<unsigned(app.battleProfile.byte(116))<<"\nPoints awarded: "<<app.battlePoints.total<<"\nPoints balance: "<<app.battleProfile.u(72)<<"\nRepeated settled frames: 180\nUser profile writes disabled in diagnostic mode\n";
    return 0;
}
int runBuntaBattleSmoke(App& app){
    app.validationMode=true;app.frontend.gameMode=original::OriginalGameMode::BuntaChallenge;
    fs::create_directories(app.root/"verification");std::ofstream report(app.root/"verification/bunta_battle_smoke.csv");
    report<<"menu_course,level,course,enemy,model,condition,player_ticks,rival_ticks,player_travel,rival_displacement,initial6000\n";
    for(unsigned index=0;index<8;++index)for(unsigned level:{0u,10u,11u,15u,16u}){
        auto& p=app.frontend.battleProfile;p=original::makeOriginalFreshBattleProfile();p.setu(0,2);p.setu(16,unsigned(app.frontend.car));p.setu(72,10000);
        for(unsigned i=0;i<8;++i)p.setu(1080+i*4,level);
        original::selectOriginalBuntaCourse(p,index);const auto expected=original::makeOriginalBuntaRaceSetup(p,2);
        app.start();const auto rivalStart=app.rivalVehicle.position;const auto initial=app.race.remaining6000;
        if(initial!=std::int32_t(expected.initialTime6000)||!app.bunta||!app.battle||!app.rivalVisible)throw std::runtime_error("Bunta race did not bind its original opponent/timer");
        for(unsigned tick=0;tick<900;++tick){
            const auto projection=app.projectRacePosition(app.vehicle.position);const float look=std::max(10.f,std::max(0.f,app.vehicle.speed)*.65f);
            const auto target=app.sampleRaceDistance(projection.sample.distance+look).center-app.vehicle.position;
            const float angle=wrapAngle(std::atan2(target.x,target.z)-app.vehicle.yaw);
            DriverInput input;input.automatic=true;input.steer=-std::clamp(std::atan2(2*app.config.wheelbase*std::sin(angle),look)/recoveredSteeringLimit,-.75f,.75f);
            input.throttle=app.vehicle.speed<18?.7f:.05f;input.brake=app.vehicle.speed>20?.3f:0;app.simulate(input);
        }
        const auto moved=length(app.rivalVehicle.position-rivalStart);
        report<<index<<','<<level<<','<<app.courseIndex<<','<<app.loadedRivalEnemy<<','<<app.loadedRivalCar<<','<<app.presentedSession().selection().physics.conditionCode<<','<<app.vehicle.tick<<','<<app.originalSession.rivalFrameCounter()<<','<<app.vehicle.travel<<','<<moved<<','<<initial<<'\n';report.flush();
        if(app.vehicle.travel<5||!std::isfinite(moved)||moved<5)throw std::runtime_error("Bunta race did not advance both original actors");
    }
    auto& p=app.frontend.battleProfile;p=original::makeOriginalFreshBattleProfile();p.setu(0,2);p.setu(16,unsigned(app.frontend.car));p.setu(72,10000);
    original::selectOriginalBuntaCourse(p,0);app.start();
    for(unsigned tick=0;tick<20000&&app.race.phase!=RacePhase::Finished;++tick)app.simulate({});
    if(!app.race.timeUp||!app.battleProgressApplied||app.battleProfile.u(1080)!=0||app.battleProfile.u(72)!=9000||!app.buntaPoints.deduction)throw std::runtime_error("Bunta timeout did not apply original loss/points");
    const auto settled=app.battleProfile.words;for(unsigned i=0;i<180;++i)app.simulate({});
    if(app.battleProfile.words!=settled)throw std::runtime_error("Bunta settled result applied twice");
    std::ofstream timeout(app.root/"verification/bunta_timeout_progress.txt");timeout<<"Natural Bunta timeout, no forced result/timer writes\nLevel: "<<app.battleProfile.u(1080)<<"\nPoints deducted: "<<app.buntaPoints.total<<"\nBalance: "<<app.battleProfile.u(72)<<"\nUser profile writes disabled in diagnostic mode\n";
    return 0;
}

// Measures the three things a player actually feels: how hard the car
// accelerates, what a wall costs, and how much lateral grip it has. The
// solver is bit-exact against the original, so any complaint has to be
// answered with numbers rather than by re-reading the solver.
int runHandlingAudit(App& app){
    app.validationMode=true;app.wet=false;app.profile=0;app.automatic=true;
    fs::create_directories(app.root/"verification");
    std::ofstream report(app.root/"verification/handling_audit.csv");
    report<<"car,course,test,metric,value\n";
    const auto line=[&](int car,const char* course,const char* test,const char* metric,double value){
        report<<car<<','<<course<<','<<test<<','<<metric<<','<<value<<'\n';report.flush();
    };
    // Akina downhill, the course the game is known by.
    for(int car:{0,1,2}){
        app.frontend.car=car;app.courseIndex=3;app.reverse=false;app.start();
        while(app.race.phase==RacePhase::Countdown)app.simulate({});

        // ---- 1. straight-line acceleration, throttle pinned, line followed ----
        double t60=-1,t100=-1,t150=-1;float top=0;int tick=0;
        float steer=0;
        for(;tick<60*90&&app.race.phase!=RacePhase::Finished;++tick){
            const auto p=app.projectRacePosition(app.vehicle.position);
            const float speed=std::max(0.f,app.vehicle.speed);
            const float look=std::max(8.f,speed*.6f);
            const Vec3 aim=app.sampleRaceDistance(p.sample.distance+look).center-app.vehicle.position;
            const float angle=wrapAngle(std::atan2(aim.x,aim.z)-app.vehicle.yaw);
            const float demand=std::clamp(std::atan2(2*app.config.wheelbase*std::sin(angle),look)/recoveredSteeringLimit,-.9f,.9f);
            steer+=std::clamp(demand-steer,-.08f,.08f);
            DriverInput d;d.automatic=true;d.steer=-steer;d.throttle=1.f;d.brake=0.f;
            app.simulate(d);
            const float kmh=app.vehicle.speedKmh();top=std::max(top,kmh);
            if(t60<0&&kmh>=60)t60=tick/60.0;
            if(t100<0&&kmh>=100)t100=tick/60.0;
            if(t150<0&&kmh>=150)t150=tick/60.0;
            if(t150>=0)break;
        }
        // Tick-by-tick launch trace: a discontinuity means an impulse, a smooth
        // ramp means the solver really is producing this acceleration.
        if(car==0){
            app.start();
            std::ofstream trace(app.root/"verification/launch_trace.csv");
            trace<<"tick,phase,countdown,speed_kmh,rpm,gear,throttle,travel"<<char(10);
            float tstr=0;
            for(int i=0;i<60*60;++i){
                // follow the road so the run is not ended by a barrier, but
                // never lift: this is a full-throttle run to terminal speed
                const auto pp=app.projectRacePosition(app.vehicle.position);
                const float sp=std::max(0.f,app.vehicle.speed);
                const float lk=std::max(8.f,sp*.6f);
                const Vec3 am=app.sampleRaceDistance(pp.sample.distance+lk).center-app.vehicle.position;
                const float an=wrapAngle(std::atan2(am.x,am.z)-app.vehicle.yaw);
                const float dm=std::clamp(std::atan2(2*app.config.wheelbase*std::sin(an),lk)/recoveredSteeringLimit,-.9f,.9f);
                tstr+=std::clamp(dm-tstr,-.08f,.08f);
                DriverInput d;d.automatic=true;d.throttle=1.f;d.steer=-tstr;
                app.simulate(d);
                trace<<i<<','<<int(app.race.phase)<<','<<app.race.countdown<<','
                     <<app.vehicle.speedKmh()<<','<<app.vehicle.rpm<<','<<app.vehicle.gear<<','
                     <<app.vehicle.throttle<<','<<app.vehicle.travel<<char(10);
            }
        }
        // Cross-check the published speed against ground actually covered:
        // vehicle.speed is the solver's 0x238, vehicle.velocity is a position
        // delta, so a mismatch is a unit fault at the publish boundary.
        {
            app.start();while(app.race.phase==RacePhase::Countdown)app.simulate({});
            float st=0;double travelStart=app.vehicle.travel;int t0=int(app.vehicle.tick);
            for(int i=0;i<60*8;++i){
                const auto pp=app.projectRacePosition(app.vehicle.position);
                const float sp=std::max(0.f,app.vehicle.speed);
                const float lk=std::max(8.f,sp*.6f);
                const Vec3 am=app.sampleRaceDistance(pp.sample.distance+lk).center-app.vehicle.position;
                const float an=wrapAngle(std::atan2(am.x,am.z)-app.vehicle.yaw);
                const float dm=std::clamp(std::atan2(2*app.config.wheelbase*std::sin(an),lk)/recoveredSteeringLimit,-.9f,.9f);
                st+=std::clamp(dm-st,-.08f,.08f);
                DriverInput d;d.automatic=true;d.steer=-st;d.throttle=1.f;
                app.simulate(d);
            }
            const double seconds=double(int(app.vehicle.tick)-t0)/60.0;
            const double metres=app.vehicle.travel-travelStart;
            line(car,"akina","units","published_kmh",app.vehicle.speedKmh());
            line(car,"akina","units","position_delta_kmh",length(app.vehicle.velocity)*3.6);
            line(car,"akina","units","mean_kmh_from_travel",seconds>0?metres/seconds*3.6:-1);
            line(car,"akina","units","metres_in_8s",metres);
        }
        line(car,"akina","accel","sec_0_60kmh",t60);
        line(car,"akina","accel","sec_0_100kmh",t100);
        line(car,"akina","accel","sec_0_150kmh",t150);
        line(car,"akina","accel","top_kmh_90s",top);

        // ---- 2. what a wall costs: hold full lock into the barrier ----
        app.start();while(app.race.phase==RacePhase::Countdown)app.simulate({});
        // build speed on the racing line first
        steer=0;
        for(int i=0;i<60*12;++i){
            const auto p=app.projectRacePosition(app.vehicle.position);
            const float speed=std::max(0.f,app.vehicle.speed);
            const float look=std::max(8.f,speed*.6f);
            const Vec3 aim=app.sampleRaceDistance(p.sample.distance+look).center-app.vehicle.position;
            const float angle=wrapAngle(std::atan2(aim.x,aim.z)-app.vehicle.yaw);
            const float demand=std::clamp(std::atan2(2*app.config.wheelbase*std::sin(angle),look)/recoveredSteeringLimit,-.9f,.9f);
            steer+=std::clamp(demand-steer,-.08f,.08f);
            DriverInput d;d.automatic=true;d.steer=-steer;d.throttle=1.f;
            app.simulate(d);
        }
        const float approach=app.vehicle.speedKmh();
        float atContact=-1,lowest=1e9f,peakImpact=0;int contactTicks=0;
        for(int i=0;i<60*6;++i){
            DriverInput d;d.automatic=true;d.steer=1.f;d.throttle=1.f; // full lock into the wall, still on power
            app.simulate(d);
            if(app.vehicle.wallContact){
                if(atContact<0)atContact=app.vehicle.speedKmh();
                ++contactTicks;peakImpact=std::max(peakImpact,app.vehicle.wallImpactSpeed);
            }
            if(atContact>=0)lowest=std::min(lowest,app.vehicle.speedKmh());
        }
        line(car,"akina","wall","approach_kmh",approach);
        line(car,"akina","wall","speed_at_contact_kmh",atContact);
        line(car,"akina","wall","lowest_kmh_after_contact",lowest>1e8f?-1:lowest);
        line(car,"akina","wall","kmh_scrubbed",(atContact>=0&&lowest<1e8f)?atContact-lowest:-1);
        line(car,"akina","wall","contact_ticks",contactTicks);
        line(car,"akina","wall","peak_impact_value",peakImpact);

        // ---- 3. lateral grip: steady steer, peak sustained lateral g ----
        app.start();while(app.race.phase==RacePhase::Countdown)app.simulate({});
        for(int i=0;i<60*10;++i){DriverInput d;d.automatic=true;d.throttle=1.f;app.simulate(d);}
        float peakLat=0,peakSlip=0,latAtPeak=0;
        for(int i=0;i<60*8;++i){
            DriverInput d;d.automatic=true;d.steer=.55f;d.throttle=.35f;
            app.simulate(d);
            const float g=std::abs(app.vehicle.lateralAcceleration)/9.80665f;
            if(g>peakLat){peakLat=g;latAtPeak=app.vehicle.speedKmh();}
            peakSlip=std::max(peakSlip,std::abs(app.vehicle.slip));
        }
        line(car,"akina","grip","peak_lateral_g",peakLat);
        line(car,"akina","grip","kmh_at_peak_g",latAtPeak);
        line(car,"akina","grip","peak_slip",peakSlip);
    }
    return 0;
}

// Every physics table is indexed by selection.vehicleIndex. If the cars all
// drive the same, either these rows are identical or the index is wrong, so
// print what the session actually resolved rather than inferring it.
int runParameterDump(App& app){
    app.validationMode=true;app.wet=false;app.profile=0;app.automatic=true;
    fs::create_directories(app.root/"verification");
    std::ofstream report(app.root/"verification/vehicle_parameters.csv");
    report<<"car,folder,profileIndex,maximumGear,workingBase,lower,upper,divisor,"
            "lossCap,lossGrowth,frame451C,frame866C,angular08,angular0C,"
            "profileWord0,profileWord1,profileWord2,profileWord3\n";
    const auto hex=[](std::uint32_t v){std::ostringstream o;o<<"0x"<<std::hex<<v;return o.str();};
    for(int car=0;car<35;++car){
        app.frontend.car=car;app.courseIndex=3;app.reverse=false;
        try{ app.start(); }
        catch(const std::exception& e){ report<<car<<",<start failed: "<<e.what()<<">\n";report.flush();continue; }
        const auto& p=app.presentedSession().parameters();
        report<<car<<','<<originalCarFolders.at(std::size_t(car))<<','
              <<p.transmission.profileIndex<<','<<p.transmission.maximumGear<<','
              <<p.transmission.workingBase<<','<<p.transmission.lower<<','
              <<p.transmission.upper<<','<<p.transmission.divisor<<','
              <<p.loss.cap0C284F80<<','<<p.loss.growth0C284F84<<','
              <<p.frame.table0C28451C<<','<<p.frame.table0C28866C<<','
              <<p.angular.carRecord0C283F18_08<<','<<p.angular.carRecord0C283F18_0C<<','
              <<hex(p.profile.words[0])<<','<<hex(p.profile.words[1])<<','
              <<hex(p.profile.words[2])<<','<<hex(p.profile.words[3])<<'\n';
        report.flush();
    }
    return 0;
}
int runCourseMatrix(App& app){
    app.validationMode=true;app.wet=false;app.profile=0;app.automatic=true;fs::create_directories(app.root/"verification");
    std::ofstream report(app.root/"verification"/"course_drive_matrix.csv");report<<"course,reverse,finished,ticks,progress,length,wall_ticks,max_speed_kmh\n";int failures=0;
    for(int course=0;course<9;course++)for(bool reverse:{false,true}){
        app.courseIndex=course;app.reverse=reverse;app.start();float steer=0,maxSpeed=0;int walls=0;
        for(int i=0;i<60*1200&&app.race.phase!=RacePhase::Finished;i++){
            auto p=app.projectRacePosition(app.vehicle.position);float speed=std::max(0.f,app.vehicle.speed);float look=std::max(6.f,speed*.55f),targetSpeed=22;
            for(float offset:{8.f,18.f,32.f,50.f}){float curvature=std::abs(app.sampleRaceDistance(p.sample.distance+offset).curvature);targetSpeed=std::min(targetSpeed,std::sqrt(4.5f/std::max(.001f,curvature)));}
            targetSpeed=std::clamp(targetSpeed,7.f,22.f);
            Vec3 aim=app.sampleRaceDistance(p.sample.distance+look).center-app.vehicle.position;
            float angle=wrapAngle(std::atan2(aim.x,aim.z)-app.vehicle.yaw);
            float demand=std::clamp(std::atan2(2*app.config.wheelbase*std::sin(angle),look)/recoveredSteeringLimit,-.92f,.92f);
            steer+=std::clamp(demand-steer,-.07f,.07f);
            DriverInput d;d.steer=-steer;d.throttle=std::clamp((targetSpeed-speed)*.6f,0.f,.9f);d.brake=std::clamp((speed-targetSpeed)*.3f,0.f,.8f);
            app.simulate(d);walls+=app.vehicle.wallContact;maxSpeed=std::max(maxSpeed,app.vehicle.speedKmh());
            if(!std::isfinite(app.vehicle.position.x)||!std::isfinite(app.vehicle.yaw))break;
        }
        bool done=app.race.phase==RacePhase::Finished&&!app.race.timeUp;if(!done)++failures;
        report<<app.course.name<<','<<reverse<<','<<done<<','<<app.race.ticks<<','<<app.progress<<','<<app.course.length<<','<<walls<<','<<maxSpeed<<'\n';report.flush();
    }
    return failures?3:0;
}
#endif // desktop window and offline diagnostics
}
#if !defined(IDAS3_PORTABLE_SCENE)
int runEngineDrivingCheck(App& app,bool tireCheck=false){
    app.validationMode=true;app.courseIndex=3;app.reverse=false;app.wet=false;app.night=false;app.automatic=true;
    app.frontend.gameMode=original::OriginalGameMode::TimeAttack;
    const auto destination=app.root/(tireCheck?"verification/native-tire-live":"verification/native-tire-live/engine-regression");fs::create_directories(destination);
    std::ofstream report(destination/"application.csv");report<<"car,upgrade,condition,wet,simulation_frames,pcm_frames,dry_rms,mix_rms,clipped_samples,release_cues,backfire_cues,dropped_frames,underflow_frames,travel,tire_cues,tire_samples,tire_rms,tire_cue_mask,max_tire_strength,max_tire_speed\n";
    const std::vector<std::pair<unsigned,unsigned>> cases=tireCheck?std::vector<std::pair<unsigned,unsigned>>{{8,0},{8,1},{8,2}}:std::vector<std::pair<unsigned,unsigned>>{{0,0},{0,6},{8,2},{19,2}};
    for(auto [car,variant]:cases){
        const unsigned upgrade=tireCheck?2:variant;
        if(tireCheck){app.courseIndex=variant==2?8:3;app.wet=variant!=0;app.night=variant==2;}
        app.frontend.car=int(car);app.frontend.battleProfile=original::makeOriginalFreshBattleProfile();
        auto& profile=app.frontend.battleProfile;profile.setu(16,car);profile.setByte(164,std::uint8_t(upgrade));profile.setByte(162,1);profile.setByte(166,1);
        app.start();app.audio.enabled=true;app.audio.scene(false,false,false);
        const unsigned frameCount=tireCheck?1500:900;std::vector<short> recording;recording.reserve(frameCount*735*2);double energy=0;unsigned clipped=0;
        for(unsigned frame=0;frame<frameCount;++frame){
            DriverInput input;input.automatic=true;input.throttle=frame%180<130?1.f:0.f;input.brake=frame%180>=160?.4f:0.f;
            const auto projection=app.course.project(app.vehicle.position,app.segment);const float lookahead=std::max(12.f,app.vehicle.speed*.65f);
            const auto target=app.course.sample(projection.sample.distance+lookahead).center-app.vehicle.position;
            const float angle=wrapAngle(std::atan2(target.x,target.z)-app.vehicle.yaw);
            input.steer=-std::clamp(std::atan2(2*app.config.wheelbase*std::sin(angle),lookahead)/recoveredSteeringLimit,-.7f,.7f);
            if(tireCheck){input.throttle=1;input.brake=0;if(frame>430)input.steer=frame%240<100?.8f:frame%240<200?-.8f:0.f;}
            app.simulate(input);
            for(unsigned i=0;i<735;++i){const auto s=app.audio.renderStereo(0,0,0,0,true);for(auto value:s){recording.push_back(value);energy+=double(value)*value;if(value==32767||value==-32767)++clipped;}}
            if(frame==450){const auto before=app.audio.engineStatistics();app.audio.scene(false,false,true);
                for(unsigned i=0;i<2048;++i)if(app.audio.renderStereo(0,0,0,0,true)!=std::array<short,2>{})throw std::runtime_error("Paused game audio was not silent");
                if(app.audio.engineStatistics().pcmFrames!=before.pcmFrames)throw std::runtime_error("Pause advanced native engine");app.audio.scene(false,false,false);}
        }
        const auto& stats=app.audio.engineStatistics();const double dryRms=std::sqrt(stats.dryEnergy/double(stats.pcmFrames*2)),mixRms=std::sqrt(energy/double(recording.size()));
        report<<car<<','<<upgrade<<','<<app.courseIndex*2+int(app.reverse)<<','<<app.wet<<','<<stats.simulationFrames<<','<<stats.pcmFrames<<','<<dryRms<<','<<mixRms<<','<<clipped<<','<<stats.releaseCues<<','<<stats.backfireCues<<','<<stats.droppedFrames<<','<<stats.underflowFrames<<','<<app.vehicle.travel<<','<<stats.tireCues<<','<<stats.tireSamples<<','<<std::sqrt(stats.tireEnergy/double(stats.pcmFrames))<<','<<stats.tireCueMask<<','<<stats.maximumTireStrength<<','<<stats.maximumTireSpeed<<'\n';report.flush();
        if(!app.originalHandling||stats.simulationFrames<600||stats.pcmFrames!=stats.simulationFrames*735||dryRms<100||mixRms<100||clipped||stats.droppedFrames||stats.underflowFrames||app.vehicle.travel<10)
            throw std::runtime_error("Live engine driving/audio check failed for car "+std::to_string(car));
        if(tireCheck&&(!stats.tireCues||stats.tireEnergy<1e8||stats.tireRequests!=stats.simulationFrames||(stats.tireCueMask&~(3u<<(variant*2)))))throw std::runtime_error("Live tire trigger/surface/audio check failed");
        std::ofstream wav(destination/("car"+std::to_string(car)+"-variant"+std::to_string(variant)+"-driving.wav"),std::ios::binary);
        const auto u16=[&](unsigned value){wav.put(char(value));wav.put(char(value>>8));};const auto u32=[&](unsigned value){u16(value);u16(value>>16);};
        wav.write("RIFF",4);u32(36+unsigned(recording.size()*2));wav.write("WAVEfmt ",8);u32(16);u16(1);u16(2);u32(44100);u32(176400);u16(4);u16(16);wav.write("data",4);u32(unsigned(recording.size()*2));
        wav.write(reinterpret_cast<const char*>(recording.data()),std::streamsize(recording.size()*2));if(!wav)throw std::runtime_error("Could not save application sound check");
    }
    return 0;
}
#ifndef IDAS3_UNITY_PLUGIN
// Plays every cue the dialogue and post-race scenes can request, through the
// same manager, score reader and mixer the game uses. A cue that decodes but
// never starts a note, or starts notes and still renders silence, fails here.
int runDialogueMusicSmoke(App& app){
    app.audio.enabled=true;
    const auto report=app.root/"verification/original-music-cues.csv";
    fs::create_directories(report.parent_path());
    std::ofstream out(report);
    out<<"cue,bank,notes,peakVoices,samples,nonSilent,peak"<<char(10);
    unsigned played=0,total=0;
    for(unsigned cue=original::originalMusicMenuCues;cue<original::originalMusicCueCount;++cue){
        const auto& descriptor=original::originalMusicCueDescriptor(cue);
        const auto before=app.audio.selectionStatistics();
        const auto notesBefore=before.notesStarted,samplesBefore=before.samples;
        app.audio.beginOriginalMusicCue(cue);
        std::uint64_t nonSilent=0;int peak=0;
        // The source manager loads on the request and starts on a later
        // 60Hz service call, which is why the scene owners tick it.
        for(unsigned sample=0;sample<44100u*3u/2u;++sample){
            if(sample%735==0)app.audio.tickResultMusic();
            const auto stereo=app.audio.renderStereo(0,0,0,0,false);
            if(stereo[0]||stereo[1])++nonSilent;
            peak=std::max(peak,std::max(std::abs(int(stereo[0])),std::abs(int(stereo[1]))));
        }
        const auto& stats=app.audio.selectionStatistics();
        const auto notes=stats.notesStarted-notesBefore,samples=stats.samples-samplesBefore;
        out<<cue<<","<<descriptor.filename<<","<<notes<<","<<stats.peakVoices<<","
           <<samples<<","<<nonSilent<<","<<peak<<char(10);
        ++total;if(notes&&nonSilent)++played;
        app.audio.endResultMusic();
    }
    out.close();
    if(played!=total)throw std::runtime_error("Only "+std::to_string(played)+" of "+std::to_string(total)+" original music cues played");
    return 0;
}

// Renders one music cue to a WAV through the game's own manager, score reader
// and mixer, optionally one file per authored channel. Diagnostic only: a
// missing instrument can be named from a stem instead of guessed at.
void writeMusicWav(const fs::path& path,const std::vector<short>& pcm){
    const std::uint32_t bytes=std::uint32_t(pcm.size()*2),rate=44100,blockAlign=4;
    std::ofstream out(path,std::ios::binary);
    const auto u32=[&](std::uint32_t v){out.write(reinterpret_cast<const char*>(&v),4);};
    const auto u16=[&](std::uint16_t v){out.write(reinterpret_cast<const char*>(&v),2);};
    out.write("RIFF",4);u32(36+bytes);out.write("WAVEfmt ",8);u32(16);u16(1);u16(2);
    u32(rate);u32(rate*blockAlign);u16(blockAlign);u16(16);
    out.write("data",4);u32(bytes);
    out.write(reinterpret_cast<const char*>(pcm.data()),bytes);
}
// The ARM harness verifies the driver's own sixty-four channel records at each
// loop boundary, so those records are ground truth for what every voice should
// be set to. Nothing checks what the native player then makes of them. This
// renders up to the traced boundary tick and writes our live voices out beside
// it, so the two can be compared field by field instead of by ear.
int runMusicVoiceDump(App& app,unsigned cue,unsigned tick){
    app.audio.enabled=true;
    const auto directory=app.root/"verification/music";
    fs::create_directories(directory);
    app.audio.endResultMusic();
    app.audio.setDiagnosticMusicChannel(~0u);
    app.audio.beginOriginalMusicCue(cue);
    // One tick is forty-four sample clocks, exactly as the harness records.
    const std::uint64_t target=std::uint64_t(tick)*44ull;
    for(std::uint64_t sample=0;sample<target;++sample){
        if(sample%735==0)app.audio.tickResultMusic();
        app.audio.renderStereo(0,0,0,0,false);
    }
    const auto voices=app.audio.selectionVoices();
    const std::string file(original::originalMusicCueDescriptor(cue).filename);
    std::ofstream report(directory/(file.substr(0,file.rfind('.'))+"-voices.csv"));
    report<<"channel,key,layer,pitch,envelope1,envelope2,lfo,totalLevel,pan,directLevel,"
          <<"effectSend,filter,filterEnv1,filterEnv2,flags0,flags1,releasing"<<char(10);
    for(const auto& v:voices){
        report<<v.channel<<','<<v.key<<','<<v.layer<<','<<v.parameters.pitch<<','
              <<v.parameters.envelope1<<','<<v.parameters.envelope2<<','<<v.parameters.lfo<<','
              <<unsigned(v.parameters.totalLevel)<<','<<unsigned(v.parameters.pan)<<','
              <<unsigned(v.parameters.directLevel)<<','<<unsigned(v.parameters.effectSend)<<','
              <<unsigned(v.parameters.filter)<<','<<v.parameters.filterEnvelope1<<','
              <<v.parameters.filterEnvelope2<<','<<v.voiceFlags0<<','<<v.voiceFlags1<<','
              <<int(v.releasing)<<char(10);
    }
    return 0;
}
int runMusicRender(App& app,unsigned cue,double seconds,bool stems){
    app.audio.enabled=true;
    const auto directory=app.root/"verification/music";
    fs::create_directories(directory);
    const std::string file(original::originalMusicCueDescriptor(cue).filename);
    const std::string name=file.substr(0,file.rfind('.'));
    const unsigned total=unsigned(seconds*44100.0);
    std::ofstream report(directory/(name+".txt"));
    report<<"cue "<<cue<<" "<<file<<", "<<seconds<<" seconds"<<"\n";
    report<<"stem,notes,peak,rms,retunes,chokes,peakVoices,unmatched,flagRejected,noChannel,cfgSeen,cfgMatched,cfgApplied,tlChecked,tlMismatch,tlRawMatch"<<"\n";
    const auto render=[&](unsigned channel,const std::string& label){
        app.audio.endResultMusic();
        app.audio.setDiagnosticMusicChannel(channel);
        const auto before=app.audio.selectionStatistics().notesStarted;
        app.audio.beginOriginalMusicCue(cue);
        std::vector<short> pcm;pcm.reserve(std::size_t(total)*2);
        double energy=0;int peak=0;
        for(unsigned sample=0;sample<total;++sample){
            if(sample%735==0)app.audio.tickResultMusic();
            const auto stereo=app.audio.renderStereo(0,0,0,0,false);
            pcm.push_back(stereo[0]);pcm.push_back(stereo[1]);
            peak=std::max(peak,std::max(std::abs(int(stereo[0])),std::abs(int(stereo[1]))));
            energy+=double(stereo[0])*stereo[0]+double(stereo[1])*stereo[1];
        }
        writeMusicWav(directory/(name+"-"+label+".wav"),pcm);
        const auto& after=app.audio.selectionStatistics();
        const auto notes=after.notesStarted-before;
        report<<label<<','<<notes<<','<<peak<<','<<int(std::sqrt(energy/(2.0*total)))
              <<','<<after.legatoRetunes<<','<<after.percussionChokes<<','<<after.peakVoices<<','<<after.retunesUnmatched<<','<<after.retunesFlagRejected<<','<<after.retunesNoChannel<<','<<after.configuresSeen<<','<<after.configuresApplied<<','<<after.parameterChanges<<','<<after.levelChecked<<','<<after.levelMismatched<<','<<after.levelRawMatches<<char(10);
        report.flush();
    };
    app.audio.dspWetEnergy=app.audio.dspInputEnergy=0;
    render(~0u,"mix");
    {const auto& st=app.audio.selectionStatistics();report<<"manager sourceLevel="<<st.sourceLevel<<" fadeLevel="<<st.fadeLevel<<char(10);report<<"total-level delta (ours minus driver) per channel:"<<char(10);for(unsigned c=0;c<16;++c)if(st.levelMismatchPerChannel[c])report<<"  ch"<<c<<": n="<<st.levelMismatchPerChannel[c]<<" mean="<<double(st.levelDeltaSum[c])/st.levelMismatchPerChannel[c]<<" min="<<st.levelDeltaMin[c]<<" max="<<st.levelDeltaMax[c]<<char(10);}
    report<<"dsp input energy "<<app.audio.dspInputEnergy<<", wet energy "<<app.audio.dspWetEnergy<<char(10);
    // Energy alone does not say whether the authored reverb is the one
    // running. Registration installs muted 0x10 routes, so a scene select
    // that quietly finds nothing leaves the sends going nowhere.
    if(const auto* dsp=app.audio.dspRuntime()){
        const auto d=dsp->diagnostics();
        report<<"dsp program active="<<d.activeProgram<<" bank="<<d.activeBank<<" preset="<<d.activePreset
              <<" loads="<<d.loadCount<<" clears="<<d.clearCount<<" ringCode="<<d.ringCode<<char(10);
        report<<"dsp effect routes:";
        for(const auto route:dsp->processor().program().effectRoutes)report<<" 0x"<<std::hex<<route<<std::dec;
        report<<char(10);
        const auto& program=dsp->processor().program();
        unsigned instructions=0;for(const auto& word:program.instructions)if(word[0]||word[1]||word[2]||word[3])++instructions;
        unsigned coefficients=0;for(const auto value:program.coefficients)if(value)++coefficients;
        report<<"dsp program steps="<<instructions<<" coefficients="<<coefficients
              <<" ringWords="<<program.ringLengthWords<<char(10);
    }
    if(stems)for(unsigned channel=0;channel<16;++channel)render(channel,"ch"+std::to_string(channel));
    app.audio.setDiagnosticMusicChannel(~0u);app.audio.endResultMusic();
    return 0;
}

// Writes every race HUD bank chunk as its own image so an element that is not
// ported yet can be found by eye. Development diagnostic; nothing ships.
int runHudChunkDump(App& app,int w=192,int h=144){
    const auto directory=app.root/(w==192&&h==144?"verification/hud-chunks"
        :"verification/hud-chunks-"+std::to_string(w)+"x"+std::to_string(h));
    fs::create_directories(directory);
    std::vector<std::uint32_t> argb(std::size_t(w)*h);
    const unsigned count=app.hud.originalBank().chunkCount();
    unsigned drawn=0;
    std::ofstream bounds(directory/"authored-bounds.csv");
    bounds<<"chunk,x,y,width,height\n";
    for(unsigned chunk=0;chunk<count;++chunk){
        std::fill(argb.begin(),argb.end(),0xff101418u);
        app.hud.originalBank().paintChunk(argb,w,h,chunk);
        bool any=false;for(const auto pixel:argb)if((pixel&0xffffffu)!=0x101418u){any=true;break;}
        if(any)++drawn;
        // Second pass at the authored position, in original 640x480 pixels.
        std::vector<std::uint32_t> placed(640u*480u,0xff101418u);
        app.hud.originalBank().paintAuthoredChunk(placed,640,480,chunk);
        int x0=640,y0=480,x1=-1,y1=-1;
        for(int y=0;y<480;++y)for(int x=0;x<640;++x)if((placed[std::size_t(y)*640+x]&0xffffffu)!=0x101418u){
            x0=std::min(x0,x);y0=std::min(y0,y);x1=std::max(x1,x);y1=std::max(y1,y);}
        if(x1>=0)bounds<<chunk<<","<<x0<<","<<y0<<","<<(x1-x0+1)<<","<<(y1-y0+1)<<"\n";
        else bounds<<chunk<<",,,,\n";
        std::ofstream out(directory/("chunk_"+std::string(3-std::to_string(chunk).size(),'0')+std::to_string(chunk)+".ppm"),std::ios::binary);
        out<<"P6"<<char(10)<<w<<" "<<h<<char(10)<<"255"<<char(10);
        for(const auto pixel:argb){const char rgb[3]{char(pixel>>16),char(pixel>>8),char(pixel)};out.write(rgb,3);}
    }
    std::ofstream report(directory/"chunks.txt");
    report<<count<<" chunks, "<<drawn<<" with visible geometry"<<"\n";
    return 0;
}

// Paints the race HUD at several points along a course so the course map can
// be looked at: it turns with the car, so a straight and a hairpin should not
// look alike. Development preview; no graphics device is used.
int runMapPreview(App& app){
    app.hud.resize(1280,720);
    const auto directory=app.root/"verification/map";
    fs::create_directories(directory);
    RaceClock clock{};clock.phase=RacePhase::Running;
    unsigned written=0;
    for(const float fraction:{.06f,.18f,.30f,.42f,.54f,.66f}){
        const float distance=app.course.length*fraction;
        const auto here=app.course.sample(distance);
        // A racing gap, not a lap: the opponent is normally close.
        const auto ahead=app.course.sample(std::min(app.course.length,distance+34.f));
        VehicleState car{};car.position=here.center;
        car.yaw=std::atan2(here.tangent.x,here.tangent.z);
        VehicleState rival{};rival.position=ahead.center;rival.yaw=car.yaw;
        UiState state;state.menu=false;state.course=&app.course;state.car=&car;
        state.race=&clock;state.rival=&rival;state.progress=distance;state.battle=true;
        const auto* argb=app.hud.paint(state);
        if(!argb)throw std::runtime_error("The HUD produced no image");
        // Raw RGBA: the field is translucent, so the alpha has to survive.
        std::ofstream out(directory/("map_"+std::to_string(unsigned(fraction*100))+".rgba"),std::ios::binary);
        for(int i=0;i<1280*720;++i){const auto p=argb[i];
            const char rgba[4]{char(p>>16),char(p>>8),char(p),char(p>>24)};out.write(rgba,4);}
        ++written;
    }
    // Unity does not read the pixel buffer: it collects UI triangles. Paint the
    // same frame with that capture on and report what the map actually emits,
    // because a map that only writes pixels is invisible in the player.
    const auto here=app.course.sample(app.course.length*.3f);
    VehicleState car{};car.position=here.center;
    car.yaw=std::atan2(here.tangent.x,here.tangent.z);
    VehicleState rival{};rival.position=app.course.sample(app.course.length*.3f+34.f).center;
    UiState state;state.menu=false;state.course=&app.course;state.car=&car;
    RaceClock running{};running.phase=RacePhase::Running;
    state.race=&running;state.rival=&rival;state.battle=true;
    Idas3UiEnable(1);Idas3UiBeginFrame(1280,720);
    app.hud.paint(state);
    idas3::unityUiSubmit(app.hud.overlayPixels(),1280,720,false,false,false);
    idas3::UnityUiFrame frame{sizeof(idas3::UnityUiFrame)};
    Idas3UiGetFrame(&frame);
    std::vector<idas3::UnityUiVertex> vertices(frame.vertexCount);
    Idas3UiCopyVertices(vertices.data(),int(vertices.size()));
    std::vector<idas3::UnityUiDraw> draws(frame.drawCount);
    Idas3UiCopyDraws(draws.data(),int(draws.size()));
    // The map occupies the bottom-left of the source canvas.
    unsigned inMap=0;float mapLeft=1e9f,mapTop=1e9f,mapRight=-1e9f,mapBottom=-1e9f;
    for(const auto& v:vertices)if(v.x>=20.f&&v.x<=190.f&&v.y>=527.f&&v.y<=697.f){
        ++inMap;mapLeft=std::min(mapLeft,v.x);mapTop=std::min(mapTop,v.y);
        mapRight=std::max(mapRight,v.x);mapBottom=std::max(mapBottom,v.y);
    }
    Idas3UiEnable(0);
    std::ofstream report(directory/"map.txt");
    report<<written<<" course map frames, course "<<app.course.name<<", length "<<app.course.length<<"\n";
    report<<"unity ui draws "<<frame.drawCount<<", vertices "<<frame.vertexCount
        <<", in the map corner "<<inMap<<" spanning "<<mapLeft<<","<<mapTop<<" to "<<mapRight<<","<<mapBottom<<"\n";
    // What covers the middle of the field has to be see-through, or the panel
    // reads as solid however the colours are chosen.
    std::map<unsigned,unsigned> alphas;
    const float midX=20.f+(190.f-20.f)*.25f,midY=527.f+(697.f-527.f)*.25f;
    for(unsigned d=0;d<frame.drawCount;++d){
        const auto& draw=draws[d];
        for(unsigned t=draw.first;t+2<draw.first+draw.count;t+=3){
            float lo=1e9f,hi=-1e9f,lo2=1e9f,hi2=-1e9f;
            for(unsigned k=0;k<3;++k){const auto& v=vertices[t+k];
                lo=std::min(lo,v.x);hi=std::max(hi,v.x);lo2=std::min(lo2,v.y);hi2=std::max(hi2,v.y);}
            if(lo<=midX&&hi>=midX&&lo2<=midY&&hi2>=midY)++alphas[vertices[t].argb>>24];
        }
    }
    report<<"covering the centre of the field:";
    for(const auto& [alpha,count]:alphas)report<<" alpha "<<alpha<<" x"<<count;
    report<<"\n";
    if(!inMap)throw std::runtime_error("The course map emitted no Unity draws");
    return 0;
}

// Walks the save-file screen and writes what it draws, so the five files, the
// selection and the empty state can be looked at without a cabinet.
// Proves the pre-save-file driver directory is carried into save files without
// the shared directory being touched. It runs entirely inside a private save
// root that this check makes and removes; no real driver save is read.
int runSaveMigrationCheck(App& app){
    const auto sandbox=app.root/"verification/save-migration";
    fs::remove_all(sandbox);
    fs::create_directories(sandbox);
    app.saveRoot=sandbox;app.settings();
    const auto legacy=sandbox/"driver_profiles_v1";
    fs::create_directories(legacy);
    const LocalDriverProfiles seeded(legacy);
    const LocalDriverSetup seededSetup(legacy);
    for(const unsigned car:{9u,22u}){
        auto profile=original::makeOriginalFreshBattleProfile();
        profile.setu(16,car);profile.setu(76,3);
        for(unsigned i=0;i<3;++i)profile.setu(44+4*i,30+i);
        if(!seeded.save(car,profile)||!seededSetup.markComplete(car))
            throw std::runtime_error("Could not seed a pre-save-file driver");
    }
    std::map<std::string,std::string> before;
    const auto scan=[&](std::map<std::string,std::string>& into){
        for(const auto& entry:fs::recursive_directory_iterator(legacy)){
            if(!entry.is_regular_file())continue;
            std::ifstream in(entry.path(),std::ios::binary);
            into[entry.path().filename().string()]=std::string(std::istreambuf_iterator<char>(in),{});
        }
    };
    scan(before);
    app.refreshSaveFiles();
    if(app.frontend.saveFiles[0].car!=App::originalCarName(9))
        throw std::runtime_error("The first driver did not become the first save file");
    if(app.frontend.saveFiles[1].car!=App::originalCarName(22))
        throw std::runtime_error("The second driver did not become the second save file");
    if(app.frontend.saveFiles[2].used)throw std::runtime_error("A third file appeared from nowhere");
    if(app.saveSlots.at(0).nameLength!=3||app.saveSlots.at(0).nameGlyphs[0]!=30)
        throw std::runtime_error("The carried file lost the driver's name");
    // Its own profile must be readable from the new file's directory.
    const LocalDriverProfiles carried(app.saveSlots.profileDirectory(0));
    if(carried.load(9).origin!=LocalDriverProfiles::Origin::Saved)
        throw std::runtime_error("The carried file cannot read its own driver profile");
    std::map<std::string,std::string> after;scan(after);
    if(before!=after)throw std::runtime_error("Migration modified the shared driver directory");
    fs::create_directories(app.root/"verification");
    std::ofstream(app.root/"verification/save-migration.txt")<<"PASS save migration: "<<before.size()<<" shared files unchanged, two drivers carried into files 1 and 2 with their names and profiles."<<char(10);
    fs::remove_all(sandbox);
    return 0;
}
// Renders the pre-race loading screen so its authored layout can be checked.
// Paints the start banner for a spread of configurations so the authored
// selection can be looked at. The two day/night chunks and the two weather
// chunks share a rectangle and a texture and differ only in the sub-rectangle
// their own UVs pick, so which is which is settled by looking, not by reading
// the manifest -- it records vertex counts, not UVs.
int runVsBannerPreview(App& app){
    const auto directory=app.root/"verification/vs-banner";
    fs::create_directories(directory);
    if(!OriginalVsBanner::available(app.root))throw std::runtime_error("Start banner assets are not imported");
    OriginalVsBanner banner;banner.load(app.root);
    std::ofstream report(directory/"vs-banner.txt");
    struct Case {const char* name;OriginalVsBannerSetup setup;};
    const Case cases[]{
        {"akina-uphill-day-dry",{3,0,true,false,false,false,1,false}},
        {"akina-downhill-night-wet",{3,1,true,true,true,false,2,false}},
        {"akinasnow-outbound-night-snow",{8,2,true,true,false,true,3,false}},
        {"myogi-inbound-day-dry-final",{0,3,true,false,false,false,0,false}},
        {"tsuchisaka-clockwise-day-dry-extra",{7,4,true,false,false,false,1,true}},
        {"irohazaka-counter-night-dry",{5,5,true,true,false,false,5,false}},
        {"usui-reverse-day-wet",{1,6,true,false,true,false,4,false}},
        {"happogahara-uphill-day-dry",{4,0,true,false,false,false,2,false}}};
    for(const auto& one:cases){
        banner.begin(one.setup);
        std::vector<std::uint32_t> canvas(640*480,0xff000000u);
        unityUiClear(canvas.data(),640,480,0xff000000u);
        banner.paint(canvas,640,480);
        std::ofstream out(directory/(std::string(one.name)+".ppm"),std::ios::binary);
        out<<"P6"<<char(10)<<640<<" "<<480<<char(10)<<"255"<<char(10);
        for(int i=0;i<640*480;++i){const auto pixel=canvas[std::size_t(i)];
            const char rgb[3]{char(pixel>>16),char(pixel>>8),char(pixel)};out.write(rgb,3);}
        report<<one.name<<" chunks";
        for(const auto chunk:banner.chunks())report<<" "<<chunk;
        report<<char(10);
    }
    report<<"unsequenced VS zoom placements: "<<banner.unsequencedZoomChunks().size()<<char(10);
    return 0;
}
int runLoadingPreview(App& app){
    const auto directory=app.root/"verification/loading";
    fs::create_directories(directory);
    OriginalLoadingScreen screen;
    if(!OriginalLoadingScreen::available(app.root))throw std::runtime_error("Loading screen assets are not imported");
    screen.load(app.root);
    std::ofstream report(directory/"loading.txt");
    report<<"artwork banks decoded: "<<screen.artworkCount()<<char(10);
    for(std::uint32_t shot=0;shot<screen.artworkCount();++shot){
        screen.begin(shot);
        std::vector<std::uint32_t> canvas(640*480,0xff000000u);
        unityUiClear(canvas.data(),640,480,0xff000000u);
        screen.paint(canvas,640,480);
        std::ofstream out(directory/("loading_"+std::to_string(screen.artwork())+".ppm"),std::ios::binary);
        out<<"P6"<<char(10)<<640<<" "<<480<<char(10)<<"255"<<char(10);
        for(int i=0;i<640*480;++i){const auto p=canvas[std::size_t(i)];
            const char rgb[3]{char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}
        report<<"painted artwork "<<screen.artwork()<<char(10);
    }
    return 0;
}
// The delivery target draws the UI as a triangle list, not as pixels, so a
// screen built out of raw pixel writes renders as nothing there while looking
// perfect in this renderer. This paints the save screen with the capture on and
// checks it actually produced geometry.
int runSaveFileUiCheck(App& app){
    app.frontend.stage=FrontendStage::SaveSelect;
    app.refreshSaveFiles();
    app.frontend.saveFiles[0]={true,"TAKUMI",App::originalCarName(0),App::originalCarGrade(0),
                               "2026/09/04",169928,31};
    app.frontend.saveSelected=0;
    app.frontend.advance(0);
    // Paint once so the cached canvas belongs to a different selection, then
    // capture the repaint that a real selection change would cause.
    app.frontend.paint(640,480);
    Idas3UiEnable(1);
    Idas3UiBeginFrame(640,480);
    app.frontend.saveSelected=1;
    app.frontend.advance(0);
    const auto& canvas=app.frontend.paint(640,480);
    unityUiSubmit(canvas.data(),640,480,false,false,true);
    UnityUiFrame frame{};
    frame.size=sizeof(frame);   // the accessor refuses a struct that does not declare its size
    if(!Idas3UiGetFrame(&frame))throw std::runtime_error("The host UI frame could not be read");
    const auto stageAfter=int(app.frontend.stage);
    const auto selected=app.frontend.saveSelected;
    std::size_t painted=0;
    for(const auto pixel:canvas)if((pixel&0xffffffu)!=0x0e1a36u)++painted;
    fs::create_directories(app.root/"verification");
    std::ofstream report(app.root/"verification/save-file-ui.txt");
    report<<"draws "<<frame.drawCount<<" vertices "<<frame.vertexCount
          <<" textures "<<frame.textureCount<<" unresolved "<<frame.unresolvedSurfaces
          <<" stage "<<stageAfter<<" selected "<<selected
          <<" canvas pixels touched "<<painted<<char(10);
    if(frame.unresolvedSurfaces)throw std::runtime_error("The save screen referenced a surface the host never saw");
    // A screen of plates, captions and a control strip is hundreds of quads.
    // The pixel-write version of this screen produced a handful.
    if(frame.drawCount<200)
        throw std::runtime_error("The save screen produced almost no UI geometry; it is drawing pixels the host cannot see");
    if(frame.textureCount<2)
        throw std::runtime_error("The save screen bound no glyph atlas, so its text cannot appear");
    // Counting draws proves the screen produced geometry; it does not prove the
    // geometry looks like anything. Replay the captured triangles the way the
    // host would and write the result out, so the Unity path can actually be
    // looked at instead of inferred.
    std::vector<UnityUiDraw> draws(frame.drawCount);
    std::vector<UnityUiVertex> vertices(frame.vertexCount);
    if(Idas3UiCopyDraws(draws.data(),int(draws.size()))!=int(draws.size())||
       Idas3UiCopyVertices(vertices.data(),int(vertices.size()))!=int(vertices.size()))
        throw std::runtime_error("The captured UI geometry could not be read back");
    std::vector<std::vector<std::uint32_t>> images(frame.textureCount);
    std::vector<std::pair<unsigned,unsigned>> sizes(frame.textureCount);
    for(unsigned t=0;t<frame.textureCount;++t){
        UnityUiTextureInfo info{};info.size=sizeof(info);
        if(!Idas3UiGetTextureInfo(t,&info))throw std::runtime_error("A bound UI texture could not be described");
        sizes[t]={info.width,info.height};
        std::vector<std::uint8_t> rgba(info.bytes);
        if(Idas3UiCopyTextureRGBA(t,rgba.data(),int(rgba.size()))!=int(rgba.size()))
            throw std::runtime_error("A bound UI texture could not be read");
        images[t].resize(std::size_t(info.width)*info.height);
        for(std::size_t i=0;i<images[t].size();++i)
            images[t][i]=(std::uint32_t(rgba[i*4+3])<<24)|(std::uint32_t(rgba[i*4])<<16)|
                         (std::uint32_t(rgba[i*4+1])<<8)|rgba[i*4+2];
    }
    std::vector<std::uint32_t> replay(std::size_t(640)*480,0xff000000u);
    const auto sample=[&](unsigned texture,float u,float v){
        if(texture>=images.size()||images[texture].empty())return 0xffffffffu;
        const auto [w,h]=sizes[texture];
        const auto x=std::min(w-1,unsigned(std::max(0.f,u)*float(w)));
        const auto y=std::min(h-1,unsigned(std::max(0.f,v)*float(h)));
        return images[texture][std::size_t(y)*w+x];
    };
    for(const auto& draw:draws){
        for(unsigned i=draw.first;i+2<draw.first+draw.count;i+=3){
            const auto& a=vertices[i];const auto& b=vertices[i+1];const auto& c=vertices[i+2];
            const float area=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
            if(std::abs(area)<1e-6f)continue;
            const int x0=std::max(0,int(std::floor(std::min({a.x,b.x,c.x}))));
            const int x1=std::min(640,int(std::ceil(std::max({a.x,b.x,c.x}))));
            const int y0=std::max(0,int(std::floor(std::min({a.y,b.y,c.y}))));
            const int y1=std::min(480,int(std::ceil(std::max({a.y,b.y,c.y}))));
            for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x){
                const float px=float(x)+.5f,py=float(y)+.5f;
                float w0=((b.x-a.x)*(py-a.y)-(b.y-a.y)*(px-a.x))/area;
                float w1=((c.x-b.x)*(py-b.y)-(c.y-b.y)*(px-b.x))/area;
                float w2=((a.x-c.x)*(py-c.y)-(a.y-c.y)*(px-c.x))/area;
                if(w0<0||w1<0||w2<0)continue;
                const float la=w1,lb=w2,lc=w0;
                const auto texel=sample(draw.texture,la*a.u+lb*b.u+lc*c.u,la*a.v+lb*b.v+lc*c.v);
                const auto tint=a.argb;
                unsigned alpha=((texel>>24)&255)*((tint>>24)&255)/255;
                alpha=unsigned(float(alpha)*draw.opacity);
                if(!alpha)continue;
                auto& destination=replay[std::size_t(y)*640+x];
                const auto mix=[&](unsigned shift){
                    const unsigned over=((texel>>shift)&255)*((tint>>shift)&255)/255;
                    const unsigned under=(destination>>shift)&255;
                    return ((over*alpha+under*(255-alpha))/255)<<shift;
                };
                destination=0xff000000u|mix(16)|mix(8)|mix(0);
            }
        }
    }
    std::ofstream out(app.root/"verification/save-file-ui.ppm",std::ios::binary);
    out<<"P6"<<char(10)<<640<<" "<<480<<char(10)<<"255"<<char(10);
    for(const auto pixel:replay){const char rgb[3]{char(pixel>>16),char(pixel>>8),char(pixel)};out.write(rgb,3);}
    Idas3UiEnable(0);
    report<<"PASS the save screen reaches the host as geometry"<<char(10);
    return 0;
}
int runSaveFilePreview(App& app){
    const auto directory=app.root/"verification/save-files";
    fs::create_directories(directory);
    app.frontend.stage=FrontendStage::SaveSelect;
    app.refreshSaveFiles();
    // With a save root supplied the real files are what is being looked at, so
    // the stand-ins stay out of the way.
    const bool fixtures=app.saveRoot.empty();
    // Fill two files in so a populated row and an empty one both appear.
    if(fixtures)app.frontend.saveFiles[0]={true,"TAKUMI",App::originalCarName(0),App::originalCarGrade(0),"2026/09/04",169928,31};
    if(fixtures)app.frontend.saveFiles[1]={true,"AAAAA",App::originalCarName(22),App::originalCarGrade(22),"2026/09/06",45655,9};
    if(fixtures)app.frontend.saveFiles[2]={true,"KEI",App::originalCarName(8),App::originalCarGrade(8),"2026/09/07",14241,2};
    std::ofstream report(directory/"save-files.txt");
    for(int slot=0;slot<int(LocalSaveSlots::count);++slot){
        app.frontend.saveSelected=slot;
        app.frontend.advance(0);
        const auto& canvas=app.frontend.paint(640,480);
        std::ofstream out(directory/("slot_"+std::to_string(slot+1)+".ppm"),std::ios::binary);
        out<<"P6"<<char(10)<<640<<" "<<480<<char(10)<<"255"<<char(10);
        for(int i=0;i<640*480;++i){const auto p=canvas[std::size_t(i)];
            const char rgb[3]{char(p>>16),char(p>>8),char(p)};out.write(rgb,3);}
        report<<"slot "<<(slot+1)<<" used "<<app.frontend.saveFiles[std::size_t(slot)].used<<"\n";
    }
    // And a real frame, so the car drawn through the panel aperture can be
    // looked at rather than assumed.
    app.frontend.saveSelected=1;app.frontend.car=22;app.frontend.make=App::originalCarMake(22);
    app.frontend.battleProfile=original::makeOriginalFreshBattleProfile();
    app.frontend.battleProfile.setu(16,22);app.frontend.battleProfile.setu(1180,app.frontend.battleProfile.u(1180)|1u);
    app.loadedProfileCar=22;
    if(!app.renderer.initialize(nullptr,1280,720,true))throw std::runtime_error(app.renderer.error);
    app.frontend.advance(0);
    if(!app.renderSaveSelect())throw std::runtime_error(app.renderer.error);
    if(!app.renderer.saveBitmap((directory/"panel.bmp").wstring()))throw std::runtime_error("Save panel capture failed");
    report<<"panel aperture captured at 1280x720\n";
    // The showroom turns its car; the panel inherits that. Sample it so the
    // turn can be seen rather than assumed.
    for(unsigned frame=1,shot=1;frame<=480;++frame){
        if(!app.renderSaveSelect(1./60.))throw std::runtime_error(app.renderer.error);
        if(frame%120)continue;
        if(!app.renderer.saveBitmap((directory/("turn_"+std::to_string(shot++)+".bmp")).wstring()))
            throw std::runtime_error("Panel turn capture failed");
    }
    report<<"panel car yaw after 8 seconds: "<<app.showroom.yaw()<<" rad"<<char(10);
    // The panel is supposed to show the file's own car as the file left it, not
    // a showroom-stock one. Render the same car twice, once with nothing fitted
    // and once with its saved parts, and require the two frames to differ.
    const auto panelPixels=[&](const char* name){
        if(!app.renderSaveSelect())throw std::runtime_error(app.renderer.error);
        if(!app.renderer.saveBitmap((directory/name).wstring()))
            throw std::runtime_error("Panel parts capture failed");
        std::ifstream in(directory/name,std::ios::binary);
        std::vector<char> bytes((std::istreambuf_iterator<char>(in)),{});
        return bytes;
    };
    app.showroom.reset();
    app.frontend.battleProfile=original::makeOriginalFreshBattleProfile();
    app.frontend.battleProfile.setu(16,22);
    for(unsigned slot=0;slot<10;++slot)app.frontend.battleProfile.setByte(156+slot,0);
    app.loadedCar=-1;
    const auto stock=panelPixels("parts_stock.bmp");
    // Bytes156..165 are the saved player parts the appearance word is built
    // from; 0630B4..06316E reads exactly these.
    for(unsigned slot=0;slot<8;++slot)app.frontend.battleProfile.setByte(156+slot,1);
    app.loadedCar=-1;
    const auto fitted=panelPixels("parts_fitted.bmp");
    if(stock.size()!=fitted.size())throw std::runtime_error("Panel parts frames differ in size");
    std::size_t changed=0;
    for(std::size_t i=0;i<stock.size();++i)if(stock[i]!=fitted[i])++changed;
    if(!changed)throw std::runtime_error("Fitting parts changed nothing in the panel");
    report<<"panel parts: "<<changed<<" bytes differ between a stock and a fitted car"<<char(10);
    // The delivery target is Unity, which reads the published scene frame
    // rather than this renderer's target. Check the aperture survives that
    // publication: the same rectangle, and the flag that stops the host from
    // widening it back to the whole screen.
    App capture;
    capture.root=app.root;capture.validationMode=true;capture.saveRoot=app.saveRoot;
    capture.settings();capture.frontend.initialize(app.root,true);
    capture.frontend.stage=FrontendStage::SaveSelect;
    capture.frontend.saveFiles=app.frontend.saveFiles;
    capture.frontend.saveSelected=1;capture.frontend.car=22;capture.frontend.make=App::originalCarMake(22);
    capture.frontend.battleProfile=original::makeOriginalFreshBattleProfile();
    capture.frontend.battleProfile.setu(16,22);
    capture.loadedProfileCar=22;
    if(!capture.renderer.initializeSceneCapture(1280,720))throw std::runtime_error(capture.renderer.error);
    capture.frontend.advance(0);
    if(!capture.renderSaveSelect())throw std::runtime_error(capture.renderer.error);
    const auto* scene=capture.renderer.sceneCapture();
    if(!scene)throw std::runtime_error("The save screen published no scene frame");
    const auto& published=scene->frame().cameras[0];
    const float fit=720.f/480.f,originX=(1280.f-640.f*fit)*.5f;
    const float wanted[4]{originX+App::savePanelBox[0]*fit,App::savePanelBox[1]*fit,
                          App::savePanelBox[2]*fit,App::savePanelBox[3]*fit};
    for(int i=0;i<4;++i)
        if(std::abs(published.viewport[i]-wanted[i])>.01f)
            throw std::runtime_error("The published aperture is not the panel box");
    if(!(published.leftHanded&2u))
        throw std::runtime_error("The published aperture is not marked, so the host would widen it");
    report<<"published aperture "<<published.viewport[0]<<","<<published.viewport[1]<<" "
          <<published.viewport[2]<<"x"<<published.viewport[3]<<" flagged as a panel\n";
    return 0;
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show){
    SetProcessDPIAware();App app;wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);app.root=fs::path(executable).parent_path().parent_path();
    int argc=0;wchar_t** argv=CommandLineToArgvW(GetCommandLineW(),&argc);bool smoke=false,headless=false,matrix=false,nightSmoke=false,reverseSmoke=false,wetSmoke=false,routeSmoke=false,menuMotion=false,menuFlow=false,legendPreview=false,legendSmoke=false,legendVisitSmoke=false,legendRunSmoke=false,legendConditions=false,finishBanner=false,dialogueMusic=false,legendLossRun=false,musicStems=false,hudChunkDump=false,mapPreview=false,saveFilePreview=false,buntaPreview=false,buntaSmoke=false,resultPreview=false,handlingAudit=false,parameterDump=false;int smokeCourse=3,smokeRival=0,smokeBuntaLevel=0,musicRenderCue=-1,previewWidth=1280,previewHeight=720,hudChunkWidth=192,hudChunkHeight=144;double musicSeconds=45;fs::path smokePath;
    for(int i=1;i<argc;i++){std::wstring arg=argv[i];if(arg==L"--root"&&i+1<argc)app.root=argv[++i];else if(arg==L"--render-smoke"||arg==L"--menu-motion-preview"||arg==L"--menu-flow-preview"){smoke=true;menuMotion=arg==L"--menu-motion-preview";menuFlow=arg==L"--menu-flow-preview";if(i+1<argc)smokePath=argv[++i];}else if(arg==L"--night-preview")nightSmoke=true;else if(arg==L"--wet-preview")wetSmoke=true;else if(arg==L"--reverse-preview")reverseSmoke=true;else if(arg==L"--preview-course"&&i+1<argc)smokeCourse=std::clamp(std::stoi(argv[++i]),0,8);else if(arg==L"--headless-smoke")headless=true;else if(arg==L"--verify-courses")matrix=true;else if(arg==L"--verify-original-routes")routeSmoke=true;else if(arg==L"--preview-size"&&i+2<argc){previewWidth=std::clamp(std::stoi(argv[i+1]),320,7680);previewHeight=std::clamp(std::stoi(argv[i+2]),240,4320);i+=2;}}
    bool engineDrivingCheck=false,tireDrivingCheck=false;
    for(int i=1;i<argc;i++){
        const std::wstring arg=argv[i];
        if(arg==L"--legend-preview")legendPreview=true;
        else if(arg==L"--verify-legend-battles")legendSmoke=true;
        else if(arg==L"--verify-legend-visit")legendVisitSmoke=true;
        else if(arg==L"--verify-legend-run")legendRunSmoke=true;
        else if(arg==L"--verify-legend-conditions")legendConditions=true;
        else if(arg==L"--verify-finish-banner")finishBanner=true;
        else if(arg==L"--verify-legend-loss-run")legendLossRun=true;
        else if(arg==L"--render-music"&&i+1<argc)musicRenderCue=int(std::stoi(argv[++i]));
        else if(arg==L"--music-seconds"&&i+1<argc)musicSeconds=std::stod(argv[++i]);
        else if(arg==L"--music-stems")musicStems=true;
        else if(arg==L"--dump-hud-chunks")hudChunkDump=true;
        else if(arg==L"--hud-chunk-size"&&i+2<argc){hudChunkWidth=_wtoi(argv[i+1]);hudChunkHeight=_wtoi(argv[i+2]);i+=2;}
        else if(arg==L"--map-preview")mapPreview=true;
        else if(arg==L"--save-file-preview")saveFilePreview=true;
        else if(arg==L"--verify-dialogue-music")dialogueMusic=true;
        else if(arg==L"--bunta-preview")buntaPreview=true;
        else if(arg==L"--result-preview")resultPreview=true;
        else if(arg==L"--verify-bunta-battles")buntaSmoke=true;
        else if(arg==L"--preview-bunta-level"&&i+1<argc)smokeBuntaLevel=std::clamp(std::stoi(argv[++i]),0,16);
        else if(arg==L"--preview-rival"&&i+1<argc)smokeRival=std::clamp(std::stoi(argv[++i]),0,5);
        else if(arg==L"--chase-preview")app.drivingView=OriginalDrivingView::Chase;
        else if(arg==L"--engine-driving-check")engineDrivingCheck=true;
        else if(arg==L"--handling-audit")handlingAudit=true;
        else if(arg==L"--parameter-dump")parameterDump=true;
        else if(arg==L"--tire-driving-check")engineDrivingCheck=tireDrivingCheck=true;
    }
    bool factoryPaintPreview=false;
    bool saveMigrationCheck=false,loadingPreview=false,saveFileUiCheck=false,vsBannerPreview=false,preRaceDialogueCheck=false,preRaceGateCheck=false;
    int voiceDumpCue=-1,voiceDumpTick=0;
    for(int i=1;i<argc;++i){
        if(std::wstring(argv[i])==L"--verify-save-migration")saveMigrationCheck=true;
        else if(std::wstring(argv[i])==L"--loading-preview")loadingPreview=true;
        else if(std::wstring(argv[i])==L"--verify-save-file-ui")saveFileUiCheck=true;
        else if(std::wstring(argv[i])==L"--vs-banner-preview")vsBannerPreview=true;
        else if(std::wstring(argv[i])==L"--verify-pre-race-dialogue")preRaceDialogueCheck=true;
        else if(std::wstring(argv[i])==L"--report-pre-race-gate")preRaceGateCheck=true;
        else if(std::wstring(argv[i])==L"--music-voice-dump"&&i+2<argc){
            voiceDumpCue=std::stoi(argv[i+1]);voiceDumpTick=std::stoi(argv[i+2]);i+=2;
        }
        // A private save root, so a diagnostic never touches a driver's files.
        else if(std::wstring(argv[i])==L"--saves"&&i+1<argc)app.saveRoot=argv[++i];
    }
    for(int i=1;i<argc;++i)if(std::wstring(argv[i])==L"--factory-paint-preview"){
        factoryPaintPreview=true;smoke=true;if(i+1<argc)smokePath=argv[++i];
    }
    LocalFree(argv);
    try{
        app.validationMode=smoke||headless||matrix||routeSmoke||legendSmoke||legendVisitSmoke||legendRunSmoke||legendConditions||finishBanner||dialogueMusic||legendLossRun||musicRenderCue>=0||hudChunkDump||mapPreview||saveFilePreview||saveMigrationCheck||loadingPreview||saveFileUiCheck||vsBannerPreview||preRaceDialogueCheck||preRaceGateCheck||voiceDumpCue>=0||buntaSmoke||engineDrivingCheck||handlingAudit||parameterDump;
        app.settings();app.originalCamera=OriginalChaseCamera::load(app.root);app.bumperCamera=OriginalChaseCamera::load(app.root,OriginalDrivingView::Bumper);app.frontend.initialize(app.root,true);if(OriginalLoadingScreen::available(app.root))app.loadingScreen.load(app.root);if(OriginalVsBanner::available(app.root)){app.vsBanner.load(app.root);app.vsBannerLoaded=true;}app.hud.loadOriginal(app.root);app.frontend.course=app.courseIndex;app.frontend.reverse=app.reverse;app.frontend.wet=app.wet;app.frontend.night=app.night;app.frontend.automatic=app.automatic;
        {std::ifstream saved(app.userdataRoot()/"native_selection.txt");int make,car;if(saved>>make>>car){app.frontend.make=std::clamp(make,0,6);app.frontend.car=std::clamp(car,0,34);}}
        app.load();app.audio.configure(app.root);
        if(saveMigrationCheck)return runSaveMigrationCheck(app);
        if(loadingPreview)return runLoadingPreview(app);
        if(saveFileUiCheck)return runSaveFileUiCheck(app);
        if(vsBannerPreview)return runVsBannerPreview(app);
        if(preRaceDialogueCheck)return runPreRaceDialogueSmoke(app);
        if(preRaceGateCheck)return runPreRaceGateCheck(app);
        if(voiceDumpCue>=0)return runMusicVoiceDump(app,unsigned(voiceDumpCue),unsigned(voiceDumpTick));
        if(engineDrivingCheck)return runEngineDrivingCheck(app,tireDrivingCheck);
        if(headless)return runHeadless(app);if(matrix)return runCourseMatrix(app);if(routeSmoke)return runOriginalRouteSmoke(app);if(legendSmoke)return runLegendBattleSmoke(app);if(legendVisitSmoke)return runLegendVisitSmoke(app);if(legendRunSmoke)return runLegendRunSmoke(app);if(legendConditions)return runLegendConditions(app);if(finishBanner)return runFinishBannerSmoke(app);if(dialogueMusic)return runDialogueMusicSmoke(app);if(legendLossRun)return runLegendLossRun(app);if(musicRenderCue>=0)return runMusicRender(app,unsigned(musicRenderCue),musicSeconds,musicStems);if(parameterDump)return runParameterDump(app);if(handlingAudit)return runHandlingAudit(app);if(hudChunkDump)return runHudChunkDump(app,hudChunkWidth,hudChunkHeight);if(mapPreview)return runMapPreview(app);if(saveFilePreview)return runSaveFilePreview(app);if(buntaSmoke)return runBuntaBattleSmoke(app);
        if(smoke){
            app.validationMode=true;app.frontend.car=0;app.frontend.make=6;app.courseIndex=smokeCourse;app.reverse=reverseSmoke;app.profile=0;app.night=nightSmoke;app.wet=wetSmoke;app.load();
            if(!app.renderer.initialize(nullptr,previewWidth,previewHeight,true))throw std::runtime_error(app.renderer.error);
            if(factoryPaintPreview)return runFactoryPaintPreview(app,smokePath.empty()?app.root/"verification/factory-paints":smokePath);
            if(menuFlow){
                if(smokePath.empty())smokePath=app.root/"verification/menu-flow/preview.bmp";
                fs::create_directories(smokePath.parent_path());
                std::ofstream flow(smokePath.parent_path()/"flow.csv");flow<<"stage,confirmation_frames,next_stage,in_menu\n";
                app.frontend.stage=FrontendStage::Title;app.frontend.gameMode=buntaPreview?original::OriginalGameMode::BuntaChallenge:legendPreview?original::OriginalGameMode::LegendOfTheStreets:original::OriginalGameMode::TimeAttack;
                if(buntaPreview){
                    auto& p=app.frontend.battleProfile;p=original::makeOriginalFreshBattleProfile();p.setu(16,unsigned(app.frontend.car));p.setu(72,10000);p.setu(1180,p.u(1180)|1u);
                    for(unsigned i=0;i<8;++i)p.setu(1080+i*4,unsigned(smokeBuntaLevel));
                }
                app.frontend.course=smokeCourse;app.frontend.reverse=reverseSmoke;app.frontend.wet=wetSmoke;app.frontend.night=nightSmoke;
                for(int transition=0;app.menu&&transition<12;++transition){
                    const auto stage=app.frontend.stage;
                    // Let the ordinary screen owner accept input after its
                    // entry fade; diagnostics use the same navigation gate.
                    for(unsigned entry=0;!app.frontend.inputReady()&&entry<32;++entry)
                        if(!app.render(1./60))throw std::runtime_error(app.renderer.error);
                    if(stage==FrontendStage::Mode){
                        const auto desired=buntaPreview?original::OriginalGameMode::BuntaChallenge:legendPreview?original::OriginalGameMode::LegendOfTheStreets:original::OriginalGameMode::TimeAttack;
                        app.frontend.change(int(desired)-int(app.frontend.gameMode));
                    }
                    if(stage==FrontendStage::Rival&&smokeRival)app.frontend.change(smokeRival);
                    if(!app.render(1./60))throw std::runtime_error(app.renderer.error);
                    const auto stem="stage-"+std::to_string(int(stage));
                    if(!app.renderer.saveBitmap((smokePath.parent_path()/(stem+".bmp")).wstring()))throw std::runtime_error("Menu flow capture failed");
                    if(app.frontend.confirm())app.start();
                    unsigned frames=0;
                    while(app.menu&&app.frontend.stage==stage&&frames<181){
                        ++frames;if(!app.render(1./60))throw std::runtime_error(app.renderer.error);
                        if(frames==18&&!app.renderer.saveBitmap((smokePath.parent_path()/(stem+"-confirm.bmp")).wstring()))throw std::runtime_error("Menu confirmation capture failed");
                    }
                    flow<<int(stage)<<','<<frames<<','<<int(app.frontend.stage)<<','<<app.menu<<'\n';
                    if(app.menu&&app.frontend.stage==stage)throw std::runtime_error("Menu confirmation did not advance");
                }
                if(app.menu||!app.originalHandling||app.race.phase!=RacePhase::Countdown||app.battle!=(legendPreview||buntaPreview)||app.bunta!=buntaPreview)throw std::runtime_error("Menu flow failed to start selected original mode");
                std::ofstream launched(smokePath.parent_path()/"launch.txt");
                launched<<"Course: "<<app.course.name<<"\nWet: "<<app.wet<<"\nNight: "<<app.night<<"\nOriginal wet flag: "<<app.presentedSession().vehicle().drive.u(0x434)<<"\nOriginal snow flag: "<<app.presentedSession().vehicle().drive.u(0x438)<<"\nRemaining6000: "<<app.race.remaining6000<<'\n';
                launched<<"Battle: "<<app.battle<<"\nRival model: "<<app.loadedRivalCar<<"\nEnemy: "<<app.loadedRivalEnemy<<'\n';
                if(!app.renderer.saveBitmap((smokePath.parent_path()/"race.bmp").wstring()))throw std::runtime_error("Menu-to-race capture failed");
                if(legendPreview||buntaPreview){
                    for(unsigned tick=0;tick<360;++tick){DriverInput input;input.throttle=.5f;input.automatic=true;app.simulate(input);}
                    if(!app.render(1./60)||!app.renderer.saveBitmap((smokePath.parent_path()/"race-moving.bmp").wstring()))throw std::runtime_error("Legend moving capture failed");
                    if(resultPreview){
                        for(unsigned tick=0;tick<30000&&app.race.phase!=RacePhase::Finished;++tick)app.simulate({});
                        if(app.race.phase!=RacePhase::Finished||!app.battleProgressApplied)throw std::runtime_error("Natural battle result did not settle");
                        // Result Init prepares the card candidate once, before
                        // the visible count; subsequent no-child paints are inert.
                        if(!app.render(0))throw std::runtime_error(app.renderer.error);
                        const auto committedProfile=app.battleProfile.words;
                        std::ofstream animation(smokePath.parent_path()/"result-animation.csv");
                        animation<<"frame,phase,displayed_balance,committed_balance,fade_alpha,in_menu\n";
                        for(unsigned frame=0;frame<427;++frame){
                            if(!app.render(1./60))throw std::runtime_error(app.renderer.error);
                            animation<<frame<<','<<app.resultAnimationFrame.sourcePhase<<','<<app.battleResults.points[4]<<','<<app.battleProfile.u(72)<<','<<app.resultAnimationFrame.fadeAlpha<<','<<app.menu<<'\n';
                            if(app.battleProfile.words!=committedProfile)throw std::runtime_error("Result animation changed committed driver profile");
                            if(frame==0||frame==15||frame==31||frame==61||frame==92||frame==407){
                                const auto file=frame==92?"result.bmp":"result-frame-"+std::to_string(frame)+".bmp";
                                if(!app.renderer.saveBitmap((smokePath.parent_path()/file).wstring()))throw std::runtime_error("Result animation capture failed");
                            }
                            if(frame<426&&app.menu)throw std::runtime_error("Result owner finished before its source boundary");
                        }
                        if(!app.menu||app.frontend.stage!=FrontendStage::Course)throw std::runtime_error("Result owner failed to return to course selection");
                        launched<<"Result: "<<(app.battleResult==original::OriginalLegendResult::Win?"Win":"Loss")<<"\nTime up: "<<app.race.timeUp<<"\nBalance: "<<app.battleProfile.u(72)<<'\n';
                    }
                }
                return 0;
            }
            if(menuMotion){
                if(smokePath.empty())smokePath=app.root/"verification/menu-motion/preview.bmp";
                fs::create_directories(smokePath.parent_path());
                std::ofstream timings(smokePath.parent_path()/"frame-times.csv");timings<<"stage,frame,render_ms\n";
                for(auto stage:{FrontendStage::Make,FrontendStage::Car,FrontendStage::Transmission,FrontendStage::Mode,FrontendStage::Course,FrontendStage::Route,FrontendStage::Weather,FrontendStage::Time}){
                    app.frontend.stage=stage;
                    for(int frame=0;frame<=120;++frame){
                        if(frame==60)app.frontend.change(1);
                        const auto before=std::chrono::steady_clock::now();
                        if(!app.render(1./60))throw std::runtime_error(app.renderer.error);
                        timings<<int(stage)<<','<<frame<<','<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-before).count()<<'\n';
                        if(frame%30==0){const auto file=smokePath.parent_path()/("stage-"+std::to_string(int(stage))+"-frame-"+std::to_string(frame)+".bmp");if(!app.renderer.saveBitmap(file.wstring()))throw std::runtime_error("Menu motion capture failed");}
                    }
                }
                return 0;
            }
            if(!app.render(1./60))throw std::runtime_error(app.renderer.error);
            if(smokePath.empty())smokePath=app.root/"verification/menu.bmp";
            fs::create_directories(smokePath.parent_path());if(!app.renderer.saveBitmap(smokePath.wstring()))throw std::runtime_error("Unable to save diagnostic frame");
            app.start();app.best=Replay{};app.bestTime=0;for(int i=0;i<300;i++)app.simulate({});app.hud.resize(1280,720);
            if(!app.render(1./60))throw std::runtime_error(app.renderer.error);
            if(!app.renderer.saveBitmap((smokePath.parent_path()/"race.bmp").wstring()))throw std::runtime_error("Unable to save race diagnostic frame");return 0;
        }
        WNDCLASSW wc{};wc.lpfnWndProc=windowProc;wc.hInstance=instance;wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.lpszClassName=L"IDAS3GroundUpNativeWindow";RegisterClassW(&wc);
        RECT rect{0,0,1280,720};AdjustWindowRect(&rect,WS_OVERLAPPEDWINDOW,FALSE);current=&app;
        app.window=CreateWindowExW(0,wc.lpszClassName,L"Initial D Arcade Stage 3 - Visual Fidelity 0.3.29 | Enter: Start | C: Camera",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,rect.right-rect.left,rect.bottom-rect.top,nullptr,nullptr,instance,nullptr);
        if(!app.window)throw std::runtime_error("Window creation failed");
        if(!app.renderer.initialize(app.window,1280,720))throw std::runtime_error(app.renderer.error);
        if(!app.audio.open())app.status("Audio output unavailable; driving remains available");
        ShowWindow(app.window,show);auto last=std::chrono::steady_clock::now();
        while(app.running){
            MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT)app.running=false;TranslateMessage(&msg);DispatchMessageW(&msg);}
            if(!app.running)break;auto now=std::chrono::steady_clock::now();double dt=std::chrono::duration<double>(now-last).count();last=now;
            app.input.poll(app.active);app.commands(dt);
            if((!app.active||IsIconic(app.window))&&!app.multiplayer.active){app.clock.reset();app.updateAudioScene(true);app.audio.update(0,0,0,0,false);Sleep(20);continue;}
            if(app.resizePending){app.resizePending=false;if(!app.renderer.resize(app.pendingWidth,app.pendingHeight))throw std::runtime_error(app.renderer.error);}
            if(!app.menu&&!app.paused&&!app.loadingActive&&!app.extraModeVisitActive()&&!app.preRaceDialogueActive&&!app.legendVisitActive&&!app.vsActive&&!(app.multiplayer.active&&app.multiplayer.waiting))app.advanceHostClock(dt,[&]{app.simulate(app.driver());});else app.clock.reset();
            if(app.messageSeconds>0){app.messageSeconds-=float(dt);if(app.messageSeconds<=0)app.message.clear();}
            if(dt>0)app.renderFps+=(float(1/dt)-app.renderFps)*.025f;
            if(!app.render(dt))throw std::runtime_error(app.renderer.error);
            // The post-result owner is its own scene, not the menu; treating it
            // as one would end the music it just asked for, every frame.
            app.updateAudioScene();
            app.audio.update(app.vehicle.rpm,app.vehicle.throttle,app.vehicle.speed,app.originalHandling?0.f:app.vehicle.slip,!app.menu&&!app.paused&&app.race.phase!=RacePhase::Finished);
        }
        app.saveSettings();DestroyWindow(app.window);current=nullptr;return 0;
    }catch(const std::exception& e){std::ofstream report(app.root/"startup_error.txt");report<<e.what()<<'\n';if(!smoke&&!headless&&!matrix&&!routeSmoke&&!legendSmoke&&!buntaSmoke&&!engineDrivingCheck)MessageBoxA(nullptr,e.what(),"Initial D remake could not start",MB_OK|MB_ICONERROR);current=nullptr;return 1;}
}
#endif
#endif // desktop-only entry points


