#pragma once
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace idas3 {
struct OriginalAudioClip {
    std::uint32_t sampleRate=0,channels=0;
    bool looping=false;
    std::size_t loopStart=0;
    std::vector<std::int16_t> samples;
    // Exclusive source frame. Zero retains the existing SPSD full-extent
    // playback/interpolation behavior; ADX and WAVE carry explicit endpoints.
    std::size_t loopEnd=0;
    std::size_t frames()const{return channels?samples.size()/channels:0;}
};
// Decode identified source streams directly; no sound-CPU/device emulation.
// Established SPSD entry points also dispatch ADX and MS ADPCM WAVE by signature.
OriginalAudioClip decodeOriginalSpsd(std::span<const std::uint8_t> bytes);
OriginalAudioClip loadOriginalSpsd(const std::filesystem::path& path);
// CRI ADX encoding3/version4, unencrypted, mono or stereo.
OriginalAudioClip decodeOriginalAdx(std::span<const std::uint8_t> bytes);
// Microsoft ADPCM in RIFF/WAVE; preserves source fact length and smpl loop.
OriginalAudioClip decodeOriginalMsAdpcmWave(std::span<const std::uint8_t> bytes);
}
