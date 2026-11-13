#include "audio.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace idas3;
using namespace idas3::original;
namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
void limiterContracts(){
    AudioPeakLimiter limiter;
    for(unsigned i=0;i<44100;++i){
        const std::array<float,2> input{.9f*std::sin(float(i)*.07f),.6f*std::cos(float(i)*.05f)};
        auto output=input;limiter.process(output);
        require(input==output,"Limiter changes an in-range signal at unity gain");
    }
    std::array<float,2> impulse{2.f,-.5f};limiter.process(impulse);
    require(std::abs(impulse[0]-.98f)<.000001f&&std::abs(impulse[1]+.245f)<.000001f,
        "Limiter delays its attack or changes the stereo ratio");
    std::array<float,2> recovery{};
    for(unsigned i=0;i<2205;++i){recovery={.2f,-.1f};limiter.process(recovery);}
    require(recovery[0]>.1623f&&recovery[0]<.1627f,"Limiter release is not 50 ms at 44100 Hz");
    for(unsigned i=0;i<44100;++i){recovery={.2f,-.1f};limiter.process(recovery);}
    require(recovery==std::array<float,2>{.2f,-.1f},"Limiter never recovers exact unity gain");
    for(unsigned i=0;i<44100;++i){
        const float sample=(i&1?-1.f:1.f)*(1.f+float(i%100)*.1f);
        std::array<float,2> output{sample,-sample*.25f};limiter.process(output);
        require(std::abs(output[0])<=.980002f&&std::abs(output[1]*4+output[0])<.000001f,
            "Limiter clips or moves the stereo image during sustained overload");
    }
    limiter.reset();recovery={.3f,.2f};limiter.process(recovery);
    require(recovery==std::array<float,2>{.3f,.2f},"Fresh output retains old gain reduction");
    std::cout<<"PASS limiter: quiet PCM, immediate linked attack, 50 ms release, unity recovery and reset\n";
}
// Independent source components, before host-output protection. Isolated cues
// are comfortably below its threshold, so their existing dry/wet mix must be
// exact; this also checks that unrelated cues were not amplified.
struct CueReference {
    OriginalOneShotPlayback cues;
    OriginalAudioDspRuntime dsp;
    explicit CueReference(const std::filesystem::path& root):cues(root),dsp(root,[this]{cues.clearDspSends();}){
        dsp.registerBank(0,"PACK20");
        constexpr std::array<const char*,5> banks{"PACK22","PACK23","PACK23","PACK24","PACK25"};
        for(unsigned i=0;i<banks.size();++i)dsp.registerBank(i+1,banks[i]);
    }
    std::array<short,2> render(){
        const auto cue=cues.renderFrame();const auto wet=dsp.render(cue.effects).wet;
        std::array<short,2> result{};
        for(unsigned c=0;c<2;++c){
            const float sample=((wet[c]/32768.f)*.38f+(cue.dry[c]/32768.f)*.45f)*.60f;
            require(std::abs(sample)<.98f,"Isolated reference unexpectedly needs output limiting");
            result[c]=short(sample*32767);
        }
        return result;
    }
};
void cueLevels(const std::filesystem::path& root){
    for(unsigned bank:{2u,4u,5u})for(unsigned cue=0;cue<(bank==2?8u:bank==4?6u:3u);++cue){
        EngineAudio audio;audio.configure(root);audio.scene(false,false,false,false,true);
        CueReference reference(root),baseline(root);
        const bool contact=bank==2&&(cue==1||cue==2||cue==4||cue==6);
        const float oldGain=bank==2&&cue==7?2.f:1.f;
        audio.playRaceCue(bank,cue);reference.cues.play(20+bank,cue,contact?2.f:oldGain);
        baseline.cues.play(20+bank,cue,oldGain);
        double energy=0,oldEnergy=0;
        for(unsigned i=0;i<44100;++i){
            const auto actual=audio.renderStereo(0,0,0,0,false),expected=reference.render(),old=baseline.render();
            require(actual==expected,"Race cue gain or source dry/DSP routing differs");
            for(unsigned c=0;c<2;++c){energy+=double(actual[c])*actual[c];oldEnergy+=double(old[c])*old[c];}
        }
        if(contact){
            require(oldEnergy>1e7,"Contact fixture was silent");
            const double gain=10*std::log10(energy/oldEnergy);
            require(gain>6&&gain<6.05,"Contact level differs from intended +6.02 dB");
            std::cout<<"Contact cue "<<cue<<": +"<<gain<<" dB\n";
            audio.resetRaceEffects();audio.setOutputGains({1,1,1,0,1});audio.playRaceCue(bank,cue);
            for(unsigned i=0;i<44100;++i)require(audio.renderStereo(0,0,0,0,false)==std::array<short,2>{},"Effects mute leaked contact audio");
        }
    }
    std::cout<<"PASS 17 race cues: contact-only increase, turbo/other levels unchanged, Effects mute\n";
}
void headroom(const std::filesystem::path& root){
    unsigned song=1;
    for(unsigned car:{0u,3u,19u,24u}){
        EngineAudio audio;audio.configure(root);audio.selectMusicTrack(song);
        auto profile=makeOriginalFreshBattleProfile();profile.setu(16,car);profile.setByte(164,63);
        audio.selectOriginalEngine(root,profile);audio.scene(false,false,false);
        const auto sourceSong=loadOriginalSpsd(root/"data/original_audio/streams"/raceMusicCatalog[song].relativePath);
        require(sourceSong.frames()>sourceSong.sampleRate*10u,"Mix fixture's song is too short");
        unsigned peak=0,clipped=0;double energy=0;std::uint32_t seed=1;
        for(unsigned tick=0;tick<600;++tick){
            OriginalEngineControlInput input;input.rpm=1200.f+float(tick%180)*40.f;
            input.throttle=tick%90<65?1.f:0.f;input.gear=1+int(tick/120);
            audio.stepOriginalEngine(input,seed);
            OriginalAngularResult skid;skid.feedbackSpeed=140;skid.feedbackStrength=.8f;skid.feedbackArgument=(tick/60)%3;
            audio.requestOriginalTire(skid);audio.finishSoundFrame(seed);
            if(tick%17==0)audio.playRaceCue(2,1);
            if(tick%23==0)audio.playRaceCue(2,2);
            if(tick%31==0)audio.playRaceCue(2,4);
            if(tick%55==0)audio.playRaceCue(2,6);
            for(unsigned i=0;i<735;++i)for(auto sample:audio.renderStereo(0,0,0,0,true)){
                const auto magnitude=unsigned(std::abs(int(sample)));
                peak=std::max(peak,magnitude);clipped+=magnitude==32767;energy+=double(sample)*sample;
            }
        }
        const auto stats=audio.engineStatistics();
        require(stats.dryEnergy>1e9&&stats.tireEnergy>1e8,"Mix fixture missed engine/skid audio");
        if(car==19)require(stats.releaseCues>0,"Mix fixture missed turbo release");
        require(!stats.droppedFrames&&!stats.underflowFrames&&std::abs(audio.raceTimingStatistics().streamFrame-sourceSong.sampleRate*10.)<.1,
            "Mix fixture did not sustain engine/music playback");
        require(!clipped&&peak<=32112,"Contact boost clips the complete race mix");
        std::cout<<"Contact full mix: car "<<car<<", song "<<song<<", peak "<<peak<<"/32767, clipped "<<clipped
            <<", RMS "<<std::sqrt(energy/(600.*735.*2.))<<", turbo cues "<<stats.releaseCues<<'\n';song+=3;
    }
}
void lifecycle(const std::filesystem::path& root){
    EngineAudio control,subject;
    for(auto* audio:{&control,&subject}){
        audio->configure(root);audio->scene(false,false,false,false,true);audio->setOutputGains({2,2,2,2,2});
        for(unsigned cue=0;cue<8;++cue)audio->playRaceCue(2,cue);
    }
    unsigned peak=0;
    for(unsigned i=0;i<44100;++i){
        if(i==1000){
            subject.scene(false,false,true,false,true);
            const auto frame=subject.oneShotStatistics().frames;
            for(unsigned j=0;j<4410;++j)require(subject.renderStereo(0,0,0,0,false)==std::array<short,2>{},"Pause output was audible");
            require(subject.oneShotStatistics().frames==frame,"Pause advanced one-shot voices");
            subject.scene(false,false,false,false,true);
        }
        subject.enabled=!(i>=2000&&i<4000);
        const auto a=subject.renderStereo(0,0,0,0,false),b=control.renderStereo(0,0,0,0,false);
        require(a==(subject.enabled?b:std::array<short,2>{}),"Pause/mute changed output envelope after resume");
        for(auto sample:b){peak=std::max(peak,unsigned(std::abs(int(sample))));require(std::abs(int(sample))<=32112,"200% output gain clips");}
    }
    require(peak>=32110,"Lifecycle fixture did not exercise output limiting");
    subject.resetRaceEffects();require(!subject.oneShotStatistics().activeVoices,"Race reset retained contacts");
    std::cout<<"PASS overloaded 200% mix: ceiling, pause, mute/resume and race reset\n";
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::runtime_error("Expected game root");
    limiterContracts();cueLevels(argv[1]);headroom(argv[1]);lifecycle(argv[1]);
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
