#include "original_music_playback.h"
#include <cstdlib>
#include <algorithm>
#include <stdexcept>
namespace idas3 {
OriginalMusicPlayback::OriginalMusicPlayback(const std::filesystem::path& root):root_(root){
    // The menu cues are reached from the title screen with no load step of
    // their own, so they stay eager; the rest arrive through a request.
    for(unsigned cue=0;cue<original::originalMusicMenuCues;++cue)require(cue);
    notes_.reserve(64);
}
void OriginalMusicPlayback::require(unsigned cue){
    if(cue>=present_.size())throw std::out_of_range("Unknown original music cue");
    if(present_[cue])return;
    const auto stem=original::originalMusicCueDescriptor(cue).filename;
    const std::string name(stem.substr(0,stem.rfind('.')));
    const char* directory=cue<original::originalMusicMenuCues?"data/original_audio/selection"
                                                             :"data/original_audio/music";
    banks_[cue]=loadOriginalMusicBank(root_/directory/(name+".dtpk"));
    // Opt-in. The chip reads a sample with no decimation filter, so a note
    // transposed up folds its own top end back down -- about fifteen decibels
    // under the signal at the selection lead's 7.55x. These levels remove that
    // and are deliberately not what the hardware does, so they stay off unless
    // asked for and the reference tests keep comparing the hardware read.
    static const unsigned bandLimitLevels=[]{
        const char* setting=std::getenv("IDAS3_MUSIC_BAND_LIMITED");
        return setting?unsigned(std::max(0,std::min(4,std::atoi(setting)))):0u;
    }();
    if(bandLimitLevels)buildOriginalMusicBandLimitedLevels(banks_[cue],bandLimitLevels);
    sequences_[cue].load(root_,cue);
    present_[cue]=1;
}
const OriginalMusicSample& OriginalMusicPlayback::sample(unsigned cue,unsigned sourceId)const{
    const auto& bank=banks_.at(cue);
    if(sourceId>=256){const auto& s=bank.samples.at(sourceId-256);if(s.sourceId==sourceId)return s;}
    else for(const auto& s:bank.builtinSamples)if(s.sourceId==sourceId)return s;
    throw std::out_of_range("Unknown original music sample identity");
}
bool OriginalMusicPlayback::playing()const{return stats_.cue>=0&&sequences_[stats_.cue].playing();}
std::uint64_t OriginalMusicPlayback::samplePosition()const{return stats_.cue>=0?sequences_[stats_.cue].samplePosition():0;}
void OriginalMusicPlayback::reset(){
    for(auto& sequence:sequences_)sequence.stop();pool_.reset();notes_.clear();track_={};stats_={};nextNote_=1;timerPhase_=0;
}
void OriginalMusicPlayback::stop(){
    for(auto& sequence:sequences_)sequence.stop();pool_.reset();notes_.clear();stopOriginalMusicTracks(std::span(&track_,1),0);
    ++stats_.stops;
}
void OriginalMusicPlayback::start(unsigned cue){
    require(cue);
    for(auto& sequence:sequences_)sequence.stop();
    // A0001280 reaches2E24: force-mute/free the old bank voices, while its
    // bit80 preserves the newly started score. Ordinary note-off has tails.
    pool_.reset();notes_.clear();stats_.cue=int(cue);stats_.sourceLevel=stats_.fadeLevel=127;
    track_={};track_.flags0=0x80;track_.header1=0x40;track_.bank2=0;
    sequences_[cue].start();timerPhase_=0;++stats_.songsStarted;
}
void OriginalMusicPlayback::apply(const original::OriginalSelectionMusicCommand& command){
    using Op=original::OriginalSelectionMusicOperation;
    switch(command.operation){
    case Op::Load:require(command.cue);break;
    case Op::Unload:for(auto& sequence:sequences_)sequence.stop();pool_.reset();notes_.clear();break;
    case Op::Start:start(command.cue);break;
    case Op::Control:control(command.word,command.argument);break;
    }
}
void OriginalMusicPlayback::setVolume(Note& note){
    // These three source banks start at14/15=127 and16/17=64; their complete
    // authored scores do not change those bank-gain offsets.
    note.volume.channelGain10=originalMusicChannelGain(std::uint8_t(stats_.sourceLevel),64,127,64,note.volume.channelFlags0);
    note.volume.bankFade06=std::uint8_t(stats_.fadeLevel);
    note.parameters.totalLevel=originalMusicTotalLevel(note.volume);
}
void OriginalMusicPlayback::updateVolumes(){
    for(auto& note:notes_)if(pool_.active(note.id)){setVolume(note);pool_.configure(note.id,note.parameters);}
}
void OriginalMusicPlayback::control(std::uint32_t word,unsigned argument){
    if(word==0x000004a0){stats_.sourceLevel=argument&127;++stats_.volumeChanges;updateVolumes();}
    else if(word==0x00000aa0){if(requestOriginalMusicFade(std::span(&track_,1),0,std::uint8_t(argument)))++stats_.fades;}
    else if(word==0x001200a0)stop();
    else throw std::invalid_argument("Unhandled original selection music manager control");
}
void OriginalMusicPlayback::dispatch(const OriginalMusicSequenceEvent& event){
    using Kind=OriginalMusicSequenceEventKind;
    std::erase_if(notes_,[&](const auto& note){return !pool_.active(note.id);});
    if(event.kind==Kind::Command){
        // Voice reset/start is represented by native start. Forward authored
        // commands so the shared sound manager handles their DSP side effects.
        if(commandOutput_)commandOutput_(unsigned(stats_.cue),event.command);
        return;
    }
    if(event.kind==Kind::Retune){
        // Source20E0..2148 reuses this monophonic layer across old keys,
        // including release-list voices. It changes pitch/level and the stored
        // key, without key-on or a sample/envelope restart.
        unsigned matched=0,flagRejected=0,onChannel=0;
        for(auto& note:notes_){
            const bool sameSlot=note.cue==unsigned(stats_.cue)&&note.channel==event.channel&&
                                note.layer==event.layerOffset;
            if(sameSlot)++onChannel;
            if(sameSlot&&(note.voiceFlags0&0x94)!=0x80)++flagRejected;
        }
        for(auto& note:notes_)if(note.cue==unsigned(stats_.cue)&&note.channel==event.channel&&
            note.layer==event.layerOffset&&(note.voiceFlags0&0x94)==0x80){
            ++matched;
            note.key=event.note;
            note.parameters.pitch=event.parameters.pitch;
            note.volume={event.velocityTableValue,event.layerGain8,event.channelVolume0A,
                event.channelGain10,event.master05,event.bankFade06,event.channelFlags0};
            setVolume(note);pool_.configure(note.id,note.parameters);++stats_.legatoRetunes;
        }
        if(!matched){
            ++stats_.retunesUnmatched;
            if(flagRejected)++stats_.retunesFlagRejected;
            else if(!onChannel)++stats_.retunesNoChannel;
        }
    }else if(event.kind==Kind::NoteOn){
        if(diagnosticChannel_!=~0u&&event.channel!=diagnosticChannel_)return;
        const auto& s=sample(unsigned(stats_.cue),event.sampleId);
        Note note{nextNote_++,unsigned(stats_.cue),event.channel,event.note,event.layerOffset,event.voiceFlags0,event.voiceFlags1,event.parameters,
            {event.velocityTableValue,event.layerGain8,event.channelVolume0A,event.channelGain10,event.master05,event.bankFade06,event.channelFlags0}};
        note.group=banks_[note.cue].originalBytes.at(event.layerOffset+0x23);
        // Original22CC:81 groups choke earlier voices with the same bank,
        // logical channel and group.2DF4 changes only the chip key state.
        if((event.voiceFlags1&0x83)==0x81)for(const auto& old:notes_)
            if(pool_.active(old.id)&&(old.voiceFlags0&0x80)&&(old.voiceFlags1&0x83)==0x81&&
               old.cue==note.cue&&old.channel==note.channel&&old.group==note.group){
                pool_.keyOffHardware(old.id);++stats_.percussionChokes;
            }
        setVolume(note);
        // The driver programmed a total level into register 0x28 and the ARM
        // harness verified it. Ours is derived instead, so it has to agree.
        ++stats_.levelChecked;
        // What the level would be from the driver's own untouched volume bytes.
        {
            const OriginalMusicVolumeContext raw{event.velocityTableValue,event.layerGain8,
                event.channelVolume0A,event.channelGain10,event.master05,event.bankFade06,
                event.channelFlags0};
            if(originalMusicTotalLevel(raw)==event.parameters.totalLevel)++stats_.levelRawMatches;
        }
        if(note.parameters.totalLevel!=event.parameters.totalLevel){
            ++stats_.levelMismatched;
            if(event.channel<16){
                ++stats_.levelMismatchPerChannel[event.channel];
                const auto d=int(note.parameters.totalLevel)-int(event.parameters.totalLevel);
                stats_.levelDeltaSum[event.channel]+=d;
                stats_.levelDeltaMin[event.channel]=std::min(stats_.levelDeltaMin[event.channel],d);
                stats_.levelDeltaMax[event.channel]=std::max(stats_.levelDeltaMax[event.channel],d);
            }
        }
        // A stereo sample is two blocks and two source voices; this record
        // says which of them it is.
        if(event.sampleChannel&&!s.stereo)throw std::runtime_error("Stereo lane on a mono original music sample");
        const auto& pcm=event.sampleChannel?s.pcmRight:s.pcm;
        pool_.start(note.id,{pcm,s.loopStart,s.loopEnd,s.looping,s.bandLimitedSpans},note.parameters,true,event.voiceFlags0,event.voiceFlags1);
        notes_.push_back(note);++stats_.notesStarted;stats_.peakVoices=std::max(stats_.peakVoices,pool_.activeVoices());
    }else{
    if(event.kind==Kind::Configure)++stats_.configuresSeen;
    for(auto& note:notes_)if(note.cue==unsigned(stats_.cue)&&note.channel==event.channel&&note.key==event.note){
        if(event.kind==Kind::NoteOff){if(!(note.voiceFlags0&8)&&!note.releasing){pool_.release(note.id);note.releasing=true;++stats_.noteOffs;}}
        else if(event.kind==Kind::Configure&&note.layer==event.layerOffset){
            ++stats_.configuresApplied;
            // Original29F0 pitch includes the release list. Original285C
            // sparse controller traversal omits it until its40-voice full scan.
            if(note.releasing&&!event.configureReleaseTails&&
                !(event.configureReleaseAt40Voices&&pool_.activeVoices()>=40))continue;
            applyOriginalMusicParameterPatch(note.parameters,event);
            // Channel volume reaches the total level, which is derived. Take
            // the driver's own inputs and recompute it, so the manager's
            // later volume changes and fades still layer on correctly.
            if(event.parameterMask&OriginalMusicTotalLevelParameter){
                note.volume={event.velocityTableValue,event.layerGain8,event.channelVolume0A,
                    event.channelGain10,event.master05,event.bankFade06,event.channelFlags0};
                setVolume(note);
            }
            pool_.configure(note.id,note.parameters,event.parameterMask==OriginalMusicAllParameters);++stats_.parameterChanges;
        }
    }
    }
}
OriginalIcsMixFrame OriginalMusicPlayback::renderFrame(){
    // Original628/630 polls voice lifetime before658/660 runs7234. Native
    // service is once per sample; ARM instruction-cycle latency is not modeled.
    pool_.pollDriver();
    // ARM716C advances TimerB state, then7234 polls its updated score/fade.
    if(timerPhase_==44){
        timerPhase_=0;tickOriginalMusicFade(track_);const auto events=pollOriginalMusicFade(track_);
        for(unsigned i=0;i<events.count;++i)if(events.events[i].local){
            const auto command=events.events[i].command;
            if((command&0xffff0000)==0xa01c0000){stats_.fadeLevel=originalMusicFadeLevel(std::uint8_t(command>>8));updateVolumes();}
            else if((command&0xffffff00)==0xa0001200)stop();
            else throw std::runtime_error("Unhandled source music fade command");
        }
    }
    if(playing())sequences_[stats_.cue].advanceSample([&](const auto& event){dispatch(event);});
    ++timerPhase_;++stats_.samples;return pool_.renderFrame();
}
}
