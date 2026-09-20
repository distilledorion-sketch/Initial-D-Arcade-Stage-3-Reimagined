// The generated header wraps only the supplied reference numerical runStep,
// PACK andUNPACK bodies in an owned RAM object. No sound CPU, app startup,
// interrupts, emulator scheduler, JIT or host audio API is compiled or called.
#include "dsp_scalar_reference.generated.h"
#include "original_audio_dsp_bank.h"
#include "original_audio_dsp_test_input.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <cstring>
using namespace idas3;
template<class T>void emit(std::ofstream& out,T value){out.write(reinterpret_cast<const char*>(&value),sizeof value);}
int main(int argc,char**argv){try{
    if(argc!=3)throw std::runtime_error("Supply project root and fixture output");
    std::filesystem::path root=argv[1];std::ofstream out(argv[2],std::ios::binary);out.write("ODSPRF01",8);
    struct Case {std::string name;unsigned preset;OriginalAudioDspProgram program;};
    std::vector<Case> cases;std::set<std::vector<std::uint8_t>> distinct;
    for(const auto& entry:std::filesystem::directory_iterator(root/"data/original_audio/dsp")){
        if(entry.path().extension()!=".idsp")continue;
        const auto name=entry.path().stem().string();const auto bank=loadOriginalAudioDspBank(root,name);
        for(unsigned i=0;i<bank.presets.size();++i){
            const auto& raw=bank.presets[i].originalRecord;std::vector<std::uint8_t> key(raw.begin(),raw.end());
            if(distinct.insert(key).second)cases.push_back({name,i,originalAudioDspProgram(bank,i,2,true)});
        }
    }
    emit(out,std::uint32_t(cases.size()));std::uint64_t codec=originalDspHashBasis;
    for(unsigned i=0;i<65536;++i)originalDspTestHash(codec,std::uint32_t(DspScalarReference::UNPACK(std::uint16_t(i))));
    for(unsigned i=0;i<0x1000000;++i)originalDspTestHash(codec,DspScalarReference::PACK(std::bit_cast<std::int32_t>(i<<8)>>8));
    emit(out,codec);unsigned count=0;
    for(const auto& [name,preset,program]:cases){
        constexpr unsigned frames=131072;DspScalarReference r(program),rotated(program);rotated.state.MDEC_CT=17329;
        std::uint64_t outputHash=originalDspHashBasis;std::uint32_t random=0x31415926;
        for(unsigned frame=0;frame<frames;++frame){
            const auto input=originalDspTestInput(random,frame);r.frame(input);rotated.frame(input);
            for(unsigned i=0;i<16;++i){
                if(std::int16_t(r.data.EFREG[i])!=std::int16_t(rotated.data.EFREG[i]))throw std::runtime_error("Original ring phase is observable");
                originalDspTestHash(outputHash,std::uint32_t(std::int32_t(std::int16_t(r.data.EFREG[i]))));
            }
        }
        std::uint64_t stateHash=originalDspHashBasis;
        for(auto v:r.state.TEMP)originalDspTestHash(stateHash,std::uint32_t(v));
        for(auto v:r.state.MEMS)originalDspTestHash(stateHash,std::uint32_t(v));
        for(unsigned i=0;i<65536;++i)originalDspTestHash(stateHash,unsigned(r.aica_ram[i*2])|(unsigned(r.aica_ram[i*2+1])<<8));
        originalDspTestHash(stateHash,r.state.MDEC_CT);
        char label[16]{};if(name.size()>=sizeof label)throw std::runtime_error("Fixture name extent");std::memcpy(label,name.data(),name.size());out.write(label,sizeof label);
        emit(out,std::uint32_t(preset));emit(out,std::uint32_t(frames));emit(out,outputHash);emit(out,stateHash);
        std::cout<<name<<": "<<frames<<" samples; output/state verified at two initial ring phases\n";++count;
    }
    if(!out)throw std::runtime_error("Cannot save scalar DSP fixture");std::cout<<"DSP reference: "<<count<<" distinct presets and all24-bit codec inputs.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
