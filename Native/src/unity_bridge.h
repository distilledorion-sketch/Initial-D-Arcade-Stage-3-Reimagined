#pragma once
#include <stdint.h>
#include "native_multiplayer.h"
#if defined(_WIN32)
#ifndef IDAS3_UNITY_EXPORT
#define IDAS3_UNITY_EXPORT __declspec(dllexport)
#endif
#define IDAS3_UNITY_CALL __cdecl
#define IDAS3_UNITY_EVENT __stdcall
#else
#ifndef IDAS3_UNITY_EXPORT
#define IDAS3_UNITY_EXPORT __attribute__((visibility("default")))
#endif
#define IDAS3_UNITY_CALL
#define IDAS3_UNITY_EVENT
#endif
#ifdef __cplusplus
extern "C" {
#endif
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SharedReadFinish(char* output,int capacity);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneReadPresence(char* output,int capacity);
IDAS3_UNITY_EXPORT void IDAS3_UNITY_CALL Idas3SharedAckFinish();
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SharedReadReplay(uint8_t* output,int capacity);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3ReplayRecordingOptions(uint32_t flags);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3LocalReplayRead(int part,uint8_t* output,int capacity);
IDAS3_UNITY_EXPORT void IDAS3_UNITY_CALL Idas3LocalReplayAck();
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3ReplayOpponentStart(int car,int enemy,const uint32_t* appearance,int count);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3ReplayOpponentFrame(const uint8_t* frame,int count);
// Same borrowed lifetime as Idas3SceneGetFrame; no change to the existing
// scene ABI. The generation/count guard prevents mixing frame publications.
IDAS3_UNITY_EXPORT const uint64_t* IDAS3_UNITY_CALL Idas3SceneGetGeometryIds(uint64_t generation,uint32_t rangeCount);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SharedReadPersonalImport(char* output,int capacity);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SharedSetRecords(const int32_t* values,int count,int enabled);
#pragma pack(push,8)
typedef struct Idas3UnityInput {
    uint32_t size;                 //sizeof(Idas3UnityInput),88 bytes
    uint32_t flags;                //bit0 focused, bit1 host driving controls blocked; other bits reserved
    double deltaSeconds;           //unscaled Unity frame delta
    uint32_t keys[8];              //held Windows virtual-key bits,0..255
    uint32_t padButtons;           //XInput button bits
    int32_t thumbLX,thumbLY,thumbRX,thumbRY; //signed−32768..32767
    uint32_t leftTrigger,rightTrigger;     //0..255
    uint32_t padConnected;
    int32_t width,height;           //positive resize request,0 retain
} Idas3UnityInput;
typedef struct Idas3UnityStatus {
    uint32_t size;                 //sizeof(Idas3UnityStatus),80 bytes
    uint32_t state;                //0 stopped,1 ready,2 failed
    uint64_t renderedFrames;
    uint64_t simulationTicks;
    uint64_t textureGeneration;
    int32_t width,height;
    int32_t frontendStage,attractChild;
    int32_t course,car,racePhase;
    uint32_t flags;                //1 menu,2 paused,4 focused,8 running,16 original handling;32 Legend,64 choice;128 multiplayer,256 waiting,512 rival intro,1024 loading,2048 disconnect,4096 mode visit,8192 can retire Legend
    float speedMetresPerSecond,rpm;
    uint32_t lastEventId,reserved;
} Idas3UnityStatus;
// Read-only presentation diagnostics; independent of the stable driving ABI.
typedef struct Idas3RivalStatus {
    uint32_t size,version;
    uint32_t preRaceActive,dialogEnemy,dialogKind,dialogPhase;
    uint32_t legendActive,choiceVisible,choiceKind,selectedIndex;
    uint32_t musicCue,musicPlaying;
    uint64_t musicSamplePosition;
} Idas3RivalStatus;
// User output/camera options, independent of source physics and saved tuning.
// Gains are finite0..1, default1. Engine includes driving tire audio. Group
// gains scale dry audio and new DSP sends; existing shared reverb tails decay.
typedef struct Idas3Options {
    uint32_t size,version;         //sizeof=40, version1
    float masterGain,musicGain,engineGain,effectsGain;
    uint32_t cameraView;           //0 bumper,1 chase
    uint32_t paused;               //read-only in Apply; use SetPaused
    uint32_t managedPauseOverlay;  //0 native pause panel,1 Unity overlay
    uint32_t reserved;             //must be0
} Idas3Options;
// Read-only driving signals for host wheel output; no force is sent here.
typedef struct Idas3WheelState {
    uint32_t size,version;
    uint64_t simulationTicks;
    float speed,steering,headingError,wallLateral,impact;
    uint32_t flags; //1 active driving,2 wall contact; steering positive right
} Idas3WheelState;
// Read-only pre-race presentation state; the driving ABI remains unchanged.
typedef struct Idas3PreRaceStatus {
    uint32_t size,version;         //sizeof=104, version1
    uint32_t phase,presentationFrame,shot,countdownRemaining;
    int32_t countdownDigit;
    uint32_t playerCar,opponentCar,opponentId,cameraView,playerRanges,rivalRanges,reserved;
    uint64_t simulationTicks,ownerTicks;
    float eyeX,eyeY,eyeZ,targetX,targetY,targetZ,verticalFov;
    uint32_t condition;
} Idas3PreRaceStatus;
// A local next-race choice. -1 uses Speedy Speed Boy (index1).
// eligible is only the native opponent-selection context, not managed lobbies.
typedef struct Idas3RaceMusicState {
    uint32_t size,version,eligible,count;
    int32_t selectedIndex,activeIndex;
    uint32_t opponentId,reserved;
} Idas3RaceMusicState;
// Read-only mixer timeline; separate from the stable driving/music-choice ABI.
typedef struct Idas3RaceAudioStatus {
    uint32_t size,version,flags;   //sizeof=56; flags1 held,2 idle,4 paused,8 race stream
    int32_t scene,track;           //scene0 menu,1 race,2 win,3 time-up,4 held; track-1 unloaded
    uint32_t reserved;
    double streamFrame;
    uint64_t idlePcmFrames,idleControlFrames,drivingControlFrames;
} Idas3RaceAudioStatus;
#pragma pack(pop)
typedef void (IDAS3_UNITY_EVENT *Idas3UnityRenderEvent)(int eventId);
IDAS3_UNITY_EXPORT uint32_t IDAS3_UNITY_CALL Idas3UnityVersion(void);
// Scene port: synchronous simulation/presentation data on Unity's main thread.
// No graphics device, external texture, render callback, or waveOut output.
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneInitialize(
    const char* assetRootUtf8,const char* saveRootUtf8,int width,int height);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneStep(const Idas3UnityInput* input);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneShutdown(void);
// Read-only replay presentation. Start requires separate replay-viewer-session storage.
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3ReplayStart(int condition,int weather,int night,int car,int manual);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3ReplayHud(int elapsed6000,int finish6000,const int* splits,int count,const int* glyphs,int glyphCount);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3ReplayAppearance(const uint32_t* values,int count);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3ReplayDetailFrame(const uint8_t* values,int count);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3ReplayAudio(double deltaSeconds,int playing,int reset,float master,float engine,float effects);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3ReplayPose(double tick,float x,float y,float z,float yaw,float speed,int gear,float pitch,int cameraMode,float orbit,int width,int height);
// Scene-mode/main-thread only. Never advances input, dialogue, or audio.
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneGetRivalStatus(Idas3RivalStatus* status);
// phase:0 inactive,1 first shot,2 second shot,3 VS,4 countdown,5 running,6 finished.
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneGetPreRaceStatus(Idas3PreRaceStatus* status);
// UTF8 length excluding NUL; copies at most capacity-1. side0 local,1 opponent.
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneCopyPreRaceName(int side,char* destination,int capacity);
// Multiplayer display metadata only, set after Start and before SetGo(1).
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneSetPreRaceNames(const char* localUtf8,const char* opponentUtf8);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3MultiplayerSetBattleRecords(const Idas3BattleRecord* local,const Idas3BattleRecord* remote);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3MultiplayerGetBattleRecord(int side,Idas3BattleRecord* record);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3MultiplayerGetAuraStatus(int side,Idas3MultiplayerAuraStatus* status);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3MultiplayerGetHudStatus(Idas3MultiplayerHudStatus* status);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3MultiplayerCopyHudText(int side,char* destination,int capacity);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3MultiplayerDiagnosticFinish(uint64_t ticks60);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3MultiplayerNextBattleRecord(const Idas3BattleRecord* before,int won,const Idas3BattleRecord* opponent,uint32_t experience,Idas3BattleRecord* after,uint32_t* nextExperience);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneCopyPreRaceBattleRecord(int side,char* destination,int capacity);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneGetRaceMusicState(Idas3RaceMusicState* state);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneGetRaceAudioStatus(Idas3RaceAudioStatus* status);
// index -1=default,0..29=catalog. field0 stableID,1 title,2 artist.
// UTF8 byte count excluding NUL; invalid request returns -1.
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneCopyRaceMusicText(int index,int field,char* destination,int capacity);
// Returns source stage1/2/3; -1 for an invalid catalog index.
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneGetRaceMusicStage(int index);
// Queues and persists only; never changes playing music. context0 requires
// native opponent selection; context1 is a managed idle multiplayer lobby.
// The managed owner validates lobby state. Native active multiplayer/loading/
// VS/dialogue states reject either context. Applied at the next race start.
// Index0 (Gamble Rumble) is retained as metadata but cannot be selected.
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneSetRaceMusicTrack(int index,int context);
// PCM16 interleaved mono/stereo. Context2 restores a preference at startup;
// otherwise the same opponent/lobby eligibility checks as built-in songs apply.
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneSetCustomRaceMusic(const short* samples,int count,int rate,int channels,int context);
// Scene-mode/main-thread only. Invalid inputs leave options unchanged. Options
// are host-persisted; these calls never write settings/profiles themselves.
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneGetOptions(Idas3Options* options);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneApplyOptions(const Idas3Options* options);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneSetControllerResponse(int response);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneGetControllerResponse();
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneSetSteeringDeadzone(float value);
IDAS3_UNITY_EXPORT float IDAS3_UNITY_CALL Idas3SceneGetSteeringDeadzone();
// Additional host steering smoothing,0=unchanged input,1=0.2-second time constant.
// Invalid/nonfinite values leave the setting unchanged; uninitialized getter=-1.
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneSetSteeringSmoothing(float value);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneSetPerformance(int rainDetail);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneSetMapSize(int size);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneSetMapZoom(int zoom);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneSetAiDifficulty(int difficulty);
IDAS3_UNITY_EXPORT float IDAS3_UNITY_CALL Idas3SceneGetSteeringSmoothing();
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneGetWheelState(Idas3WheelState* state);
// Online pause requests return0; unpause is always allowed. Restart/return
// reject online and inactive race owners; use MultiplayerLeave for a peer exit.
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneSetPaused(int paused);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneRestart(void);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneShowImportedCourseMenu(const char* root);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneRegisterImportedCourse(const char* root);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneStartImportedCourse(const char* root,int reverse);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneStartImportedCourseConditions(const char* root,int reverse,int night,int wet);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneReturnToCourse(void);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneCanFullTune(void);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneFullTune(void);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneRetire(void);
// Match found:1 original notification cue,2 safe interruption to course select.
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3SceneChallenger(int action);
// Scene-mode/main-thread only. Start loads a private stock two-car race and
// holds all local simulation. SetGo(1) releases the car showcase/VS sequence,
// followed by the original180-tick countdown. No collisions/AI/progress saves.
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3MultiplayerStart(const Idas3MultiplayerConfig* config);
// Read-only saved-car selection; returns 1 saved, 2 stock, 0 unavailable, -1 error.
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3MultiplayerReadCar(int selection,uint32_t* words,uint32_t count);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3MultiplayerCurrentCar(void);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3MultiplayerCurrentGear(void);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3MultiplayerStartSaved(const Idas3MultiplayerConfig* config,int selection,const uint32_t* local,const uint32_t* remote,uint32_t count);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3MultiplayerReadRaceCar(int side,uint32_t* words,uint32_t count);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3MultiplayerSetGo(int released);
// Settled protocol winner: -1 both time-up,0 host,1 guest,2 draw. Audio only;
// the race must already be finished. An identical repeat is a no-op.
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3MultiplayerSetResult(int winner);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3MultiplayerGetLocalSnapshot(Idas3MultiplayerSnapshot* snapshot);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3MultiplayerApplyRemoteSnapshot(const Idas3MultiplayerSnapshot* snapshot);
// Experimental version2 input authority. Call Enable while StartSaved is held.
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3MultiplayerEnableAuthority(uint64_t race,int remoteAutomatic,int boost);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3MultiplayerEnableAuthorityRules(uint64_t race,int remoteAutomatic,int boost,int collisions);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3MultiplayerAuthorityPacket(uint8_t* bytes,uint32_t capacity);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3MultiplayerAuthorityReceive(const uint8_t* bytes,uint32_t count);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3MultiplayerAuthorityStatus(Idas3AuthorityStatus* status);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3MultiplayerGetRemoteSnapshot(Idas3MultiplayerSnapshot* snapshot);
// Neutral terminal finish; keeps MP ownership until Leave. Repeated calls are
// idempotent. Status.flags bit2048 reports disconnected; no snapshot wire change.
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3MultiplayerDisconnect(void);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3MultiplayerLeave(void);
// Queue calls copy all arguments. Issue the positive returned event token
// once through GL.IssuePluginEvent(GetRenderEventFunc(),token), in queue order.
// Init texture must be a live D3D11 Texture2D created by Unity. The bridge
// retains a COM reference until Init executes and derives its device there.
// Save root must be explicit and outside the asset root's userdata directory.
// audioEnabled!=0 uses the existing native waveOut mixer/output.
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3UnityQueueInitialize(
    const char* assetRootUtf8,const char* saveRootUtf8,void* unityTexture,
    int width,int height,int audioEnabled);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3UnityQueueFrame(const Idas3UnityInput* input);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3UnityQueueShutdown(void);
IDAS3_UNITY_EXPORT Idas3UnityRenderEvent IDAS3_UNITY_CALL Idas3UnityGetRenderEventFunc(void);
// Thread-safe snapshots; no App/GPU work occurs in these accessors. Texture
// pointer borrowed until Shutdown. A resize produces a new generation; refresh
// Unity's external texture when it changes. Destroy external wrappers before
// issuing Shutdown, and execute Shutdown before Editor domain/plugin unload.
IDAS3_UNITY_EXPORT void* IDAS3_UNITY_CALL Idas3UnityGetTexture(void);
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3UnityGetStatus(Idas3UnityStatus* status);
// Returns UTF8 length excludingNUL; copies at most capacity−1 and terminates.
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3UnityCopyError(char* destination,int capacity);
// Main-thread shutdown/drain aid AFTER issuing the event. Does not execute any
// App or graphics work:1 completed,0 timeout,−1 invalid token. Never wait from
// inside the render callback. Keep the DLL loaded if shutdown has not drained.
IDAS3_UNITY_EXPORT int IDAS3_UNITY_CALL Idas3UnityWaitForEvent(int eventId,int timeoutMilliseconds);
#ifdef __cplusplus
}
#endif
