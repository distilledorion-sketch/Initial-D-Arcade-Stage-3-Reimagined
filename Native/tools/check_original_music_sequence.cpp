#include "original_music_sequence.h"
#include "original_selection_music.h"
#include <cctype>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace idas3;
int main(int argc,char**argv){
    try{
        if(argc!=2)throw std::runtime_error("Supply the project root");
        const std::filesystem::path root=argv[1];std::uint64_t commands=0,notes=0,patches=0,retunes=0;
        unsigned checked=0;
        // Every cue of the source BGM table, not only the three menu scores.
        // Each is named by the bank the table gives it, so the two names that
        // serve two cues are checked twice against the same fixture.
        for(unsigned cue=0;cue<original::originalMusicCueCount;++cue){
            const auto stem=original::originalMusicCueDescriptor(cue).filename;
            std::string name(stem.substr(0,stem.rfind('.')));
            for(auto& c:name)c=char(std::tolower(static_cast<unsigned char>(c)));
            const auto fixtures=root/"verification"/"original-music-sequence";
            if(!std::filesystem::exists(fixtures/(name+".commands")))continue;
            ++checked;
            OriginalMusicSequence sequence;sequence.load(root,cue);
            std::ifstream input(fixtures/(name+".commands"),std::ios::binary);
            std::vector<std::array<std::uint32_t,2>> source;
            for(std::array<std::uint32_t,2> v{};input.read(reinterpret_cast<char*>(v.data()),8);)source.push_back(v);
            if(source.empty()||!input.eof())throw std::runtime_error("Missing independent ARM command fixture");
            std::ifstream retuneInput(fixtures/(name+".retunes"),std::ios::binary);
            if(!retuneInput.is_open())throw std::runtime_error("Missing source mono-retune fixture");
            std::vector<std::array<std::uint32_t,7>> sourceRetunes;
            for(std::array<std::uint32_t,7> v{};retuneInput.read(reinterpret_cast<char*>(v.data()),28);)sourceRetunes.push_back(v);
            if(!retuneInput.eof())throw std::runtime_error("Truncated source mono-retune fixture");
            const unsigned period=sequence.loopEndTick()-sequence.loopStartTick();
            std::vector<std::pair<std::uint64_t,unsigned>> expected;
            for(auto v:source)expected.emplace_back(std::uint64_t(v[0])*44,v[1]);
            for(unsigned repeat=1;repeat<=2;++repeat)for(auto v:source)if(v[0]>=sequence.loopStartTick())
                expected.emplace_back((std::uint64_t(v[0])+std::uint64_t(period)*repeat)*44,v[1]);
            auto expectedRetunes=sourceRetunes;
            for(unsigned repeat=1;repeat<=2;++repeat)for(auto v:sourceRetunes)if(v[0]>=sequence.loopStartTick()){
                v[0]+=period*repeat;expectedRetunes.push_back(v);
            }
            std::size_t index=0,retuneIndex=0;sequence.start();
            const std::uint64_t end=(std::uint64_t(sequence.loopEndTick())+2ull*period)*44;
            for(std::uint64_t sample=0;sample<end;++sample){
                sequence.advanceSample([&](const OriginalMusicSequenceEvent& e){
                    if(e.kind==OriginalMusicSequenceEventKind::Command){
                        if(index>=expected.size()||expected[index]!=std::pair<std::uint64_t,unsigned>{sample,e.command})
                            throw std::runtime_error("Native scheduler differs from ARM command sample time");
                        ++index;++commands;
                    }else if(e.kind==OriginalMusicSequenceEventKind::NoteOn){
                        if((e.voiceFlags0&0xf0)!=0x80||e.voiceFlags0&1)throw std::runtime_error("Unexpected source note flags");
                        ++notes;
                    }else if(e.kind==OriginalMusicSequenceEventKind::Configure){
                        if(e.configureReleaseTails==e.configureReleaseAt40Voices)throw std::runtime_error("Invalid release controller contract");
                        ++patches;
                    }else if(e.kind==OriginalMusicSequenceEventKind::Retune){
                        const std::array<std::uint32_t,7> value{unsigned(sample/44),e.command,e.channel,e.note,e.layerOffset,e.parameters.pitch,e.parameters.totalLevel};
                        if(sample%44||retuneIndex>=expectedRetunes.size()||value!=expectedRetunes[retuneIndex])throw std::runtime_error("Source mono-retune mismatch");
                        ++retuneIndex;++retunes;
                    }
                });
            }
            if(index!=expected.size()||retuneIndex!=expectedRetunes.size()||sequence.samplePosition()!=end)throw std::runtime_error("Incomplete native loop trace");
            sequence.stop();sequence.advanceSample([](const auto&){throw std::runtime_error("Stopped score emitted event");});
            if(sequence.samplePosition()!=end)throw std::runtime_error("Stopped score advanced");
            sequence.start();std::vector<unsigned> startCommands,expectedStartCommands;
            for(const auto& v:source)if(v[0]==0)expectedStartCommands.push_back(v[1]);
            sequence.advanceSample([&](const auto&e){if(e.kind==OriginalMusicSequenceEventKind::Command)startCommands.push_back(e.command);});
            if(startCommands!=expectedStartCommands||sequence.samplePosition()!=1)throw std::runtime_error("Restart lost original sample-zero initialization");
        }
        if(checked<3)throw std::runtime_error("The three menu score fixtures are required");
        std::cout<<"Original music scheduler passed for "<<checked<<" cues: "<<commands<<" ARM command timestamps, "<<notes<<" notes, "<<patches<<" parameter patches, "<<retunes<<" mono retunes; sample-zero start and two steady loop repeats.\n";
        return 0;
    }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
