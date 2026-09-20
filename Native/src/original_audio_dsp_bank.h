#pragma once
#include "original_audio_dsp.h"
#include <filesystem>
#include <string_view>
namespace idas3 {
struct OriginalAudioDspPreset {
    std::uint32_t sourceOffset=0;
    std::array<std::uint8_t,0xc24> originalRecord{};
};
struct OriginalAudioDspScene {
    std::uint32_t sourceOffset=0;
    unsigned bankId=255,preset=255; // bankFF uses this bank's intrinsic identity
};
struct OriginalAudioDspBank {
    unsigned bankId=0,declaredRingCode=0;
    std::vector<OriginalAudioDspPreset> presets;
    std::vector<OriginalAudioDspScene> scenes;
};
OriginalAudioDspBank decodeOriginalAudioDspBank(std::span<const std::uint8_t> data);
OriginalAudioDspBank loadOriginalAudioDspBank(const std::filesystem::path& root,std::string_view name);
// Ring code is latched from the FIRST loaded DTPK by ARM1230, separately from
// preset selection. The caller must provide it rather than use each preset's
// bank-declared code. The source18E4 table supports both2MiB and8MiB RAM.
OriginalAudioDspProgram originalAudioDspProgram(const OriginalAudioDspBank& bank,unsigned preset,
    unsigned latchedRingCode,bool memory8Mb);
}
