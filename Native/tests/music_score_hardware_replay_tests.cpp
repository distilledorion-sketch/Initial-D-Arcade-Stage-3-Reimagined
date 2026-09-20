// The layer test proves our voice matches the hardware for the parameters the
// bank authors. It never runs the registers the driver actually programmed at
// run time: it invents a pitch, a pan, a direct level and a send. Those are
// exactly the fields a score supplies, so the combinations a cue really plays
// have gone unchecked. This replays every distinct parameter set in the scores
// through both implementations and requires them to agree sample for sample.
#include "original_music_voice.h"
#include "original_music_bank.h"
#include "original_music_sequence.h"
#include "music_voice_hardware_reference.generated.h"
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <map>
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
constexpr std::int64_t SIGNED_SLACK=4;
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
    c.UpdatePitch();c.UpdateAEG();c.UpdateFEG();c.UpdateAtts();c.UpdateStreamStep();
    if(initial||oldLfo!=p.lfo)c.UpdateLFO(false);
}
// The driver's own encoding byte decides how the chip reads the block; forcing
// PCM16 would silently compare two different sounds.
unsigned pcms(const OriginalMusicSample& s){
    if(s.encoding==0)return hw::PCM16;
    if(s.encoding==1)return hw::PCM8;
    if(s.encoding==2)return hw::ADPCM;
    throw std::runtime_error("Unsupported original music sample encoding in replay");
}
void run(const OriginalMusicSample& sample,const std::vector<std::uint8_t>& raw,
         const OriginalMusicVoiceParameters& p,unsigned duration,unsigned releaseAt){
    OriginalMusicVoice native;native.start({sample.pcm,sample.loopStart,sample.loopEnd,sample.looping},p);
    hw::ChannelCommonData r{};hw::ChannelEx c{};c.ccd=&r;c.quiet=true;
    r.SA_hi=1;r.SA_low=0;r.PCMS=pcms(sample);r.LPCTL=sample.looping;r.LSA=sample.loopStart;r.LEA=sample.loopEnd;
    hw::aica_ram.fill(0);
    for(std::size_t i=0;i<raw.size()&&0x10000+i<hw::aica_ram.size();++i)hw::aica_ram[0x10000+i]=raw[i];
    configure(c,r,p,true);c.UpdateSA();c.UpdateLoop();c.disable();c.KEY_ON();++cases;
    for(unsigned frame=0;frame<duration;++frame){
        if(frame==releaseAt){native.release();c.KEY_OFF();}
        auto actual=native.renderFrame();int left=0,right=0,dsp=0;c.Step(left,right,dsp);++frames;
        const bool filtered=c.FEG.active;
        eqSigned(actual.dry[0],left,filtered,"left",frame);eqSigned(actual.dry[1],right,filtered,"right",frame);
        eqSigned(actual.effects[p.effectSend&15],dsp,filtered,"DSP",frame);
        eq(native.active(),c.enabled,"active",frame);
        if(!c.enabled)break;
        eq(native.envelope(),c.AEG.val,"AEG",frame);eq(native.phase(),c.AEG.state,"AEG phase",frame);
        eq(native.filterValue(),c.FEG.value,"FEG",frame);eq(native.lfoState(),c.lfo.state,"LFO state",frame);
        eq(native.position(),c.CA,"sample address",frame);eq(native.fraction(),c.step.fp,"fraction",frame);
    }
}
}
int main(int argc,char** argv){try{
    hw::staticinitialise();
    const auto root=std::filesystem::path(argc>1?argv[1]:".");
    unsigned distinct=0;
    for(unsigned cue:{0u,1u}){
        OriginalMusicSequence sequence;sequence.load(root,cue);
        const auto bank=loadOriginalMusicBank(root/"data/original_audio/selection"/
            (std::string(cue?"SELECT":"TYPE")+".dtpk"));
        // One representative run per distinct register set keeps the replay
        // bounded; repeats of the same parameters cannot diverge differently.
        std::map<std::string,bool> seen;
        for(const auto& event:sequence.events()){
            if(event.kind!=OriginalMusicSequenceEventKind::NoteOn)continue;
            const auto& p=event.parameters;
            std::string key(reinterpret_cast<const char*>(&p),sizeof(p));
            key+=std::to_string(event.sampleId);
            if(!seen.emplace(key,true).second)continue;
            const auto& s=event.sampleId<256?bank.builtinSamples.at(0):bank.samples.at(event.sampleId-256);
            if(s.encoding>2)continue;
            context=std::string(cue?"SELECT":"TYPE")+" channel "+std::to_string(event.channel)+
                " key "+std::to_string(event.note)+" sample "+std::to_string(event.sampleId);
            ++distinct;run(s,s.encodedBytes,p,12000,8000);
        }
    }
    std::cout<<"Music score hardware replay passed: "<<distinct<<" distinct parameter sets, "
             <<cases<<" cases, "<<frames<<" sample frames, "<<comparisons<<" comparisons, max filtered-sign drift "<<signedDrift<<"\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
