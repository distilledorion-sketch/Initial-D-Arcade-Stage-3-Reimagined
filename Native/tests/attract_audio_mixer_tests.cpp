#include "audio.h"
#include "original_stream_gain.h"
#include <algorithm>
#include <cmath>
#include <iostream>
using namespace idas3;
namespace {
std::uint64_t compared{};
std::array<short,2> expected(const OriginalAudioClip& clip,double frame,unsigned volume=127,bool sourceVolume=true){
    std::array<short,2> out{};const auto at=std::size_t(frame);if(at>=clip.frames())return out;
    const float fraction=float(frame-at);
    for(unsigned c=0;c<2;++c){const auto a=at*clip.channels+std::min(c,clip.channels-1),b=std::min(at+1,clip.frames()-1)*clip.channels+std::min(c,clip.channels-1);
        const float value=(clip.samples[a]+(clip.samples[b]-clip.samples[a])*fraction)/32768.f;
        const float gain=sourceVolume?original::originalStreamGain(std::uint8_t(volume)):1.f;
        out[c]=short(std::clamp(value*.38f*gain*.60f,-1.f,1.f)*32767);
    }return out;
}
void check(EngineAudio& audio,const OriginalAudioClip& clip,double& cursor,unsigned volume,unsigned samples,bool sourceVolume=true){
    for(unsigned i=0;i<samples;++i){const auto actual=audio.renderStereo(800,0,0,0,false),want=expected(clip,cursor,volume,sourceVolume);
        if(actual!=want)throw std::runtime_error("Attract PCM/mixer differs at source frame "+std::to_string(cursor));
        if(cursor<clip.frames())cursor+=double(clip.sampleRate)/44100;++compared;
    }
}
void silent(EngineAudio& audio,unsigned samples=4096){for(unsigned i=0;i<samples;++i){if(audio.renderStereo(800,0,0,0,false)!=std::array<short,2>{})throw std::runtime_error("Unexpected music outside source cue");++compared;}}
}
int main(int argc,char**argv){try{if(argc!=2)throw std::runtime_error("project root required");const std::filesystem::path root=argv[1];
    EngineAudio audio;audio.configure(root);audio.scene(true,false,false);silent(audio);
    const auto logo=loadOriginalSpsd(root/"data/original_audio/streams/logo.bin"),opening=loadOriginalSpsd(root/"data/original_audio/streams/01_gamble_rumble.bin");
    double cursor=0;
    for(unsigned frame=0;frame<390;++frame){audio.scene(true,false,false);audio.attract(4,frame);
        audio.attract(4,frame); // A0dt redraw must not restart a source cue.
        if(frame<31||frame==389)silent(audio,735);else check(audio,logo,cursor,127,735);
    }
    if(audio.attractStatistics().plays!=1)throw std::runtime_error("Rosso cue retriggered");
    for(unsigned child:{3u,5u,6u,8u,11u,12u}){audio.attract(child,0);silent(audio);audio.attract(child,31);silent(audio);}
    audio.attract(7,0);audio.attract(7,0);cursor=0;
    if(audio.attractStatistics().stream!=11||audio.attractStatistics().volume!=125||audio.attractStatistics().volumeChanges!=1)throw std::runtime_error("Demo original track/volume not selected");
    // Full original opening stream, including its one-shot end. No invented
    // loop and no fallback oscillator; microphone/device output is not used.
    check(audio,opening,cursor,125,unsigned(opening.frames()*2+8192));
    audio.attract(7,5831,true);silent(audio);audio.attract(8,0);silent(audio);
    audio.attract(4,31,false,false);silent(audio);audio.attract(4,32,false,true);silent(audio);
    audio.attract(7,0);cursor=0;check(audio,opening,cursor,125,2048);
    audio.scene(true,false,true);const auto paused=audio.attractStatistics().frame;silent(audio);
    if(audio.attractStatistics().frame!=paused)throw std::runtime_error("Paused attract advanced PCM");
    audio.scene(true,false,false);check(audio,opening,cursor,125,2048);
    audio.attract(~0u,0);silent(audio); // Start accepted; enters selection.
    audio.musicTrack=0;audio.scene(false,false,false);cursor=0;
    // Ordinary user-selected race music remains on its existing desktop mix.
    check(audio,opening,cursor,127,4096,false);
    audio.scene(true,false,false);silent(audio);
    // Real conquered stream: load is silent until Play, one-shot PCM uses
    // source volume, the finish-scene update cannot replace it with WIN.
    using C=original::OriginalLegendReturnCommand;
    const auto clear=loadOriginalSpsd(root/"data/original_audio/streams/cr.bin");
    auto command=[&](C op,unsigned a=0){audio.applyLegendStreamCommand({op,a});};
    command(C::SoundSet,1);command(C::StreamVolume,127);command(C::StreamStart,16);silent(audio);
    if(audio.legendStreamStatistics().frame!=0)throw std::runtime_error("Prepared conquered stream advanced before Play");
    command(C::StreamVolume,127);command(C::StreamPlay);command(C::StreamPlay);cursor=0;
    for(unsigned frame=0;frame<150;++frame){audio.scene(false,true,false);audio.tickLegendStream();check(audio,clear,cursor,127,735);}
    if(audio.legendStreamStatistics().starts!=1)throw std::runtime_error("Conquered stream restarted");
    command(C::StreamFade,8);
    for(unsigned frame=0;frame<152;++frame){
        audio.tickLegendStream();const unsigned volume=127-(frame?((frame-1)/9+1):0);
        if(audio.legendStreamStatistics().volume!=volume)throw std::runtime_error("Conquered fade cadence differs");
        check(audio,clear,cursor,volume,735);
        if(frame==25){audio.scene(false,true,true);const auto paused=audio.legendStreamStatistics();audio.tickLegendStream();silent(audio);
            if(audio.legendStreamStatistics().frame!=paused.frame||audio.legendStreamStatistics().volume!=paused.volume)throw std::runtime_error("Pause advanced conquered stream/fade");
            audio.scene(false,true,false);}
    }
    command(C::StreamStop);silent(audio);
    if(audio.legendStreamStatistics().stream!=-1||audio.legendStreamStatistics().stops!=1)throw std::runtime_error("Conquered cleanup failed");
    command(C::StreamStart,16);command(C::StreamPlay);cursor=0;check(audio,clear,cursor,127,unsigned(clear.frames()*2+2048));
    if(audio.legendStreamStatistics().frame<clear.frames())throw std::runtime_error("Conquered stream looped");
    audio.beginOriginalMusicCue(3);audio.tickResultMusic();
    if(audio.legendStreamStatistics().playing||audio.legendStreamStatistics().stream!=-1)throw std::runtime_error("Next rival retained conquered stream");
    std::cout<<"Attract mixer: "<<compared<<" stereo frames match original decoded waveforms/source silence; Rosso timing, full opening/no loop, Start exit, duplicate updates, pause, race music and conquered stream/fade/handoff preserved\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
