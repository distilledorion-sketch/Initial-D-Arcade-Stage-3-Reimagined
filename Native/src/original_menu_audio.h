#pragma once
#include "original_audio.h"
#include <array>

namespace idas3 {
// Shared original DTPK PCM record decoder; sequences choose the record/rate.
struct OriginalMenuSound;
OriginalMenuSound decodeOriginalSfxPlayback(std::span<const std::uint8_t> bytes,unsigned playbackId);
// Original 141F80 -> 1435C0 -> 1ED9C0 command words, from 31EB68.
enum class OriginalMenuCue : std::uint32_t {
    Change=0x000201a9u, Confirm=0x000301a9u, Back=0x000801a9u,
    ResultCount=0x000501a9u, UpgradeNotice=0x000e01a9u
};
struct OriginalMenuSound {
    OriginalAudioClip clip;
    std::uint32_t command=0;
    std::uint16_t playbackId=0,sampleId=0;
    std::uint8_t sequenceVolume=0;
    // Original playback record retained for subsequent envelope/DSP mixing.
    // clip contains exact raw PCM, before those sound-driver effects.
    std::array<std::uint8_t,64> playback{};
};
// Bounded decoder for the original menu bank's single-SFX, PCM16 records.
// Unsupported sequence types/codecs/rate words are rejected, not approximated.
OriginalMenuSound decodeOriginalMenuSound(std::span<const std::uint8_t> bank,std::uint32_t command);
OriginalMenuSound loadOriginalMenuSound(const std::filesystem::path& root,OriginalMenuCue cue);
// Original result/tuning141F80 IDs. Exact PACK21 command and sample records;
// source owner timing and force/retrigger policy belong to its controller.
OriginalMenuSound loadOriginalTuningSound(const std::filesystem::path& root,unsigned sourceCueId);
// Scene4 slot0 -> PACK22, original142460/141FC0 contact feedback.
// Scene4 slot3 -> PACK24, original1420C0: cue1 refill, cue2 start countdown.
OriginalMenuSound loadOriginalRaceSound(const std::filesystem::path& root,unsigned bank,unsigned cue);
}
