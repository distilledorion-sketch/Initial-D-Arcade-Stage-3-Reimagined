#include "original_oneshot_playback.h"
#include "original_sfx_sequence.h"
#include "original_music_voice.h"
#include "original_music_control.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
using namespace idas3;
namespace {
std::uint64_t checks=0;
void require(bool yes,const char* message){++checks;if(!yes)throw std::runtime_error(message);}
std::vector<std::uint8_t> read(const std::filesystem::path&p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("Missing test fixture");return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
struct Fixture {
    OriginalMenuSound sound;OriginalMusicVoiceParameters p;OriginalMusicVolumeContext volume;
    std::array<std::uint8_t,128> velocities;unsigned start=0,end=0,flags=0;
};
std::vector<Fixture> fixtures(const std::filesystem::path&root,unsigned bank,std::span<const std::uint8_t>source){
    const auto b=read(root/"data/original_audio/oneshot"/("PACK"+std::to_string(bank)+".idso"));std::size_t at=0;
    const auto u=[&](){unsigned value=0;for(unsigned i=0;i<4;++i)value|=unsigned(b.at(at++))<<(i*8);return value;};
    require(u()==0x4f534449&&u()==1&&u()==bank,"Fixture header");const auto count=u();at+=8;std::vector<Fixture>out;
    for(unsigned i=0;i<count;++i){Fixture f;f.sound=decodeOriginalSfxPlayback(source,i);std::array<unsigned,18>regs;for(auto&reg:regs)reg=u();
        at++;f.flags=b.at(at++);at+=6;
        f.volume={b.at(at),b.at(at+1),b.at(at+2),b.at(at+3),b.at(at+4),b.at(at+5),b.at(at+6)};at+=8;
        std::copy_n(b.begin()+at,128,f.velocities.begin());at+=128;f.start=regs[2];f.end=regs[3];
        f.p={std::uint16_t(regs[6]),std::uint16_t(regs[4]),std::uint16_t(regs[5]),std::uint16_t(regs[7]),std::uint8_t(regs[10]>>8),std::uint8_t(regs[9]),std::uint8_t(regs[9]>>8),std::uint8_t(regs[8]),std::uint8_t(regs[10])};
        for(unsigned j=0;j<5;++j)f.p.filterLevels[j]=std::uint16_t(regs[11+j]);f.p.filterEnvelope1=std::uint16_t(regs[16]);f.p.filterEnvelope2=std::uint16_t(regs[17]);out.push_back(std::move(f));
    }require(at==b.size(),"Fixture exact length");return out;
}
}
int main(int argc,char**argv){try{
    if(argc!=2)throw std::runtime_error("Expected project root");const std::filesystem::path root=argv[1];OriginalOneShotPlayback actual(root);
    std::uint64_t audible=0,wet=0,stereo=0,frames=0;
    for(unsigned bank:{20u,21u,22u,24u,25u}){
        const auto bytes=read(root/"data/original_audio"/(bank==21?"menu":"race")/("PACK"+std::to_string(bank)+".dtpk"));auto sounds=fixtures(root,bank,bytes);
        const unsigned count=bank==20?4:bank==21?15:bank==22?8:bank==24?6:3;
        for(unsigned cue=0;cue<count;++cue){
            const auto sequence=decodeOriginalSfxSequence(bytes,0xa9|((bank-20)<<8)|(cue<<16));actual.reset();actual.play(bank,cue);
            struct V{OriginalMusicVoice voice;unsigned playback,velocity;};std::vector<V> voices;std::size_t next=0;unsigned gain=127;
            auto dispatch=[&](unsigned frame){while(next<sequence.events.size()&&sequence.events[next].tick*44<=frame){const auto e=sequence.events[next++];const auto&s=sounds.at(e.playback);
                if((s.flags&0x83)==0x80)for(auto&v:voices)if(v.playback==e.playback)v.voice.release();
                auto p=s.p;auto volume=s.volume;volume.velocityTableValue=s.velocities[e.volume];volume.channelGain10=std::uint8_t(gain);p.totalLevel=originalMusicTotalLevel(volume);
                V v;v.playback=e.playback;v.velocity=e.volume;v.voice.start({s.sound.clip.samples,s.start,s.end,false},p);voices.push_back(std::move(v));}};
            const unsigned duration=unsigned(sequence.events.back().tick*44+300000);dispatch(0);
            for(unsigned frame=0;frame<duration;++frame){
                if(frame==900){gain=73;actual.setBankVolume(bank,73);for(auto&v:voices)if(v.voice.active()){const auto&s=sounds[v.playback];auto p=v.voice.parameters();auto volume=s.volume;volume.velocityTableValue=s.velocities[v.velocity];volume.channelGain10=73;p.totalLevel=originalMusicTotalLevel(volume);v.voice.configure(p);}}
                if(frame==1400){actual.clearDspSends();for(auto&v:voices)v.voice.clearDspSend();}
                if(frame==1800){gain=91;actual.setBankVolume(bank,91);for(auto&v:voices)if(v.voice.active()){const auto&s=sounds[v.playback];auto p=v.voice.parameters();auto volume=s.volume;volume.velocityTableValue=s.velocities[v.velocity];volume.channelGain10=91;p.totalLevel=originalMusicTotalLevel(volume);v.voice.configure(p);}}
                OriginalIcsMixFrame expected;for(auto&v:voices){const auto f=v.voice.renderFrame();for(unsigned i=0;i<2;++i)expected.dry[i]+=f.dry[i];for(unsigned i=0;i<16;++i)expected.effects[i]+=f.effects[i];}
                const auto got=actual.renderFrame();require(got.dry==expected.dry&&got.effects==expected.effects,"Native cue differs from original captured hardware registers");
                audible+=got.dry[0]!=0||got.dry[1]!=0;stereo+=got.dry[0]!=got.dry[1];for(auto v:got.effects)wet+=v!=0;
                ++frames;dispatch(frame+1);
                if(frame>2000&&next==sequence.events.size()&&!actual.statistics().activeVoices)break;
            }
            require(actual.statistics().notes==sequence.events.size(),"Complete compound sequence");require(!actual.statistics().activeVoices,"Original LEA retires voice");
        }
    }
    require(audible&&wet&&stereo,"Original stereo and DSP sends must be present");
    actual.reset();actual.play(21,2);actual.play(21,2);require(actual.statistics().notes==2&&actual.statistics().chokes==1,"Same-cue hardware keyoff preserves new allocation");
    actual.reset();for(unsigned i=0;i<32;++i)actual.play(21,5);require(actual.statistics().activeVoices==32&&actual.statistics().chokes==0,"Result count permits source overlap");
    actual.play(21,5);require(actual.statistics().dropped==1&&actual.statistics().notes==32,"Incoming priority256 must not wrap during allocation");
    actual.play(25,0);require(actual.statistics().dropped==2,"Priority0 stored voices protected");
    actual.stopBank(21);require(!actual.statistics().activeVoices,"Bank unload force-mutes matching voices");
    actual.reset();actual.play(21,5);actual.play(24,2);actual.stopBank(21);require(actual.statistics().activeVoices==1,"Unrelated bank voice survives unload");
    actual.reset();for(unsigned i=0;i<32;++i)actual.play(24,i%5);actual.play(25,0);require(actual.statistics().stolen==1&&actual.statistics().notes==33,"Source priority allocator steals eligible oldest voice");
    const std::array<std::uint8_t,5> priorities{178,128,200,200,128};require(originalOneShotSteal(priorities,128)==2,"First strongest numeric priority wins");
    require(originalOneShotSteal(priorities,200)==2&&originalOneShotSteal(priorities,201)==-1&&originalOneShotSteal(priorities,256)==-1,"Source tie and rejection policy");
    const auto cases=read(root/"verification/original-oneshot-audio/priority-cases.bin");std::size_t at=0;
    const auto word=[&](){std::uint32_t v=0;for(unsigned i=0;i<4;++i)v|=std::uint32_t(cases.at(at++))<<(i*8);return v;};
    const unsigned caseCount=word();for(unsigned i=0;i<caseCount;++i){const auto count=word(),incoming=word();const int expected=std::int32_t(word());
        require(at+count<=cases.size(),"Source priority fixture bound");const auto values=std::span(cases).subspan(at,count);at+=count;
        require(originalOneShotSteal(values,incoming)==expected,"Native priority differs from actual original ARM1F8C");}
    require(at==cases.size(),"Source priority fixture exact length");
    std::cout<<"PASS "<<checks<<" checks; "<<frames<<" exact PCM frames; "<<audible<<" audible, "<<wet<<" wet, "<<stereo<<" stereo-asymmetric frames\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
