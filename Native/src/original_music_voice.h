#pragma once
#include "original_ics_player.h"
#include <span>
namespace idas3 {
// Decoded source sample frames. Yamaha ADPCM mode2 restores its predictor at
// loopStart, so its decoded loop is repeatable; mode3 needs a streaming decoder.
struct OriginalMusicPcm {
    std::span<const std::int16_t> pcm;
    unsigned loopStart=0,loopEnd=0;
    bool looping=false;
    // Optional pre-filtered copies of the same block, same length and loop
    // points, each low-passed to Nyquist/2^k so a transposed note does not fold
    // its own top end back down. Empty on the hardware-exact path, which is the
    // default and the one the reference tests compare.
    std::span<const std::span<const std::int16_t>> bandLimited{};
};
struct OriginalMusicVoiceParameters {
    std::uint16_t pitch=0,envelope1=31,envelope2=31,lfo=0;
    std::uint8_t totalLevel=0,pan=0,directLevel=15,effectSend=0,filter=0x20;
    std::array<std::uint16_t,5> filterLevels{8191,8191,8191,8191,8191};
    std::uint16_t filterEnvelope1=0,filterEnvelope2=0;
    bool operator==(const OriginalMusicVoiceParameters&)const=default;
};
// One native sample voice at44100Hz. The song scheduler supplies the original
// voice parameters; no sound CPU, device mappings or executable code is used.
class OriginalMusicVoice {
public:
    void start(OriginalMusicPcm sample,const OriginalMusicVoiceParameters& parameters);
    void configure(const OriginalMusicVoiceParameters& parameters);
    void clearDspSend(){parameters_.effectSend=0;}
    void release();
    void stop(){active_=false;}
    OriginalIcsMixFrame renderFrame();
    bool active()const{return active_;}
    unsigned position()const{return position_;}
    unsigned fraction()const{return fraction_;}
    unsigned envelope()const{return unsigned(envelope_);}
    unsigned phase()const{return phase_;}
    unsigned lfoState()const{return lfoState_;}
    unsigned modulatedIncrement()const{return (increment_*lfoPitch_)>>10;}
    unsigned filterValue()const{return filterValue_;}
    const OriginalMusicVoiceParameters& parameters()const{return parameters_;}
private:
    OriginalMusicPcm sample_;
    OriginalMusicVoiceParameters parameters_;
    unsigned position_=0,fraction_=0,increment_=0,phase_=0;
    std::int32_t envelope_=0;
    std::array<unsigned,4> rates_{};
    unsigned lfoState_=0,lfoCounter_=0,lfoPeriod_=1,lfoPitch_=1024,lfoAmplitude_=0;
    unsigned filterValue_=0,filterPhase_=0;
    // The block this voice reads: the authored one unless a band-limited level
    // was supplied and the pitch calls for it.
    std::span<const std::int16_t> read_;
    std::array<unsigned,4> filterRates_{};
    std::int32_t filterPrevious1_=0,filterPrevious2_=0;
    std::int64_t filterRemainder_=0;
    bool filterActive_=false;
    bool active_=false;
    void advanceEnvelope();
    void updateLfoValue();
    void advanceFilter();
    std::int32_t filterSample(std::int32_t value);
};
class OriginalMusicVoicePool {
public:
    void reset();
    void start(std::uint64_t noteId,OriginalMusicPcm sample,const OriginalMusicVoiceParameters& parameters,
        bool driverManaged=false,std::uint8_t sourceFlags0=0,std::uint8_t sourceFlags1=0);
    // Original984..B94 lifecycle decisions at a native sample-clock service
    // boundary. The original ARM main-loop poll latency is not emulated.
    void pollDriver();
    void release(std::uint64_t noteId);
    //2DF4 alone: key-off the chip without source flags20/release-list mutation.
    void keyOffHardware(std::uint64_t noteId);
    void configure(std::uint64_t noteId,const OriginalMusicVoiceParameters& parameters,bool writesEffectSend=false);
    // ARM2E94 clears physical slot sends without changing the driver's cached
    // voice parameters. Pitch/volume writes must keep those sends cleared.
    void clearDspSends();
    void releaseAll();
    OriginalIcsMixFrame renderFrame();
    unsigned activeVoices()const;
    bool active(std::uint64_t noteId)const;
    bool releasing(std::uint64_t noteId)const;
    unsigned peakVoices()const{return peak_;}
    std::uint64_t startedVoices()const{return starts_;}
private:
    struct Slot{
        std::uint64_t noteId=0;OriginalMusicVoice voice;
        unsigned loopStart=0;std::uint8_t sourceFlags0=0,sourceFlags1=0;
        bool driverManaged=false,looping=false,oneShotKeyoff=false,dspSendCleared=false;
    };
    std::array<Slot,64> slots_{};
    std::uint64_t starts_=0;
    unsigned peak_=0;
};
}
