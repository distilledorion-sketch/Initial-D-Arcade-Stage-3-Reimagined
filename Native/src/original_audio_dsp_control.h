#pragma once
#include <cstdint>
#include <span>
namespace idas3 {
struct OriginalAudioDspControl {
    unsigned selectedBank18=0,selectedPreset19=255;
};
struct OriginalAudioDspRegistration {
    unsigned bankId=0,presetCount=0;
    bool registered=false;
};
enum class OriginalAudioDspOperation { None,Clear,Load };
struct OriginalAudioDspSelection {
    OriginalAudioDspOperation operation=OriginalAudioDspOperation::None;
    unsigned registryIndex=0,preset=0;
    // Load writes zero to all64 physical slot effectSend bytes before reset.
    bool clearVoiceSends=false;
};
// ARM5F78: cache equality is checked before registered-bank lookup. PresetFF
// does nothing;7F clears DSP registers/routes and invalidates only preset19.
OriginalAudioDspSelection selectOriginalAudioDsp(OriginalAudioDspControl& state,
    unsigned bank,unsigned preset,std::span<const OriginalAudioDspRegistration> registry);
// ARM5F18 / A470xx00 changes this field alone. It does not invalidate preset19.
void setOriginalAudioDspBank(OriginalAudioDspControl& state,unsigned bank);
// Raw A4 argument bytes; the driver masks level to4 bits and pan to7 bits.
std::uint16_t originalAudioDspReturnLevel(std::uint16_t route,unsigned argument);
std::uint16_t originalAudioDspReturnPan(std::uint16_t route,unsigned argument);
}
