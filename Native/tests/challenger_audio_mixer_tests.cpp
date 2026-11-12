#include "audio.h"
#include "original_tuning.h"
#include <iostream>
#include <stdexcept>
using namespace idas3;
using namespace idas3::original;
namespace {
std::uint64_t compared=0,cueFrames=0,wetFrames=0,resumedFrames=0,retainedTailFrames=0;
unsigned maxTailDelta=0;
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
struct CueReference {
    OriginalOneShotPlayback cue;
    OriginalAudioDsp dsp;
    AudioPeakLimiter limiter;
    std::unique_ptr<OriginalIcsPlayer> engine;
    bool measured;
    CueReference(const std::filesystem::path& root,const OriginalAudioDsp& inherited,bool count=true,
        std::unique_ptr<OriginalIcsPlayer> voices={}):cue(root),dsp(inherited),engine(std::move(voices)),measured(count){
        if(engine)for(unsigned channel=0;channel<4;++channel)engine->stop(channel);
        cue.play(20,3);
    }
    std::array<short,2> render(const AudioOutputGains& gains,bool enabled){
        const auto pcm=cue.renderFrame();auto sends=pcm.effects;
        for(auto& send:sends)if(gains.effects!=1.f)send=std::int32_t(float(send)*gains.effects);
        const auto motor=engine?engine->renderFrame():OriginalIcsMixFrame{};
        for(unsigned bus=0;bus<16;++bus)sends[bus]+=gains.engine==1.f?motor.effects[bus]:std::int32_t(float(motor.effects[bus])*gains.engine);
        const auto wet=dsp.render(sends).wet;
        if(measured&&(wet[0]||wet[1]))++wetFrames;
        std::array<float,2> mix{};std::array<short,2> out{};
        for(unsigned c=0;c<2;++c)mix[c]=((motor.dry[c]/32768.f)*gains.engine+(pcm.dry[c]/32768.f)*.45f*gains.effects+(wet[c]/32768.f)*.38f)*.60f*gains.master;
        limiter.process(mix);
        for(unsigned c=0;c<2;++c)out[c]=short(std::clamp(enabled?mix[c]:0.f,-1.f,1.f)*32767);
        if(measured&&(out[0]||out[1]))++cueFrames;
        return out;
    }
};
void setup(EngineAudio& a,const std::filesystem::path& root,unsigned scene){
    a.configure(root);
    if(scene==0){
        a.scene(true,false,false);OriginalSelectionMusicState manager;
        for(const auto& c:requestOriginalSelectionMusic(manager,OriginalSelectionMusicCue::Type))a.selection(c);
        for(unsigned i=0;i<6;++i)for(const auto& c:tickOriginalSelectionMusic(manager))a.selection(c);
        a.playTuningCue(12);
    }else{
        a.selectOriginalEngine(root,makeOriginalFreshBattleProfile());
        auto song=std::make_shared<OriginalAudioClip>();song->sampleRate=44100;song->channels=2;song->looping=true;
        song->samples.resize(88200);
        for(unsigned i=0;i<song->samples.size();++i)song->samples[i]=short(8000*std::sin(float(i)*.027f));
        a.customRaceMusic=song;a.musicTrack=-2;
        a.scene(false,false,false,false,scene==2);
        a.setReplayEngine({6500.f,1.f,3,true});
        a.playRaceCue(4,5);a.playRaceCue(2,1);a.playRaceCue(5,0);
        if(scene==3)a.useDevelopmentEngine();
    }
}
void run(const std::filesystem::path& root,unsigned scene,unsigned duration){
    EngineAudio interrupted,uninterrupted;
    for(auto* a:{&interrupted,&uninterrupted})setup(*a,root,scene);
    OriginalEnginePlayback sourceEngine(root);sourceEngine.select(makeOriginalFreshBattleProfile());
    unsigned sourceSeed=1,sourceSamples=0;
    const auto sourceFrame=[&](){
        if(sourceSamples%735==0){sourceEngine.step({6500.f,1.f,3,sourceSamples==0},sourceSeed,{});sourceEngine.finishSoundFrame();}
        sourceEngine.renderFrame();++sourceSamples;
    };
    const auto voices=[&]()->std::unique_ptr<OriginalIcsPlayer>{
        return scene==1||scene==2?std::make_unique<OriginalIcsPlayer>(sourceEngine.snapshotVoices()):nullptr;
    };
    unsigned nonzeroBackground=0;
    for(unsigned i=0;i<12000;++i){
        const auto expected=uninterrupted.renderStereo(6500,1,45,.4f,scene!=0);
        check(interrupted.renderStereo(6500,1,45,.4f,scene!=0)==expected,"Background setup differs");
        sourceFrame();
        nonzeroBackground+=expected[0]||expected[1];
    }
    check(nonzeroBackground>1000,"Interruption test has no audible background");
    // Original interrupt sound set3 stops/unloads scene voices but neither
    // clears nor reloads the global effect processor. Compare a live-state
    // reference against a reset-state negative control to expose this boundary.
    const auto inherited=interrupted.dspRuntime()->processor();
    OriginalAudioDsp resetState;resetState.configure(inherited.program());
    CueReference resetReference(root,resetState,false,voices());
    const auto stream=interrupted.raceTimingStatistics().streamFrame;
    const auto music=interrupted.selectionSamplePosition();
    const auto dspFrames=interrupted.dspRuntime()->diagnostics().frames;
    const auto cueClock=interrupted.oneShotStatistics().frames;
    const auto serial=interrupted.outputResetSerial();
    interrupted.beginChallenger();CueReference reference(root,inherited,true,voices());
    check(interrupted.challengerActive()&&interrupted.outputResetSerial()==serial+1,"Notice failed to cut queued scene audio");
    for(unsigned i=0;i<duration;++i){
        if(i==333){interrupted.beginChallenger();check(interrupted.outputResetSerial()==serial+1,"Repeated begin restarted the cue");}
        AudioOutputGains gains;
        if(i>=6000&&i<8000)gains.effects=0;
        if(i>=16000&&i<18000){gains.master=.37f;gains.effects=2;gains.music=0;gains.engine=2;gains.tires=2;}
        interrupted.setOutputGains(gains);interrupted.enabled=i<10000||i>=11000;
        const auto expected=reference.render(gains,interrupted.enabled);
        const auto reset=resetReference.render(gains,interrupted.enabled);
        if(expected!=reset)++retainedTailFrames;
        for(unsigned channel=0;channel<2;++channel)maxTailDelta=std::max(maxTailDelta,unsigned(std::abs(int(expected[channel])-int(reset[channel]))));
        const auto actual=interrupted.renderStereo(6500,1,45,.4f,true);
        if(actual!=expected)throw std::runtime_error("Notice differs from source cue with inherited DSP state: scene="+
            std::to_string(scene)+", frame="+std::to_string(i)+", expected="+std::to_string(expected[0])+","+
            std::to_string(expected[1])+", actual="+std::to_string(actual[0])+","+std::to_string(actual[1]));
        ++compared;
    }
    check(interrupted.raceTimingStatistics().streamFrame==stream,"Notice advanced the previous stream");
    check(interrupted.selectionSamplePosition()==music,"Notice advanced the previous score");
    check(interrupted.dspRuntime()->diagnostics().frames==dspFrames,"Notice advanced the suspended scene's DSP");
    check(interrupted.oneShotStatistics().frames==cueClock,"Notice advanced old sound effects");
    interrupted.setOutputGains({});interrupted.enabled=true;interrupted.endChallenger();
    check(!interrupted.challengerActive()&&interrupted.outputResetSerial()==serial+2,"Cancellation failed to clear queued cue audio");
    interrupted.endChallenger();check(interrupted.outputResetSerial()==serial+2,"Repeated end altered audio");
    unsigned resumedNonzero=0;
    for(unsigned i=0;i<6000;++i){
        const auto expected=uninterrupted.renderStereo(6500,1,45,.4f,scene!=0);
        check(interrupted.renderStereo(6500,1,45,.4f,scene!=0)==expected,"Cancellation changed the resumed scene PCM");
        sourceFrame();
        resumedNonzero+=expected[0]||expected[1];++resumedFrames;
    }
    check(resumedNonzero>1000,"Cancellation did not resume audible background");
    // A subsequent opponent must start a fresh cue, even after an early drop.
    interrupted.beginChallenger();CueReference again(root,interrupted.dspRuntime()->processor(),true,voices());
    for(unsigned i=0;i<2048;++i){check(interrupted.renderStereo(0,0,0,0,false)==again.render({},true),"Second notice did not restart its cue with current scene DSP state");++compared;}
    interrupted.endChallenger();
}
#include "challenger_output_checks.inl"
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::runtime_error("Native asset root required");
    for(unsigned scene=0;scene<4;++scene){run(argv[1],scene,182*735);run(argv[1],scene,1245);}
    checkOutputClocks(argv[1]);
    checkReleaseGates(argv[1]);
    check(cueFrames>100000,"Source challenger cue was not audible");
    check(wetFrames>1000,"Source challenger effect return was discarded");
    check(retainedTailFrames>1000,"Live DSP inheritance was not distinguished from resetting it");
    std::cout<<"PASS challenger audio: "<<compared<<" source cue/DSP stereo comparisons; "<<cueFrames
        <<" audible cue frames; "<<wetFrames<<" wet frames; "<<resumedFrames
        <<" exact resumed scene frames; "<<retainedTailFrames<<" frames distinguish inherited reverb from a reset (peak PCM delta "<<maxTailDelta<<"). "
        <<queuedCases<<" queued output-clock cases, "<<onlineCases<<" confirmed-output cases, "<<outputPhaseDiscriminators
        <<" distinguish future synthesis phase; "<<resetAndGateCases<<" reset/pause/silence cases. Menu, race, showcase, development engine; full notice/early drop, repeated begin/end, mute and gains. No output device.\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
