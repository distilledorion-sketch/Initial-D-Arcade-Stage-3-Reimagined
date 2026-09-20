#include "original_music_voice.h"
#include <iostream>
using namespace idas3;
namespace {
void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
}
int main()try{
    const std::array<std::int16_t,5> sample{0,12000,-12000,24000,32123};
    OriginalMusicPcm pcm{sample,1,4,true};
    OriginalMusicVoiceParameters p;p.totalLevel=1;p.envelope2=31|(15<<10);
    OriginalMusicVoice voice;voice.start(pcm,p);
    // Literal sample output at unit pitch, exclusiveLEA and maximum direct
    // level. The32123 sentinel must never leak into the loop interpolation.
    constexpr std::array<int,10> unit{0,11490,-11491,22981,11490,-11491,22981,11490,-11491,22981};
    for(int expected:unit){const auto f=voice.renderFrame();require(f.dry[0]==expected&&f.dry[1]==expected,"Music voice sample/loop endpoint");}
    p.pitch=0x7800;voice.start(pcm,p);
    constexpr std::array<int,12> half{0,5745,11490,0,-11491,5745,22981,17236,11490,0,-11491,5745};
    for(int expected:half){const auto f=voice.renderFrame();require(f.dry[0]==expected&&f.dry[1]==expected,"Music voice half-rate phase");}
    const auto position=voice.position(),fraction=voice.fraction();p.totalLevel=255;voice.configure(p);
    require(voice.position()==position&&voice.fraction()==fraction,"Controller update restarted the note");
    require(voice.renderFrame().dry==std::array<std::int32_t,2>{},"Total level mute");
    p.totalLevel=1;p.pan=31;voice.configure(p);auto left=voice.renderFrame();require(left.dry[1]==0&&left.dry[0]!=0,"Left pan direction");
    p.pan=15;voice.configure(p);auto right=voice.renderFrame();require(right.dry[0]==0&&right.dry[1]!=0,"Right pan direction");
    voice.release();unsigned released=0;while(voice.active()&&released<200){voice.renderFrame();++released;}
    require(released==159,"Source RR31,KRS15 release did not last159 sample frames");
    // KRS15 suppresses pitch-dependent rate scaling. AR0 never attacks; a
    // release still terminates it. Loop-start linking advances toDecay1.
    p={};p.envelope1=0;p.envelope2=31|(15<<10);voice.start(pcm,p);
    for(unsigned i=0;i<1000;++i)voice.renderFrame();require(voice.phase()==0&&voice.envelope()==640u<<16,"AR0 did not hold its start level");
    p.envelope1=31;p.envelope2|=0x4000;voice.start(pcm,p);voice.renderFrame();require(voice.phase()==1,"Loop-start-linked attack did not enter decay");
    // A non-looping note ends atLEA rather than reusing an instrument forever.
    pcm.looping=false;p.envelope2=31|(15<<10);voice.start(pcm,p);
    for(unsigned i=0;i<4;++i)voice.renderFrame();require(!voice.active(),"One-shot never ended");
    require(voice.renderFrame().dry==std::array<std::int32_t,2>{},"Ended voice retained output");
    bool rejected=false;try{voice.start({sample,4,4,true},p);}catch(const std::invalid_argument&){rejected=true;}require(rejected,"Empty sample loop accepted");
    // New notes may overlap an older note's release. Releasing one identity
    // must neither cut its tail nor stop a later note using the same sample.
    OriginalMusicVoicePool pool;pcm.looping=true;
    pool.start(100,pcm,p);pool.renderFrame();pool.release(100);pool.start(101,pcm,p);
    require(pool.activeVoices()==2&&pool.peakVoices()==2,"Music note-off cut its release tail");
    for(unsigned i=0;i<159;++i)pool.renderFrame();require(pool.activeVoices()==1,"Released music note killed a later note");
    pool.release(100);require(pool.activeVoices()==1,"Stale note-off affected a reused slot");
    pool.releaseAll();for(unsigned i=0;i<159;++i)pool.renderFrame();require(pool.activeVoices()==0,"Stop did not release held music notes");
    pool.reset();require(pool.startedVoices()==0&&pool.peakVoices()==0,"Song reset retained allocation history");
    // Loading an original DSP preset clears chip sends, while later volume
    // writes retain the driver's cached send value. They must not restore it.
    p={};p.effectSend=0xf5;p.envelope2=31|(15<<10);
    OriginalMusicVoicePool dryReference;pool.start(200,pcm,p);dryReference.start(200,pcm,p);
    pool.renderFrame();dryReference.renderFrame();pool.clearDspSends();
    for(unsigned i=0;i<50;++i){
        p.totalLevel=std::uint8_t(i%4);pool.configure(200,p);dryReference.configure(200,p);
        const auto cleared=pool.renderFrame(),original=dryReference.renderFrame();
        require(cleared.dry==original.dry,"DSP clear changed dry waveform or phase");
        require(cleared.effects==std::array<std::int32_t,16>{},"Volume write restored cleared DSP send");
    }
    pool.configure(200,p,true);const auto restored=pool.renderFrame();
    require(restored.effects[5]!=0,"Explicit DSP write did not restore send");
    pool.clearDspSends();pool.start(201,pcm,p);pool.renderFrame();
    require(pool.renderFrame().effects[5]!=0,"New note inherited cleared DSP send");
    std::cout<<"PASS original music voice PCM, loop endpoints, controller phase, pan, envelopes, lifetime and source DSP send clear/restore\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
