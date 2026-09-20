#pragma once
#include <cstdint>

namespace idas3::original {
enum class OriginalRivalLightRequest {Hold,On,Off};
struct OriginalRivalLightState {
    //063840 clears this word during rival construction. Arithmetic wraps.
    std::uint32_t frames1732{};
};
struct OriginalRivalLightInputs {
    std::uint32_t numericRaceMode{},profileMode{},enemy{};
    // Selected ACar actor+80: override+2400 when present, otherwise+2396.
    // Ordinary native rivalPublicActor().u(80) is the public source actor.
    std::uint32_t selectedActorFlags80{};
    // HUD+100 from065C20/06A420 BEFORE this frame's GO/rules/solver.
    float signedAdvantage100{};
    // Current accumulated authoring-path index, race+1460. This can have
    // advanced in the GO/rules stage since the HUD advantage was captured.
    std::int32_t playerProgress1460{};
};
//0638C0 tail. Call once AFTER that frame's ACar pose/mesh publication and
// BEFORE scene projection updates. Hold preserves outerlight81. Initial
// night LightON/query binding belongs to063D20, not this request helper.
OriginalRivalLightRequest advanceOriginalRivalLightRequest(
    OriginalRivalLightState&,const OriginalRivalLightInputs&);
}
