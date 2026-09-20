#pragma once
#include "original_music_bank.h"
#include "original_music_sequence.h"
#include "original_music_control.h"
#include "original_selection_music.h"
#include <array>
#include <utility>
#include <vector>
namespace idas3 {
struct OriginalMusicPlaybackStatistics {
    std::uint64_t samples=0,songsStarted=0,notesStarted=0,noteOffs=0,parameterChanges=0,volumeChanges=0,fades=0,stops=0;
    std::uint64_t percussionChokes=0,legatoRetunes=0;
    // A retune that found no voice to move is a note the melody loses.
    std::uint64_t retunesUnmatched=0,retunesFlagRejected=0,retunesNoChannel=0;
    std::uint64_t configuresSeen=0,configuresApplied=0,configuresReleaseSkipped=0;
    // Our recomputed total level against the driver's own programmed TL.
    std::uint64_t levelChecked=0,levelMismatched=0,levelRawMatches=0;
    std::array<std::uint32_t,16> levelMismatchPerChannel{};
    std::array<std::int64_t,16> levelDeltaSum{};
    std::array<std::int32_t,16> levelDeltaMin{},levelDeltaMax{};
    unsigned peakVoices=0,sourceLevel=127,fadeLevel=127;
    int cue=-1;
};
class OriginalMusicPlayback {
public:
    using SourceCommandOutput=std::function<void(unsigned,std::uint32_t)>;
    explicit OriginalMusicPlayback(const std::filesystem::path& root);
    void apply(const original::OriginalSelectionMusicCommand& command);
    void stop();
    void reset();
    OriginalIcsMixFrame renderFrame();
    // Forward authored bank/scene commands before processing this frame's
    // sends in the shared effects processor.
    void setSourceCommandOutput(SourceCommandOutput output){commandOutput_=std::move(output);}
    void clearDspSends(){pool_.clearDspSends();}
    const OriginalMusicPlaybackStatistics& statistics()const{return stats_;}
    // One live voice, for lining our synthesis parameters up against the traced
    // 64-byte driver channel records the ARM harness already verifies.
    struct LiveVoice {
        unsigned channel=0,key=0,layer=0,voiceFlags0=0,voiceFlags1=0;
        OriginalMusicVoiceParameters parameters;
        bool releasing=false;
    };
    std::vector<LiveVoice> liveVoices()const{
        std::vector<LiveVoice> out;
        for(const auto& note:notes_)
            if(pool_.active(note.id))
                out.push_back({note.channel,note.key,note.layer,note.voiceFlags0,
                               note.voiceFlags1,note.parameters,note.releasing});
        return out;
    }
    // Development diagnostic only: play one authored channel in isolation so a
    // missing instrument can be named rather than guessed at. The game never
    // sets this; ~0u is every channel.
    void setDiagnosticChannel(unsigned channel){diagnosticChannel_=channel;}
    bool playing()const;
    unsigned activeVoices()const{return pool_.activeVoices();}
    std::uint64_t samplePosition()const;
private:
    struct Note {
        std::uint64_t id=0;
        unsigned cue=0,channel=0,key=0,layer=0;
        unsigned voiceFlags0=0,voiceFlags1=0;
        OriginalMusicVoiceParameters parameters;
        OriginalMusicVolumeContext volume;
        unsigned group=0;
        bool releasing=false;
    };
    // Every cue of the source BGM table, loaded on the request that names it.
    // Decoding all 39 banks up front costs seconds and 12MB for scores a given
    // session may never reach, and the source loads one bank at a time too.
    std::filesystem::path root_;
    std::vector<OriginalMusicBank> banks_=std::vector<OriginalMusicBank>(original::originalMusicCueCount);
    std::vector<OriginalMusicSequence> sequences_=std::vector<OriginalMusicSequence>(original::originalMusicCueCount);
    std::vector<char> present_=std::vector<char>(original::originalMusicCueCount,0);
    OriginalMusicVoicePool pool_;
    std::vector<Note> notes_;
    OriginalMusicTrackControl track_;
    OriginalMusicPlaybackStatistics stats_;
    std::uint64_t nextNote_=1;
    unsigned timerPhase_=0,diagnosticChannel_=~0u;
    SourceCommandOutput commandOutput_;
    void start(unsigned cue);
    void require(unsigned cue);
    void dispatch(const OriginalMusicSequenceEvent& event);
    void control(std::uint32_t word,unsigned argument);
    void updateVolumes();
    void setVolume(Note& note);
    const OriginalMusicSample& sample(unsigned cue,unsigned sourceId)const;
};
}
