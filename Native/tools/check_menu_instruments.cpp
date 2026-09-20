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
// This standalone diagnostic observes a private copy of the native player;
// no shipping class layout or production playback behavior is modified.
#define private public
#include "original_music_playback.h"
#undef private
using namespace idas3;
struct Stats {std::uint64_t starts=0,active=0,nonzero=0;double energy=0,effects=0;int peak=0;std::set<unsigned> samples;};
int main(int argc,char**argv)try{
    if(argc!=3)throw std::runtime_error("game-root output-directory required");
    std::filesystem::create_directories(argv[2]);
    for(unsigned cue=0;cue<2;++cue){
        OriginalMusicPlayback music(argv[1]);
        music.apply({original::OriginalSelectionMusicOperation::Start,cue});
        music.apply({original::OriginalSelectionMusicOperation::Control,cue,0x4a0,cue?103u:109u});
        std::map<std::pair<unsigned,unsigned>,Stats> totals;
        std::uint64_t lastId=0,unmatched=0,retunes=0;
        const auto count=music.sequences_[cue].loopEndTick()*44ull;
        std::size_t ei=0;
        for(std::uint64_t frame=0;frame<count;++frame){
            const auto& events=music.sequences_[cue].events();
            while(ei<events.size()&&events[ei].tick*44ull==frame){
                const auto& event=events[ei++];
                if(event.kind==OriginalMusicSequenceEventKind::Retune){
                    ++retunes;bool target=false;
                    for(const auto& note:music.notes_)target|=music.pool_.active(note.id)&&note.channel==event.channel&&note.layer==event.layerOffset&&(note.voiceFlags0&0x94)==0x80;
                    if(!target)++unmatched;
                }
            }
            music.renderFrame();
            for(const auto& note:music.notes_)if(note.id>lastId){
                ++totals[{note.channel,note.layer}].starts;
                lastId=std::max(lastId,note.id);
            }
            if(frame%64)continue;
            for(const auto& note:music.notes_){
                const auto it=std::find_if(music.pool_.slots_.begin(),music.pool_.slots_.end(),[&](const auto& slot){return slot.noteId==note.id&&slot.voice.active();});
                if(it==music.pool_.slots_.end())continue;
                auto copy=it->voice;const auto pcm=copy.renderFrame();auto& s=totals[{note.channel,note.layer}];++s.active;
                for(const auto v:pcm.dry){s.energy+=double(v)*v;s.peak=std::max(s.peak,std::abs(v));}
                for(const auto v:pcm.effects)s.effects+=double(v)*v;
                s.nonzero+=pcm.dry[0]!=0||pcm.dry[1]!=0;
            }
        }
        std::ofstream out(std::filesystem::path(argv[2])/(cue?"select.csv":"type.csv"));
        out<<"channel,layer,starts,active_observations,nonzero_observations,dry_rms,effects_rms,peak\n";
        for(const auto& [key,s]:totals)out<<key.first<<','<<key.second<<','<<s.starts<<','<<s.active<<','<<s.nonzero<<','<<std::sqrt(s.energy/std::max(1ull,2*s.active))<<','<<std::sqrt(s.effects/std::max(1ull,s.active))<<','<<s.peak<<'\n';
        std::cout<<(cue?"SELECT":"TYPE")<<" frames="<<count<<" notes="<<music.stats_.notesStarted<<" retunes="<<retunes<<" unmatched="<<unmatched<<" voices="<<music.stats_.peakVoices<<'\n';
    }
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
