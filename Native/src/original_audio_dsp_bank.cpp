#include "original_audio_dsp_bank.h"
#include <algorithm>
#include <bit>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
namespace idas3 {
namespace {
unsigned word(std::span<const std::uint8_t>b,std::size_t p){
    if(p>b.size()||b.size()-p<4)throw std::runtime_error("Truncated original DSP bank");
    return unsigned(b[p])|(unsigned(b[p+1])<<8)|(unsigned(b[p+2])<<16)|(unsigned(b[p+3])<<24);
}
unsigned half(std::span<const std::uint8_t>b,std::size_t p){return unsigned(b[p])|(unsigned(b[p+1])<<8);}
}
OriginalAudioDspBank decodeOriginalAudioDspBank(std::span<const std::uint8_t>b){
    if(b.size()<24||std::memcmp(b.data(),"IDSP0001",8))throw std::runtime_error("Original DSP bank identity");
    OriginalAudioDspBank bank;bank.bankId=word(b,8);bank.declaredRingCode=word(b,12);
    const auto presetCount=word(b,16),sceneCount=word(b,20);
    if(bank.bankId>255||bank.declaredRingCode>3||presetCount>128||sceneCount>255||
        b.size()!=24ull+0xc28ull*presetCount+12ull*sceneCount)throw std::runtime_error("Original DSP bank extent");
    std::size_t at=24;
    for(unsigned i=0;i<presetCount;++i){
        OriginalAudioDspPreset p;p.sourceOffset=word(b,at);at+=4;
        std::copy_n(b.begin()+at,p.originalRecord.size(),p.originalRecord.begin());at+=p.originalRecord.size();
        for(unsigned j=0;j<768;++j)if(word(p.originalRecord,36+j*4)&0xffff0000u)throw std::runtime_error("Unknown original DSP register bits");
        for(unsigned j=192;j<256;++j)if(word(p.originalRecord,36+j*4))throw std::runtime_error("Unknown original DSP register padding");
        bank.presets.push_back(p);
    }
    for(unsigned i=0;i<sceneCount;++i){
        OriginalAudioDspScene s{word(b,at),word(b,at+4),word(b,at+8)};at+=12;
        if(s.bankId>255||s.preset>255)throw std::runtime_error("Original DSP scene selector");bank.scenes.push_back(s);
    }
    return bank;
}
OriginalAudioDspBank loadOriginalAudioDspBank(const std::filesystem::path&root,std::string_view name){
    if(name.empty()||name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")!=std::string_view::npos)throw std::invalid_argument("Original DSP bank name");
    std::ifstream f(root/"data/original_audio/dsp"/(std::string(name)+".idsp"),std::ios::binary);
    if(!f)throw std::runtime_error("Missing original DSP bank");const std::vector<std::uint8_t>b((std::istreambuf_iterator<char>(f)),{});return decodeOriginalAudioDspBank(b);
}
OriginalAudioDspProgram originalAudioDspProgram(const OriginalAudioDspBank&bank,unsigned preset,unsigned ringCode,bool memory8Mb){
    if(ringCode>3)throw std::invalid_argument("Original DSP ring code");
    const auto& b=bank.presets.at(preset).originalRecord;OriginalAudioDspProgram p;
    p.ringLengthWords=8192u<<ringCode;p.ringBaseBytes=(memory8Mb?0x800000u:0x200000u)-p.ringLengthWords*2;
    for(unsigned i=0;i<16;++i)p.effectRoutes[i]=std::uint16_t(half(b,i*2));
    for(unsigned i=0;i<128;++i)p.coefficients[i]=std::bit_cast<std::int16_t>(std::uint16_t(word(b,36+i*4)));
    for(unsigned i=0;i<64;++i)p.memoryAddresses[i]=std::uint16_t(word(b,36+512+i*4));
    for(unsigned i=0;i<128;++i)for(unsigned j=0;j<4;++j)p.instructions[i][j]=std::uint16_t(word(b,36+1024+i*16+j*4));
    return p;
}
}
