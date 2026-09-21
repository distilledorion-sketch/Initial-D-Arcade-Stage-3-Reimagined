#pragma once
#include <stdint.h>
#pragma pack(push,8)
// Native scene bridge v1. Independent local source physics, render-only peer.
typedef struct Idas3MultiplayerConfig {
    uint32_t size,version,course,reverse,wet,night,localCar,remoteCar,localSlot,automatic;
} Idas3MultiplayerConfig;
// Lobby record only; poses remain the existing 128-byte stream.
typedef struct Idas3BattleRecord {
    uint32_t battles,wins,level,streak;
} Idas3BattleRecord;
typedef struct Idas3MultiplayerAuraStatus {
    uint32_t size,version,level,streak,eligible,submittedRanges,colorArgb,sourceFrame;
} Idas3MultiplayerAuraStatus;
// Read-only snapshot of the most recently painted online HUD.
typedef struct Idas3MultiplayerHudStatus {
    uint32_t size,version,active,cameraView,localCar,remoteCar,sourceProfileMode;
    uint32_t game2dCommands,portraitCommands,playerGlyphs,rivalGlyphs,mirrorEnabled;
    uint32_t playerMapMarkers,rivalMapMarkers,sectionCount,sectionCapacity,elapsed6000;
    float signedAdvantage,localProgressMetres,remoteProgressMetres;
    uint32_t cumulativeSections[4],renderedSectionDurations[4];
} Idas3MultiplayerHudStatus;
typedef struct Idas3MultiplayerSnapshot {
    uint32_t size,version;
    uint64_t sequence,raceTicks;
    uint32_t flags,car;
    float bodyPosition[3],actorPosition[3];
    // Radians in the native Mesh/host convention, not source SH4 Euler order.
    float yaw,pitch,roll,steering;
    float suspension[4],wheelRotation[4];
    float speed,rpm,progress;
    uint32_t headlightPhase;
    int32_t headlightCounter;
    uint32_t headlightVisible;
} Idas3MultiplayerSnapshot;
typedef struct Idas3AuthorityStatus {
    uint32_t size,version;
    uint64_t frame,confirmed,verified,contactFrames,rollbacks,replayedFrames;
    uint32_t maxDepth,stalled;
    float maxCorrection,maxReplayMs;
    int32_t winner;
    uint32_t reserved;
    uint64_t hostFinishTicks,clientFinishTicks;
} Idas3AuthorityStatus;
#pragma pack(pop)
enum Idas3MultiplayerFlags {
    Idas3MpActive=1,Idas3MpWaiting=2,Idas3MpFinished=4,Idas3MpTimeUp=8,
    Idas3MpHeadlights=16,Idas3MpBrake=32,Idas3MpPaused=64
};
#ifdef __cplusplus
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
static_assert(sizeof(Idas3MultiplayerConfig)==40);
static_assert(sizeof(Idas3BattleRecord)==16);
static_assert(sizeof(Idas3MultiplayerHudStatus)==112);
static_assert(sizeof(Idas3MultiplayerSnapshot)==128);
static_assert(sizeof(Idas3AuthorityStatus)==96);
static_assert(offsetof(Idas3MultiplayerSnapshot,bodyPosition)==32);
static_assert(offsetof(Idas3MultiplayerSnapshot,headlightPhase)==116);
namespace idas3 {
inline void validateMultiplayerConfig(const Idas3MultiplayerConfig& c){
    if(c.size!=sizeof(c)||(c.version!=1&&c.version!=2)||c.course>=12||c.localCar>=35||c.remoteCar>=35||
       c.localSlot>1||c.reverse>1||c.wet>1||c.night>1||c.automatic>1)
        throw std::invalid_argument("Invalid multiplayer configuration v1");
    if(c.course==11&&!c.night)throw std::invalid_argument("Enna Skyline requires night scenery");
    if(c.course==8&&!c.wet)throw std::invalid_argument("Akina Snow requires wet=1");
}
// Reject malformed/foreign packets. Finite overshoots on wheel/control fields
// are bounded before any trigonometry or original collision lookup is reached.
inline Idas3MultiplayerSnapshot sanitizeMultiplayerSnapshot(const Idas3MultiplayerSnapshot& incoming,unsigned car){
    if(incoming.size!=sizeof(incoming)||incoming.version!=1||incoming.car!=car||
       !(incoming.flags&Idas3MpActive)||(incoming.flags&~127u)||incoming.headlightVisible>1)
        throw std::invalid_argument("Invalid multiplayer snapshot v1/car/flags");
    auto s=incoming;
    const auto finite=[](float value){if(!std::isfinite(value))throw std::invalid_argument("Non-finite multiplayer pose");};
    for(const auto value:s.bodyPosition){finite(value);if(std::abs(value)>100000.f)throw std::invalid_argument("Multiplayer body outside world bounds");}
    for(const auto value:s.actorPosition){finite(value);if(std::abs(value)>100000.f)throw std::invalid_argument("Multiplayer actor outside world bounds");}
    const auto angle=[&](float& v){finite(v);v=std::remainder(v,6.2831853071795864769f);};
    angle(s.yaw);angle(s.pitch);angle(s.roll);finite(s.steering);s.steering=std::clamp(s.steering,-3.1415927f,3.1415927f);
    for(auto& v:s.suspension){finite(v);v=std::clamp(v,-2.f,2.f);}
    for(auto& v:s.wheelRotation)angle(v);
    finite(s.speed);finite(s.rpm);finite(s.progress);
    s.speed=std::clamp(s.speed,-100.f,200.f);s.rpm=std::clamp(s.rpm,0.f,20000.f);s.progress=std::clamp(s.progress,-100000.f,100000.f);
    s.headlightCounter=std::clamp(s.headlightCounter,-1,40);
    return s;
}
}
#endif
