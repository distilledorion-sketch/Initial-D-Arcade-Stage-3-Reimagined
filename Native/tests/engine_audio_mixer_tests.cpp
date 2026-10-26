#include "audio.h"
#include <iostream>
#include <vector>
using namespace idas3;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::vector<short> capture(const std::filesystem::path& root,unsigned batch){
    EngineAudio audio;auto profile=original::makeOriginalFreshBattleProfile();profile.setu(16,0);audio.selectOriginalEngine(root,profile);
    std::uint32_t seed=1;std::vector<short> result;
    for(unsigned frame=0;frame<600;){
        const unsigned count=std::min(batch,600-frame);
        for(unsigned i=0;i<count;++i,++frame){
            original::OriginalEngineControlInput input;input.rpm=1000.f+float(frame%200)*32.f;input.throttle=frame%200<150?1.f:0.f;input.gear=1+int(frame/200);
            audio.stepOriginalEngine(input,seed);audio.finishSoundFrame(seed);
        }
        for(unsigned i=0;i<count*735;++i){
            // Synthesizer inputs deliberately differ across render cadences.
            // Original handling must use the solver-bound PCM, not these args.
            const auto stereo=audio.renderStereo(float(batch)*1111.f,float(batch)*.1f,float(batch)*10.f,float(batch),true);
            result.insert(result.end(),stereo.begin(),stereo.end());
        }
    }
    const auto stats=audio.engineStatistics();require(stats.pcmFrames==600*735&&stats.simulationFrames==600,"PCM clock differs from60Hz driving");
    require(!stats.droppedFrames&&!stats.underflowFrames,"Bounded frame cadence caused PCM loss");require(stats.dryEnergy>1e9,"Original engine silent");
    audio.scene(false,false,true);for(unsigned i=0;i<1000;++i)require(audio.renderStereo(9000,1,30,1,true)==std::array<short,2>{},"Paused mixer not silent");
    require(audio.engineStatistics().pcmFrames==stats.pcmFrames,"Pause advanced engine PCM");
    audio.selectOriginalEngine(root,profile);require(audio.engineStatistics().pcmFrames==0,"Restart retained prior PCM counters");
    for(unsigned i=0;i<1000;++i)require(audio.renderStereo(9000,1,30,1,true)==std::array<short,2>{},"Restart retained stale audio");
    return result;
}
std::vector<short> onlineCapture(const std::filesystem::path& root,bool delayed){
    EngineAudio audio;auto profile=original::makeOriginalFreshBattleProfile();
    audio.selectOriginalEngine(root,profile);audio.setOutputGains({1,0,1,0,0});
    original::OriginalEngineControlInput input;input.rpm=6500;input.throttle=1;input.gear=3;
    std::uint32_t seed=1;audio.stepOriginalEngine(input,seed);audio.finishSoundFrame(seed);
    audio.applyConfirmedOnlineAudio({},{});
    std::vector<short> out;
    for(unsigned frame=0;frame<180;++frame){
        // Half-second gaps followed by a burst used to alternate silence and
        // queue overflow. Identical controls must give identical continuous PCM.
        if(!delayed||frame%30==0)for(unsigned n=0;n<(delayed?30u:1u);++n)audio.applyConfirmedOnlineAudio({},{});
        double energy=0;
        for(unsigned i=0;i<735;++i){const auto pcm=audio.renderStereo(0,0,0,0,true);for(auto s:pcm){out.push_back(s);energy+=double(s)*s;}}
        require(energy>1e5,"Delayed opponent silenced the online engine");
    }
    const auto stats=audio.engineStatistics();
    require(!stats.droppedFrames&&!stats.underflowFrames,"Network confirmations overflowed or starved PCM");
    audio.scene(false,false,true);
    for(unsigned i=0;i<1000;++i)require(audio.renderStereo(0,0,0,0,true)==std::array<short,2>{},"Online pause not silent");
    require(audio.engineStatistics().pcmFrames==stats.pcmFrames,"Online pause advanced PCM");
    audio.selectOriginalEngine(root,profile);audio.scene(false,false,false);
    for(unsigned i=0;i<1000;++i)require(audio.renderStereo(0,0,0,0,true)==std::array<short,2>{},"Restart retained online output mode");
    return out;
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("project root required");
    const auto reference=capture(argv[1],1);
    for(unsigned batch:{2u,3u,4u,6u,10u})require(capture(argv[1],batch)==reference,"Engine waveform depends on render batching or synthesizer inputs");
    require(onlineCapture(argv[1],false)==onlineCapture(argv[1],true),"Network arrival timing changed steady engine PCM");
    const auto held=[&](float rpm,float throttle){
        EngineAudio audio;audio.selectOriginalEngine(argv[1],original::makeOriginalFreshBattleProfile());
        audio.scene(false,false,false,false,true);audio.setShowcaseEngine({rpm,throttle,0,true});
        std::vector<short> pcm;double energy=0;
        for(unsigned i=0;i<44100;++i)for(auto s:audio.renderStereo(0,0,0,0,true)){pcm.push_back(s);energy+=double(s)*s;}
        require(energy>1e6,"Showcase engine is silent");
        require(audio.engineStatistics().simulationFrames==0,"Showcase audio advanced driving controls");
        return pcm;
    };
    require(held(800,0)!=held(6500,1),"Showcase rev controls still produce fixed idle sound");
    // Render PCM only; no output device. BGM OFF suppresses the race song,
    // while retaining its clock, the finish jingle and the user's mute state.
    EngineAudio cardAudio;cardAudio.configure(argv[1]);cardAudio.musicTrack=1;
    auto cardProfile=original::makeOriginalFreshBattleProfile();cardProfile.setByte(1184,1);
    cardAudio.selectOriginalEngine(argv[1],cardProfile);cardAudio.setOutputGains({1,1,0,0,0});
    cardAudio.scene(false,false,false);
    for(unsigned i=0;i<44100;++i)require(cardAudio.renderStereo(0,0,0,0,false)==std::array<short,2>{},"BGM OFF leaked race music");
    require(cardAudio.raceTimingStatistics().streamFrame>1000,"BGM OFF stopped the music clock");
    cardAudio.scene(false,true,false);double finishEnergy=0;
    for(unsigned i=0;i<44100;++i)for(auto s:cardAudio.renderStereo(0,0,0,0,false))finishEnergy+=double(s)*s;
    require(finishEnergy>1e5,"BGM OFF silenced the finish jingle");
    cardAudio.enabled=false;cardProfile.setByte(1184,0);cardAudio.selectOriginalEngine(argv[1],cardProfile);
    require(!cardAudio.enabled&&!cardAudio.cardRaceMusicOff,"Card BGM overwrote the user's mute preference");
    std::cout<<"PASS online engine sustains through 500ms confirmation gaps and 30-frame bursts, exact steady-state PCM, pause and restart.\n";
    std::cout<<"PASS actual game mixer: exact PCM across1,2,3,4,6,10 simulation frames per render batch; ignores development synth inputs; pause and restart silent; no dropped/underflow frames. Offline mixer coverage does not establish waveOut scheduling under GPU stalls.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
