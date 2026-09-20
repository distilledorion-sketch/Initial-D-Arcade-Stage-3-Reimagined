#pragma once
#include "original_audio_dsp_bank.h"
#include "original_audio_dsp_control.h"
#include <functional>
#include <optional>
#include <string>
namespace idas3 {
struct OriginalAudioDspDiagnostics {
    bool ringLatched=false,activeProgram=false;
    unsigned ringCode=255,activeBank=255,activePreset=255;
    unsigned cacheBank=0,cachePreset=255;
    std::uint64_t frames=0,loadCount=0,clearCount=0,inputPeak=0;
    double wetEnergy=0; // Sum of squared raw stereo return samples; no host gain.
};
class OriginalAudioDspRuntime {
public:
    using ClearVoiceSends=std::function<void()>;
    OriginalAudioDspRuntime()=default;
    explicit OriginalAudioDspRuntime(const std::filesystem::path& root,
        ClearVoiceSends clearVoiceSends={},bool memory8Mb=true);
    void configure(const std::filesystem::path& root,
        ClearVoiceSends clearVoiceSends={},bool memory8Mb=true);
    // Slots are the original ordered eight bank registrations, not intrinsic
    // bank IDs. Registration does not request a scene or DSP preset.
    void registerBank(unsigned slot,std::string_view name);
    void unregisterBank(unsigned slot);
    OriginalAudioDspOperation selectScene(unsigned slot,unsigned scene);
    OriginalAudioDspOperation select(unsigned bank,unsigned preset);
    // A470xx00 changes cache bank alone, retaining cache preset and live program.
    void setBank(unsigned bank);
    // A41r/A42r write the current global effect return, preserving processor
    // state and the source preset/cache identity. Arguments are raw bytes.
    void setReturnLevel(unsigned index,unsigned argument);
    void setReturnPan(unsigned index,unsigned argument);
    OriginalAudioDspFrame render(std::span<const std::int32_t,16> input,
        std::array<std::int16_t,2> external={});
    OriginalAudioDspDiagnostics diagnostics()const;
    const OriginalAudioDspControl& control()const{return control_;}
    const OriginalAudioDsp& processor()const{return processor_;}
    const OriginalAudioDspBank* registeredBank(unsigned slot)const;
private:
    struct Slot {std::string name;std::optional<OriginalAudioDspBank> bank;};
    std::array<Slot,8> slots_;
    std::filesystem::path root_;
    ClearVoiceSends clearVoiceSends_;
    OriginalAudioDspControl control_;
    OriginalAudioDsp processor_;
    OriginalAudioDspDiagnostics diagnostics_;
    bool memory8Mb_=true,configured_=false;
};
}
