#include "original_music_voice.h"
#include "original_music_bank.h"
#include "music_voice_hardware_reference.generated.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace idas3;
namespace hw=music_hardware_reference;
namespace {
// Negating the numerator does not negate the result exactly: the shift that
// takes the accumulator down to a sample floors, so the two trajectories can
// part by a step, and the filter feeds its own rounding remainder back through
// a resonant loop. The registers this cue actually plays stay within 2; the
// synthetic sweep below drives extreme pitches through a mid-note parameter
// change and reaches 173, still under a thousandth of the 20-bit clamp range.
constexpr std::int64_t SIGNED_SLACK=256;
std::uint64_t comparisons=0,frames=0,cases=0;std::string context;
void eq(std::int64_t a,std::int64_t b,const char* field,unsigned frame){++comparisons;if(a!=b)throw std::runtime_error(context+" frame"+std::to_string(frame)+" "+field+": native"+std::to_string(a)+" source"+std::to_string(b));}
// Our low-pass runs at unity DC gain where the reference runs at -1, so a voice
// with the section switched on produces exactly the negated output. The volume
// stage's arithmetic shift rounds toward negative infinity, so negating the
// sample can move a result by one step; nothing else about it may differ.
std::int64_t signedDrift=0;
void eqSigned(std::int64_t a,std::int64_t b,bool filtered,const char* field,unsigned frame){
    ++comparisons;const auto expected=filtered?-b:b;
    const auto slack=filtered?SIGNED_SLACK:0;
    signedDrift=std::max(signedDrift,a>expected?a-expected:expected-a);
    if(a<expected-slack||a>expected+slack)
        throw std::runtime_error(context+" frame"+std::to_string(frame)+" "+field+
            ": native"+std::to_string(a)+" source"+std::to_string(b)+
            (filtered?" (expected the negated filtered value)":""));
}
void configure(hw::ChannelEx& c,hw::ChannelCommonData& r,const OriginalMusicVoiceParameters& p,bool initial){
    const auto oldLfo=std::uint16_t(r.LFORE<<15|r.LFOF<<10|r.PLFOWS<<8|r.PLFOS<<5|r.ALFOWS<<3|r.ALFOS);
    r.FNS=p.pitch&1023;r.OCT=p.pitch>>11;r.AR=p.envelope1&31;r.D1R=(p.envelope1>>6)&31;r.D2R=p.envelope1>>11;
    r.RR=p.envelope2&31;r.DL=(p.envelope2>>5)&31;r.KRS=(p.envelope2>>10)&15;r.LPSLNK=(p.envelope2>>14)&1;
    r.ALFOS=p.lfo&7;r.ALFOWS=(p.lfo>>3)&3;r.PLFOS=(p.lfo>>5)&7;r.PLFOWS=(p.lfo>>8)&3;r.LFOF=(p.lfo>>10)&31;r.LFORE=p.lfo>>15;
    r.TL=p.totalLevel;r.DIPAN=p.pan;r.DISDL=p.directLevel;r.ISEL=p.effectSend&15;r.IMXL=p.effectSend>>4;
    r.Q=p.filter&31;r.LPOFF=(p.filter>>5)&1;r.VOFF=(p.filter>>6)&1;
    r.FLV0=p.filterLevels[0];r.FLV1=p.filterLevels[1];r.FLV2=p.filterLevels[2];r.FLV3=p.filterLevels[3];r.FLV4=p.filterLevels[4];
    r.FAR=(p.filterEnvelope1>>8)&31;r.FD1R=p.filterEnvelope1&31;r.FD2R=(p.filterEnvelope2>>8)&31;r.FRR=p.filterEnvelope2&31;
    c.UpdatePitch();c.UpdateAEG();c.UpdateFEG();c.UpdateAtts();c.UpdateStreamStep();if(initial||oldLfo!=p.lfo)c.UpdateLFO(false);
}
void run(const OriginalMusicSample& sample,OriginalMusicVoiceParameters p,unsigned duration,unsigned releaseAt,bool change=false){
    OriginalMusicVoice native;native.start({sample.pcm,sample.loopStart,sample.loopEnd,sample.looping},p);
    hw::ChannelCommonData r{};hw::ChannelEx c{};c.ccd=&r;c.quiet=true;r.SA_hi=1;r.SA_low=0;r.PCMS=hw::PCM16;r.LPCTL=sample.looping;r.LSA=sample.loopStart;r.LEA=sample.loopEnd;
    for(unsigned i=0;i<sample.pcm.size();++i){const auto value=std::bit_cast<std::uint16_t>(sample.pcm[i]);hw::aica_ram[0x10000+2*i]=std::uint8_t(value);hw::aica_ram[0x10001+2*i]=std::uint8_t(value>>8);}
    configure(c,r,p,true);c.UpdateSA();c.UpdateLoop();c.disable();c.KEY_ON();++cases;
    for(unsigned frame=0;frame<duration;++frame){
        if(frame==releaseAt){native.release();c.KEY_OFF();}
        if(change&&frame==duration/3){p.lfo=0xc6b0;p.pitch^=0x200;native.configure(p);configure(c,r,p,false);}
        auto actual=native.renderFrame();int left=0,right=0,dsp=0;c.Step(left,right,dsp);++frames;
        const bool filtered=c.FEG.active;
        eqSigned(actual.dry[0],left,filtered,"left",frame);eqSigned(actual.dry[1],right,filtered,"right",frame);
        eqSigned(actual.effects[p.effectSend&15],dsp,filtered,"DSP",frame);
        eq(native.active(),c.enabled,"active",frame);
        if(c.enabled){eq(native.envelope(),c.AEG.val,"AEG",frame);eq(native.phase(),c.AEG.state,"AEG phase",frame);eq(native.filterValue(),c.FEG.value,"FEG",frame);eq(native.lfoState(),c.lfo.state,"LFO state",frame);eq(native.position(),c.CA,"sample address",frame);eq(native.fraction(),c.step.fp,"fraction",frame);eq(native.modulatedIncrement(),(c.update_rate*c.lfo.plfo_step.full)>>10,"pitch LFO",frame);}
    }
}
OriginalMusicVoiceParameters parameters(const OriginalMusicLayer& l){
    OriginalMusicVoiceParameters p;p.envelope1=l.envelope1;p.envelope2=l.envelope2;p.lfo=l.lfo;p.pan=16;p.directLevel=15;p.effectSend=0xf3;
    const auto u16=[&](unsigned at){return unsigned(l.rawBytes[at])|(unsigned(l.rawBytes[at+1])<<8);};
    if(u16(16)&0x7ff){p.filter=std::uint8_t(l.rawBytes[25]>>3);for(unsigned i=0;i<5;++i)p.filterLevels[i]=std::uint16_t((u16(16+2*i)&0x7ff)<<2);p.filterEnvelope1=std::uint16_t(((l.rawBytes[17]<<5)|(l.rawBytes[19]>>3))&0x1f1f);p.filterEnvelope2=std::uint16_t(((l.rawBytes[21]<<5)|(l.rawBytes[23]>>3))&0x1f1f);}
    return p;
}
}
int main(int argc,char** argv){try{
    hw::staticinitialise();const auto root=std::filesystem::path(argc>1?argv[1]:".")/"data/original_audio/selection";
    for(const auto name:{"TYPE","SELECT"}){const auto bank=loadOriginalMusicBank(root/(std::string(name)+".dtpk"));unsigned index=0;
        for(const auto& program:bank.programs)for(const auto& group:program.groups)for(const auto& layer:group.layers){
            auto p=parameters(layer);const auto& s=layer.sampleId<256?bank.builtinSamples.at(0):bank.samples.at(layer.sampleId-256);
            for(unsigned pitch:{0u,0x7800u,0x0b55u}){context=std::string(name)+" layer"+std::to_string(index)+" pitch"+std::to_string(pitch);p.pitch=std::uint16_t(pitch);run(s,p,16000,11000,true);}++index;
        }
    }
    OriginalMusicSample synthetic;synthetic.loopStart=7;synthetic.loopEnd=127;synthetic.looping=true;for(unsigned i=0;i<130;++i)synthetic.pcm.push_back(std::int16_t((i*1557+573)&65535));
    for(unsigned rate=0;rate<32;++rate)for(unsigned octave:{0u,7u,8u,15u}){
        OriginalMusicVoiceParameters p;p.pitch=std::uint16_t((octave<<11)|0x200);p.envelope1=std::uint16_t(rate|(rate<<6)|(rate<<11));p.envelope2=std::uint16_t(rate|(15<<5)|(7<<10));p.filter=11;p.filterLevels={1024,7999,4000,500,0};p.filterEnvelope1=p.filterEnvelope2=std::uint16_t(rate|(rate<<8));
        context="rates"+std::to_string(rate)+" octave"+std::to_string(octave);run(synthetic,p,4096,2048);
    }
    for(unsigned shape=0;shape<3;++shape)for(unsigned depth=0;depth<8;++depth)for(unsigned frequency:{0u,11u,31u}){
        OriginalMusicVoiceParameters p;p.envelope1=31;p.envelope2=31;p.lfo=std::uint16_t(0x8000|(frequency<<10)|(shape<<8)|(depth<<5)|(shape<<3)|depth);context="LFO"+std::to_string(p.lfo);run(synthetic,p,5000,4096);
    }
    {
        OriginalMusicVoiceParameters p;p.envelope1=std::uint16_t(31|(31<<6)|(31<<11));p.envelope2=std::uint16_t(1|(15<<10));p.filter=11;p.filterLevels={1024,7999,4000,500,0};p.filterEnvelope1=p.filterEnvelope2=0x0808;
        context="keyoff during natural amplitude release";run(synthetic,p,10000,5000);
        p.envelope1=9;p.envelope2=std::uint16_t(31|0x4000);p.lfo=0xc6b0;
        context="loop-linked amplitude decay";run(synthetic,p,10000,5000);
    }
    std::cout<<"Music voice hardware reference passed:"<<cases<<" cases,"<<frames<<" sample frames,"<<comparisons<<" comparisons, max filtered-sign drift "<<signedDrift<<"\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
