#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace idas3::original {
// The source BGM table indexes every cue, not only the three menu scores.
// 3..33 are the rivals' own themes and 34..38 the after-race tracks; the
// dialogue owners request those by raw id, so the enum names only the three
// the selection bindings refer to and the rest are cast in.
enum class OriginalSelectionMusicCue : std::uint32_t { Type=0,Select=1,Result=2 };
inline constexpr unsigned originalMusicCueCount=39,originalMusicMenuCues=3;
struct OriginalSelectionMusicDescriptor {
    std::string_view filename;
    std::uint32_t command,slotMask;
    std::uint16_t sourceLevel;
};
const OriginalSelectionMusicDescriptor& originalSelectionMusicDescriptor(OriginalSelectionMusicCue);
const OriginalSelectionMusicDescriptor& originalMusicCueDescriptor(unsigned cue);
struct OriginalSelectionMusicBinding {
    std::uint32_t initAddress,requestAddress;
    OriginalSelectionMusicCue cue;
    std::string_view sourceOwner;
};
std::span<const OriginalSelectionMusicBinding> originalSelectionMusicBindings();
std::optional<OriginalSelectionMusicCue> originalSelectionMusicCueForInit(std::uint32_t address);

// Native representation of the source BGM manager's relevant fields. Slot
// registration and bank loading are supplied by the decoder/resource owner.
struct OriginalSelectionMusicState {
    std::int32_t handle0=-1,selectedCue28=-1;
    std::uint8_t playing4=0,loadBusy20=0,loaded32=0,pendingStart33=0,volumePending34=0;
    std::uint32_t command8=0,delay36=0;
    std::uint16_t controlArgument22=8,sourceLevel24=0;
    std::int32_t volumeCounter0C31EB34=0;
    bool slotRegistered=false;
    std::int32_t scene=-1;
};
struct OriginalSelectionMusicLoadResult {
    std::int32_t handle=0;
    bool loadingBusy=false,loaded=true,slotRegistered=true;
};
enum class OriginalSelectionMusicOperation { Load,Unload,Start,Control };
struct OriginalSelectionMusicCommand {
    OriginalSelectionMusicOperation operation{};
    std::uint32_t cue=0,word=0,argument=0;
    std::int32_t handle=-1;
    bool operator==(const OriginalSelectionMusicCommand&)const=default;
};
struct OriginalSelectionMusicCommands {
    std::array<OriginalSelectionMusicCommand,5> commands{};
    unsigned count=0;
    bool cueChanged=false;
    auto begin()const{return commands.begin();}
    auto end()const{return commands.begin()+count;}
};
//141EC0->142A60, followed by the Init caller's conditional142FC0. Same cue
// restores its source level only; it never reloads or queues another start.
OriginalSelectionMusicCommands requestOriginalSelectionMusic(OriginalSelectionMusicState&,
    OriginalSelectionMusicCue,const OriginalSelectionMusicLoadResult& load={});
//143060, including142FE0's signed post-decrement and pending volume writes.
// Call at60Hz. A host pause freezes the owner/mixer; it is not an arcade cue.
OriginalSelectionMusicCommands tickOriginalSelectionMusic(OriginalSelectionMusicState&,bool loaderBusy=false);
//143140:001200A0, argument0. Leaves selection/resource/pending fields intact.
OriginalSelectionMusicCommands stopOriginalSelectionMusic(OriginalSelectionMusicState&);
//1431E0:00000AA0, argument from+22 (8 for these cues). The source sends this
// at selection exit; the sound-driver's envelope interpretation is separate.
OriginalSelectionMusicCommands exitOriginalSelectionMusic(OriginalSelectionMusicState&);
//1416A0's scene-change boundary: selection1, race4. Same scene is a no-op.
// Native resource ownership replaces the source allocation/free operations.
OriginalSelectionMusicCommands changeOriginalSelectionMusicScene(OriginalSelectionMusicState&,std::int32_t scene);
}
