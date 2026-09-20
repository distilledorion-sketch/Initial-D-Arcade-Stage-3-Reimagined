#include "original_music_control.h"
#include <algorithm>
#include <bit>
#include <stdexcept>
namespace idas3 {
namespace {
constexpr std::array<std::uint16_t,128> fadeSteps{41943,27962,20971,16777,13981,11983,10485,9320,8388,7626,6990,6452,5991,5592,5242,4934,4660,4415,4194,3994,3813,3647,3495,3355,3226,3106,2995,2892,2796,2706,2621,2542,2467,2396,2330,2267,2207,2150,2097,2046,1997,1950,1906,1864,1823,1784,1747,1711,1677,1644,1613,1582,1553,1525,1497,1471,1446,1421,1398,1375,1353,1331,1310,1290,1271,1252,1233,1215,1198,1181,1165,1149,1133,1118,1103,1089,1075,1061,1048,1035,1023,1010,998,986,975,964,953,942,932,921,911,902,892,883,873,864,855,847,838,830,822,814,806,798,791,783,776,769,762,755,748,742,735,729,723,716,710,704,699,693,687,682,676,671,665,660,655,650};
constexpr std::array<std::uint8_t,128> fadeLevels{0,11,15,19,22,25,27,29,31,33,35,37,39,40,42,43,45,46,47,49,50,51,52,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,73,74,75,76,77,78,78,79,80,81,82,82,83,84,85,85,86,87,88,88,89,90,90,91,92,92,93,94,94,95,96,96,97,98,98,99,100,100,101,102,102,103,103,104,105,105,106,106,107,108,108,109,109,110,110,111,112,112,113,113,114,114,115,116,116,117,117,118,118,119,119,120,120,121,121,122,122,123,123,124,124,125,125,126,127};
std::uint32_t mulLevel(std::uint32_t value,unsigned level){
    value=(value*(level*2u))>>8;return value?value+1:value;
}
}
std::uint8_t originalMusicTotalLevel(const OriginalMusicVolumeContext& c){
    std::uint32_t n=c.velocityTableValue;
    n=mulLevel(n,c.layerGain8);n=mulLevel(n,c.channelVolume0A);
    n=mulLevel(n,c.channelGain10);n=mulLevel(n,c.master05);
    if(!(c.channelFlags0&1))n=mulLevel(n,c.bankFade06&127);
    n^=255;if(n!=255)n>>=1;return std::uint8_t(n);
}
std::uint8_t originalMusicChannelGain(std::uint8_t level,std::uint8_t offset,
    std::uint8_t alternate,std::uint8_t alternateOffset,std::uint8_t flags){
    if(flags&1){level=alternate;offset=alternateOffset;}
    return std::uint8_t(std::clamp(int(level)+2*int(offset)-128,0,127));
}
std::uint8_t originalMusicFadeLevel(std::uint8_t n){return fadeLevels[n&127];}
std::uint32_t originalMusicManagerCommand(std::uint32_t word,std::uint32_t argument,unsigned slot){
    if(word!=0x4a0&&word!=0xaa0&&word!=0x1200a0)throw std::invalid_argument("Unsupported original music control");
    std::uint32_t packed=word+((slot&15)<<24);
    if(word!=0x1200a0)packed+=(argument&127)<<16;
    return (packed>>24)|((packed>>8)&0xff00)|((packed<<8)&0xff0000)|(packed<<24);
}
bool requestOriginalMusicFade(std::span<OriginalMusicTrackControl> tracks,unsigned bank,std::uint8_t arg){
    for(auto&t:tracks)if(t.flags0&&!(t.header1&128)&&t.bank2==(bank&255)){
        t.flags0|=6;t.increment20=0u-fadeSteps[arg&127];return true;
    }return false;
}
void stopOriginalMusicTracks(std::span<OriginalMusicTrackControl> tracks,unsigned bank){
    if(bank&128)return;
    for(auto&t:tracks)if(t.bank2==(bank&7)&&!(t.header1&128))t.flags0=0;
}
bool originalMusicStopKillsVoice(std::uint8_t f0,std::uint8_t f1,unsigned voiceBank,unsigned command){
    return (f0&0xc0)==0x80 && !(f0&0x14) && !(f1&0x40) && (voiceBank&7)==(command&7);
}
void tickOriginalMusicFade(OriginalMusicTrackControl&t){
    if(!(t.flags0&128)||(t.flags0&0x21)||!(t.flags0&2))return;
    auto value=std::bit_cast<std::int32_t>(t.volume1C+t.increment20);
    t.volume1C=std::uint32_t(std::clamp(value,0,0x7f0000));
}
OriginalMusicControlEvents pollOriginalMusicFade(OriginalMusicTrackControl&t){
    OriginalMusicControlEvents out;
    if(!t.flags0||(t.flags0&0x21)||!(t.flags0&2))return out;
    const auto value=t.volume1C&0x7f0000;if(value==t.lastVolume24)return out;
    t.lastVolume24=value;
    const bool local=(t.header1&0x40)!=0,external=(t.header1&0x20)!=0;
    if(local||external)out.events[out.count++]={0xa01c0000|(value>>8)|t.bank2,local,external};
    if(!value){
        if(local||external)out.events[out.count++]={0xa0001200|t.bank2,local,external};
        t.flags0=0;out.stopRelatedGroup=true;
    }else if(value==0x7f0000)t.flags0&=std::uint8_t(~6);
    return out;
}
}

