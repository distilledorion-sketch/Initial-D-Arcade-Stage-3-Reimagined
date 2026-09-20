#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace idas3 {
// DTPK location bits23..24 become the AICA PCMS field unchanged.
struct OriginalMusicSample {
    std::uint16_t sourceId=0; // <256 driver built-in; >=256 bank-local.
    std::uint8_t encoding=0; // 0 PCM16,1 PCM8,2 Yamaha ADPCM;3 stream is rejected.
    std::uint16_t loopStart=0,loopEnd=0; // AICA LSA / exclusive LEA, in samples.
    bool looping=false;
    std::uint32_t bankOffset=0;
    std::array<std::uint32_t,4> descriptor{};
    std::vector<std::uint8_t> encodedBytes;
    std::vector<std::int16_t> pcm;
    // Descriptor flag 0x80 is a stereo pair: a second block of the same
    // length immediately after the first, which the driver gives its own
    // voice. Empty for the ordinary mono samples.
    bool stereo=false;
    std::vector<std::int16_t> pcmRight;
    // Optional band-limited copies of pcm, built only when a caller asks for
    // them. See original_music_band_limit.h; the hardware read ignores these.
    std::vector<std::vector<std::int16_t>> bandLimited;
    std::vector<std::span<const std::int16_t>> bandLimitedSpans;
};
struct OriginalMusicLayer {
    std::uint32_t bankOffset=0;
    std::array<std::uint8_t,64> rawBytes{};
    std::uint16_t sampleId=0,sampleOffset=0,envelope1=0,envelope2=0,lfo=0;
    std::int8_t coarsePitch=0,finePitch=0;
};
struct OriginalMusicGroup {
    std::uint32_t bankOffset=0;
    std::array<std::uint8_t,64> header{};
    std::array<std::int16_t,128> noteToLayer{}; // -1 means no source layer.
    std::vector<OriginalMusicLayer> layers;
};
struct OriginalMusicProgram {
    std::uint32_t bankOffset=0;
    // Original instrument bytes retained verbatim, including split records,
    // pitch and envelope words. Interpretation belongs to the voice handler.
    std::vector<std::uint8_t> rawBytes;
    std::vector<OriginalMusicGroup> groups;
};
struct OriginalMusicSong {
    std::uint32_t command=0,bankOffset=0;
    std::vector<std::uint8_t> rawBytes;
};
struct OriginalMusicBank {
    std::vector<std::uint8_t> originalBytes;
    std::vector<OriginalMusicSample> samples;
    std::vector<OriginalMusicSample> builtinSamples; // Loaded from sibling asset.
    std::vector<OriginalMusicProgram> programs;
    std::vector<OriginalMusicSong> songs;
    std::uint32_t combinationTableOffset=0,programTableOffset=0;
    std::uint32_t volumeTableOffset=0,sequenceTableOffset=0,sampleTableOffset=0;
    std::vector<std::array<std::uint8_t,128>> volumeTables;
};
// Fills every sample's band-limited levels. Off the hardware-exact path.
void buildOriginalMusicBandLimitedLevels(OriginalMusicBank&,unsigned levels);
// Scope: the recovered TYPE and SELECT banks, mono PCMS0/1/2 samples and A8.
// Unsupported layouts/encodings fail closed; no sequencer runs in this parser.
OriginalMusicBank decodeOriginalMusicBank(std::span<const std::uint8_t> bytes);
OriginalMusicBank loadOriginalMusicBank(const std::filesystem::path& path);
}
