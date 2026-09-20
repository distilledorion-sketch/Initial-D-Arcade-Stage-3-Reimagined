#include "original_sfx_sequence.h"
#include <stdexcept>
#include <fstream>
#include <iterator>
#include <algorithm>
namespace idas3 {
OriginalSfxSequence decodeOriginalSfxSequence(std::span<const std::uint8_t> bytes,std::uint32_t command){
    const auto b=[&](std::size_t p){if(p>=bytes.size())throw std::runtime_error("Truncated SFX sequence bank");return bytes[p];};
    const auto w=[&](std::size_t p){return unsigned(b(p))|(unsigned(b(p+1))<<8);};
    const auto u=[&](std::size_t p){return w(p)|(w(p+2)<<16);};
    if(u(0)!=0x4b505444u||u(8)!=bytes.size())throw std::runtime_error("Invalid SFX DTPK header");
    const std::size_t table=u(0x2c);if(!table||u(table)>255)throw std::runtime_error("Invalid SFX sequence groups");
    std::size_t group=0;
    for(unsigned i=0;i<=u(table);++i){const auto at=table+4+4*i;if(b(at+3)==(command&255)&&b(at+2)==((command>>8)&255)){group=table+w(at);break;}}
    const unsigned track=command>>16;if(!group||track>u(group))throw std::runtime_error("Missing SFX sequence track");
    std::size_t cursor=table+u(group+4+track*4);if(b(cursor++)!=0xc0)throw std::runtime_error("Unverified SFX sequence header");
    OriginalSfxSequence result;unsigned tick=0,status=0;
    for(unsigned guard=0;guard<256;++guard){
        const auto next=b(cursor);if(next==0xff){if(result.events.empty())throw std::runtime_error("Empty SFX sequence");return result;}
        if(next&128){status=next;++cursor;}
        if(status!=0xdf)throw std::runtime_error("Unverified SFX sequence opcode");
        const unsigned playback=b(cursor++),volume=b(cursor++),tail=b(cursor++);
        if(playback>127||volume>127||(tail&127)!=0)throw std::runtime_error("Unverified SFX sequence command fields");
        result.events.push_back({tick,std::uint8_t(playback),std::uint8_t(volume)});
        if(!(tail&128)){unsigned delay=b(cursor++);if(delay&128)delay=((delay&127)<<7)|b(cursor++);tick+=delay;}
    }
    throw std::runtime_error("SFX sequence event bound exceeded");
}
void OriginalSfxSequencer::dispatch(const Output& output){
    if(!sequence_)return;
    while(next_<sequence_->events.size()&&sequence_->events[next_].tick<=tick_-start_){if(output)output(sequence_->events[next_]);++next_;}
    if(next_==sequence_->events.size())sequence_=nullptr;
}
void OriginalSfxSequencer::start(const OriginalSfxSequence& sequence,const Output& output){sequence_=&sequence;next_=0;start_=tick_;dispatch(output);}
void OriginalSfxSequencer::advanceSample(const Output& output){if(++samplePhase_==samplesPerTick){samplePhase_=0;++tick_;dispatch(output);}}
OriginalTirePlayback::OriginalTirePlayback(const std::filesystem::path& root){
    std::ifstream f(root/"data/original_audio/race/PACK23.dtpk",std::ios::binary);
    if(!f)throw std::runtime_error("Original skid bank unavailable");
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};
    std::ifstream cues(root/"data/original_audio/race/skid_cues.bin",std::ios::binary);
    const auto word=[&](){std::array<unsigned char,4>b{};if(!cues.read(reinterpret_cast<char*>(b.data()),4))throw std::runtime_error("Missing skid cue table");return unsigned(b[0])|(unsigned(b[1])<<8)|(unsigned(b[2])<<16)|(unsigned(b[3])<<24);};
    if(word()!=0x4b534449||word()!=0x31303030||word()!=6)throw std::runtime_error("Unknown skid cue format");
    for(unsigned cue=0;cue<6;++cue){commands_[cue]=word();if((commands_[cue]&65535)!=0x3a9)throw std::runtime_error("Unexpected skid bank command");sequences_[cue]=decodeOriginalSfxSequence(bytes,commands_[cue]);}
    if(cues.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Unexpected skid cue table tail");
    for(unsigned i=0;i<sounds_.size();++i){sounds_[i]=decodeOriginalSfxPlayback(bytes,i);if(sounds_[i].clip.sampleRate!=22050)throw std::runtime_error("Unverified skid PCM sample rate");}
}
void OriginalTirePlayback::reset(){sequencer_.reset();voices_={};nextVoice_=starts_=cueStarts_=0;volume_=127;}
void OriginalTirePlayback::play(const OriginalSfxSequenceEvent& event){
    if(event.playback>=sounds_.size())throw std::runtime_error("Skid playback outside bank");
    for(auto& voice:voices_)if(voice.active&&voice.playback==event.playback){voice={event.playback,0,event.volume,true};++starts_;return;}
    auto free=std::find_if(voices_.begin(),voices_.end(),[](const auto& voice){return !voice.active;});
    if(free==voices_.end())free=voices_.begin()+(nextVoice_++%voices_.size());
    *free={event.playback,0,event.volume,true};++starts_;
}
void OriginalTirePlayback::apply(const original::OriginalTireCommand& command){
    using Type=original::OriginalTireCommandType;
    if(command.type==Type::Stop){sequencer_.stop();for(auto& voice:voices_)voice.active=false;}
    else if(command.type==Type::Volume)volume_=std::clamp(command.value,0,127);
    else{++cueStarts_;sequencer_.start(sequences_.at(unsigned(command.value)),[this](const auto& event){play(event);});}
}
std::int32_t OriginalTirePlayback::renderFrame(){
    std::int32_t mixed=0;
    for(auto& voice:voices_)if(voice.active){
        const auto& clip=sounds_[voice.playback].clip;const unsigned index=voice.halfFrame>>1;
        if(index>=clip.frames()){voice.active=false;continue;}
        int value=clip.samples[index];if(voice.halfFrame&1)value=(value>>1)+(int(clip.samples[std::min<std::size_t>(index+1,clip.frames()-1)])>>1);
        mixed+=std::int32_t(std::int64_t(value)*voice.volume*volume_/(127*127));++voice.halfFrame;
    }
    sequencer_.advanceSample([this](const auto& event){play(event);});return mixed;
}
}
