#pragma once
#include "original_ics_audio.h"
#include <memory>
namespace idas3 {
struct OriginalIcsMixFrame {
    std::array<std::int32_t,2> dry{};
    // Original20-bit channel sends, indexed by the authored DSP input bus.
    // Preserved for effect processing; never mixed into dry output as a proxy.
    std::array<std::int32_t,16> effects{};
};
// Native44100Hz PCM/ICS player for the identified engine bank format. No
// sound-CPU execution or device emulation. Layer/control conversion is separate
// from the dry PCM stream; cabinet DSP still consumes the exposed effect buses.
class OriginalIcsPlayer {
public:
    explicit OriginalIcsPlayer(OriginalIcsVoiceTables tables);
    void select(unsigned channel,std::shared_ptr<const OriginalIcsBank> bank,unsigned program);
    void setValue(unsigned channel,unsigned value);
    // Set effectRegisterWrite for an explicit send/bus command, including an
    // unchanged value. Volume/pitch recomputation preserves cleared sends.
    void setControls(unsigned channel,const OriginalIcsVoiceControls& controls,bool effectRegisterWrite=false);
    void setMasterVolume(unsigned value);
    void stop(unsigned channel);
    void reset();
    void clearDspSends();
    OriginalIcsMixFrame renderFrame();
    unsigned activeVoices()const;
    std::uint64_t startedVoices()const{return starts_;}
private:
    struct Channel {
        std::shared_ptr<const OriginalIcsBank> bank;
        unsigned program=0,value=0;
        OriginalIcsVoiceControls controls;
        std::array<int,16> voices;
        Channel(){voices.fill(-1);}
    };
    struct Voice {
        std::shared_ptr<const OriginalIcsBank> bank;
        unsigned sample=0,position=0,fraction=0,increment=0;
        unsigned envelope=0,envelopePhase=0,decayStep=0,releaseStep=0;
        OriginalIcsVoiceParameters parameters;
        bool active=false;
    };
    OriginalIcsVoiceTables tables_;
    std::array<Channel,4> channels_;
    std::array<Voice,64> voices_;
    std::array<std::int32_t,256> gain_{};
    unsigned master_=127;
    std::uint64_t starts_=0;
    void update(unsigned channel,bool effectRegisterWrite=false);
    void release(int voice);
    void configure(Voice& voice,const OriginalIcsVoiceParameters& parameters,bool effectRegisterWrite);
};
}
