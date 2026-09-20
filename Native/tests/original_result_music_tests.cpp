#include "original_music_playback.h"
#include "original_audio_dsp_runtime.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace idas3;using namespace idas3::original;
int main(int argc,char**argv)try{
    if(argc!=2)throw std::runtime_error("Expected project root");const std::filesystem::path root=argv[1];
    const auto descriptor=originalSelectionMusicDescriptor(OriginalSelectionMusicCue::Result);
    if(descriptor.filename!="RESULT.bin"||descriptor.command!=0x1a8||descriptor.slotMask!=0x100||descriptor.sourceLevel!=108)throw std::runtime_error("Wrong original RESULT descriptor");
    OriginalMusicPlayback player(root);OriginalAudioDspRuntime dsp(root,[&]{player.clearDspSends();});
    dsp.registerBank(0,"PACK20");dsp.registerBank(1,"PACK21");dsp.registerBank(2,"RESULT");
    if(dsp.diagnostics().activeProgram||dsp.diagnostics().ringCode!=2)throw std::runtime_error("Result registration incorrectly selected a program");
    player.setSourceCommandOutput([&](unsigned cue,std::uint32_t command){if(cue!=2)throw std::runtime_error("Result cue rebound");if((command&0xffff0000u)==0xa0190000u)dsp.selectScene(2,(command>>8)&127);});
    OriginalSelectionMusicState manager;const auto feed=[&](const OriginalSelectionMusicCommands&commands){for(const auto& command:commands)player.apply(command);};
    feed(requestOriginalSelectionMusic(manager,OriginalSelectionMusicCue::Result));
    OriginalMusicSequence source;source.load(root,2);source.start();
    if(source.loopStartTick()!=38263||source.loopEndTick()!=62604)throw std::runtime_error("Source RESULT loop markers changed");
    const auto frames=std::uint64_t(source.loopEndTick()*2-source.loopStartTick())*44+1;
    std::uint64_t expectedNotes=0,nonzero=0,stereo=0,wetFrames=0,clips=0,peak=0,hash=1469598103934665603ull;
    for(std::uint64_t frame=0;frame<frames;++frame){
        if(frame%735==0){feed(tickOriginalSelectionMusic(manager));const auto same=requestOriginalSelectionMusic(manager,OriginalSelectionMusicCue::Result);if(same.count)throw std::runtime_error("Same result cue restarted");}
        source.advanceSample([&](const auto&e){expectedNotes+=e.kind==OriginalMusicSequenceEventKind::NoteOn;});
        const auto pcm=player.renderFrame();const auto wet=dsp.render(pcm.effects).wet;
        if(frame==0&&(dsp.diagnostics().activeBank!=1||dsp.diagnostics().activePreset!=0||dsp.diagnostics().loadCount!=1))throw std::runtime_error("RESULT A019 did not select its actual intrinsic bank1");
        nonzero+=pcm.dry[0]!=0||pcm.dry[1]!=0;stereo+=pcm.dry[0]!=pcm.dry[1];wetFrames+=wet[0]!=0||wet[1]!=0;
        for(unsigned c=0;c<2;++c){const auto value=std::int64_t(pcm.dry[c])+wet[c];peak=std::max(peak,std::uint64_t(std::abs(value)));clips+=std::abs(double(value)*.38*.60)>32767;hash^=std::uint32_t(value);hash*=1099511628211ull;}
    }
    const auto stats=player.statistics();
    if(!player.playing()||stats.cue!=2||stats.sourceLevel!=108||stats.songsStarted!=1||stats.notesStarted!=expectedNotes||stats.peakVoices>=64||!stats.legatoRetunes||!stats.parameterChanges||nonzero<frames/2||stereo<frames/4||!wetFrames||clips||dsp.diagnostics().loadCount!=1)
        throw std::runtime_error("Incomplete RESULT song, source control or DSP playback");
    std::cout<<"RESULT complete source intro+2cycles: "<<frames<<"frames "<<double(frames)/44100<<"seconds; notes="<<stats.notesStarted<<" controllers="<<stats.parameterChanges<<" retunes="<<stats.legatoRetunes<<" peakVoices="<<stats.peakVoices<<" rawCombinedPeak="<<peak<<" desktopPeak="<<peak*.38*.60<<" clips="<<clips<<" wetFrames="<<wetFrames<<" hash="<<hash<<'\n';
    player.apply({OriginalSelectionMusicOperation::Control,2,0x1200a0});if(player.playing()||player.activeVoices())throw std::runtime_error("RESULT force stop left voices");
    for(unsigned frame=0;frame<128;++frame){const auto pcm=player.renderFrame();if(pcm.dry!=std::array<std::int32_t,2>{}||pcm.effects!=std::array<std::int32_t,16>{})throw std::runtime_error("RESULT stop leaked source samples");}
    std::cout<<"Original RESULT music passed without an output device.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
