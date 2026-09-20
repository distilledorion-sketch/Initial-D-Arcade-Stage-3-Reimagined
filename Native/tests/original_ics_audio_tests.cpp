#include "original_ics_audio.h"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
using namespace idas3;
int main(int argc,char** argv)try{
    if(argc!=3)throw std::invalid_argument("project-root ARM-reference-file required");
    const auto directory=std::filesystem::path(argv[1])/"data/original_audio/continuous";
    std::vector<std::filesystem::path> paths;
    for(const auto& f:std::filesystem::directory_iterator(directory))if(f.path().extension()==".dtpk")paths.push_back(f.path());
    std::sort(paths.begin(),paths.end());std::vector<OriginalIcsBank> banks;std::vector<std::map<unsigned,const OriginalIcsLayer*>> layers;
    banks.reserve(paths.size());std::size_t samples=0,frames=0,layerCount=0;
    for(const auto& path:paths){
        try{banks.push_back(loadOriginalIcsBank(path));}catch(const std::exception& e){throw std::runtime_error(path.filename().string()+": "+e.what());}
        layers.emplace_back();const auto& bank=banks.back();
        for(const auto& program:bank.programs)for(const auto& layer:program.layers){layers.back()[layer.bankOffset]=&layer;++layerCount;}
        samples+=bank.samples.size();for(const auto& sample:bank.samples)frames+=sample.pcm.size();
    }
    auto pitches=OriginalIcsPitchTable::load(argv[1]);
    std::ifstream f(argv[2],std::ios::binary);char header[8]{};f.read(header,8);
    if(std::string(header,8)!="ICSREF01")throw std::runtime_error("Original ARM reference missing");
    const auto word=[&](){unsigned v=0;if(!f.read(reinterpret_cast<char*>(&v),4))throw std::runtime_error("Truncated ARM reference");return v;};
    const unsigned count=word(),pitchCount=word();
    if(count!=layerCount*6*256||pitchCount!=0x1800)throw std::runtime_error("Incomplete ICS reference coverage");
    for(unsigned i=0;i<count;++i){
        const auto bank=word(),at=word(),control=word(),value=word(),expected=word();
        const auto actual=std::uint32_t(layers.at(bank).at(at)->evaluate(control,value));
        if(actual!=expected)throw std::runtime_error("ICS interpolation differs bank"+std::to_string(bank)+" offset"+std::to_string(at)+" control"+std::to_string(control)+" input"+std::to_string(value));
    }
    for(unsigned i=0;i<pitchCount;++i)if(pitches.registerWord(i)!=word())throw std::runtime_error("ICS pitch conversion differs");
    if(f.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Unexpected ARM reference tail");
    // Deliberately corrupt actual asset headers/offsets, and truncate PCM.
    std::ifstream raw(paths.front(),std::ios::binary);std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(raw),{}};
    const auto reject=[&](const auto& b){try{decodeOriginalIcsBank(b);}catch(const std::exception&){return;}throw std::runtime_error("Malformed ICS bank accepted");};
    auto invalid=bytes;invalid[0]=0;reject(invalid);invalid=bytes;std::fill_n(invalid.begin()+0x34,4,255);reject(invalid);
    invalid=bytes;invalid.resize(64);reject(invalid);
    std::cout<<"PASS"<<banks.size()<<" banks,"<<layerCount<<" layers,"<<samples<<" PCM samples,"<<frames<<" decoded frames,"<<count<<" original ARM curve comparisons and"<<pitchCount<<" exact pitch-register comparisons. Covers ICS data/interpolation; voice lifetimes, output mixing and DSP are separate.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
