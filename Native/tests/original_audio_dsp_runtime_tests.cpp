#include "original_audio_dsp_runtime.h"
#include "original_audio_dsp_test_input.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace idas3;
namespace {
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
std::uint64_t stateHash(const OriginalAudioDsp& p){
    auto hash=originalDspHashBasis;
    for(auto value:p.temporary())originalDspTestHash(hash,std::uint32_t(value));
    for(auto value:p.memoryRegisters())originalDspTestHash(hash,std::uint32_t(value));
    for(auto value:p.memory())originalDspTestHash(hash,value);
    originalDspTestHash(hash,p.decrementCounter());return hash;
}
bool programIs(const OriginalAudioDspRuntime& runtime,const std::filesystem::path& root,std::string_view name){
    const auto source=originalAudioDspProgram(loadOriginalAudioDspBank(root,name),0,runtime.diagnostics().ringCode,true);
    const auto& actual=runtime.processor().program();
    return actual.instructions==source.instructions&&actual.coefficients==source.coefficients&&
        actual.memoryAddresses==source.memoryAddresses&&actual.effectRoutes==source.effectRoutes;
}
void feed(OriginalAudioDspRuntime& runtime,unsigned frames){
    std::uint32_t random=0x31415926;
    for(unsigned frame=0;frame<frames;++frame)runtime.render(originalDspTestInput(random,frame));
}
}
int main(int argc,char** argv){try{
    require(argc==2,"Supply project root");const std::filesystem::path root=argv[1];
    unsigned sendClears=0;OriginalAudioDspRuntime runtime(root,[&]{++sendClears;});
    runtime.registerBank(0,"PACK20");runtime.registerBank(1,"PACK21");runtime.registerBank(2,"TYPE");
    auto d=runtime.diagnostics();require(d.ringLatched&&d.ringCode==2&&!d.activeProgram&&d.loadCount==0&&sendClears==0,"Bank registration must not select a preset");
    require(runtime.selectScene(2,0)==OriginalAudioDspOperation::Load&&programIs(runtime,root,"TYPE"),"Original TYPE A019 scene selection");
    require(sendClears==1&&runtime.diagnostics().activeBank==1,"Load clears sends and records active intrinsic bank");
    feed(runtime,18000);const auto snapshot=stateHash(runtime.processor());const auto frames=runtime.diagnostics().frames;
    require(runtime.diagnostics().wetEnergy>0&&runtime.diagnostics().inputPeak==0x30000,"Effects and input diagnostics");
    require(runtime.selectScene(2,0)==OriginalAudioDspOperation::None&&stateHash(runtime.processor())==snapshot&&sendClears==1,"Repeated preset keeps phase and tail");
    require(runtime.select(1,255)==OriginalAudioDspOperation::None&&stateHash(runtime.processor())==snapshot,"Preset FF no-op");
    require(runtime.select(1,99)==OriginalAudioDspOperation::None&&runtime.select(254,0)==OriginalAudioDspOperation::None&&stateHash(runtime.processor())==snapshot,"Missing preset or bank no-op");
    runtime.unregisterBank(2);require(stateHash(runtime.processor())==snapshot&&programIs(runtime,root,"TYPE"),"Unload preserves active program and ring");
    require(runtime.select(1,0)==OriginalAudioDspOperation::None&&sendClears==1,"Unloaded cached identity still short-circuits lookup");
    const auto energy=runtime.diagnostics().wetEnergy;std::array<std::int32_t,16> silence{};
    for(unsigned i=0;i<8000;++i)runtime.render(silence);
    require(runtime.diagnostics().wetEnergy>energy&&runtime.diagnostics().frames==frames+8000,"Unloaded program continues its delay tails");
    runtime.registerBank(2,"SELECT");d=runtime.diagnostics();require(d.ringCode==2&&d.activeBank==1&&d.loadCount==1,"Later SELECT ring declaration does not replace first latch");
    require(runtime.selectScene(2,0)==OriginalAudioDspOperation::Load&&programIs(runtime,root,"SELECT"),"Original SELECT scene loads its program");
    require(runtime.processor().program().ringLengthWords==32768&&runtime.processor().program().ringBaseBytes==0x7f0000,"Boot ring dimensions");
    require(sendClears==2&&runtime.diagnostics().activeBank==2,"Second load clears sends once");
    require(std::all_of(runtime.processor().memory().begin(),runtime.processor().memory().end(),[](auto v){return v==0x6000;}),"Preset replacement fills the ring with source floating silence");
    // A470 changes only the cache bank. A40000 then hits that cache although
    // the actual engine program differs from the selection program still running.
    runtime.registerBank(6,"PACK4");feed(runtime,18000);const auto selectedState=stateHash(runtime.processor());
    runtime.setBank(22);d=runtime.diagnostics();require(d.cacheBank==22&&d.cachePreset==0&&d.activeBank==2,"Engine bank command must not relabel the active program");
    require(runtime.select(22,0)==OriginalAudioDspOperation::None&&programIs(runtime,root,"SELECT")&&stateHash(runtime.processor())==selectedState&&sendClears==2,"Original A4 cache quirk preserves selection effect");
    // Clear differs from Load: it mutes registers and arithmetic state while
    // preserving ring contents, the decrement phase and all physical voice sends.
    const auto ring=runtime.processor().memory();const auto phase=runtime.processor().decrementCounter();
    require(runtime.select(22,127)==OriginalAudioDspOperation::Clear,"Preset 7F clear");d=runtime.diagnostics();
    require(!d.activeProgram&&d.cacheBank==22&&d.cachePreset==255&&d.clearCount==1&&d.loadCount==2&&sendClears==2,"Clear cache and send semantics");
    require(runtime.processor().memory()==ring&&runtime.processor().decrementCounter()==phase,"Clear must retain ring and phase");
    require(std::all_of(runtime.processor().temporary().begin(),runtime.processor().temporary().end(),[](auto v){return v==0;})&&
        std::all_of(runtime.processor().memoryRegisters().begin(),runtime.processor().memoryRegisters().end(),[](auto v){return v==0;}),"Clear zeros TEMP and MEMS");
    require(std::all_of(runtime.processor().program().effectRoutes.begin(),runtime.processor().program().effectRoutes.end(),[](auto v){return v==0x10;}),"Clear source route words");
    const auto cleared=runtime.render(silence);require(cleared.wet==std::array<std::int32_t,2>{}&&cleared.effects==std::array<std::int16_t,16>{},"Clear removes effect output");
    require(runtime.select(22,0)==OriginalAudioDspOperation::Load&&programIs(runtime,root,"PACK4")&&sendClears==3,"After invalidation engine preset can load");
    feed(runtime,18000);const auto routingState=stateHash(runtime.processor());const auto routingDiagnostics=runtime.diagnostics();
    for(unsigned output:{0u,1u,3u}){runtime.setReturnLevel(output,output==3?5:10);runtime.setReturnPan(output,64);}
    require(runtime.processor().program().effectRoutes[0]==0xa10&&runtime.processor().program().effectRoutes[1]==0xa10&&
        runtime.processor().program().effectRoutes[3]==0x510,"Original global engine return level and center pan");
    require(stateHash(runtime.processor())==routingState&&runtime.diagnostics().frames==routingDiagnostics.frames&&
        runtime.diagnostics().loadCount==routingDiagnostics.loadCount&&runtime.diagnostics().cacheBank==routingDiagnostics.cacheBank&&
        runtime.diagnostics().cachePreset==routingDiagnostics.cachePreset&&sendClears==3,"Route commands preserve arithmetic/ring/cache and sends");
    runtime.setReturnLevel(0,0xf7);require(runtime.processor().program().effectRoutes[0]==0x710,"Return level masks argument to four bits");
    runtime.setReturnPan(0,0xff);require(runtime.processor().program().effectRoutes[0]==0x70f,"Return pan uses source table after seven-bit mask");
    require(runtime.select(22,0)==OriginalAudioDspOperation::None&&runtime.processor().program().effectRoutes[0]==0x70f,"Cached preset preserves current route mutations");
    for(unsigned slot=0;slot<8;++slot)runtime.unregisterBank(slot);
    runtime.registerBank(2,"SELECT");require(runtime.diagnostics().ringCode==2&&runtime.diagnostics().activeBank==22,"Unregistering every slot preserves latch and active program");
    // The initial latch follows first registration, not lowest occupied slot.
    OriginalAudioDspRuntime first(root);first.registerBank(7,"SELECT");first.registerBank(0,"PACK20");
    require(first.diagnostics().ringCode==3&&!first.diagnostics().activeProgram,"First registration ring latch");
    first.selectScene(7,0);require(first.processor().program().ringLengthWords==65536,"SELECT-first ring size");
    // Two authored banks share intrinsic bank1. Original lookup chooses the
    // lowest registered slot, regardless of which slot requested the scene.
    OriginalAudioDspRuntime ordered(root);ordered.registerBank(0,"PACK20");ordered.registerBank(5,"TYPE");ordered.registerBank(1,"RESULT");
    require(ordered.selectScene(5,0)==OriginalAudioDspOperation::Load&&programIs(ordered,root,"RESULT"),"Ordered registration lookup for duplicate intrinsic IDs");
    require(ordered.selectScene(7,0)==OriginalAudioDspOperation::None&&ordered.selectScene(5,255)==OriginalAudioDspOperation::None,"Absent or invalid source scene no-op");
    ordered.unregisterBank(1);ordered.select(1,127);require(ordered.selectScene(5,0)==OriginalAudioDspOperation::Load&&programIs(ordered,root,"TYPE"),"Remaining matching registry entry after clear");
    OriginalAudioDspRuntime twoMb(root,{},false);twoMb.registerBank(0,"PACK20");twoMb.selectScene(0,0);
    require(twoMb.processor().program().ringBaseBytes==0x1f0000,"Explicit source 2MiB configuration");
    std::cout<<"Original DSP runtime: authored TYPE/SELECT scenes, first-bank latch, ordered registry, A4 cache quirk, send-clear callback, preserved unload tails and source Clear passed.\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
