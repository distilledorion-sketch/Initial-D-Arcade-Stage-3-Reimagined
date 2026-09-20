// Bounded complete-song audio audit. No window, original runtime or audio device.
#include "audio.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace idas3;
using namespace idas3::original;
int main(int argc,char**argv){try{
    if(argc!=3)throw std::runtime_error("Expected project root and evidence directory");
    const std::filesystem::path root=argv[1],out=argv[2];std::filesystem::create_directories(out);
    std::ofstream log(out/"full-songs.log");
    const auto began=std::chrono::steady_clock::now();
    for(unsigned cue=0;cue<2;++cue){
        EngineAudio audio;audio.configure(root);audio.scene(true,false,false);
        OriginalMusicPlayback player(root);OriginalAudioDspRuntime dsp(root,[&]{player.clearDspSends();});
        dsp.registerBank(0,"PACK20");dsp.registerBank(1,"PACK21");dsp.registerBank(2,cue?"SELECT":"TYPE");
        player.setSourceCommandOutput([&](unsigned,std::uint32_t command){if((command&0xffff0000u)==0xa0190000u)dsp.selectScene(2,(command>>8)&127);});
        OriginalMusicSequence score;score.load(root,cue);
        const auto length=std::uint64_t(score.loopEndTick()+score.loopEndTick()-score.loopStartTick())*44+1;
        OriginalSelectionMusicState manager;
        const auto feed=[&](const OriginalSelectionMusicCommands&commands){for(const auto&command:commands){audio.selection(command);player.apply(command);}};
        feed(requestOriginalSelectionMusic(manager,OriginalSelectionMusicCue(cue)));
        std::vector<double> blockTimes;
        std::uint64_t dryPeak=0,wetPeak=0,rawCombinedPeak=0,inputPeak=0,desktopClips=0,pcmLimitSamples=0,hash=1469598103934665603ull;
        std::int64_t firstInput=-1,firstWet=-1;double preclipPeak=0,totalBlockMs=0;std::uint64_t frame=0;
        while(frame<length){
            feed(tickOriginalSelectionMusic(manager));
            std::array<std::array<short,2>,735> actual{};const unsigned n=unsigned(std::min<std::uint64_t>(735,length-frame));
            const auto start=std::chrono::steady_clock::now();
            for(unsigned i=0;i<n;++i)actual[i]=audio.renderStereo(800,0,0,0,false);
            const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            totalBlockMs+=elapsed;if(n==735)blockTimes.push_back(elapsed);
            for(unsigned i=0;i<n;++i){
                const auto source=player.renderFrame();
                for(const auto value:source.effects){const auto amplitude=std::uint64_t(std::abs(std::int64_t(value)));inputPeak=std::max(inputPeak,amplitude);if(value&&firstInput<0)firstInput=std::int64_t(frame+i);}
                const auto wet=dsp.render(source.effects).wet;
                for(unsigned c=0;c<2;++c){
                    dryPeak=std::max(dryPeak,std::uint64_t(std::abs(std::int64_t(source.dry[c]))));wetPeak=std::max(wetPeak,std::uint64_t(std::abs(std::int64_t(wet[c]))));
                    if(wet[c]&&firstWet<0)firstWet=std::int64_t(frame+i);
                    rawCombinedPeak=std::max(rawCombinedPeak,std::uint64_t(std::abs(std::int64_t(source.dry[c])+wet[c])));
                    const float value=((source.dry[c]/32768.f)*.38f+(wet[c]/32768.f)*.38f)*.60f;
                    preclipPeak=std::max(preclipPeak,std::abs(double(value)*32767));desktopClips+=std::abs(value)>1;
                    const auto expected=short(std::clamp(value,-1.f,1.f)*32767);
                    if(actual[i][c]!=expected)throw std::runtime_error("Complete-song actual mixer mismatch at"+std::to_string(frame+i));
                    pcmLimitSamples+=std::abs(int(actual[i][c]))==32767;hash^=std::uint16_t(actual[i][c]);hash*=1099511628211ull;
                }
            }
            frame+=n;
        }
        const auto d=audio.dspRuntime()->diagnostics();
        if(!audio.selectionPlaying()||d.activeBank!=cue+1||d.activePreset!=0||d.loadCount!=1||firstInput<0||firstWet<0||desktopClips||pcmLimitSamples||inputPeak>=524288)
            throw std::runtime_error("Complete-song DSP state, range, signal or clipping check failed");
        if(audio.selectionStatistics().sourceLevel!=(cue?103u:109u)||audio.selectionStatistics().peakVoices>=64||d.frames!=length||d.inputPeak!=inputPeak)
            throw std::runtime_error("Complete-song source level, pool or clock mismatch");
        std::sort(blockTimes.begin(),blockTimes.end());
        const auto print=[&](std::ostream&s){s<<(cue?"SELECT":"TYPE")<<": frames="<<length<<" seconds="<<double(length)/44100
            <<" activeProgram="<<d.activeBank<<"/"<<d.activePreset<<" loads="<<d.loadCount<<" firstInputSample="<<firstInput<<" firstWetSample="<<firstWet
            <<" sourceLevel="<<audio.selectionStatistics().sourceLevel<<" peakVoices="<<audio.selectionStatistics().peakVoices
            <<" dryPeak="<<dryPeak<<" rawWetPeak="<<wetPeak<<" rawCombinedPeak="<<rawCombinedPeak<<" dspInputPeak="<<inputPeak
            <<" desktopPreclipPeak="<<preclipPeak<<" desktopClips="<<desktopClips<<" pcmLimitSamples="<<pcmLimitSamples<<" wetEnergy="<<d.wetEnergy
            <<" mean735ms="<<totalBlockMs/double(length)*735<<" median735ms="<<blockTimes[blockTimes.size()/2]<<" p95735ms="<<blockTimes[blockTimes.size()*95/100]
            <<" cpuSeconds="<<totalBlockMs/1000<<" outputHash="<<hash<<'\n';};
        print(log);log.flush();print(std::cout);std::cout.flush();
    }
    const auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count();
    log<<"PASS: actual EngineAudio and independent source player/shared DSP matched every stereo frame. No output device. Audit wallSeconds="<<seconds<<'\n';
    std::cout<<"Full-song shared DSP audit passed in"<<seconds<<" seconds\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
