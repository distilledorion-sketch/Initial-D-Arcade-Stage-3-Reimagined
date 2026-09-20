#include "original_music_sequence.h"
#include "original_selection_music.h"
#include <cctype>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <cstring>
namespace idas3 {
namespace {
std::uint32_t word(const std::vector<unsigned char>& b,std::size_t p){
    if(p>b.size()||b.size()-p<4)throw std::runtime_error("Truncated original music sequence");
    return unsigned(b[p])|(unsigned(b[p+1])<<8)|(unsigned(b[p+2])<<16)|(unsigned(b[p+3])<<24);
}
OriginalMusicVoiceParameters parameters(const std::vector<unsigned char>& b,std::size_t p){
    OriginalMusicVoiceParameters v;
    v.envelope1=std::uint16_t(word(b,p+0x10));v.envelope2=std::uint16_t(word(b,p+0x14));
    v.pitch=std::uint16_t(word(b,p+0x18));v.lfo=std::uint16_t(word(b,p+0x1c));
    v.effectSend=std::uint8_t(word(b,p+0x20));v.pan=std::uint8_t(word(b,p+0x24)&31);
    v.directLevel=std::uint8_t((word(b,p+0x24)>>8)&15);
    v.filter=std::uint8_t(word(b,p+0x28));v.totalLevel=std::uint8_t(word(b,p+0x28)>>8);
    for(unsigned i=0;i<5;++i)v.filterLevels[i]=std::uint16_t(word(b,p+0x2c+i*4));
    v.filterEnvelope1=std::uint16_t(word(b,p+0x40));v.filterEnvelope2=std::uint16_t(word(b,p+0x44));
    return v;
}
}
void applyOriginalMusicParameterPatch(OriginalMusicVoiceParameters& p,const OriginalMusicSequenceEvent& e){
    if(e.parameterMask==OriginalMusicAllParameters){p=e.parameters;return;}
    if(e.parameterMask&OriginalMusicPitch)p.pitch=e.parameters.pitch;
    if(e.parameterMask&OriginalMusicLfo)p.lfo=e.parameters.lfo;
    if(e.parameterMask&OriginalMusicPan)p.pan=e.parameters.pan;
    if(e.parameterMask&OriginalMusicTotalLevelParameter)p.totalLevel=e.parameters.totalLevel;
    for(unsigned i=0;i<5;++i)if(e.parameterMask&(OriginalMusicFilterLevel0<<i))p.filterLevels[i]=e.parameters.filterLevels[i];
}
std::filesystem::path originalMusicSequencePath(const std::filesystem::path& root,unsigned cue){
    // The score is a property of the bank, so it is named by the bank the cue
    // asks for. Two names serve two cues each, which is why the stamped cue in
    // the file is compared by bank rather than by number.
    const auto stem=original::originalMusicCueDescriptor(cue).filename;
    std::string name(stem.substr(0,stem.rfind('.')));
    for(auto& c:name)c=char(std::tolower(static_cast<unsigned char>(c)));
    const char* directory=cue<original::originalMusicMenuCues?"selection":"music";
    return root/"data"/"original_audio"/directory/(name+".idms");
}
void OriginalMusicSequence::load(const std::filesystem::path& root,unsigned cue){
    if(cue>=original::originalMusicCueCount)throw std::runtime_error("Unknown original music cue");
    const auto path=originalMusicSequencePath(root,cue);
    std::ifstream in(path,std::ios::binary);
    if(!in)throw std::runtime_error("Original music sequence is missing");
    const std::vector<unsigned char> b((std::istreambuf_iterator<char>(in)),{});
    // The menu scores carry record version 3; every other cue version 4.
    const unsigned version=cue<original::originalMusicMenuCues?3u:4u;
    const auto stamped=b.size()>=16?word(b,12):~0u;
    if(b.size()<32||std::memcmp(b.data(),"IDMSEQ1\0",8)||word(b,8)!=version||word(b,16)!=44||
       stamped>=original::originalMusicCueCount||
       original::originalMusicCueDescriptor(stamped).filename!=original::originalMusicCueDescriptor(cue).filename)
        throw std::runtime_error("Invalid original music sequence header");
    const auto start=word(b,20),end=word(b,24),count=word(b,28);
    if(start>=end||end>1000000||count>1000000||b.size()!=32ull+112ull*count)
        throw std::runtime_error("Invalid original selection music sequence extent");
    std::vector<OriginalMusicSequenceEvent> decoded;decoded.reserve(count);
    for(unsigned i=0;i<count;++i){
        const std::size_t at=32ull+i*112ull;OriginalMusicSequenceEvent e;
        const auto kind=word(b,at);if(kind>4)throw std::runtime_error("Unknown original music event");
        e.kind=OriginalMusicSequenceEventKind(kind);e.tick=word(b,at+4);e.command=word(b,at+8);
        e.channel=word(b,at+12);e.note=word(b,at+16);e.layerOffset=word(b,at+20);
        e.sampleId=word(b,at+24);e.parameterMask=word(b,at+28);e.parameters=parameters(b,at+32);
        if(e.kind==OriginalMusicSequenceEventKind::NoteOn){
            // A stereo pair marks itself in bit 24 and carries the driver's
            // stereo pan in bits 16..23. Each lane already carries its own pan
            // in its parameters, so the marker only has to be accepted here.
            if(e.parameterMask&0xfe000000u)throw std::runtime_error("Invalid original music voice flags");
            e.voiceFlags0=std::uint8_t(e.parameterMask);e.voiceFlags1=std::uint8_t(e.parameterMask>>8);
            e.parameterMask=OriginalMusicAllParameters;
        }
        if(e.kind==OriginalMusicSequenceEventKind::Configure){
            e.configureReleaseTails=(e.command>>28)==0xe;
            e.configureReleaseAt40Voices=(e.command>>28)==0xb;
        }
        // The eighth volume byte says which half of a stereo sample the
        // driver gave this voice; it is zero for every mono record.
        e.sampleChannel=b[at+111];
        if(e.sampleChannel>1)throw std::runtime_error("Invalid original music sample channel");
        e.velocityTableValue=b[at+104];e.layerGain8=b[at+105];e.channelVolume0A=b[at+106];
        e.channelGain10=b[at+107];e.master05=b[at+108];e.bankFade06=b[at+109];e.channelFlags0=b[at+110];
        if(e.tick>=end||(!decoded.empty()&&e.tick<decoded.back().tick)||e.channel>15||e.note>127||
           (e.kind==OriginalMusicSequenceEventKind::Configure&&
            (e.parameterMask&~(255u|OriginalMusicTotalLevelParameter)))||
           (e.kind==OriginalMusicSequenceEventKind::Retune&&e.parameterMask!=257u))
            throw std::runtime_error("Invalid original music event data");
        decoded.push_back(e);
    }
    auto loop=std::lower_bound(decoded.begin(),decoded.end(),start,[](const auto& e,unsigned t){return e.tick<t;});
    if(loop==decoded.end())throw std::runtime_error("Original music loop has no events");
    loopEventIndex_=std::size_t(loop-decoded.begin());events_=std::move(decoded);
    cue_=cue;loopStartTick_=start;loopEndTick_=end;reset();
}
void OriginalMusicSequence::reset(){
    playing_=false;samplePosition_=0;loopOffsetTicks_=0;eventIndex_=0;
}
void OriginalMusicSequence::advanceSample(const Callback& callback){
    if(!playing_)return;
    if(events_.empty()||loopEndTick_<=loopStartTick_)throw std::runtime_error("Original music sequence has not been loaded");
    if(samplePosition_%44==0){
        const auto tick=samplePosition_/44;
        if(tick>=std::uint64_t(loopEndTick_)+loopOffsetTicks_){
            loopOffsetTicks_+=loopEndTick_-loopStartTick_;eventIndex_=loopEventIndex_;
        }
        while(eventIndex_<events_.size()&&std::uint64_t(events_[eventIndex_].tick)+loopOffsetTicks_<=tick)
            callback(events_[eventIndex_++]);
    }
    ++samplePosition_;
}
}
