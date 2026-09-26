#include "original_oneshot_playback.h"
#include "original_sfx_sequence.h"
#include "original_music_voice.h"
#include "original_music_control.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace idas3 {
namespace {
std::vector<std::uint8_t> read(const std::filesystem::path& path){
    std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("Missing original one-shot asset");
    return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
}
struct Reader {
    std::span<const std::uint8_t> b;std::size_t at=0;
    unsigned byte(){if(at>=b.size())throw std::runtime_error("Truncated original one-shot asset");return b[at++];}
    unsigned word(){auto v=byte();return v|(byte()<<8);}
    std::uint32_t dword(){auto v=word();return v|(word()<<16);}
    std::uint64_t qword(){auto v=std::uint64_t(dword());return v|(std::uint64_t(dword())<<32);}
};
std::uint64_t fingerprint(std::span<const std::uint8_t> b){std::uint64_t h=14695981039346656037ull;for(auto v:b)h=(h^v)*1099511628211ull;return h;}
constexpr std::array<unsigned,5> bankNumbers{20,21,22,24,25};
unsigned bankIndex(unsigned number){auto at=std::find(bankNumbers.begin(),bankNumbers.end(),number);if(at==bankNumbers.end())throw std::out_of_range("Original one-shot bank");return unsigned(at-bankNumbers.begin());}
}
int originalOneShotSteal(std::span<const std::uint8_t> priorities,unsigned incoming){
    int selected=-1;unsigned maximum=incoming;
    for(unsigned i=0;i<priorities.size();++i)if(priorities[i]>=incoming&&(selected<0||priorities[i]>maximum)){selected=int(i);maximum=priorities[i];}
    return selected;
}
struct OriginalOneShotPlayback::Impl {
    struct Sound {
        OriginalMenuSound source;OriginalMusicVoiceParameters parameters;
        OriginalMusicVolumeContext volume;std::array<std::uint8_t,128> velocity{};
        unsigned loopStart=0,loopEnd=0,incomingPriority=0;
        std::uint8_t flags0=0,flags1=0,group=0,priority=0;
    };
    struct Bank {std::vector<Sound> sounds;std::vector<OriginalSfxSequence> cues;std::uint8_t volume=127;};
    struct Slot {
        OriginalMusicVoice voice;OriginalMusicVoiceParameters parameters;OriginalMusicVolumeContext volume;
        unsigned bank=0,playback=0;std::uint64_t order=0;std::uint8_t flags1=0,priority=0;bool sendCleared=false;
        float outputGain=1.f;
    };
    struct Track {OriginalSfxSequencer sequence;unsigned bank=0;float outputGain=1.f;};
    std::array<Bank,5> banks;std::array<Slot,32> slots;std::array<Track,16> tracks;
    OriginalOneShotStatistics stats;std::uint64_t nextOrder=0;
    unsigned active()const{return unsigned(std::count_if(slots.begin(),slots.end(),[](const auto&s){return s.voice.active();}));}
    void volume(Slot& s){
        s.volume.channelGain10=originalMusicChannelGain(127,64,banks[s.bank].volume,64,s.volume.channelFlags0);
        s.parameters.totalLevel=originalMusicTotalLevel(s.volume);
        auto p=s.parameters;if(s.sendCleared)p.effectSend=0;s.voice.configure(p);
    }
    void note(unsigned bank,const OriginalSfxSequenceEvent& event,float outputGain){
        const auto& sound=banks[bank].sounds.at(event.playback);
        if(!event.volume)return; // All imported commands have nonzero velocity.
        if((sound.flags1&0x83)==0x80)for(auto& s:slots)
            if(s.voice.active()&&s.bank==bank&&s.playback==event.playback&&(s.flags1&0x83)==0x80){s.voice.release();++stats.chokes;}
        auto free=std::find_if(slots.begin(),slots.end(),[](const auto&s){return !s.voice.active();});
        if(free==slots.end()){
            std::array<unsigned,32> order{};for(unsigned i=0;i<order.size();++i)order[i]=i;
            std::sort(order.begin(),order.end(),[&](unsigned a,unsigned b){return slots[a].order<slots[b].order;});
            std::array<std::uint8_t,32> priorities{};for(unsigned i=0;i<order.size();++i)priorities[i]=slots[order[i]].priority;
            const int chosen=originalOneShotSteal(priorities,sound.incomingPriority);
            if(chosen<0){++stats.dropped;return;}
            free=slots.begin()+order[chosen];free->voice.stop();++stats.stolen;
        }
        free->bank=bank;free->playback=event.playback;free->order=nextOrder++;
        free->outputGain=outputGain;
        free->flags1=sound.flags1;free->priority=sound.priority;free->sendCleared=false;
        free->parameters=sound.parameters;free->volume=sound.volume;free->volume.velocityTableValue=sound.velocity[event.volume];
        free->volume.channelGain10=originalMusicChannelGain(127,64,banks[bank].volume,64,free->volume.channelFlags0);
        free->parameters.totalLevel=originalMusicTotalLevel(free->volume);
        free->voice.start({sound.source.clip.samples,sound.loopStart,sound.loopEnd,false},free->parameters);
        ++stats.notes;stats.peakVoices=std::max(stats.peakVoices,active());
    }
};
OriginalOneShotPlayback::OriginalOneShotPlayback(const std::filesystem::path& root):impl_(std::make_unique<Impl>()){
    for(unsigned index=0;index<bankNumbers.size();++index){
        auto& bank=impl_->banks[index];const unsigned number=bankNumbers[index];const auto name="PACK"+std::to_string(number);
        const auto bytes=read(root/"data/original_audio"/(number==21?"menu":"race")/(name+".dtpk"));
        const auto fixture=read(root/"data/original_audio/oneshot"/(name+".idso"));Reader r{fixture};
        if(r.dword()!=0x4f534449||r.dword()!=1||r.dword()!=number)throw std::runtime_error("Unknown original one-shot fixture");
        const auto count=r.dword();if(!count||count>128||r.qword()!=fingerprint(bytes))throw std::runtime_error("Original one-shot bank identity mismatch");
        Reader source{bytes,0x30};const auto playbackTable=source.dword();source.at=playbackTable+16;
        if(source.word()+1!=count)throw std::runtime_error("Original one-shot playback count mismatch");
        for(unsigned playback=0;playback<count;++playback){
            Impl::Sound s;s.source=decodeOriginalSfxPlayback(bytes,playback);std::array<std::uint32_t,18> regs{};for(auto&reg:regs)reg=r.dword();
            s.flags0=std::uint8_t(r.byte());s.flags1=std::uint8_t(r.byte());s.group=std::uint8_t(r.byte());s.priority=std::uint8_t(r.byte());s.incomingPriority=r.dword();
            s.volume={std::uint8_t(r.byte()),std::uint8_t(r.byte()),std::uint8_t(r.byte()),std::uint8_t(r.byte()),std::uint8_t(r.byte()),std::uint8_t(r.byte()),std::uint8_t(r.byte())};
            if(r.byte()!=0)throw std::runtime_error("Original one-shot reserved field");for(auto&v:s.velocity)v=std::uint8_t(r.byte());
            const auto& record=s.source.playback;
            if((regs[0]&0x380)||s.flags0!=0xc8||(s.flags1&0x83)!=0&&(s.flags1&0x83)!=0x80||s.flags1!=(record[34]&0xc3)||s.group!=record[35]||s.incomingPriority!=128u+record[36]||s.priority!=std::uint8_t(s.incomingPriority))
                throw std::runtime_error("Unverified original one-shot voice mode");
            s.loopStart=regs[2];s.loopEnd=regs[3];if(s.loopStart>=s.loopEnd||s.loopEnd>s.source.clip.frames())throw std::runtime_error("Original one-shot sample bounds");
            auto&p=s.parameters;p.envelope1=std::uint16_t(regs[4]);p.envelope2=std::uint16_t(regs[5]);p.pitch=std::uint16_t(regs[6]);p.lfo=std::uint16_t(regs[7]);
            p.effectSend=std::uint8_t(regs[8]);p.directLevel=std::uint8_t(regs[9]>>8);p.pan=std::uint8_t(regs[9]);p.totalLevel=std::uint8_t(regs[10]>>8);p.filter=std::uint8_t(regs[10]);
            for(unsigned i=0;i<5;++i)p.filterLevels[i]=std::uint16_t(regs[11+i]);p.filterEnvelope1=std::uint16_t(regs[16]);p.filterEnvelope2=std::uint16_t(regs[17]);
            if(originalMusicTotalLevel(s.volume)!=p.totalLevel)throw std::runtime_error("Original one-shot level mismatch");
            bank.sounds.push_back(std::move(s));
        }
        if(r.at!=fixture.size())throw std::runtime_error("Unexpected original one-shot fixture tail");
        // These complete source A9 groups contain4/15/8/6/3 tracks respectively.
        const unsigned cueCount=number==20?4:number==21?15:number==22?8:number==24?6:3;
        for(unsigned cue=0;cue<cueCount;++cue){auto sequence=decodeOriginalSfxSequence(bytes,0xa9|((number-20)<<8)|(cue<<16));
            for(const auto& e:sequence.events)if(!e.volume||e.playback>=count)throw std::runtime_error("Unverified original one-shot sequence");bank.cues.push_back(std::move(sequence));}
    }
}
OriginalOneShotPlayback::~OriginalOneShotPlayback()=default;
void OriginalOneShotPlayback::reset(){for(auto& s:impl_->slots)s={};for(auto&t:impl_->tracks)t.sequence.reset();for(auto&b:impl_->banks)b.volume=127;impl_->stats={};impl_->nextOrder=0;}
void OriginalOneShotPlayback::play(unsigned number,unsigned cue,float outputGain){
    if(!std::isfinite(outputGain)||outputGain<0.f||outputGain>4.f)throw std::invalid_argument("One-shot output gain must be finite in0..4");
    const unsigned bank=bankIndex(number);const auto& sequence=impl_->banks[bank].cues.at(cue);++impl_->stats.cues;
    auto track=std::find_if(impl_->tracks.begin(),impl_->tracks.end(),[](const auto&t){return !t.sequence.active();});
    //7D98..7DE4: first free track, otherwise first SFX track. The source16
    // sequencer slots are shared with music; this partition keeps their bound.
    if(track==impl_->tracks.end())track=impl_->tracks.begin();track->bank=bank;track->outputGain=outputGain;
    track->sequence.start(sequence,[&](const auto& e){impl_->note(bank,e,outputGain);});
}
void OriginalOneShotPlayback::stopBank(unsigned number){const unsigned bank=bankIndex(number);for(auto&s:impl_->slots)if(s.bank==bank)s.voice.stop();for(auto&t:impl_->tracks)if(t.bank==bank)t.sequence.stop();}
void OriginalOneShotPlayback::setBankVolume(unsigned number,std::uint8_t level){const unsigned bank=bankIndex(number);impl_->banks[bank].volume=level&127;for(auto&s:impl_->slots)if(s.voice.active()&&s.bank==bank)impl_->volume(s);}
void OriginalOneShotPlayback::clearDspSends(){for(auto&s:impl_->slots)if(s.voice.active()){s.voice.clearDspSend();s.sendCleared=true;}}
OriginalIcsMixFrame OriginalOneShotPlayback::renderFrame(){
    OriginalIcsMixFrame result;for(auto&s:impl_->slots)if(s.voice.active()){const auto f=s.voice.renderFrame();for(unsigned i=0;i<2;++i)result.dry[i]+=std::int32_t(f.dry[i]*s.outputGain);for(unsigned i=0;i<16;++i)result.effects[i]+=std::int32_t(f.effects[i]*s.outputGain);}
    for(auto&t:impl_->tracks)t.sequence.advanceSample([&](const auto&e){impl_->note(t.bank,e,t.outputGain);});++impl_->stats.frames;return result;
}
OriginalOneShotStatistics OriginalOneShotPlayback::statistics()const{auto s=impl_->stats;s.activeVoices=impl_->active();return s;}
}
