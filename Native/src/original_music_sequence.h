#pragma once
#include "original_music_voice.h"
#include <filesystem>
#include <functional>
#include <vector>
namespace idas3 {
enum class OriginalMusicSequenceEventKind : unsigned { Command, NoteOn, NoteOff, Configure, Retune };
enum OriginalMusicParameterMask : unsigned {
    OriginalMusicPitch=1, OriginalMusicLfo=2, OriginalMusicPan=4,
    OriginalMusicFilterLevel0=8, OriginalMusicFilterLevel1=16,
    OriginalMusicFilterLevel2=32, OriginalMusicFilterLevel3=64,
    OriginalMusicFilterLevel4=128,
    OriginalMusicTotalLevelParameter=256,
    OriginalMusicAllParameters=0xffffffffu
};
struct OriginalMusicSequenceEvent {
    OriginalMusicSequenceEventKind kind=OriginalMusicSequenceEventKind::Command;
    unsigned tick=0,command=0,channel=0,note=0,layerOffset=0,sampleId=0,sampleChannel=0;
    unsigned parameterMask=OriginalMusicAllParameters;
    OriginalMusicVoiceParameters parameters;
    // Inputs to original42D4 before its staged quantization. The manager can
    // replace bankFade06 to apply its original dynamic4A0/AA0 volume control.
    std::uint8_t velocityTableValue=0,layerGain8=0,channelVolume0A=0,
        channelGain10=0,master05=0,bankFade06=0,channelFlags0=0;
    std::uint8_t voiceFlags0=0,voiceFlags1=0; //0&8 ignores ordinary note-off
    // Source29F0 pitch visits releasing voices;285C CC's sparse path omits
    // their list until40 allocated voices selects its full64-voice scan.
    bool configureReleaseTails=false,configureReleaseAt40Voices=false;
};
// Where a cue keeps its score: the menu three under selection, the rest under
// music, each named by the bank the source BGM table gives that cue.
std::filesystem::path originalMusicSequencePath(const std::filesystem::path& root,unsigned cue);
void applyOriginalMusicParameterPatch(OriginalMusicVoiceParameters& destination,
                                     const OriginalMusicSequenceEvent& event);
// The authored TYPE/SELECT score, decoded through bounded original ARM routines.
// One tick is exactly44 sample clocks. NoteOff matches channel+note; Configure
// matches channel+note+layerOffset; its two flags govern release-phase targets.
// The caller assigns a new monotonic voice ID for each NoteOn and owns tails.
// Retune matches channel+layerOffset across all active voices (including tails),
// sets their key to event.note, and updates pitch/TL+volume context without
// restarting sample/envelope. The used source mono channels have no glide.
class OriginalMusicSequence {
public:
    using Callback=std::function<void(const OriginalMusicSequenceEvent&)>;
    void load(const std::filesystem::path& root,unsigned cue); // game cue0 TYPE,1 SELECT,2 RESULT
    void reset();
    void start(){reset();playing_=true;}
    void stop(){playing_=false;}
    // Call before rendering each44100Hz sample, beginning at sample0.
    void advanceSample(const Callback& callback);
    bool playing()const{return playing_;}
    unsigned loopStartTick()const{return loopStartTick_;}
    unsigned loopEndTick()const{return loopEndTick_;}
    unsigned cue()const{return cue_;}
    std::uint64_t samplePosition()const{return samplePosition_;}
    const std::vector<OriginalMusicSequenceEvent>& events()const{return events_;}
private:
    std::vector<OriginalMusicSequenceEvent> events_;
    unsigned cue_=0,loopStartTick_=0,loopEndTick_=0;
    std::size_t eventIndex_=0,loopEventIndex_=0;
    std::uint64_t samplePosition_=0,loopOffsetTicks_=0;
    bool playing_=false;
};
}
