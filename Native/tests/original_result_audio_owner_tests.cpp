#include "audio.h"
#include <iostream>
#include <stdexcept>
using namespace idas3;using namespace idas3::original;
namespace {
std::uint64_t checks=0,nonzero=0;
void require(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
void compare(EngineAudio& actual,EngineAudio& reference,unsigned frames){
    for(unsigned i=0;i<frames;++i){const auto pcm=actual.renderStereo(800,0,0,0,false);require(pcm==reference.renderStereo(800,0,0,0,false),"Result owner changed expected source PCM");nonzero+=pcm[0]!=0||pcm[1]!=0;}
    require(actual.selectionSamplePosition()==reference.selectionSamplePosition(),"Result music sample clock differs");
    require(actual.dspRuntime()->diagnostics().frames==reference.dspRuntime()->diagnostics().frames,"Result DSP clock differs");
}
void apply(EngineAudio& audio,const OriginalSelectionMusicCommands& commands){for(const auto& command:commands)audio.selection(command);}
void skipped(const std::filesystem::path& root){
    EngineAudio audio,reference;audio.configure(root);reference.configure(root);
    audio.scene(false,false,false);reference.scene(false,false,false);compare(audio,reference,1024);
    const auto dspBefore=audio.dspRuntime()->diagnostics();
    audio.playRaceCue(4,1);reference.playRaceCue(4,1);
    audio.beginResultMusic(false);
    for(unsigned frame=0;frame<8;++frame){audio.tickResultMusic();audio.scene(false,true,false,frame%2!=0);compare(audio,reference,735);}
    require(audio.selectionStatistics().songsStarted==0,"Skipped result started a score");
    require(audio.dspRuntime()->diagnostics().loadCount==dspBefore.loadCount,"Skipped result changed DSP program");
    require(audio.dspRuntime()->registeredBank(5)!=nullptr,"Skipped result replaced race sound set");
    audio.scene(false,true,true);reference.scene(false,false,true);const auto pause=audio.dspRuntime()->diagnostics().frames;compare(audio,reference,128);
    require(audio.dspRuntime()->diagnostics().frames==pause,"Paused result advanced DSP");
    audio.scene(false,true,false);reference.scene(false,false,false);compare(audio,reference,735);
    audio.endResultMusic();audio.scene(false,false,false);compare(audio,reference,735);
}
void requested(const std::filesystem::path& root){
    EngineAudio audio,reference;audio.configure(root);reference.configure(root);
    audio.scene(false,false,false);reference.scene(false,false,false);compare(audio,reference,1024);
    // A still-running finish prompt belongs to a race bank unloaded by the
    // common result sound-set call, so it cannot bleed into this new owner.
    audio.playRaceCue(4,1);
    OriginalSelectionMusicState manager;
    audio.beginResultMusic();apply(reference,requestOriginalSelectionMusic(manager,OriginalSelectionMusicCue::Result));
    require(!audio.selectionPlaying(),"Result Init invented a manager tick");
    require(audio.dspRuntime()->registeredBank(2)&&audio.dspRuntime()->registeredBank(2)->bankId==1,"Result loaded wrong bank identity");
    require(!audio.dspRuntime()->registeredBank(5),"Requested result kept race bank registration");
    audio.scene(false,true,false);compare(audio,reference,1);
    for(unsigned frame=0;frame<20;++frame){
        audio.beginResultMusic();audio.tickResultMusic();apply(reference,tickOriginalSelectionMusic(manager));
        audio.scene(false,true,false,frame%2!=0);compare(audio,reference,735);
    }
    require(audio.selectionStatistics().songsStarted==1&&audio.selectionStatistics().cue==2,"Result restarted or selected another score");
    require(audio.selectionStatistics().sourceLevel==108&&audio.selectionStatistics().volumeChanges==6,"Result lost source level commands");
    require(audio.dspRuntime()->diagnostics().activeBank==1&&audio.dspRuntime()->diagnostics().loadCount==1,"Result lost sample-zero DSP selection");
    const auto position=audio.selectionSamplePosition();audio.scene(false,true,true);reference.scene(true,false,true);compare(audio,reference,128);
    require(audio.selectionSamplePosition()==position,"Pause advanced result music");
    audio.scene(false,true,false);reference.scene(true,false,false);compare(audio,reference,735);
    audio.playTuningCue(12);reference.playTuningCue(12);
    audio.endResultMusic();apply(reference,changeOriginalSelectionMusicScene(manager,0));
    require(!audio.selectionPlaying()&&!audio.dspRuntime()->registeredBank(2),"Result cleanup retained score or bank");
    audio.scene(true,false,false);reference.scene(true,false,false);compare(audio,reference,4096);
    audio.beginResultMusic();apply(reference,requestOriginalSelectionMusic(manager,OriginalSelectionMusicCue::Result));
    audio.tickResultMusic();apply(reference,tickOriginalSelectionMusic(manager));compare(audio,reference,735);
    require(audio.selectionStatistics().songsStarted==2,"Fresh result visit did not restart");
    audio.attract(3,0);reference.attract(3,0);compare(audio,reference,128);
    require(!audio.selectionPlaying()&&!audio.dspRuntime()->registeredBank(2),"Attract retained result score/bank");
}
void bunta(const std::filesystem::path& root){
    for(unsigned cue:{33u,34u,35u}){
        EngineAudio audio,reference;audio.configure(root);reference.configure(root);
        audio.scene(false,false,false);reference.scene(false,false,false);compare(audio,reference,1024);
        // Source sound-set table31DEB4 registers PACK21, like set1, but is
        // a distinct owner. Compare its real PCM with the existing independent
        // manager command path, including all three authored Bunta cues.
        OriginalSelectionMusicState manager;
        audio.beginOriginalMusicCue(cue,true,2);
        apply(reference,requestOriginalSelectionMusic(manager,static_cast<OriginalSelectionMusicCue>(cue)));
        require(audio.dspRuntime()->registeredBank(1)&&audio.dspRuntime()->registeredBank(2),"Bunta omitted static or score bank");
        require(!audio.dspRuntime()->registeredBank(5),"Bunta retained race sound bank");
        require(!audio.selectionPlaying(),"Bunta Init invented manager tick");
        for(unsigned frame=0;frame<32;++frame){
            audio.beginOriginalMusicCue(cue,true,2);audio.tickResultMusic();apply(reference,tickOriginalSelectionMusic(manager));
            audio.scene(false,true,false);compare(audio,reference,735);
        }
        require(audio.selectionStatistics().songsStarted==1&&audio.originalMusicCue()==cue,"Bunta repeat restarted or selected wrong cue");
        require(audio.selectionStatistics().sourceLevel==originalMusicCueDescriptor(cue).sourceLevel,"Bunta changed authored source level");
        audio.playMenuCue(OriginalMenuCue::Confirm);reference.playMenuCue(OriginalMenuCue::Confirm);compare(audio,reference,256);
        require(audio.oneShotStatistics().activeVoices>0,"Bunta PACK21 manager rejected one-shot cue");
        const auto position=audio.selectionSamplePosition();audio.scene(false,true,true);reference.scene(true,false,true);compare(audio,reference,128);
        require(audio.selectionSamplePosition()==position,"Paused Bunta advanced score clock");
        audio.scene(false,true,false);reference.scene(true,false,false);compare(audio,reference,735);
        audio.fadeOriginalMusicCue();apply(reference,exitOriginalSelectionMusic(manager));compare(audio,reference,735);
        audio.playTuningCue(12);reference.playTuningCue(12);compare(audio,reference,64);
        require(audio.oneShotStatistics().activeVoices>0,"Bunta PACK21 tuning cue not active for handoff check");
        // If the owned score Load secretly downgrades set2 to set1, this
        // external menu Load fails to unload PACK21's previous voice tails.
        OriginalSelectionMusicState menu;
        apply(audio,requestOriginalSelectionMusic(menu,OriginalSelectionMusicCue::Select));
        require(audio.oneShotStatistics().activeVoices==0,"Bunta-to-menu transition retained previous sound-set voices");
        audio.playMenuCue(OriginalMenuCue::Confirm);
        for(unsigned frame=0;frame<256;++frame)audio.renderStereo(800,0,0,0,false);
        require(audio.oneShotStatistics().activeVoices>0,"Menu lost PACK21 after Bunta return");
        audio.attract(3,0);
        require(!audio.selectionPlaying()&&!audio.dspRuntime()->registeredBank(2)&&audio.oneShotStatistics().activeVoices==0,"Attract retained Bunta/menu score or voices");
    }
}
}
int main(int argc,char**argv)try{
    if(argc!=2)throw std::runtime_error("Expected project root");skipped(argv[1]);requested(argv[1]);bunta(argv[1]);
    require(nonzero>10000,"Audio fixture never reached audible content");
    std::cout<<"Original result audio owner: "<<checks<<" checks, "<<nonzero<<" nonzero PCM frames; skipped source gate, RESULT level108, six volume commands, pause, exit and restart; Bunta33/34/35 source PCM, level, set2 PACK21 cues, fade and menu/attract handoff; no output device.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
