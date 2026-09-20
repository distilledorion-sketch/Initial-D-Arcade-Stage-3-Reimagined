#include "audio.h"
#include "original_stream_gain.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace idas3;
using namespace idas3::original;
namespace {
std::uint64_t comparisons=0,nonzero=0,wetFrames=0,tailFrames=0;
void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
// This fixture independently connects the released source player/processor.
// EngineAudio itself is the system under test; no waveOut device is opened.
struct Reference : OriginalMusicPlayback {
    OriginalAudioDspRuntime dsp;
    int soundSet=0;
    explicit Reference(const std::filesystem::path&root):OriginalMusicPlayback(root),dsp(root,[this]{clearDspSends();}){
        dsp.registerBank(0,"PACK20");
        setSourceCommandOutput([this](unsigned,std::uint32_t word){if((word&0xffff0000u)==0xa0190000u)dsp.selectScene(2,(word>>8)&127);});
    }
    void setSoundSet(int next){
        if(soundSet==next)return;for(unsigned i=1;i<8;++i)dsp.unregisterBank(i);
        if(next==1)dsp.registerBank(1,"PACK21");
        if(next==4){constexpr std::array<const char*,5> banks{"PACK22","PACK23","PACK23","PACK24","PACK25"};for(unsigned i=0;i<5;++i)dsp.registerBank(i+1,banks[i]);}
        soundSet=next;
    }
    void apply(const OriginalSelectionMusicCommand&command){
        if(command.operation==OriginalSelectionMusicOperation::Load){setSoundSet(1);dsp.registerBank(2,std::filesystem::path(originalMusicCueDescriptor(command.cue).filename).stem().string());}
        if(command.operation==OriginalSelectionMusicOperation::Unload)dsp.unregisterBank(2);
        OriginalMusicPlayback::apply(command);
    }
    std::array<short,2> render(bool enabled=true,bool menu=true,std::array<float,2> stream={}){
        OriginalIcsMixFrame pcm{};if(menu)pcm=renderFrame();
        const auto wet=dsp.render(pcm.effects).wet;
        if(wet[0]||wet[1]){++wetFrames;if(!playing())++tailFrames;}
        std::array<short,2> out{};
        for(unsigned c=0;c<2;++c)out[c]=short(std::clamp(enabled?(stream[c]+(pcm.dry[c]/32768.f)*.38f+(wet[c]/32768.f)*.38f)*.60f:0.f,-1.f,1.f)*32767);
        return out;
    }
};
void compare(EngineAudio& audio,Reference& reference,unsigned frames,bool enabled=true){
    for(unsigned i=0;i<frames;++i){const auto expected=reference.render(enabled);
        const auto actual=audio.renderStereo(800,0,0,0,false);
        if(actual!=expected)throw std::runtime_error("Selection mixer PCM differs at frame "+std::to_string(comparisons));
        ++comparisons;if(actual[0]||actual[1])++nonzero;
    }
    require(audio.selectionSamplePosition()==reference.samplePosition(),"Selection clock differs from player");
    require(audio.selectionPlaying()==reference.playing(),"Selection play state differs from player");
    require(audio.dspRuntime()->diagnostics().frames==reference.dsp.diagnostics().frames,"Shared DSP clock differs");
    require(audio.dspRuntime()->diagnostics().loadCount==reference.dsp.diagnostics().loadCount,"Shared DSP source reload count differs");
}
void feed(EngineAudio& audio,Reference& reference,const OriginalSelectionMusicCommands& commands){
    for(const auto& command:commands){audio.selection(command);reference.apply(command);}
}
void silence(EngineAudio& audio,unsigned n){for(unsigned i=0;i<n;++i){require(audio.renderStereo(800,0,0,0,false)==std::array<short,2>{},"Unexpected residual selection/stream PCM");++comparisons;}}
void play(EngineAudio& audio,Reference& reference,OriginalSelectionMusicState& manager,OriginalSelectionMusicCue cue){
    const auto request=requestOriginalSelectionMusic(manager,cue);require(request.cueChanged,"Fixture expected new cue");
    require(request.count&&request.commands[request.count-1].operation==OriginalSelectionMusicOperation::Load,"Source load missing");
    const auto dspBefore=audio.dspRuntime()->diagnostics();
    feed(audio,reference,request);
    require(audio.dspRuntime()->diagnostics().loadCount==dspBefore.loadCount,"Bank registration prematurely selected a DSP preset");
    require(audio.dspRuntime()->registeredBank(2)&&audio.dspRuntime()->registeredBank(2)->bankId==unsigned(cue)+1,"Music bank did not occupy actual source resource2");
    const auto startedBefore=audio.selectionStatistics().songsStarted;const auto audibleBefore=nonzero;
    // Ordinary owner update precedes the sound manager; these are exactly the
    // source pending-start + six immediate/post-start4A0 commands.
    for(unsigned frame=0;frame<6;++frame){const auto commands=tickOriginalSelectionMusic(manager);feed(audio,reference,commands);
        bool starts=false;for(const auto&command:commands)starts|=command.operation==OriginalSelectionMusicOperation::Start;
        if(starts){
            require(audio.dspRuntime()->diagnostics().loadCount==dspBefore.loadCount,"Manager Start prematurely selected DSP before source score command");
            compare(audio,reference,1);const auto selected=audio.dspRuntime()->diagnostics();
            require(selected.activeBank==unsigned(cue)+1&&selected.activePreset==0&&selected.loadCount==dspBefore.loadCount+1,"Authored A019 sample0 did not select the actual TYPE/SELECT preset");
            compare(audio,reference,734);
        }else compare(audio,reference,735);
    }
    require(audio.selectionStatistics().songsStarted==startedBefore+1,"Source start duplicated");
    require(audio.selectionStatistics().cue==int(cue),"TYPE/SELECT cue binding reversed");
    require(audio.selectionStatistics().sourceLevel==originalSelectionMusicDescriptor(cue).sourceLevel,"Source level not applied");
    compare(audio,reference,44100);require(nonzero>audibleBefore+100,"Selected source bank produced no audible notes");
}
std::array<float,2> streamSample(const OriginalAudioClip& clip,double frame,unsigned volume=127,bool sourceVolume=false){
    std::array<float,2> out{};const auto at=std::size_t(frame);if(at>=clip.frames())return out;const float fraction=float(frame-at);
    for(unsigned c=0;c<2;++c){const auto a=at*clip.channels+std::min(c,clip.channels-1),b=std::min(at+1,clip.frames()-1)*clip.channels+std::min(c,clip.channels-1);
        const float value=(clip.samples[a]+(clip.samples[b]-clip.samples[a])*fraction)/32768.f;
        out[c]=value*.38f*(sourceVolume?originalStreamGain(std::uint8_t(volume)):1.f);
    }return out;
}
void stream(EngineAudio& audio,Reference&reference,const OriginalAudioClip& clip,unsigned volume,unsigned count,bool menu=false){
    double frame=0;for(unsigned i=0;i<count;++i){const auto expected=reference.render(true,menu,streamSample(clip,frame,volume,menu));require(audio.renderStereo(800,0,0,0,false)==expected,"Race/attract stream or retained DSP tail differs");frame+=double(clip.sampleRate)/44100;++comparisons;}
}
void finishStreams(const std::filesystem::path& root){
    using Outcome=EngineAudio::FinishOutcome;
    EngineAudio audio;audio.musicTrack=1;audio.configure(root);Reference reference(root);
    audio.scene(false,false,false);reference.setSoundSet(4);
    const auto race=loadOriginalSpsd(root/"data/original_audio/streams"/raceMusicCatalog[1].relativePath);
    const auto loss=loadOriginalSpsd(root/"data/original_audio/streams/LOSE.bin");
    const auto win=loadOriginalSpsd(root/"data/original_audio/streams/WIN.bin");
    const auto timeup=loadOriginalSpsd(root/"data/original_audio/streams/TIMEUP.bin");
    require(!loss.looping&&loss.frames()>4096&&loss.frames()<std::size_t(loss.sampleRate)*120,"LOSE fixture must be a bounded non-looping source clip");
    stream(audio,reference,race,127,4096);
    const auto finishScene=[&](Outcome outcome,bool timeUp=false){audio.scene(false,true,false,timeUp,false,false,outcome);};
    const auto emptyStream=[&](unsigned frames){
        for(unsigned i=0;i<frames;++i){
            const auto expected=reference.render(true,false);
            require(expected==std::array<short,2>{},"Neutral fixture unexpectedly has a DSP tail");
            require(audio.renderStereo(800,0,0,0,false)==expected,"Pending/draw/end emitted stream PCM");++comparisons;
        }
    };
    finishScene(Outcome::Loss);
    require(audio.raceTimingStatistics().scene==5&&!audio.raceMusicFinished(),"Loss did not select its own loaded finish scene");
    double lossFrame=0;std::uint64_t lossAudible=0;
    const auto lossReset=audio.outputResetSerial();
    // Compare the complete one-shot, including interpolated output and its last
    // sample. Every 60 Hz owner update must preserve the current stream clock.
    while(lossFrame<double(loss.frames())){
        const auto before=audio.raceTimingStatistics().streamFrame;finishScene(Outcome::Loss);
        require(audio.raceTimingStatistics().streamFrame==before&&audio.outputResetSerial()==lossReset,"Repeated loss owner frame restarted or reloaded LOSE");
        for(unsigned i=0;i<735&&lossFrame<double(loss.frames());++i){
            const auto expected=reference.render(true,false,streamSample(loss,lossFrame));
            const auto actual=audio.renderStereo(800,0,0,0,false);
            require(actual==expected,"Loss mixer PCM differs from original LOSE source");
            if(actual[0]||actual[1])++lossAudible;
            lossFrame+=double(loss.sampleRate)/44100;++comparisons;
        }
        require(audio.raceTimingStatistics().streamFrame==lossFrame,"Loss stream clock differs from source one-shot");
    }
    require(lossAudible>100&&audio.raceMusicFinished(),"LOSE did not finish audibly as a one-shot");
    const auto ended=audio.raceTimingStatistics().streamFrame;
    for(unsigned i=0;i<6;++i){finishScene(Outcome::Loss);emptyStream(735);}
    require(audio.raceMusicFinished()&&audio.raceTimingStatistics().streamFrame==ended&&audio.outputResetSerial()==lossReset,"Completed LOSE restarted on repeated finish frames");

    // Pending/draw must cut both a live race song and a preceding WIN stream.
    // Include the time-up flag so unresolved/no-contest results cannot fall
    // through to either the victory clip or TIMEUP.
    for(const auto outcome:{Outcome::Pending,Outcome::Draw})for(const bool timeUp:{false,true}){
        audio.scene(false,false,false);stream(audio,reference,race,127,2048);
        require(audio.raceTimingStatistics().scene==1&&audio.raceTimingStatistics().track==1,"Returning to race lost the selected track");
        finishScene(outcome,timeUp);
        const auto neutralReset=audio.outputResetSerial();
        require(audio.raceTimingStatistics().scene==6&&audio.raceMusicFinished()&&audio.raceTimingStatistics().streamFrame==0,"Neutral finish retained a race/WIN/TIMEUP stream");
        for(unsigned frame=0;frame<6;++frame){finishScene(outcome,timeUp);emptyStream(735);}
        require(audio.outputResetSerial()==neutralReset,"Repeated neutral finish frame reloaded audio");
        finishScene(Outcome::Win);stream(audio,reference,win,127,2048);
        finishScene(outcome,timeUp);emptyStream(4096);
        require(audio.raceTimingStatistics().scene==6&&audio.raceMusicFinished(),"Neutral finish failed to clear previous WIN");
    }
    finishScene(Outcome::Loss,true);stream(audio,reference,timeup,127,4096);
    require(audio.raceTimingStatistics().scene==3,"Resolved time-up result did not keep original TIMEUP");
    const auto timeoutFrame=audio.raceTimingStatistics().streamFrame;const auto timeoutReset=audio.outputResetSerial();
    finishScene(Outcome::Win,true);
    require(audio.raceTimingStatistics().streamFrame==timeoutFrame&&audio.outputResetSerial()==timeoutReset,"Resolved outcome change restarted the same TIMEUP stream");
    audio.scene(false,false,false);stream(audio,reference,race,127,8192);
    require(audio.musicTrack==1&&audio.raceTimingStatistics().track==1,"Finish outcome replaced the selected race music");

    // The source common-result sequencer owns its music independently from
    // these finish flags. Drive the original manager as the PCM reference.
    OriginalSelectionMusicState resultManager;
    audio.beginResultMusic();
    for(const auto& command:requestOriginalSelectionMusic(resultManager,OriginalSelectionMusicCue::Result))reference.apply(command);
    const auto resultStarts=audio.selectionStatistics().songsStarted;
    constexpr std::array<Outcome,4> outcomes{Outcome::Win,Outcome::Loss,Outcome::Pending,Outcome::Draw};
    for(unsigned frame=0;frame<24;++frame){
        audio.beginResultMusic();audio.tickResultMusic();
        for(const auto& command:tickOriginalSelectionMusic(resultManager))reference.apply(command);
        const auto before=audio.selectionSamplePosition();const auto reset=audio.outputResetSerial();
        finishScene(outcomes[frame%outcomes.size()],frame%2!=0);
        require(audio.raceTimingStatistics().scene==0&&audio.selectionSamplePosition()==before&&audio.outputResetSerial()==reset,"Finish flags interrupted sequenced result ownership");
        compare(audio,reference,735);
    }
    require(audio.selectionPlaying()&&audio.selectionStatistics().songsStarted==resultStarts+1&&audio.selectionStatistics().cue==2,"Sequenced result cue stopped or restarted under finish flags");
    audio.endResultMusic();for(const auto& command:changeOriginalSelectionMusicScene(resultManager,0))reference.apply(command);
    audio.scene(false,false,false);reference.setSoundSet(4);stream(audio,reference,race,127,4096);
    // The original skipped-sound branch also keeps its preceding finish clip.
    finishScene(Outcome::Loss);audio.beginResultMusic(false);
    double retainedFrame=0;
    for(const auto outcome:outcomes){
        const auto reset=audio.outputResetSerial();finishScene(outcome,true);
        require(audio.raceTimingStatistics().scene==5&&audio.outputResetSerial()==reset,"Skipped result sound block lost preceding LOSE ownership");
        for(unsigned i=0;i<735;++i){
            const auto expected=reference.render(true,false,streamSample(loss,retainedFrame));
            require(audio.renderStereo(800,0,0,0,false)==expected,"Skipped result owner replaced retained LOSE PCM");
            retainedFrame+=double(loss.sampleRate)/44100;++comparisons;
        }
    }
    audio.endResultMusic();
    std::cout<<"Finish mixer outcomes: full original LOSE PCM, repeated frames, one-shot completion, pending/draw silence, TIMEUP precedence, selected-track return and sequenced/skipped result ownership pass.\n";
}
}
int main(int argc,char**argv){try{
    if(argc!=2)throw std::runtime_error("project root required");const std::filesystem::path root=argv[1];
    EngineAudio audio;audio.configure(root);audio.scene(true,false,false);Reference reference(root);OriginalSelectionMusicState manager;
    require(audio.dspRuntime()->diagnostics().ringCode==2&&!audio.dspRuntime()->diagnostics().activeProgram,"Boot registration must latch PACK20 ring without choosing a program");
    // Never call open(): renderStereo is the actualwaveOut mixer with no device.
    compare(audio,reference,128);
    play(audio,reference,manager,OriginalSelectionMusicCue::Type);
    const auto initialStarts=audio.selectionStatistics().songsStarted;const auto beforeSame=audio.selectionSamplePosition();
    for(unsigned frame=0;frame<20;++frame){const auto unchanged=requestOriginalSelectionMusic(manager,OriginalSelectionMusicCue::Type);require(!unchanged.count&&!unchanged.cueChanged,"Same cue should not issue playback commands");feed(audio,reference,unchanged);audio.scene(true,false,false);feed(audio,reference,tickOriginalSelectionMusic(manager));compare(audio,reference,735);}
    require(audio.selectionStatistics().songsStarted==initialStarts&&audio.selectionSamplePosition()==beforeSame+20*735,"Same-cue owner frames restarted music");
    const auto beforePause=audio.selectionSamplePosition();const auto renderBefore=audio.selectionStatistics().samples;const auto dspBefore=audio.dspRuntime()->diagnostics().frames;
    audio.scene(true,false,true);silence(audio,4096);audio.scene(true,false,true);silence(audio,333);
    require(audio.selectionSamplePosition()==beforePause&&audio.selectionStatistics().samples==renderBefore,"Pause advanced score or voice phase");
    require(audio.dspRuntime()->diagnostics().frames==dspBefore,"Pause advanced shared DSP tails");
    audio.scene(true,false,false);compare(audio,reference,4096); // exact continuation
    audio.enabled=false;const auto beforeMute=audio.selectionSamplePosition();compare(audio,reference,8192,false);
    require(audio.selectionSamplePosition()==beforeMute+8192,"User mute froze music phase");audio.enabled=true;compare(audio,reference,8192);
    const auto cueBefore=audio.selectionStatistics().cue;const auto levelBefore=audio.selectionStatistics().sourceLevel;
    for(unsigned i=0;i<3;++i){audio.nextMusic();compare(audio,reference,735);}
    require(audio.selectionStatistics().cue==cueBefore&&audio.selectionStatistics().sourceLevel==levelBefore&&audio.selectionStatistics().songsStarted==initialStarts,"Race song picker replaced/restarted menu score");
    feed(audio,reference,exitOriginalSelectionMusic(manager));require(audio.selectionStatistics().fades==1,"Source exit control lost");
    compare(audio,reference,45000);require(!audio.selectionPlaying()&&audio.selectionStatistics().fadeLevel==0,"Original exit fade failed to stop score");
    compare(audio,reference,4096);feed(audio,reference,stopOriginalSelectionMusic(manager));compare(audio,reference,128);
    play(audio,reference,manager,OriginalSelectionMusicCue::Select);
    require(nonzero>1000,"Both-bank comparison never reached audible source notes");
    // Attract owns its stream and stops selection before delivering sourceplay.
    const auto opening=loadOriginalSpsd(root/"data/original_audio/streams/01_gamble_rumble.bin");
    const auto tailBefore=tailFrames;const auto liveDsp=audio.dspRuntime()->diagnostics();
    audio.attract(7,0);reference.setSoundSet(0);reference.stop();require(!audio.selectionPlaying(),"Attract did not stop selection");stream(audio,reference,opening,125,8192,true);
    require(tailFrames>tailBefore+100&&audio.dspRuntime()->diagnostics().loadCount==liveDsp.loadCount&&audio.dspRuntime()->diagnostics().activeBank==liveDsp.activeBank,"Attract transition erased the selection effect tail or selected a replacement preset");
    audio.attract(~0u,0);compare(audio,reference,2048);
    // Start accepts the card/make path again, clearing any attract stream.
    feed(audio,reference,changeOriginalSelectionMusicScene(manager,0));reference.stop();
    play(audio,reference,manager,OriginalSelectionMusicCue::Type);
    audio.musicTrack=0;audio.scene(false,false,false);reference.setSoundSet(4);reference.stop();require(!audio.selectionPlaying(),"Race retained selection score");stream(audio,reference,opening,127,8192);
    audio.nextMusic();const auto second=loadOriginalSpsd(root/"data/original_audio/streams/02_speedy_speed_boy.bin");stream(audio,reference,second,127,8192);
    audio.scene(false,true,false);const auto win=loadOriginalSpsd(root/"data/original_audio/streams/WIN.bin");stream(audio,reference,win,127,4096);
    audio.scene(false,true,false,true);const auto timeup=loadOriginalSpsd(root/"data/original_audio/streams/TIMEUP.bin");stream(audio,reference,timeup,127,4096);
    audio.scene(true,false,false);compare(audio,reference,4096);
    audio.attract(7,0);reference.setSoundSet(0);stream(audio,reference,opening,125,2048,true);audio.musicTrack=0;audio.scene(false,false,false);reference.setSoundSet(4);stream(audio,reference,opening,127,8192);
    require(!audio.selectionPlaying(),"Attract-to-race restarted old selection cue");
    require(wetFrames>1000&&tailFrames>1000,"Shared DSP comparison never reached real wet output or retained tails");
    finishStreams(root);
    std::cout<<"Selection actual mixer: "<<comparisons<<" stereo frames exact; "<<nonzero<<" nonzero source frames; "<<wetFrames<<" wet frames, "<<tailFrames<<" tail frames. TYPE/SELECT109/103, source events, samecue, pause/resume including DSP, mute phase, menu song-picker isolation, fade/stop, retained effects, attract and race/finish playlists pass without opening a device.\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
