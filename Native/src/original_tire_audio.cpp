#include "original_tire_audio.h"
#include <bit>
#include <cmath>
#include <limits>
namespace idas3::original {
namespace {
std::int32_t volumeWord(float v){
    std::int32_t converted;
    if(!std::isfinite(v)||v>=2147483648.f||v<-2147483648.f)converted=std::numeric_limits<std::int32_t>::min();
    else converted=std::int32_t(v);
    return std::bit_cast<std::int16_t>(std::uint16_t(converted));
}
}
void resetOriginalTireAudio(OriginalTireAudioState& s){s.volume=127;s.kind=s.surface=s.phase=s.frames=0;s.requested=0;}
void requestOriginalTireAudio(OriginalTireAudioState& s,std::uint8_t disabled,
        float speed,std::uint32_t kind,std::uint32_t mask,float strength){
    if(disabled==1||10.f>speed)return;
    s.kind=kind;s.requested=1;s.surface=(mask&2)?4:(mask&1)?2:0;s.strength=strength;
}
std::vector<OriginalTireCommand> stepOriginalTireAudio(OriginalTireAudioState& s,
        std::uint8_t disabled,std::uint32_t& seed){
    std::vector<OriginalTireCommand> commands;
    const auto stop=[&]{commands.push_back({OriginalTireCommandType::Stop,0});};
    const auto volume=[&]{commands.push_back({OriginalTireCommandType::Volume,volumeWord(s.volume)});};
    const float threshold=s.surface==4?.3f:.33f;
    if(disabled==1){stop();s.requested=0;s.phase=0;return commands;}
    s.volume=127;
    if(threshold>s.strength){
        const float ratio=s.strength/threshold;
        if(threshold*.08f>ratio){stop();return commands;}
        s.volume=std::fma(std::sqrt(ratio),.4f,.6f)*127.f;
        s.requested=0;s.phase=0;
    }else if(.5f>s.strength){
        float ratio=(s.strength-threshold)/(.5f-threshold);
        if(0.f>ratio)ratio=0;
        s.volume=std::fma(ratio,.08f,.92f)*127.f;
    }
    switch(s.phase){
    case 0:
        if(s.requested==1){
            s.phase=1;seed=seed*1103515245u+12345u;
            const auto choice=(seed>>16)&1u;
            commands.push_back({OriginalTireCommandType::Play,std::bit_cast<std::int32_t>(s.surface+choice)});
            volume();s.frames=0;
        }
        break;
    case 1:
        volume();
        if(s.requested==0){s.frames=0;s.phase=2;}
        else if(std::bit_cast<std::int32_t>(++s.frames)>198)s.phase=0;
        break;
    case 2:
        if(s.requested==1){if(std::bit_cast<std::int32_t>(s.frames)>0)stop();s.phase=0;}
        else{
            ++s.frames;s.volume-=std::bit_cast<float>(0x40877777u);volume();
            if(0.f>s.volume){s.volume=0;stop();s.phase=0;}
        }
        break;
    default:break;
    }
    s.requested=0;
    return commands;
}
}
