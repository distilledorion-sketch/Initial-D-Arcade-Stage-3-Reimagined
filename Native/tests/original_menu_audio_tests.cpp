#include "original_menu_audio.h"
#include "sh4_scalar_reference.h"
#include <bit>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

int main(int argc,char**argv){
    try{
        if(argc<2||argc>3)throw std::runtime_error("Expected project root [canonical-image]");
        std::ifstream in(std::filesystem::path(argv[1])/"data/original_audio/menu/PACK21.dtpk",std::ios::binary);
        std::vector<std::uint8_t> bank{std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
        auto u32=[&](std::size_t p){return std::uint32_t(bank.at(p))|std::uint32_t(bank.at(p+1))<<8|std::uint32_t(bank.at(p+2))<<16|std::uint32_t(bank.at(p+3))<<24;};
        std::size_t compared=0;
        for(unsigned cue=0;cue<15;++cue){
            const auto sound=idas3::decodeOriginalMenuSound(bank,0x1a9u|(cue<<16));
            if(sound.clip.sampleRate!=(cue==14?11025u:22050u)||sound.clip.channels!=1||sound.clip.looping)throw std::runtime_error("Unexpected menu PCM metadata");
            const auto location=u32(u32(0x3c)+4+16*sound.sampleId);
            for(std::size_t j=0;j<sound.clip.samples.size();++j){
                auto value=std::bit_cast<std::uint16_t>(sound.clip.samples[j]);
                if((value&255)!=bank.at(location+j*2)||(value>>8)!=bank.at(location+j*2+1))throw std::runtime_error("PCM byte mismatch");
                ++compared;
            }
        }
        if(idas3::loadOriginalMenuSound(argv[1],idas3::OriginalMenuCue::Back).sampleId!=6)throw std::runtime_error("Back cue must use playback8/sample6, not sample8");
        const auto count=idas3::loadOriginalMenuSound(argv[1],idas3::OriginalMenuCue::ResultCount);
        if(count.command!=0x000501a9||count.playbackId!=5||count.sampleId!=7||count.clip.sampleRate!=22050||count.clip.samples.size()!=8856)
            throw std::runtime_error("Result count must use original playback5/sample7 at22050Hz");
        const auto upgrade=idas3::loadOriginalMenuSound(argv[1],idas3::OriginalMenuCue::UpgradeNotice);
        if(upgrade.command!=0x000e01a9||upgrade.playbackId!=14||upgrade.sampleId!=14||upgrade.clip.sampleRate!=11025||upgrade.clip.samples.size()!=16512)
            throw std::runtime_error("Upgrade notice must use original playback14/sample14 at11025Hz");
        std::size_t sourceInstructions=0,sourceCases=0;
        if(argc==3){
            using namespace idas3::reference;
            RefMemory memory(argv[2]);constexpr unsigned manager=0x0d000000,stack=0x0d100000,stop=0x00ff0000;
            for(bool present:{false,true})for(unsigned cue=0;cue<15;++cue){
                memory.clear();memory.zeroRegion(manager,64);memory.zeroRegion(stack,0x10000);
                memory.write32(0x0c8ff1cc,present?manager:0);memory.write32(manager+40,0x0c31eb68);
                RefCpu cpu(memory);cpu.r[4]=cue;cpu.r[5]=1;cpu.r[15]=stack+0xf000;cpu.pr=stop;
                unsigned calls=0,command=0;
                // Only the platform sound-driver queue is a hook.141F80 and
                //1435C0 execute actual source bytes and the original cue table.
                cpu.callHooks[0x0c1ed9c0]=[&](auto& c){++calls;command=c.r[4];};
                sourceInstructions+=cpu.run(0x0c141f80,stop,1000);
                if(calls!=unsigned(present)||(present&&command!=(0x1a9u|(cue<<16))))throw std::runtime_error("Original sound-manager command mismatch");
                if(present&&cue==5&&command!=unsigned(idas3::OriginalMenuCue::ResultCount))throw std::runtime_error("ResultCount alias differs from source");
                if(present&&cue==14&&command!=unsigned(idas3::OriginalMenuCue::UpgradeNotice))throw std::runtime_error("UpgradeNotice alias differs from source");
                ++sourceCases;
            }
        }
        unsigned rejected=0;
        auto bad=bank;bad.resize(63);try{idas3::decodeOriginalMenuSound(bad,0x201a9);}catch(const std::exception&){++rejected;}
        try{idas3::decodeOriginalMenuSound(bank,0xff01a9);}catch(const std::exception&){++rejected;}
        bad=bank;bad[0]=0;try{idas3::decodeOriginalMenuSound(bad,0x201a9);}catch(const std::exception&){++rejected;}
        if(rejected!=3)throw std::runtime_error("Malformed bank accepted");
        std::cout<<"Original menu audio: 15 cues, "<<compared<<" PCM samples byte-exact; 3 malformed inputs rejected; "
            <<sourceCases<<" actual sound-manager cases, "<<sourceInstructions<<" instructions (sound-driver queue boundary only)\n";
        return 0;
    }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
