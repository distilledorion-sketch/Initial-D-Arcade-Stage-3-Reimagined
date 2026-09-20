#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>
#include <string>
#include <functional>
#include <utility>
#include <memory>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <cmath>
#include <algorithm>
#include "original_ics_player.h"
#include "original_music_bank.h"
#include "original_music_control.h"
#include "original_selection_music.h"
#include "original_audio_dsp_runtime.h"
#define private public
#include "original_music_playback.h"
#undef private
using namespace idas3;
int main(int argc,char**argv)try{
    if(argc!=3)throw std::runtime_error("game-root output-directory required");
    std::filesystem::create_directories(argv[2]);
    std::ofstream out(std::filesystem::path(argv[2])/"channel-final-mix.csv");
    out<<"cue,channel,frames,dry_rms,wet_rms,mix_rms,mono_rms,side_rms,dry_wet_correlation,peak\n";
    for(unsigned cue=0;cue<2;++cue)for(int ch=-1;ch<=10;++ch){
        OriginalMusicPlayback music(argv[1]);
        auto& sequence=music.sequences_[cue];
        if(ch>=0)std::erase_if(sequence.events_,[&](const auto& e){return e.kind!=OriginalMusicSequenceEventKind::Command&&e.channel!=unsigned(ch);});
        if(ch>=0&&std::none_of(sequence.events_.begin(),sequence.events_.end(),[](const auto& e){return e.kind==OriginalMusicSequenceEventKind::NoteOn;}))continue;
        OriginalAudioDspRuntime dsp(argv[1],[&]{music.clearDspSends();},true);
        dsp.registerBank(0,"PACK20");dsp.registerBank(1,"PACK21");dsp.registerBank(2,cue?"SELECT":"TYPE");
        music.setSourceCommandOutput([&](unsigned,std::uint32_t word){if((word&0xffff0000u)==0xa0190000u)dsp.selectScene(2,(word>>8)&127);});
        music.apply({original::OriginalSelectionMusicOperation::Start,cue});
        music.apply({original::OriginalSelectionMusicOperation::Control,cue,0x4a0,cue?103u:109u});
        const auto frames=std::min(44100ull*30,sequence.loopEndTick()*44ull);
        double dry=0,wet=0,mix=0,mono=0,side=0,cross=0;int peak=0;
        for(std::uint64_t frame=0;frame<frames;++frame){
            const auto d=music.renderFrame();const auto w=dsp.render(d.effects).wet;
            std::array<double,2> mixed{};
            for(unsigned c=0;c<2;++c){mixed[c]=d.dry[c]+w[c];dry+=double(d.dry[c])*d.dry[c];wet+=double(w[c])*w[c];mix+=mixed[c]*mixed[c];cross+=double(d.dry[c])*w[c];peak=std::max(peak,std::abs(int(mixed[c])));}
            mono+=std::pow((mixed[0]+mixed[1])*.5,2);side+=std::pow((mixed[0]-mixed[1])*.5,2);
        }
        out<<cue<<','<<ch<<','<<frames<<','<<std::sqrt(dry/(2*frames))<<','<<std::sqrt(wet/(2*frames))<<','<<std::sqrt(mix/(2*frames))<<','<<std::sqrt(mono/frames)<<','<<std::sqrt(side/frames)<<','<<(dry&&wet?cross/std::sqrt(dry*wet):0)<<','<<peak<<'\n';out.flush();
        std::cout<<"cue"<<cue<<" channel"<<ch<<" mixRMS="<<std::sqrt(mix/(2*frames))<<'\n';
    }
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
