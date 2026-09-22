#include "original_engine_playback.h"
#include <bit>
#include <fstream>
#include <stdexcept>
namespace idas3 {
OriginalEngineSoundSelection selectOriginalEngineSound(const original::OriginalBattleProfile& p){
    OriginalEngineSoundSelection result{p.u(16),0};
    if(result.family>=35)throw std::out_of_range("Original profile sound car");
    const unsigned upgrade=p.byte(164);
    if(upgrade>4){result.level=4;if(result.family==0&&p.byte(152)<=2)result.family=35;}
    else if(upgrade>2)result.level=3;
    else if(upgrade>1)result.level=2;
    else result.level=p.byte(162)?1:0;
    return result;
}
original::OriginalEngineConfiguration configureProfileEngineSound(const original::OriginalBattleProfile& profile){
    const auto selection=selectOriginalEngineSound(profile);
    auto config=original::configureOriginalEngine(selection.family,selection.level,profile.byte(152),profile.byte(162),profile.byte(166));
    // The source sound-level threshold enables Levin A boost at Step 2,
    // but its authored tuning table installs the turbo at Step 3 (60000).
    // Correct that mismatch at the game-profile boundary, leaving the
    // reference controller and every other car/package unchanged.
    if(profile.u(16)==1&&profile.byte(152)==0&&profile.byte(164)<3)
        config.auxiliaryLoop=config.releaseCue=false;
    return config;
}
OriginalEngineBankSelections OriginalEngineBankSelections::load(const std::filesystem::path& root){
    std::ifstream f(root/"data/original_audio/continuous/selections.bin",std::ios::binary);
    const auto word=[&](){std::array<unsigned char,4>b{};if(!f.read(reinterpret_cast<char*>(b.data()),4))throw std::runtime_error("Missing/truncated engine bank selections");return unsigned(b[0])|(unsigned(b[1])<<8)|(unsigned(b[2])<<16)|(unsigned(b[3])<<24);};
    if(word()!=0x53454449||word()!=0x31303030||word()!=36)throw std::runtime_error("Unknown engine selection format");
    OriginalEngineBankSelections result;
    for(auto& family:result.families)for(auto& group:family){group={word(),word(),word()};if(group.bank<1||group.bank>14)throw std::runtime_error("Unknown engine bank");}
    result.auxiliary={word(),word(),0};
    if(result.auxiliary.bank!=10||f.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Invalid auxiliary bank selection");
    return result;
}
std::array<std::string,2> OriginalEngineInitialization::loadedBankNames()const{
    return {"PACK"+std::to_string(loadedBankIndices[0])+".bin","PACK"+std::to_string(loadedBankIndices[1])+".bin"};
}
bool OriginalSoundCueQueue::request(bool forced,std::uint32_t& seed){
    if(forced)return true;
    if(cooldown>0)return false;
    seed=seed*1103515245u+12345u;cooldown=10+int(((seed>>16)&32767u)%20u);return true;
}
void OriginalSoundCueQueue::tick(){cooldown=std::bit_cast<std::int32_t>(std::uint32_t(cooldown)-1u);}
OriginalEnginePlayback::OriginalEnginePlayback(const std::filesystem::path& root):root_(root),
    tables_(original::OriginalEngineTables::load(root)),selections_(OriginalEngineBankSelections::load(root)),
    player_(OriginalIcsVoiceTables::load(root)){}
std::shared_ptr<const OriginalIcsBank> OriginalEnginePlayback::bank(unsigned index){
    if(index<1||index>=banks_.size())throw std::out_of_range("Original engine bank index");
    if(!banks_[index])banks_[index]=std::make_shared<OriginalIcsBank>(loadOriginalIcsBank(root_/"data/original_audio/continuous"/("PACK"+std::to_string(index)+".dtpk")));
    return banks_[index];
}
void OriginalEnginePlayback::select(const original::OriginalBattleProfile& profile){
    const auto selection=selectOriginalEngineSound(profile);
    configuration_=configureProfileEngineSound(profile);
    const auto& firstGroup=selections_.families[selection.family][0];
    initialization_={{firstGroup.bank,selections_.auxiliary.bank},firstGroup.effect};
    player_.reset();controls_={};original::resetOriginalEngineControl(state_);
    // Scene4 creates the release queue. The host selects once for each race.
    releaseQueue_={};
    for(unsigned channel=0;channel<2;++channel){
        const auto& group=selections_.families[selection.family][channel];
        const auto selected=bank(group.bank);const unsigned program=group.instrument&255u;
        player_.select(channel,selected,program);controls_[channel].volume=selected->programs.at(program).header[2];
        controls_[channel].cutoff=63; //30A5(127) -> (127+64)&127
        player_.setControls(channel,controls_[channel]);
    }
    const auto& aux=selections_.auxiliary;
    const auto selected=bank(aux.bank);const unsigned program=aux.instrument&255u;
    player_.select(3,selected,program);controls_[3].volume=selected->programs.at(program).header[2];
    player_.setControls(3,controls_[3]);
}
void OriginalEnginePlayback::step(const original::OriginalEngineControlInput& input,std::uint32_t& seed,
        const std::function<void(unsigned,unsigned)>& cueOutput){
    auto in=input;in.handles={0,1,2,3};
    const auto commands=original::stepOriginalEngineControl(tables_,configuration_,state_,in,seed,
        [&](const original::OriginalEngineCommand& cue,std::uint32_t& shared){
            if(cue.target==original::OriginalEngineCommandTarget::RaceCue1424A0){if(cueOutput)cueOutput(2,unsigned(cue.value));}
            else if(releaseQueue_.request(false,shared)){if(cueOutput)cueOutput(5,unsigned(cue.value));}
        });
    applyContinuous(commands);
}
void OriginalEnginePlayback::applyContinuous(std::span<const original::OriginalEngineCommand> commands){
    for(const auto& command:commands){
        if(command.target!=original::OriginalEngineCommandTarget::Continuous)continue;
        const auto channel=command.handle;auto& c=controls_.at(channel);
        switch(command.command){
        case 0xa6:player_.setValue(channel,unsigned(command.value)&255u);continue;
        case 0x10a5:c.volume=std::uint8_t(unsigned(command.value)&127u);break;
        case 0x40a5:c.effectSend=std::uint8_t((unsigned(command.value)+64u)&127u);break;
        default:throw std::runtime_error("Unhandled original engine controller command");
        }
        player_.setControls(channel,c,command.command==0x40a5);
    }
}
}
