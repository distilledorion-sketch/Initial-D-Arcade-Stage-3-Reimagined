#include "original_ics_audio.h"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iterator>
#include <stdexcept>
namespace idas3 {
namespace {
struct Reader {
    std::span<const std::uint8_t> b;
    void check(std::size_t at,std::size_t count)const{if(at>b.size()||count>b.size()-at)throw std::runtime_error("ICS record outside bank");}
    std::uint8_t u8(std::size_t at)const{check(at,1);return b[at];}
    std::uint16_t u16(std::size_t at)const{return u8(at)|(std::uint16_t(u8(at+1))<<8);}
    std::uint32_t u32(std::size_t at)const{return u16(at)|(std::uint32_t(u16(at+2))<<16);}
};
std::vector<std::uint8_t> read(const std::filesystem::path& path){
    std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Original ICS asset unavailable");
    return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};
}
}
std::int32_t OriginalIcsLayer::evaluate(unsigned control,unsigned value)const{
    if(control>=controls.size()||value>255||controls[control].empty())throw std::out_of_range("Original ICS control input");
    const auto& segments=controls[control];unsigned start=first;std::size_t index=0;
    while(value>segments[index].end&&index+1<segments.size()){start=unsigned(segments[index].end)+1;++index;}
    const auto& segment=segments[index];
    // ARM MUL retains the low32 bits before arithmetic shift by8.
    const auto product=std::uint32_t(segment.slope)*(std::uint32_t(value)-start);
    return std::int32_t(segment.base)+(std::bit_cast<std::int32_t>(product)>>8);
}
OriginalIcsBank decodeOriginalIcsBank(std::span<const std::uint8_t> bytes){
    Reader r{bytes};r.check(0,64);
    if(r.u32(0)!=0x4b505444||r.u32(8)!=bytes.size())throw std::runtime_error("Invalid DTPK header/size");
    const std::size_t ics=r.u32(0x34),sampleTable=r.u32(0x3c),volume=r.u32(0x28);
    if(!ics||!sampleTable||!volume)throw std::runtime_error("Missing ICS bank tables");
    const auto programs=r.u32(ics)+1,samples=r.u32(sampleTable)+1;
    if(!programs||programs>128||!samples||samples>256)throw std::runtime_error("Invalid ICS table count");
    OriginalIcsBank result;result.samples.reserve(samples);result.programs.reserve(programs);
    const auto volumeRow=r.u32(volume)>=1?1u:0u;
    for(unsigned i=0;i<128;++i)result.volumeTable[i]=r.u8(volume+4+128*volumeRow+i);
    for(unsigned i=0;i<samples;++i){
        const auto at=sampleTable+4+16*i;const auto location=r.u32(at),flags=r.u32(at+8),count=r.u32(at+12);
        if((location&0xfd800000u)||flags!=0||count%2)throw std::runtime_error("Unverified ICS sample encoding; expected PCM16 mono");
        const auto offset=location&0x7fffff;r.check(offset,count);
        OriginalIcsSample sample;sample.loopStart=r.u16(at+4);sample.loopEnd=r.u16(at+6);sample.looping=(location&0x2000000)!=0;
        if(sample.looping&&(sample.loopStart>=sample.loopEnd||sample.loopEnd>=count/2))throw std::runtime_error("Invalid ICS loop bounds");
        sample.pcm.resize(count/2);
        for(std::size_t j=0;j<sample.pcm.size();++j)sample.pcm[j]=std::bit_cast<std::int16_t>(r.u16(offset+2*j));
        result.samples.push_back(std::move(sample));
    }
    for(unsigned i=0;i<programs;++i){
        const auto relative=r.u32(ics+4+4*i);if(!relative)throw std::runtime_error("Missing ICS program");
        std::size_t at=ics+relative;OriginalIcsProgram program;r.check(at,8);
        std::copy_n(bytes.begin()+at,8,program.header.begin());at+=8;
        const unsigned layers=unsigned(program.header[0])+1;if(layers>16)throw std::runtime_error("ICS voice limit exceeded");
        for(unsigned j=0;j<layers;++j){
            OriginalIcsLayer layer;layer.bankOffset=std::uint32_t(at);r.check(at,24);
            std::copy_n(bytes.begin()+at,12,layer.header.begin());layer.sample=r.u16(at+2);layer.first=r.u8(at+4);layer.last=r.u8(at+5);layer.flags=r.u8(at+10);
            if(layer.first>layer.last||layer.sample<256||unsigned(layer.sample-256)>=samples)throw std::runtime_error("Unverified ICS layer range/sample reference");
            const auto length=r.u8(at);if(length<24)throw std::runtime_error("Invalid ICS layer size");r.check(at,length);
            for(unsigned control=0;control<6;++control){
                const unsigned count=r.u8(at+12+2*control),offset=r.u8(at+13+2*control),stride=control==4?6:4;
                if(!count||12+offset+count*stride>length)throw std::runtime_error("Invalid ICS control table");
                for(unsigned k=0;k<count;++k){
                    const auto p=at+12+offset+k*stride;
                    OriginalIcsSegment segment{r.u8(p),control==4?r.u16(p+2):std::uint16_t(r.u8(p+1)),std::bit_cast<std::int16_t>(r.u16(p+(control==4?4:2)))};
                    if(k&&segment.end<=layer.controls[control].back().end)throw std::runtime_error("Unordered ICS control knots");
                    layer.controls[control].push_back(segment);
                }
            }
            program.layers.push_back(std::move(layer));at+=length;
        }
        result.programs.push_back(std::move(program));
    }
    return result;
}
OriginalIcsBank loadOriginalIcsBank(const std::filesystem::path& path){const auto b=read(path);return decodeOriginalIcsBank(b);}
OriginalIcsPitchTable OriginalIcsPitchTable::load(const std::filesystem::path& root){
    const auto bytes=read(root/"data/original_audio/continuous/pitch_table.bin");
    if(bytes.size()!=768)throw std::runtime_error("Invalid original ICS pitch table");Reader r{bytes};OriginalIcsPitchTable result;
    for(unsigned i=0;i<384;++i)result.fractional[i]=r.u16(i*2);return result;
}
std::uint16_t OriginalIcsPitchTable::registerWord(unsigned pitch)const{
    if(pitch>0x17ff)throw std::out_of_range("Original ICS pitch outside driver6C5C clamp");
    const unsigned octave=(pitch/384-8)&15;
    return std::uint16_t((octave<<11)+fractional[pitch%384]);
}
OriginalIcsVoiceTables OriginalIcsVoiceTables::load(const std::filesystem::path& root){
    const auto bytes=read(root/"data/original_audio/continuous/voice_tables.bin");
    if(bytes.size()!=296||!std::equal(bytes.begin(),bytes.begin()+8,"ICSV0001"))throw std::runtime_error("Invalid original ICS voice tables");
    OriginalIcsVoiceTables out;out.pitch=OriginalIcsPitchTable::load(root);Reader r{bytes};
    for(unsigned i=0;i<32;++i)out.pan[i]=r.u8(8+i);
    for(unsigned i=0;i<128;++i)out.cutoff[i]=r.u16(40+2*i);
    return out;
}
OriginalIcsVoiceParameters originalIcsVoiceParameters(const OriginalIcsBank& bank,
        const OriginalIcsLayer& layer,const OriginalIcsVoiceTables& tables,
        const OriginalIcsVoiceControls& c,unsigned value,unsigned master){
    if(value<layer.first||value>layer.last||master>127)throw std::out_of_range("ICS voice outside active range");
    const auto volume=layer.evaluate(0,value);
    if(volume<0||volume>=128)throw std::out_of_range("ICS volume curve outside lookup");
    unsigned amplitude=(bank.volumeTable[volume]*(unsigned(c.volume)<<1))>>8;
    if(amplitude)++amplitude;
    OriginalIcsVoiceParameters out;out.preMasterAmplitude=std::uint8_t(amplitude);
    amplitude=(amplitude*(master<<1))>>8;
    out.totalLevel=std::uint8_t(amplitude?((amplitude+1)^255)>>1:255);
    const auto pitch=layer.evaluate(4,value)+(int(c.transpose)-64)*32+((int(c.finePitch)-64)>>1);
    out.pitch=tables.pitch.registerWord(unsigned(std::clamp(pitch,0,0x17ff)));
    const auto pan=std::clamp(layer.evaluate(1,value)+int(c.pan)-64,0,124);
    out.pan=tables.pan[pan>>2];
    const auto cutoff=std::bit_cast<std::int32_t>((std::uint32_t(layer.evaluate(2,value))>>1)+unsigned(c.cutoff)*2-128);
    out.cutoff=tables.cutoff[std::clamp(cutoff,0,127)];
    out.filterMode=std::uint8_t((layer.header[9]&1)?32:std::clamp(layer.evaluate(5,value)+int(c.resonance)-64,0,31));
    const auto send=std::clamp(layer.evaluate(3,value)+int(c.effectSend>>2)-16,0,15);
    out.effectSend=std::uint8_t((send<<4)|((c.effectBus==255?layer.header[1]:c.effectBus)&15));
    out.directLevel=layer.header[8];
    return out;
}
}
