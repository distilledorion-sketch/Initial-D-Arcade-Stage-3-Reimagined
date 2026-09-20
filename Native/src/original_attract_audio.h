#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace idas3::original {
// Original 31F250 records are 72 bytes: filename[64], loop u32,
// channel u16, volume s16. IDs differ from the desktop song-picker order.
struct OriginalStreamDescriptor {
    std::string_view filename;
    std::uint32_t loop;
    std::uint16_t channel;
    std::int16_t volume;
};
std::span<const OriginalStreamDescriptor> originalStreamDescriptors();
const OriginalStreamDescriptor& originalStreamDescriptor(unsigned id);

enum class OriginalAttractSoundOperation { Play, Stop, Volume };
struct OriginalAttractSoundCommand {
    OriginalAttractSoundOperation operation;
    unsigned value{};
    bool operator==(const OriginalAttractSoundCommand&)const=default;
};
struct OriginalAttractSoundCommands {
    std::array<OriginalAttractSoundCommand,3> commands{};
    unsigned count{};
    auto begin()const{return commands.begin();}
    auto end()const{return commands.begin()+count;}
};
// sourceFrame is Rosso owner+84, or Demo owner+0x4a4 BEFORE Main.
// soundEnabled is the source202140 packet+4==1 gate, not the user's mute.
// demoFinished is the source Main's completion branch, not a host timeout.
// Parent02DF60 additionally stops the stream on EVERY child switch.
OriginalAttractSoundCommands originalAttractSoundCommands(unsigned child,
    std::uint32_t sourceFrame,bool soundEnabled,bool demoFinished=false);
}
