#include "original_audio_dsp_control.h"
#include <stdexcept>
namespace idas3 {
std::uint16_t originalAudioDspReturnLevel(std::uint16_t route,unsigned argument){
    if(argument>255)throw std::invalid_argument("Original DSP return level");
    return std::uint16_t((route&255)|((argument&15)<<8));
}
std::uint16_t originalAudioDspReturnPan(std::uint16_t route,unsigned argument){
    if(argument>255)throw std::invalid_argument("Original DSP return pan");
    static constexpr unsigned char table[32]={31,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    return std::uint16_t((route&0xff00)|table[(argument&127)>>2]);
}
void setOriginalAudioDspBank(OriginalAudioDspControl&s,unsigned bank){
    if(bank>255)throw std::invalid_argument("Original DSP bank selector");s.selectedBank18=bank;
}
OriginalAudioDspSelection selectOriginalAudioDsp(OriginalAudioDspControl&s,unsigned bank,unsigned preset,
    std::span<const OriginalAudioDspRegistration> registry){
    if(bank>255||preset>255||s.selectedBank18>255||s.selectedPreset19>255)throw std::invalid_argument("Original DSP selection");
    if(preset==255)return {};
    if(preset==127){s.selectedPreset19=255;return {OriginalAudioDspOperation::Clear};}
    if(bank==s.selectedBank18&&preset==s.selectedPreset19)return {};
    for(unsigned i=0;i<registry.size();++i)if(registry[i].registered&&registry[i].bankId==bank){
        if(!registry[i].presetCount||preset>=registry[i].presetCount)return {};
        s.selectedBank18=bank;s.selectedPreset19=preset;return {OriginalAudioDspOperation::Load,i,preset,true};
    }
    return {};
}
}
