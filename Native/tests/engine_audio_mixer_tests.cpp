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
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("project root required");
    const auto reference=capture(argv[1],1);
    for(unsigned batch:{2u,3u,4u,6u,10u})require(capture(argv[1],batch)==reference,"Engine waveform depends on render batching or synthesizer inputs");
    std::cout<<"PASS actual game mixer: exact PCM across1,2,3,4,6,10 simulation frames per render batch; ignores development synth inputs; pause and restart silent; no dropped/underflow frames. Offline mixer coverage does not establish waveOut scheduling under GPU stalls.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
