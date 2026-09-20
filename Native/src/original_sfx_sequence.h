#pragma once
#include "original_menu_audio.h"
#include "original_tire_audio.h"
#include <functional>
namespace idas3 {
struct OriginalSfxSequenceEvent {
    std::uint32_t tick=0;
    std::uint8_t playback=0,volume=0;
    bool operator==(const OriginalSfxSequenceEvent&)const=default;
};
struct OriginalSfxSequence {std::vector<OriginalSfxSequenceEvent> events;};
// Original C0/DF PCM sequences used by PACK23. Rejects unsupported opcodes.
OriginalSfxSequence decodeOriginalSfxSequence(std::span<const std::uint8_t> bank,std::uint32_t command);
class OriginalSfxSequencer {
public:
    using Output=std::function<void(const OriginalSfxSequenceEvent&)>;
    void start(const OriginalSfxSequence& sequence,const Output& output);
    void stop(){sequence_=nullptr;next_=0;}
    void reset(){stop();samplePhase_=0;tick_=start_=0;}
    void advanceSample(const Output& output);
    bool active()const{return sequence_!=nullptr;}
    std::uint64_t tick()const{return tick_;}
    // ARM1630/CDC set TimerB=0xD4 with divider0. One timer period is
    // (256-212)=44 AICA sample clocks. External MIDI sync is not enabled.
    static constexpr unsigned samplesPerTick=44;
private:
    const OriginalSfxSequence* sequence_=nullptr;
    std::size_t next_=0;
    unsigned samplePhase_=0;
    std::uint64_t tick_=0,start_=0;
    void dispatch(const Output& output);
};
// Authored PACK23 sequence and PCM playback, using the existing desktop
// one-shot mixing policy. Original shared voice allocation/envelopes/DSP
// remain separate; the complete sample sequences and rates are preserved.
class OriginalTirePlayback {
public:
    explicit OriginalTirePlayback(const std::filesystem::path& root);
    void reset();
    void apply(const original::OriginalTireCommand& command);
    std::int32_t renderFrame();
    unsigned startedSamples()const{return starts_;}
    unsigned startedCues()const{return cueStarts_;}
    const std::array<std::uint32_t,6>& commands()const{return commands_;}
private:
    std::array<OriginalSfxSequence,6> sequences_;
    std::array<std::uint32_t,6> commands_{};
    std::array<OriginalMenuSound,23> sounds_;
    struct Voice {unsigned playback=0,halfFrame=0,volume=127;bool active=false;};
    std::array<Voice,8> voices_{};
    unsigned nextVoice_=0,volume_=127,starts_=0,cueStarts_=0;
    OriginalSfxSequencer sequencer_;
    void play(const OriginalSfxSequenceEvent& event);
};
}
