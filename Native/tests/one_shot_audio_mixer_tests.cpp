#include "audio.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace idas3;
using namespace idas3::original;
namespace {
std::uint64_t comparisons=0,nonzero=0,wetFrames=0;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
// Independent connection of source components: EngineAudio is the system
// under test. This checks routing and lifecycle, not just decoded sample bytes.
struct Reference {
    OriginalMusicPlayback music;
    OriginalOneShotPlayback cues;
    OriginalAudioDspRuntime dsp;
    int soundSet=0;
    explicit Reference(const std::filesystem::path& root):music(root),cues(root),
        dsp(root,[this]{music.clearDspSends();cues.clearDspSends();}){
        dsp.registerBank(0,"PACK20");
        music.setSourceCommandOutput([this](unsigned,std::uint32_t word){
            if((word&0xffff0000u)==0xa0190000u)dsp.selectScene(2,(word>>8)&127);
        });
    }
    void scene(int next){
        if(next==soundSet)return;
        if(soundSet==1)cues.stopBank(21);
        if(soundSet==4)for(unsigned bank:{22u,24u,25u})cues.stopBank(bank);
        for(unsigned slot=1;slot<8;++slot)dsp.unregisterBank(slot);
        if(next==1)dsp.registerBank(1,"PACK21");
        if(next==4){constexpr std::array<const char*,5> banks{"PACK22","PACK23","PACK23","PACK24","PACK25"};
            for(unsigned i=0;i<5;++i)dsp.registerBank(i+1,banks[i]);}
        soundSet=next;
    }
    void apply(const OriginalSelectionMusicCommand& command){
        using Op=OriginalSelectionMusicOperation;
        if(command.operation==Op::Load){scene(1);const auto& descriptor=originalSelectionMusicDescriptor(static_cast<OriginalSelectionMusicCue>(command.cue));
            dsp.registerBank(2,std::filesystem::path(descriptor.filename).stem().string());}
        if(command.operation==Op::Unload)dsp.unregisterBank(2);
        music.apply(command);
    }
    std::array<short,2> render(bool enabled=true){
        const auto song=music.renderFrame(),effect=cues.renderFrame();auto sends=song.effects;
        for(unsigned i=0;i<16;++i)sends[i]+=effect.effects[i];
        const auto wet=dsp.render(sends).wet;if(wet[0]||wet[1])++wetFrames;
        std::array<short,2> result{};
        for(unsigned c=0;c<2;++c)result[c]=short(std::clamp(enabled?((song.dry[c]/32768.f)*.38f+
            (wet[c]/32768.f)*.38f+(effect.dry[c]/32768.f)*.45f)*.60f:0.f,-1.f,1.f)*32767);
        return result;
    }
};
void compare(EngineAudio& audio,Reference& reference,unsigned frames,bool enabled=true){
    for(unsigned i=0;i<frames;++i){const auto expected=reference.render(enabled),actual=audio.renderStereo(800,0,0,0,false);
        require(actual==expected,"One-shot audio routing/mix differs from source components");
        ++comparisons;if(actual[0]||actual[1])++nonzero;
    }
}
void feed(EngineAudio& audio,Reference& reference,const OriginalSelectionMusicCommands& commands){
    for(const auto& command:commands){audio.selection(command);reference.apply(command);}
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::runtime_error("Expected game root");
    EngineAudio audio;audio.configure(argv[1]);audio.scene(true,false,false);Reference reference(argv[1]);
    // Attract has no PACK21/race scene manager: input cannot manufacture a cue.
    audio.playMenuCue(OriginalMenuCue::Confirm);audio.playTuningCue(12);audio.playRaceCue(4,1);
    compare(audio,reference,2048);require(!nonzero,"Absent source manager emitted audio");
    OriginalSelectionMusicState manager;
    feed(audio,reference,requestOriginalSelectionMusic(manager,OriginalSelectionMusicCue::Type));
    for(unsigned tick=0;tick<6;++tick){feed(audio,reference,tickOriginalSelectionMusic(manager));compare(audio,reference,735);}
    for(const auto cue:{OriginalMenuCue::Change,OriginalMenuCue::Confirm,OriginalMenuCue::Back,
            OriginalMenuCue::ResultCount,OriginalMenuCue::UpgradeNotice}){
        audio.playMenuCue(cue);reference.cues.play(21,static_cast<std::uint32_t>(cue)>>16);compare(audio,reference,2048);
    }
    for(unsigned cue:{2u,5u,6u,11u,12u,13u}){audio.playTuningCue(cue);reference.cues.play(21,cue);compare(audio,reference,2048);}
    for(unsigned repeat=0;repeat<12;++repeat){audio.playMenuCue(OriginalMenuCue::Change);reference.cues.play(21,2);compare(audio,reference,128);}
    // Pause freezes voices and DSP. User mute advances both clocks.
    audio.scene(true,false,true);const auto dspFrame=audio.dspRuntime()->diagnostics().frames;
    for(unsigned i=0;i<4096;++i)require(audio.renderStereo(800,0,0,0,false)==std::array<short,2>{},"Paused cues were audible");
    require(audio.dspRuntime()->diagnostics().frames==dspFrame,"Pause advanced cue effect tails");
    audio.scene(true,false,false);compare(audio,reference,2048);
    audio.enabled=false;compare(audio,reference,4096,false);audio.enabled=true;compare(audio,reference,2048);
    // A new song in the same menu set must retain PACK21 voice tails.
    audio.playTuningCue(12);reference.cues.play(21,12);
    feed(audio,reference,requestOriginalSelectionMusic(manager,OriginalSelectionMusicCue::Select));
    for(unsigned tick=0;tick<6;++tick){feed(audio,reference,tickOriginalSelectionMusic(manager));compare(audio,reference,735);}
    compare(audio,reference,8192);
    // Entering a silent attract owner unloads the menu bank. Its existing DSP
    // tail survives; no new cue may escape the missing source manager.
    audio.attract(3,0);reference.scene(0);reference.music.stop();
    audio.playMenuCue(OriginalMenuCue::Confirm);compare(audio,reference,8192);
    require(nonzero>10000&&wetFrames>1000,"Fixture failed to exercise audible music/cues and effects");
    // Held race stream gives an isolated production race-cue path, including
    // its real DSP routing. No engine is selected and active=false is silent.
    EngineAudio race;race.configure(argv[1]);race.scene(false,false,false,false,true);
    Reference raceReference(argv[1]);raceReference.scene(4);
    race.playRaceCue(2,7);raceReference.cues.play(22,7,2.f);
    race.playRaceCue(4,2);raceReference.cues.play(24,2);
    unsigned peak=0,clipped=0;
    for(unsigned i=0;i<44100;++i){const auto actual=race.renderStereo(800,0,0,0,false),expected=raceReference.render();
        require(actual==expected,"Evo III presentation gain was not routed through the race mixer");
        for(auto sample:actual){peak=std::max(peak,unsigned(std::abs(int(sample))));clipped+=std::abs(int(sample))==32767;}
    }
    require(peak>100&&!clipped,"Misfire mix was silent or clipped in the concurrent-cue fixture");
    race.resetRaceEffects();race.setOutputGains({1,1,1,0,1});race.playRaceCue(2,7);
    for(unsigned i=0;i<44100;++i)require(race.renderStereo(800,0,0,0,false)==std::array<short,2>{},"Effects mute did not silence misfire");
    std::cout<<"Misfire mix: +6.02 dB, concurrent-cue peak "<<peak<<"/32767, no clipped fixture samples, Effects mute respected.\n";
    std::cout<<"One-shot application mixer: "<<comparisons<<" exact stereo frames, "<<nonzero
        <<" nonzero frames, "<<wetFrames<<" effect frames; menu/tuning cues, repeated cues, pause, mute, same-set song change and attract cleanup. No output device.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
