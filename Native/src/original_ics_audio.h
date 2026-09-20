#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>
namespace idas3 {
struct OriginalIcsSegment {
    std::uint8_t end=0;
    std::uint16_t base=0;
    std::int16_t slope=0;
};
struct OriginalIcsLayer {
    std::uint16_t sample=0;
    std::uint8_t first=0,last=0,flags=0;
    std::array<std::uint8_t,12> header{};
    std::array<std::vector<OriginalIcsSegment>,6> controls;
    std::uint32_t bankOffset=0;
    // Original ARM driver6FAC: signed fixed-point interpolation, including
    // the distinct six-byte pitch records (control4).
    std::int32_t evaluate(unsigned control,unsigned value)const;
};
struct OriginalIcsProgram {
    std::array<std::uint8_t,8> header{};
    std::vector<OriginalIcsLayer> layers;
};
struct OriginalIcsSample {
    std::uint16_t loopStart=0,loopEnd=0;
    bool looping=false;
    std::vector<std::int16_t> pcm;
};
struct OriginalIcsBank {
    std::vector<OriginalIcsProgram> programs;
    std::vector<OriginalIcsSample> samples;
    std::array<std::uint8_t,128> volumeTable{}; // driver3034 selects row1
};
OriginalIcsBank decodeOriginalIcsBank(std::span<const std::uint8_t> bytes);
OriginalIcsBank loadOriginalIcsBank(const std::filesystem::path& path);
struct OriginalIcsPitchTable {
    std::array<std::uint16_t,384> fractional{};
    static OriginalIcsPitchTable load(const std::filesystem::path& root);
    std::uint16_t registerWord(unsigned pitch)const; // ARM6F60
};
struct OriginalIcsVoiceTables {
    OriginalIcsPitchTable pitch;
    std::array<std::uint8_t,32> pan{};
    std::array<std::uint16_t,128> cutoff{};
    static OriginalIcsVoiceTables load(const std::filesystem::path& root);
};
struct OriginalIcsVoiceControls {
    std::uint8_t volume=127,pan=64,cutoff=64,effectSend=64;
    std::uint8_t effectBus=255,transpose=64,finePitch=64,resonance=64;
};
struct OriginalIcsVoiceParameters {
    std::uint8_t totalLevel=255,preMasterAmplitude=0,pan=0,filterMode=0,effectSend=0,directLevel=0;
    std::uint16_t pitch=0,cutoff=0;
};
// Native equivalents of driver6BC4,6C30,6C94,6CC8,6D08,6D44.
// Effect send addresses the DSP bus; dry direct level comes from layer+8.
OriginalIcsVoiceParameters originalIcsVoiceParameters(const OriginalIcsBank& bank,
    const OriginalIcsLayer& layer,const OriginalIcsVoiceTables& tables,
    const OriginalIcsVoiceControls& controls,unsigned value,unsigned masterVolume=127);
}
