#include "original_ics_audio.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
using namespace idas3;
int main(int argc,char** argv)try{
    if(argc!=3)throw std::invalid_argument("project-root original-ARM-voice-reference required");
    const auto directory=std::filesystem::path(argv[1])/"data/original_audio/continuous";
    std::vector<std::filesystem::path> paths;
    for(const auto& f:std::filesystem::directory_iterator(directory))if(f.path().extension()==".dtpk")paths.push_back(f.path());
    std::sort(paths.begin(),paths.end());std::vector<OriginalIcsBank> banks;std::vector<std::map<unsigned,const OriginalIcsLayer*>> layers;
    banks.reserve(paths.size());unsigned layerCount=0;
    for(const auto& path:paths){
        banks.push_back(loadOriginalIcsBank(path));layers.emplace_back();
        for(const auto& program:banks.back().programs)for(const auto& layer:program.layers){layers.back()[layer.bankOffset]=&layer;++layerCount;}
    }
    const auto tables=OriginalIcsVoiceTables::load(argv[1]);
    std::ifstream f(argv[2],std::ios::binary);char header[8]{};f.read(header,8);
    if(std::string(header,8)!="ICSVREF1")throw std::runtime_error("Missing ARM voice reference");
    const auto word=[&](){unsigned v=0;if(!f.read(reinterpret_cast<char*>(&v),4))throw std::runtime_error("Truncated ARM voice reference");return v;};
    const unsigned count=word();if(count!=11278||layerCount!=156||banks.size()!=14)throw std::runtime_error("Unexpected reference coverage");
    std::set<std::pair<unsigned,unsigned>> covered;
    for(unsigned i=0;i<count;++i){
        const auto bank=word(),at=word(),value=word();std::array<std::uint8_t,12> c{};
        if(!f.read(reinterpret_cast<char*>(c.data()),c.size()))throw std::runtime_error("Truncated voice controls");
        OriginalIcsVoiceControls controls{c[0],c[1],c[2],c[3],c[4],c[5],c[6],c[7]};
        const auto& layer=*layers.at(bank).at(at);
        const auto parameters=originalIcsVoiceParameters(banks.at(bank),layer,tables,controls,value,c[8]);
        const std::array<unsigned,7> native{parameters.totalLevel,parameters.preMasterAmplitude,parameters.pitch,parameters.pan,parameters.cutoff,parameters.filterMode,parameters.effectSend};
        for(unsigned j=0;j<native.size();++j){
            const auto expected=word();if(native[j]!=expected)throw std::runtime_error("Voice parameter mismatch bank"+std::to_string(bank)+" layer"+std::to_string(at)+" value"+std::to_string(value)+" parameter"+std::to_string(j)+" native"+std::to_string(native[j])+" original"+std::to_string(expected));
        }
        if(parameters.directLevel!=layer.header[8])throw std::runtime_error("Dry direct level changed");
        covered.insert({bank,at});
    }
    if(covered.size()!=layerCount||f.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Incomplete or surplus voice reference");
    std::cout<<"PASS "<<count<<" voice cases, "<<count*7<<" exact original ARM results, all "<<layerCount<<" layers in "<<banks.size()<<" banks. Includes every active curve input and extrema of volume, pan, cutoff, effect send/bus, transpose, fine pitch, resonance and master gain. Original ARM7 stored-PC hook is documented in reference manifest. Proves parameter conversion, not envelopes/DSP or live mixing.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
