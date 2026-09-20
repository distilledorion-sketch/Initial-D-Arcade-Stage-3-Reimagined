#include "original_audio_dsp_bank.h"
#include "original_audio_dsp_control.h"
#include <bit>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
using namespace idas3;
namespace {
unsigned checks=0;
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
std::vector<std::uint8_t> read(const std::filesystem::path&p){
    std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("Missing DSP test fixture");
    return {(std::istreambuf_iterator<char>(f)),{}};
}
unsigned word(const std::vector<std::uint8_t>&b,std::size_t p){
    return unsigned(b.at(p))|(unsigned(b.at(p+1))<<8)|(unsigned(b.at(p+2))<<16)|(unsigned(b.at(p+3))<<24);
}
template<class F>void rejects(F&&fn){bool caught=false;try{fn();}catch(const std::exception&){caught=true;}check(caught,"Malformed DSP input accepted");}
}
int main(int argc,char**argv){try{
    if(argc!=2)throw std::runtime_error("Provide project root");const std::filesystem::path root=argv[1];
    unsigned banks=0,presets=0,scenes=0;
    for(const auto&entry:std::filesystem::directory_iterator(root/"data/original_audio/dsp")){
        if(entry.path().extension()!=".idsp")continue;
        const auto bank=loadOriginalAudioDspBank(root,entry.path().stem().string());++banks;scenes+=unsigned(bank.scenes.size());
        check(bank.declaredRingCode<=3,"Bank ring code");
        for(unsigned i=0;i<bank.presets.size();++i){
            ++presets;const auto p=originalAudioDspProgram(bank,i,2,true);
            const auto b=read(root/"verification/original-audio-dsp"/(entry.path().stem().string()+".captured"));
            check(b.size()==32+0xc00+64&&std::string(b.begin(),b.begin()+8)=="IDSR0001","Source capture identity");
            check(word(b,8)==2&&word(b,12)==0x4fe0&&word(b,16)==p.ringBaseBytes&&word(b,20)==p.ringLengthWords,"Source ring register");
            check(word(b,24)==bank.bankId&&word(b,28)==i,"Source selection identity");
            check(p.initialMemoryWords.empty(),"Source default memory must use6000 fill");
            for(unsigned j=0;j<128;++j)check(std::bit_cast<std::uint16_t>(p.coefficients[j])==word(b,32+j*4),"Source coefficient mismatch");
            for(unsigned j=0;j<64;++j)check(p.memoryAddresses[j]==word(b,32+512+j*4),"Source memory-address mismatch");
            for(unsigned j=0;j<128;++j)for(unsigned k=0;k<4;++k)check(p.instructions[j][k]==word(b,32+1024+j*16+k*4),"Source instruction mismatch");
            for(unsigned j=0;j<16;++j)check(p.effectRoutes[j]==word(b,32+0xc00+j*4),"Source effect route mismatch");
        }
    }
    check(banks==60&&presets==59&&scenes==60,"Incomplete source bank inventory");
    const auto type=loadOriginalAudioDspBank(root,"TYPE"),select=loadOriginalAudioDspBank(root,"SELECT"),empty=loadOriginalAudioDspBank(root,"PACK10");
    check(type.bankId==1&&select.bankId==2&&type.declaredRingCode==2&&select.declaredRingCode==3,"Distinct source bank ring declarations");
    check(originalAudioDspProgram(select,0,2,true).ringLengthWords==32768,"Selected bank must not change latched ring");
    check(type.scenes.at(0).bankId==1&&select.scenes.at(0).bankId==2&&type.scenes[0].preset==0&&select.scenes[0].preset==0,"Source selection scenes");
    check(empty.presets.empty()&&empty.scenes.size()==1,"Preserve registered bank without a DSP table");
    rejects([&]{originalAudioDspProgram(empty,0,2,true);});
    for(unsigned code=0;code<4;++code)for(bool memory8:{false,true}){
        const auto p=originalAudioDspProgram(type,0,code,memory8);
        check(p.ringLengthWords==(8192u<<code)&&p.ringBaseBytes==(memory8?0x800000u:0x200000u)-2*p.ringLengthWords,"Source18E4 ring placement");
    }
    rejects([&]{originalAudioDspProgram(type,0,4,true);});
    rejects([&]{loadOriginalAudioDspBank(root,"../TYPE");});
    auto b=read(root/"data/original_audio/dsp/TYPE.idsp");
    for(std::size_t n:{0u,7u,23u,24u,100u})rejects([&]{decodeOriginalAudioDspBank(std::span(b).first(n));});
    for(std::size_t offset:{0u,11u,15u,19u,23u,28u+36u+2u,28u+36u+192u*4u}){
        auto bad=b;bad[offset]=255;rejects([&]{decodeOriginalAudioDspBank(bad);});
    }
    b.push_back(0);rejects([&]{decodeOriginalAudioDspBank(b);});
    std::ifstream fixture(root/"verification/original-audio-dsp/control-reference.txt");check(bool(fixture),"Missing source control fixture");
    std::array<OriginalAudioDspRegistration,4> registry{};for(auto&r:registry)fixture>>r.bankId;
    for(unsigned i=0;i<4;++i)registry[i].presetCount=i==3?0:1;
    unsigned oldBank,oldPreset,requestedBank,requestedPreset,mask,outcome,newBank,newPreset,cases=0;
    while(fixture>>oldBank>>oldPreset>>requestedBank>>requestedPreset>>mask>>outcome>>newBank>>newPreset){
        ++cases;for(unsigned i=0;i<4;++i)registry[i].registered=(mask&(1u<<i))!=0;
        OriginalAudioDspControl state{oldBank,oldPreset};const auto result=selectOriginalAudioDsp(state,requestedBank,requestedPreset,registry);
        check(unsigned(result.operation)==outcome&&state.selectedBank18==newBank&&state.selectedPreset19==newPreset,"Original control predicate mismatch");
        check(result.clearVoiceSends==(outcome==2),"Source send-reset boundary mismatch");
        if(outcome==2)check(registry[result.registryIndex].bankId==requestedBank&&result.preset==requestedPreset,"Original registry lookup mismatch");
    }
    check(cases==1500,"Incomplete source control fixture");
    std::ifstream routeFixture(root/"verification/original-audio-dsp/return-reference.txt");check(bool(routeFixture),"Missing source return-route fixture");
    unsigned mono,index,operation,argument,oldRoute,newRoute,routeCases=0;
    while(routeFixture>>mono>>index>>operation>>argument>>oldRoute>>newRoute){
        ++routeCases;const auto result=operation==1?originalAudioDspReturnLevel(std::uint16_t(oldRoute),argument):originalAudioDspReturnPan(std::uint16_t(oldRoute),argument);
        check(result==newRoute,"Original A4 return route mismatch");
    }
    check(routeCases==16384,"Incomplete return-route fixture");
    OriginalAudioDspControl state{1,0};setOriginalAudioDspBank(state,22);
    check(state.selectedBank18==22&&state.selectedPreset19==0,"A470 must not invalidate preset cache");
    check(selectOriginalAudioDsp(state,22,0,{}).operation==OriginalAudioDspOperation::None,"A4 bank/preset source cache quirk");
    std::array<OriginalAudioDspRegistration,2> duplicate{{{1,0,true},{1,1,true}}};state={0,255};
    check(selectOriginalAudioDsp(state,1,0,duplicate).operation==OriginalAudioDspOperation::None,"First intrinsic-ID match is authoritative");
    rejects([&]{setOriginalAudioDspBank(state,256);});rejects([&]{selectOriginalAudioDsp(state,1,256,{});});
    std::cout<<"Original DSP bank/control passed: "<<banks<<" banks, "<<presets<<" presets, "<<cases<<" source control cases, "<<routeCases<<" return-route cases, "<<checks<<" checks\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
