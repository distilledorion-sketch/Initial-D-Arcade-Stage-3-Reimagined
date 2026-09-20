#include "original_audio_dsp.h"
#include "original_audio_dsp_bank.h"
#include "original_audio_dsp_test_input.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace idas3;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T read(std::ifstream& in){T value{};in.read(reinterpret_cast<char*>(&value),sizeof value);if(!in)throw std::runtime_error("Truncated DSP reference");return value;}
std::uint64_t stateHash(const OriginalAudioDsp& dsp){
    auto hash=originalDspHashBasis;
    for(auto v:dsp.temporary())originalDspTestHash(hash,std::uint32_t(v));
    for(auto v:dsp.memoryRegisters())originalDspTestHash(hash,std::uint32_t(v));
    for(auto v:dsp.memory())originalDspTestHash(hash,v);
    originalDspTestHash(hash,dsp.decrementCounter());return hash;
}
OriginalAudioDspProgram passThrough(){
    OriginalAudioDspProgram p;
    p.instructions[0]={0,0xb000,2,0};p.coefficients[0]=32760; // input0 *4095/4096
    p.instructions[1]={0,0,0x1002,0}; // output prior accumulator
    return p;
}
void focused(){
    std::array<std::int32_t,16> in{};
    // One-instruction accumulator output latency, positive and negative clipping.
    auto p=passThrough();p.instructions[0][2]|=0x1100; // old ACC -> EF1
    OriginalAudioDsp dsp;dsp.configure(p);in[0]=0x12340;
    auto r=dsp.render(in);require(r.effects[1]==0,"Output must precede current multiply");
    require(r.effects[0]==std::int16_t(((std::int64_t(in[0])*16*4095)>>12)>>8),"Accumulator latency/multiply");
    p.instructions[1][2]|=0x10;dsp.configure(p);in[0]=524287;
    require(dsp.render(in).effects[0]==32767,"Positive saturating shifter");in[0]=-524288;
    require(dsp.render(in).effects[0]==-32768,"Negative saturating shifter");
    // The non-saturating shifter truncates to signed24 before TEMP storage.
    p.instructions[1]={0x100,0,0x1022,0};dsp.configure(p);in[0]=524287;
    const auto product=(std::int64_t(in[0])*16*4095)>>12;
    const auto raw=std::uint32_t(product*2)&0xffffff;
    const auto wrapped=std::int32_t(raw)-(raw&0x800000?0x1000000:0);
    require(dsp.render(in).effects[0]==std::int16_t(wrapped>>8),"Wrap shifter output");
    require(dsp.temporary()[1]==wrapped,"TEMP store must be signed24");
    // Five near-full-scale products overflow the signed26 accumulator. The
    // subsequent saturated output has the opposite sign from an unbounded sum.
    p={};
    for(unsigned step=0;step<5;++step){p.instructions[step]={0,0xb000,std::uint16_t(step?1:2),0};p.coefficients[step]=32760;}
    p.instructions[5]={0,0,0x1002,0};dsp.configure(p);in[0]=524287;
    require(dsp.render(in).effects[0]==-32768,"Positive signed26 accumulator wrap");
    in[0]=-524288;require(dsp.render(in).effects[0]==32767,"Negative signed26 accumulator wrap");
    // MRD and MWT on the same odd instruction read the pre-write word.
    // IWT two instructions later consumes that read; the later read sees MWT.
    p=passThrough();p.initialMemoryWords.assign(2,0x6000);p.initialMemoryWords[1]=packOriginalDspFloat(-0x234000);
    p.instructions[1]={0,0,0x6002,0};
    p.instructions[3]={0,0x40,0x2002,0};
    p.instructions[4]={0,0xa000,2,0};p.coefficients[4]=32760;
    p.instructions[5]={0,0x42,0x1002,0};
    p.instructions[6]={0,0xa080,2,0};p.coefficients[6]=32760;
    p.instructions[7]={0,0,0x1102,0};
    dsp.configure(p);in[0]=0x10000;const auto before=p.initialMemoryWords[1];r=dsp.render(in);
    const auto written=packOriginalDspFloat(std::int32_t((std::int64_t(in[0])*16*4095)>>12));
    require(dsp.memory()[1]==written,"Delayed SRAM write");
    require(dsp.memoryRegisters()[0]==unpackOriginalDspFloat(before),"Same-step SRAM read must precede write");
    require(dsp.memoryRegisters()[1]==unpackOriginalDspFloat(written),"Following SRAM read observes committed write");
    require(r.effects[0]==std::int16_t(((std::int64_t(unpackOriginalDspFloat(before))*4095)>>12)>>8),"Two-instruction SRAM read latency");
    require(r.effects[1]==std::int16_t(((std::int64_t(unpackOriginalDspFloat(written))*4095)>>12)>>8),"Second delayed read latency");
    // Analytic 3dB-step output gain table, two separately truncated products.
    for(unsigned level=0;level<16;++level)for(unsigned pan=0;pan<32;++pan)for(int sign:{-1,1}){
        p=passThrough();p.effectRoutes[0]=std::uint16_t(level*256+pan);dsp.configure(p);in[0]=sign*200000;r=dsp.render(in);
        const auto gain=[](unsigned n){return n?int(32768*std::pow(2.0,(int(n)-15)/2.0)):0;};
        const auto full=(std::int64_t(r.effects[0])*gain(level))>>15;
        const auto partial=(full*gain(15-(pan&15)))>>15;
        require(r.wet[0]==((pan&16)?full:partial)&&r.wet[1]==((pan&16)?partial:full),"Effect return level/pan");
    }
    // Preserve leaves phase, memory and clock intact. Reset clears owned state.
    p=passThrough();dsp.configure(p);dsp.render(in);auto hash=stateHash(dsp);auto frames=dsp.frames();
    dsp.configure(p,true);require(stateHash(dsp)==hash&&dsp.frames()==frames,"Preserved DSP state");
    dsp.configure(p);require(dsp.frames()==0&&dsp.decrementCounter()==1,"Fresh DSP phase");
    require(std::all_of(dsp.memory().begin(),dsp.memory().end(),[](auto v){return v==0x6000;}),"Source floating silence initialization");
    // Unsupported hardware modes must fail rather than silently approximate.
    for(unsigned kind=0;kind<7;++kind){
        p=passThrough();
        if(kind==0)p.instructions[1][3]|=0x8000;
        if(kind==1)p.instructions[1][2]|=0x8000;
        if(kind==2)p.instructions[1][2]|=0x80;
        if(kind==3)p.instructions[1][3]|=0x100;
        if(kind==4)p.instructions[0][2]|=0x2000;
        if(kind==5)p.instructions[1][1]|=0x40;
        if(kind==6){p.instructions[1][2]|=0x2000;p.instructions[3][1]=0x8040;}
        bool rejected=false;try{dsp.configure(p);}catch(const std::invalid_argument&){rejected=true;}
        require(rejected,"Unverified DSP instruction accepted");
    }
}
}
int main(int argc,char** argv){try{
    require(argc==2,"Supply project root");const std::filesystem::path root=argv[1];
    std::ifstream fixture(root/"verification/original-audio-dsp/scalar_reference.bin",std::ios::binary);
    char magic[8]{};fixture.read(magic,8);require(std::memcmp(magic,"ODSPRF01",8)==0,"DSP reference magic");
    const auto count=read<std::uint32_t>(fixture);const auto expectedCodec=read<std::uint64_t>(fixture);
    auto codec=originalDspHashBasis;
    for(unsigned i=0;i<65536;++i)originalDspTestHash(codec,std::uint32_t(unpackOriginalDspFloat(std::uint16_t(i))));
    for(unsigned i=0;i<0x1000000;++i)originalDspTestHash(codec,packOriginalDspFloat(std::bit_cast<std::int32_t>(i<<8)>>8));
    require(codec==expectedCodec,"Exhaustive DSP float codec mismatch");
    for(unsigned index=0;index<count;++index){
        char label[17]{};fixture.read(label,16);const auto preset=read<std::uint32_t>(fixture),frames=read<std::uint32_t>(fixture);
        const auto expectedOutput=read<std::uint64_t>(fixture),expectedState=read<std::uint64_t>(fixture);
        OriginalAudioDsp dsp;dsp.configure(originalAudioDspProgram(loadOriginalAudioDspBank(root,label),preset,2,true));
        auto outputHash=originalDspHashBasis;std::uint32_t random=0x31415926;bool tail=false;
        for(unsigned frame=0;frame<frames;++frame){
            const auto r=dsp.render(originalDspTestInput(random,frame));
            for(auto value:r.effects){originalDspTestHash(outputHash,std::uint32_t(std::int32_t(value)));if(frame>10000&&value)tail=true;}
        }
        if(outputHash!=expectedOutput||stateHash(dsp)!=expectedState){std::cerr<<label<<" output "<<std::hex<<outputHash<<" expected "<<expectedOutput<<" state "<<stateHash(dsp)<<" expected "<<expectedState<<'\n';throw std::runtime_error("Native DSP differs from isolated scalar reference");}
        require(tail,"Original effect did not continue after input silence");
    }
    unsigned presets=0;
    for(const auto& entry:std::filesystem::directory_iterator(root/"data/original_audio/dsp"))if(entry.path().extension()==".idsp"){
        const auto bank=loadOriginalAudioDspBank(root,entry.path().stem().string());
        for(unsigned i=0;i<bank.presets.size();++i){OriginalAudioDsp dsp;dsp.configure(originalAudioDspProgram(bank,i,2,true));++presets;}
    }
    focused();std::cout<<"DSP: "<<count<<" distinct source presets x131072 samples and complete final state; "<<presets<<" preset validation; exhaustive float codec; delay, shifter, routing and reset checks passed.\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
