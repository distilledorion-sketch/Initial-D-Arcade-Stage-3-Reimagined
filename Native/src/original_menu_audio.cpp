#include "original_menu_audio.h"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace idas3 {
namespace {
struct Reader {
    std::span<const std::uint8_t> b;
    void check(std::size_t p,std::size_t n)const{if(p>b.size()||n>b.size()-p)throw std::runtime_error("DTPK record outside bank");}
    std::uint8_t u8(std::size_t p)const{check(p,1);return b[p];}
    std::uint16_t u16(std::size_t p)const{return u8(p)|std::uint16_t(u8(p+1))<<8;}
    std::uint32_t u32(std::size_t p)const{return u16(p)|std::uint32_t(u16(p+2))<<16;}
};
}
OriginalMenuSound decodeOriginalMenuSound(std::span<const std::uint8_t> bytes,std::uint32_t command){
    Reader r{bytes};r.check(0,64);
    if(r.u32(0)!=0x4b505444u||r.u32(8)!=bytes.size())throw std::runtime_error("Invalid DTPK header/length");
    const auto sequence=r.u32(0x2c),playbacks=r.u32(0x30),samples=r.u32(0x3c);
    const auto groups=r.u32(sequence)+1u;
    if(!sequence||!playbacks||!samples||groups>256)throw std::runtime_error("Unsupported DTPK tables");
    std::size_t group=0;
    for(std::uint32_t i=0;i<groups;++i){
        const auto entry=sequence+4u+4u*i;
        if(r.u8(entry+3)==(command&255u)&&r.u8(entry+2)==((command>>8)&255u)){
            group=std::size_t(sequence)+r.u16(entry);break;
        }
    }
    if(!group)throw std::runtime_error("DTPK command group missing");
    const auto track=command>>16;
    if(track>r.u32(group))throw std::runtime_error("DTPK command track missing");
    const std::size_t composition=sequence+std::size_t(r.u32(group+4+4*track));
    r.check(composition,6);
    if(r.u8(composition)!=0xc0||r.u8(composition+1)!=0xdf||
       (r.u8(composition+4)&0xf0)!=0x80||r.u8(composition+5)!=0xff)
        throw std::runtime_error("Unsupported compound/non-SFX DTPK sequence");
    auto out=decodeOriginalSfxPlayback(bytes,r.u8(composition+2)|((r.u8(composition+4)&15u)<<7));
    out.command=command;out.sequenceVolume=r.u8(composition+3);return out;
}
OriginalMenuSound decodeOriginalSfxPlayback(std::span<const std::uint8_t> bytes,unsigned playbackId){
    Reader r{bytes};r.check(0,64);
    if(r.u32(0)!=0x4b505444u||r.u32(8)!=bytes.size())throw std::runtime_error("Invalid DTPK header/length");
    const auto playbacks=r.u32(0x30),samples=r.u32(0x3c);
    if(!playbacks||!samples||playbackId>65535)throw std::runtime_error("Unsupported DTPK playback tables");
    OriginalMenuSound out;out.playbackId=std::uint16_t(playbackId);
    if(out.playbackId>r.u32(playbacks+0x10))throw std::runtime_error("DTPK playback missing");
    const std::size_t record=playbacks+0x50u+64u*out.playbackId;r.check(record,64);
    std::copy_n(bytes.begin()+record,64,out.playback.begin());
    if(out.playback[0]!=(out.playbackId&255u))throw std::runtime_error("DTPK playback ID mismatch");
    out.sampleId=out.playback[2];
    if(out.sampleId>r.u32(samples))throw std::runtime_error("DTPK sample missing");
    const auto rate=(out.playback[10]<<8)|out.playback[11];
    switch(rate){case 0:out.clip.sampleRate=44100;break;case 0xf400:out.clip.sampleRate=22050;break;case 0xe800:out.clip.sampleRate=11025;break;default:throw std::runtime_error("Unverified DTPK rate word");}
    const std::size_t sample=samples+4u+16u*out.sampleId;
    const auto location=r.u32(sample),channelFlags=r.u32(sample+8),channelBytes=r.u32(sample+12);
    if((location&0xff800000u)||channelFlags!=0||channelBytes%2)
        throw std::runtime_error("Unsupported DTPK sample format; menu requires PCM16 mono");
    r.check(location,channelBytes);out.clip.channels=1;
    // Loop points remain in the preserved bank. These menu one-shots do not loop.
    out.clip.samples.resize(channelBytes/2);
    for(std::size_t i=0;i<out.clip.samples.size();++i)
        out.clip.samples[i]=std::bit_cast<std::int16_t>(r.u16(location+2*i));
    return out;
}
OriginalMenuSound loadOriginalMenuSound(const std::filesystem::path& root,OriginalMenuCue cue){
    const auto path=root/"data/original_audio/menu/PACK21.dtpk";
    std::ifstream in(path,std::ios::binary);
    if(!in)throw std::runtime_error("Missing original menu DTPK bank");
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
    return decodeOriginalMenuSound(bytes,static_cast<std::uint32_t>(cue));
}
OriginalMenuSound loadOriginalTuningSound(const std::filesystem::path& root,unsigned cue){
    switch(cue){case 2:case 5:case 6:case 11:case 12:case 13:break;default:throw std::out_of_range("Original tuning cue");}
    //31EB68 uses12-byte rows;141F80->1435C0 submits the first word.
    return loadOriginalMenuSound(root,OriginalMenuCue(0x000001a9u|(cue<<16)));
}
OriginalMenuSound loadOriginalRaceSound(const std::filesystem::path& root,unsigned bank,unsigned cue){
    if((bank!=2&&bank!=4&&bank!=5)||cue>=(bank==2?8u:bank==4?6u:3u))throw std::out_of_range("Original race sound cue");
    const auto path=root/"data/original_audio/race"/(bank==2?"PACK22.dtpk":bank==4?"PACK24.dtpk":"PACK25.dtpk");
    std::ifstream in(path,std::ios::binary);
    if(!in)throw std::runtime_error("Missing original race DTPK bank");
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
    return decodeOriginalMenuSound(bytes,0xa9u|(bank<<8)|(cue<<16));
}
}
