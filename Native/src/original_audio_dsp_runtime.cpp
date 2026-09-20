#include "original_audio_dsp_runtime.h"
#include <algorithm>
#include <stdexcept>
#include <utility>
namespace idas3 {
OriginalAudioDspRuntime::OriginalAudioDspRuntime(const std::filesystem::path& root,
    ClearVoiceSends clearVoiceSends,bool memory8Mb){configure(root,std::move(clearVoiceSends),memory8Mb);}
void OriginalAudioDspRuntime::configure(const std::filesystem::path& root,
    ClearVoiceSends clearVoiceSends,bool memory8Mb){
    root_=root;clearVoiceSends_=std::move(clearVoiceSends);memory8Mb_=memory8Mb;
    slots_={};control_={};processor_=OriginalAudioDsp{};diagnostics_={};configured_=true;
}
const OriginalAudioDspBank* OriginalAudioDspRuntime::registeredBank(unsigned slot)const{
    if(slot>=slots_.size())throw std::out_of_range("Original DSP registry slot");
    const auto& entry=slots_[slot].bank;return entry?&*entry:nullptr;
}
void OriginalAudioDspRuntime::registerBank(unsigned slot,std::string_view name){
    if(!configured_)throw std::logic_error("Original DSP runtime is not configured");
    if(slot>=slots_.size())throw std::out_of_range("Original DSP registry slot");
    auto bank=loadOriginalAudioDspBank(root_,name);
    if(!diagnostics_.ringLatched){
        diagnostics_.ringLatched=true;diagnostics_.ringCode=bank.declaredRingCode;
        OriginalAudioDspProgram silent;silent.ringLengthWords=8192u<<diagnostics_.ringCode;
        silent.ringBaseBytes=(memory8Mb_?0x800000u:0x200000u)-2*silent.ringLengthWords;
        silent.effectRoutes.fill(0x10);processor_.configure(silent);
    }
    slots_[slot]={std::string(name),std::move(bank)};
}
void OriginalAudioDspRuntime::unregisterBank(unsigned slot){
    if(slot>=slots_.size())throw std::out_of_range("Original DSP registry slot");
    slots_[slot]={}; // Source registry removal leaves loaded registers and cache.
}
OriginalAudioDspOperation OriginalAudioDspRuntime::selectScene(unsigned slot,unsigned scene){
    const auto* bank=registeredBank(slot);
    if(!bank||scene>=bank->scenes.size())return OriginalAudioDspOperation::None;
    const auto& selection=bank->scenes[scene];
    if(!selection.sourceOffset)return OriginalAudioDspOperation::None;
    return select(selection.bankId==255?bank->bankId:selection.bankId,selection.preset);
}
OriginalAudioDspOperation OriginalAudioDspRuntime::select(unsigned bank,unsigned preset){
    if(!configured_)throw std::logic_error("Original DSP runtime is not configured");
    std::array<OriginalAudioDspRegistration,8> registry{};
    for(unsigned i=0;i<slots_.size();++i)if(const auto& b=slots_[i].bank)
        registry[i]={b->bankId,unsigned(b->presets.size()),true};
    auto nextControl=control_;const auto selection=selectOriginalAudioDsp(nextControl,bank,preset,registry);
    if(selection.operation==OriginalAudioDspOperation::Load){
        const auto& source=*slots_[selection.registryIndex].bank;
        auto program=originalAudioDspProgram(source,selection.preset,diagnostics_.ringCode,memory8Mb_);
        if(selection.clearVoiceSends&&clearVoiceSends_)clearVoiceSends_();
        processor_.configure(program);
        diagnostics_.activeProgram=true;diagnostics_.activeBank=source.bankId;
        diagnostics_.activePreset=selection.preset;++diagnostics_.loadCount;
    }else if(selection.operation==OriginalAudioDspOperation::Clear){
        processor_.clearProgram();diagnostics_.activeProgram=false;
        diagnostics_.activeBank=diagnostics_.activePreset=255;++diagnostics_.clearCount;
    }
    control_=nextControl;return selection.operation;
}
void OriginalAudioDspRuntime::setBank(unsigned bank){setOriginalAudioDspBank(control_,bank);}
void OriginalAudioDspRuntime::setReturnLevel(unsigned index,unsigned argument){
    const auto current=processor_.program().effectRoutes.at(index);
    processor_.setEffectRoute(index,originalAudioDspReturnLevel(current,argument));
}
void OriginalAudioDspRuntime::setReturnPan(unsigned index,unsigned argument){
    const auto current=processor_.program().effectRoutes.at(index);
    processor_.setEffectRoute(index,originalAudioDspReturnPan(current,argument));
}
OriginalAudioDspFrame OriginalAudioDspRuntime::render(std::span<const std::int32_t,16> input,
    std::array<std::int16_t,2> external){
    for(auto value:input){const auto magnitude=value<0?std::uint64_t(-std::int64_t(value)):std::uint64_t(value);
        diagnostics_.inputPeak=std::max(diagnostics_.inputPeak,magnitude);}
    const auto result=processor_.render(input,external);++diagnostics_.frames;
    for(auto value:result.wet)diagnostics_.wetEnergy+=double(value)*double(value);
    return result;
}
OriginalAudioDspDiagnostics OriginalAudioDspRuntime::diagnostics()const{
    auto result=diagnostics_;result.cacheBank=control_.selectedBank18;result.cachePreset=control_.selectedPreset19;return result;
}
}
