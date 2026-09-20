#include "original_audio_dsp.h"
#include <algorithm>
#include <bit>
#include <stdexcept>
namespace idas3 {
namespace {
std::int32_t signedWidth(std::int64_t value,unsigned bits){
    const auto modulus=std::uint64_t(1)<<bits;
    const auto raw=std::uint64_t(value)&(modulus-1);
    return std::int32_t(std::int64_t(raw)-(raw&(modulus>>1)?std::int64_t(modulus):0));
}
constexpr std::array<std::int32_t,16> outputGain{
    0,256,362,512,724,1024,1448,2048,2896,4096,5792,8192,11585,16384,23170,32768};
}
std::uint16_t packOriginalDspFloat(std::int32_t value){
    const auto raw=std::uint32_t(value)&0xffffffu;
    unsigned sign=raw>>23,exponent=0;
    auto transitions=(raw^(raw<<1))&0xffffffu;
    while(exponent<12&&!(transitions&0x800000u)){transitions<<=1;++exponent;}
    const auto shifted=raw<<(exponent<12?exponent:11);
    return std::uint16_t((sign<<15)|(exponent<<11)|((shifted>>11)&0x7ffu));
}
std::int32_t unpackOriginalDspFloat(std::uint16_t value){
    const unsigned sign=value>>15,exponent=(value>>11)&15;
    auto raw=unsigned(value&0x7ff)<<11;
    raw|=(exponent>11?sign:sign^1u)<<22;
    raw|=sign<<23;
    return signedWidth(raw,24)>>std::min(exponent,11u);
}
void OriginalAudioDsp::configure(const OriginalAudioDspProgram& p,bool preserveState){
    if((p.ringLengthWords!=8192&&p.ringLengthWords!=16384&&p.ringLengthWords!=32768&&p.ringLengthWords!=65536)||
       (p.ringBaseBytes&2047)||p.initialMemoryWords.size()>65536)
        throw std::invalid_argument("Invalid original DSP ring configuration");
    std::array<Instruction,128> decoded{};
    for(unsigned step=0;step<128;++step){
        const auto& w=p.instructions[step];auto& d=decoded[step];
        d.tra=(w[0]>>9)&127;d.twt=(w[0]&256)!=0;d.twa=(w[0]>>1)&127;
        d.xsel=(w[1]&0x8000)!=0;d.ysel=(w[1]>>13)&3;d.ira=(w[1]>>7)&63;
        d.iwt=(w[1]&64)!=0;d.iwa=(w[1]>>1)&31;
        d.table=(w[2]&0x8000)!=0;d.mwt=(w[2]&0x4000)!=0;d.mrd=(w[2]&0x2000)!=0;
        d.ewt=(w[2]&0x1000)!=0;d.ewa=(w[2]>>8)&15;d.adrl=(w[2]&128)!=0;
        d.frcl=(w[2]&64)!=0;d.shift=(w[2]>>4)&3;d.yrl=(w[2]&8)!=0;
        d.negb=(w[2]&4)!=0;d.zero=(w[2]&2)!=0;d.bsel=(w[2]&1)!=0;
        d.nofl=(w[3]&0x8000)!=0;d.masa=(w[3]>>9)&63;
        d.adreb=(w[3]&256)!=0;d.nxadr=(w[3]&128)!=0;
        // The original recovered presets avoid the hardware ambiguities in
        // the numerical references. Fail explicitly if that scope changes.
        if(d.nofl||d.table||d.adrl||d.adreb||d.ira>49||
           ((d.mrd||d.mwt)&&!(step&1))||
           (d.iwt&&(step<2||!(p.instructions[step-2][2]&0x2000)))||
           (d.iwt&&d.ira==d.iwa&&(d.xsel||d.yrl)))
            throw std::invalid_argument("Original DSP program exceeds the verified instruction subset");
    }
    const bool keep=preserveState&&configured_;
    program_=p;instructions_=decoded;configured_=true;
    if(!keep)reset();
    else decrement_=((decrement_-1)&(program_.ringLengthWords-1))+1;
}
void OriginalAudioDsp::reset(){
    temporary_={};memoryRegisters_={};effects_={};decrement_=1;frames_=0;
    memory_.assign(65536,0x6000);
    std::copy(program_.initialMemoryWords.begin(),program_.initialMemoryWords.end(),memory_.begin());
}
void OriginalAudioDsp::clearProgram(){
    if(!configured_)configure(OriginalAudioDspProgram{});
    program_.instructions={};program_.coefficients={};program_.memoryAddresses={};
    program_.effectRoutes.fill(0x10);instructions_={};temporary_={};memoryRegisters_={};effects_={};
}
void OriginalAudioDsp::setEffectRoute(unsigned index,std::uint16_t route){
    if(index>=program_.effectRoutes.size())throw std::out_of_range("Original DSP effect return index");
    program_.effectRoutes[index]=route;
}
OriginalAudioDspFrame OriginalAudioDsp::render(std::span<const std::int32_t,16> input,std::array<std::int16_t,2> external){
    if(!configured_)return {};
    std::int32_t accumulator=0,fraction=0,yLatch=0;
    std::array<std::uint16_t,4> reads{};
    // An odd-step SRAM write commits one instruction later. All verified
    // programs access SRAM only on odd steps, so this retains the source
    // kernel's observable ordering while making the deferred write explicit.
    bool writePending=false;unsigned writeAddress=0;std::uint16_t writeValue=0;
    for(unsigned step=0;step<128;++step){
        if(writePending){memory_[writeAddress]=writeValue;writePending=false;}
        const auto& d=instructions_[step];
        std::int32_t inputs=0;
        if(d.ira<32)inputs=memoryRegisters_[d.ira];
        else if(d.ira<48)inputs=signedWidth(std::int64_t(input[d.ira-32])*16,24);
        else inputs=std::int32_t(external[d.ira-48])*256;
        // INPUTS is sampled before IWT. Original programs consume the read
        // issued exactly two instructions earlier, with NOFL always clear.
        if(d.iwt)memoryRegisters_[d.iwa]=unpackOriginalDspFloat(reads[step&3]);
        const auto temp=temporary_[(d.tra+decrement_)&127];
        std::int32_t addend=d.zero?0:d.bsel?accumulator:temp;
        if(d.negb&&!d.zero)addend=signedWidth(-std::int64_t(addend),26);
        const auto x=d.xsel?inputs:temp;
        std::int32_t y=0;
        switch(d.ysel){
        case 0:y=fraction;break;
        case 1:y=std::int32_t(program_.coefficients[step])>>3;break;
        case 2:y=yLatch>>11;break;
        default:y=(yLatch>>4)&4095;break;
        }
        y=signedWidth(y,13);
        if(d.yrl)yLatch=inputs;
        const auto scaled=std::int64_t(accumulator)*((d.shift==1||d.shift==2)?2:1);
        const auto shifted=d.shift<2?std::int32_t(std::clamp<std::int64_t>(scaled,-0x800000,0x7fffff)):signedWidth(scaled,24);
        // Output uses the previous ACC; the newly computed sum is available
        // to the next instruction. All narrow stores explicitly wrap.
        accumulator=signedWidth(((std::int64_t(x)*y)>>12)+addend,26);
        if(d.twt)temporary_[(d.twa+decrement_)&127]=shifted;
        if(d.frcl)fraction=d.shift==3?(shifted&4095):signedWidth(shifted>>11,13);
        if(d.mrd||d.mwt){
            unsigned address=(unsigned(program_.memoryAddresses[d.masa])+decrement_+unsigned(d.nxadr))&(program_.ringLengthWords-1);
            if(d.mrd)reads[(step+2)&3]=memory_[address];
            if(d.mwt){writePending=true;writeAddress=address;writeValue=packOriginalDspFloat(shifted);}
        }
        if(d.ewt)effects_[d.ewa]=std::bit_cast<std::int16_t>(std::uint16_t(shifted>>8));
    }
    if(writePending)memory_[writeAddress]=writeValue;
    if(!--decrement_)decrement_=program_.ringLengthWords;
    ++frames_;
    OriginalAudioDspFrame result;result.effects=effects_;
    for(unsigned bus=0;bus<16;++bus){
        const auto route=program_.effectRoutes[bus];
        const auto level=(route>>8)&15,pan=route&31;
        const auto value=std::int32_t((std::int64_t(effects_[bus])*outputGain[level])>>15);
        const auto panned=std::int32_t((std::int64_t(value)*outputGain[15-(pan&15)])>>15);
        result.wet[0]+=(pan&16)?value:panned;result.wet[1]+=(pan&16)?panned:value;
    }
    return result;
}
}
