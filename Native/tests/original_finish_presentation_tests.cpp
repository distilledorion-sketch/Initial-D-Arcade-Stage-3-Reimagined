#include "audio.h"
#include <cmath>
#include <iostream>
#include <vector>
using namespace idas3;
namespace fs=std::filesystem;
unsigned checks=0;
void require(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
short mix(std::int32_t pcm){return short(std::clamp((pcm/32768.f)*.60f,-1.f,1.f)*32767);}
int main(int argc,char**argv)try{
    if(argc<2)throw std::invalid_argument("Native assets required");
    const fs::path root=fs::absolute(argv[1]);
    const std::string mode=argc>2?argv[2]:"both";
    auto profile=original::makeOriginalFreshBattleProfile();
    if(mode!="tail"){
        EngineAudio audio;audio.configure(root);audio.selectOriginalEngine(root,profile);
        audio.playRaceCue(2,7);
        require(audio.backfireFrame()==0,"Final racing cue did not start frame0");
        unsigned seed=123;
        audio.stopOriginalDriving();
        const auto controls=audio.engineStatistics().simulationFrames;
        require(audio.backfireFrame()==0,"Finish gate prematurely cleared existing flame");
        for(unsigned tick=0;tick<4;++tick){
            audio.stepOriginalEngine({},seed);audio.finishSoundFrame(seed);
            require(audio.backfireFrame()==(tick==0?1:-1),"Final-tick flame did not age across stopped engine ticks");
            require(seed==123&&audio.engineStatistics().simulationFrames==controls,"Stopped presentation consumed gameplay RNG/control");
        }
        audio.selectOriginalEngine(root,profile);
        require(audio.backfireFrame()==-1&&!audio.originalDrivingStopped(),"Race restart retained stop/presentation state");
    }
    if(mode!="flash")for(unsigned rendered:{1600u,3000u,5880u}){
        EngineAudio audio;audio.selectOriginalEngine(root,profile);
        OriginalEnginePlayback reference(root);reference.select(profile);
        original::OriginalEngineControlInput control;control.rpm=6500;control.throttle=1;control.gear=3;
        unsigned seed=19,referenceSeed=19;
        std::vector<OriginalIcsPlayer> checkpoints;
        std::vector<OriginalIcsMixFrame> pcm(1024);
        for(unsigned tick=0;tick<8;++tick){
            audio.stepOriginalEngine(control,seed);audio.finishSoundFrame(seed);
            reference.step(control,referenceSeed,{});reference.finishSoundFrame();
            checkpoints.push_back(reference.snapshotVoices());
            for(unsigned i=0;i<735;++i)pcm.push_back(reference.renderFrame());
        }
        require(seed==referenceSeed,"PCM reference input stream mismatch");
        for(unsigned i=0;i<rendered;++i){
            const auto out=audio.renderStereo(0,0,0,0,true);
            require(out==std::array<short,2>{mix(pcm[i].dry[0]),mix(pcm[i].dry[1])},"Pre-stop output does not match reference sample cursor");
        }
        const auto at=rendered-1024;
        auto released=checkpoints.at(at/735);
        for(unsigned i=0;i<at%735;++i)released.renderFrame();
        for(unsigned channel=0;channel<4;++channel)released.stop(channel);
        require(released.activeVoices()>0,"Reference has no audible release voices");
        audio.stopOriginalDriving();const auto stoppedSeed=seed;
        const auto frames=audio.engineStatistics().simulationFrames;
        audio.stepOriginalEngine(control,seed);audio.finishSoundFrame(seed);
        require(seed==stoppedSeed&&audio.engineStatistics().simulationFrames==frames,"Release tail restarted controls/RNG");
        // Pause holds the output envelope and repeat stop is idempotent.
        audio.scene(false,false,true);
        for(unsigned i=0;i<20;++i)require(audio.renderStereo(0,0,0,0,true)==std::array<short,2>{},"Paused release not silent");
        audio.scene(false,false,false);audio.stopOriginalDriving();
        unsigned nonzero=0;
        for(unsigned i=0;i<1000;++i){
            const auto expected=released.renderFrame();
            const auto out=audio.renderStereo(0,0,0,0,true);
            require(out==std::array<short,2>{mix(expected.dry[0]),mix(expected.dry[1])},"Released engine PCM differs from audible-boundary reference");
            nonzero+=out[0]!=0||out[1]!=0;
        }
        require(nonzero>0&&released.activeVoices()==0,"Release tail did not sound then terminate");
        require(audio.engineStatistics().simulationFrames==frames&&seed==stoppedSeed,"Output rendering changed driving control/RNG");
        audio.selectOriginalEngine(root,profile);
        for(unsigned i=0;i<100;++i)require(audio.renderStereo(0,0,0,0,true)==std::array<short,2>{},"Restart retained released PCM");
    }
    std::cout<<"PASS "<<checks<<" final-tick flash, frozen control/RNG, audible-cursor key-off PCM, pause, repeat stop and restart checks. No output device opened.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
