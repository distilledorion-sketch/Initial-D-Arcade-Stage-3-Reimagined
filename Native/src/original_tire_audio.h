#pragma once
#include <cstdint>
#include <vector>
namespace idas3::original {
//142400/142520 request globals and142120's continuous skid-sound state.
struct OriginalTireAudioState {
    float strength=0,volume=127;
    std::uint32_t kind=0,surface=0,phase=0,frames=0;
    std::uint8_t requested=0;
};
enum class OriginalTireCommandType { Stop, Play, Volume };
struct OriginalTireCommand {
    OriginalTireCommandType type{};
    std::int32_t value=0;
    bool operator==(const OriginalTireCommand&)const=default;
};
// Scene1416A0 leaves the previous angular strength untouched.
void resetOriginalTireAudio(OriginalTireAudioState& state);
// speed is drive+238 *3.6 at the angular request, before transmission updates.
void requestOriginalTireAudio(OriginalTireAudioState& state,std::uint8_t disabled,
    float speed,std::uint32_t kind,std::uint32_t weatherMask,float strength);
// Called by1415E0 before updating the scene's sound queues. The selection RNG
// is the same37C778 state used by the original driving/controller routines.
std::vector<OriginalTireCommand> stepOriginalTireAudio(OriginalTireAudioState& state,
    std::uint8_t disabled,std::uint32_t& sharedRandomSeed);
}
