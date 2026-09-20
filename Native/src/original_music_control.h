#pragma once
#include <array>
#include <cstdint>
#include <span>
namespace idas3 {
// Original ARM42D4 inputs. Preserve these alongside each note; an already
// quantized TL does not contain enough information to apply later controls.
struct OriginalMusicVolumeContext {
    std::uint8_t velocityTableValue=0,layerGain8=127,channelVolume0A=127;
    std::uint8_t channelGain10=127,master05=127,bankFade06=127,channelFlags0=0;
};
std::uint8_t originalMusicTotalLevel(const OriginalMusicVolumeContext&);
std::uint8_t originalMusicChannelGain(std::uint8_t level14,std::uint8_t offset16,
    std::uint8_t alternate15=127,std::uint8_t alternate17=64,std::uint8_t channelFlags0=0);
std::uint8_t originalMusicFadeLevel(std::uint8_t commandLevel);
// SH4 1ED340 wire packing, returned in ARM command byte order. Only these three
// manager control words are accepted; bank slot follows the original low nibble.
std::uint32_t originalMusicManagerCommand(std::uint32_t word,std::uint32_t argument,unsigned bankSlot);
struct OriginalMusicTrackControl {
    std::uint8_t flags0=0,header1=0,bank2=0;
    std::uint32_t volume1C=0x7f0000,increment20=0,lastVolume24=0x7f0000;
};
struct OriginalMusicControlEvent {std::uint32_t command=0;bool local=false,external=false;};
struct OriginalMusicControlEvents {
    std::array<OriginalMusicControlEvent,2> events{};unsigned count=0;
    bool stopRelatedGroup=false;
};
// ARM7940: only the FIRST matching normal score track begins a fade.
bool requestOriginalMusicFade(std::span<OriginalMusicTrackControl> tracks,unsigned bankSlot,std::uint8_t argument);
// ARM78B8 (A0001200): clear matching normal tracks. Immediate voice mute/free remains a
// caller action, matched by source voice.bank&7 and source music-kind flag.
void stopOriginalMusicTracks(std::span<OriginalMusicTrackControl> tracks,unsigned commandLowByte);
bool originalMusicStopKillsVoice(std::uint8_t flags0,std::uint8_t flags1,unsigned voiceBank,unsigned commandLowByte);
// Call once per actual TimerB advancement (44 samples for TYPE/SELECT), before
// poll. The surrounding716C global correction/tempo remains the scheduler's.
void tickOriginalMusicFade(OriginalMusicTrackControl&);
OriginalMusicControlEvents pollOriginalMusicFade(OriginalMusicTrackControl&);
}
