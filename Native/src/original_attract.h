#pragma once
#include <cstdint>
#include <optional>
#include <span>

namespace idas3::original {
// Original02ABC0 registration order, including its cabinet-mode1-only nodes9/10.
// IDs are the source A_ASEQ_* IDs, not host frontend screen IDs.
std::span<const std::uint32_t> originalAttractChildren(std::uint32_t cabinetMode);
std::optional<std::uint32_t> originalAttractNextChild(
    std::uint32_t currentChild,std::uint32_t cabinetMode);

struct OriginalAttractCompletion {
    std::int32_t requestedChild{-1}; // -1 invokes the original next-child operation.
    bool preparePendingReplay{},clearPendingReplay{},clearReplayState352{};
};
//02CAAE..02CC50, called only after the child has requested finish. The link
// override is the result of02DE20; pendingReplay is byte2F4E24. Replay loading
// and link hardware remain explicit caller-owned operations.
OriginalAttractCompletion originalAttractCompletion(
    std::uint32_t currentChild,bool linkRestartOverride,bool pendingReplay);

//02CC64..02CD46 permits calling the credit/start owner202B60(0,0) only here.
// freePlayMode is2029E0's packet+64; switchByte is original400670.
bool originalAttractCanCheckStart(std::uint32_t currentChild,
    std::uint32_t freePlayMode,std::uint8_t switchByte);

struct OriginalAttractTitleState {
    unsigned child=6,phase=0,frame=0;
    std::uint32_t fadeArgb=0xff000000;
    bool completed=false;
};
//083340/083DC0 title fades. Native assets are ready before entry and the
// cabinet-link handshake is an explicit completed boundary on this desktop.
void stepOriginalAttractTitle(OriginalAttractTitleState&,bool resourcesReady=true,
    bool linkHandshakeCompleted=true);

struct OriginalAttractExitState {
    std::uint32_t pendingFrames348{},frame84{};
    bool finishFlag28{};
};
struct OriginalAttractExitEvents {
    bool skipChildUpdate{},clearBackgroundBlack{},finishRequested{};
};
//02CA28 and02D760 tail plus02E100. acceptedStart must already be the outcome
// of202B60, not an unconditioned host key. It is ignored during the exit wait.
OriginalAttractExitEvents tickOriginalAttractExit(
    OriginalAttractExitState&,bool acceptedStart);

//076980/076B80. A fresh resume byte0 enters source child5 (iSelMain01).
// Unknown profile modes on a resumed race issue no source child request.
std::optional<std::uint32_t> originalSingleEntryChild(
    std::uint8_t resumeByte31CE43,std::uint32_t profileMode);
}
