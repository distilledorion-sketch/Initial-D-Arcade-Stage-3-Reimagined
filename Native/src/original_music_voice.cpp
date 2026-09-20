#include "original_music_voice.h"
#include "original_music_band_limit.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
namespace idas3 {
namespace {
// AICA rate timing measurements, also tabulated in the local Flycast hardware
// reference (sgc_if.cpp). These are hardware timing data, separate from the
// original game's recovered instrument fields and note/controller arithmetic.
constexpr std::array<float,64> attackMs{
    -1,-1,8100,6900,6000,4800,4000,3400,3000,2400,2000,1700,1500,
    1200,1000,860,760,600,500,430,380,300,250,220,190,150,130,110,95,
    76,63,55,47,38,31,27,24,19,15,13,12,9.4f,7.9f,6.8f,6,4.7f,3.8f,
    3.4f,3,2.4f,2,1.8f,1.6f,1.3f,1.1f,.93f,.85f,.65f,.53f,.44f,.40f,.35f,0,0};
constexpr std::array<float,64> decayMs{
    -1,-1,118200,101300,88600,70900,59100,50700,44300,35500,29600,25300,
    22200,17700,14800,12700,11100,8900,7400,6300,5500,4400,3700,3200,2800,
    2200,1800,1600,1400,1100,920,790,690,550,460,390,340,270,230,200,170,140,
    110,98,85,68,57,49,43,34,28,25,22,18,14,12,11,8.5f,7.1f,6.1f,5.4f,4.3f,3.6f,3.1f};
struct VoiceTables {
    std::array<unsigned,64> attack{},decay{};
    std::array<std::int32_t,256> gain{};
    std::array<std::array<unsigned,256>,8> pitchLfo{};
    VoiceTables(){
        for(unsigned i=0;i<64;++i){
            const double a=attackMs[i],d=decayMs[i];
            attack[i]=a<0?0:a==0?65536u:unsigned(std::llround(65536.0/(1.0-1.0/std::pow(640.0,1.0/(44.1*a)))));
            decay[i]=d<0?0:unsigned(std::llround(67108863.0/(44.1*d)));
        }
        for(unsigned i=0;i<255;++i)gain[i]=std::int32_t(32768.0*std::exp2(-double(i)/16));
        constexpr std::array<float,8> cents{0,3.61f,7.22f,14.44f,28.88f,57.75f,115.5f,231};
        for(unsigned depth=0;depth<8;++depth)for(unsigned phase=0;phase<256;++phase)
            pitchLfo[depth][phase]=unsigned(1024.f*std::pow(2.f,cents[depth]*float(int(phase)-128)/128.f/1200.f));
    }
};
const VoiceTables& tables(){static const VoiceTables value;return value;}
unsigned sendAttenuation(unsigned level){return level?((15-level)*8):255;}
constexpr std::array<int,32> filterResonance{
    2048,1536,1024,512,0,-256,-512,-768,-1024,-1280,-1536,-1792,-2048,-2176,-2304,-2432,
    -2560,-2688,-2816,-2944,-3072,-3136,-3200,-3264,-3328,-3392,-3456,-3520,-3584,-3648,-3712,-3776};
}
void OriginalMusicVoice::start(OriginalMusicPcm sample,const OriginalMusicVoiceParameters& parameters){
    if(sample.pcm.empty()||!sample.loopEnd||sample.loopEnd>sample.pcm.size()||sample.loopStart>=sample.loopEnd)
        throw std::invalid_argument("Original music sample/loop range");
    sample_=sample;read_=sample.pcm;position_=fraction_=0;phase_=0;envelope_=640<<16;active_=true;
    lfoState_=lfoCounter_=0;filterPhase_=0;filterValue_=unsigned(parameters.filterLevels[0])<<16;
    filterPrevious1_=filterPrevious2_=0;filterRemainder_=0;configure(parameters);
}
void OriginalMusicVoice::configure(const OriginalMusicVoiceParameters& p){
    if((p.pitch&0x8400u)||(p.envelope1&0x20u)||(p.envelope2&0x8000u)||p.pan>31||p.directLevel>15)
        throw std::invalid_argument("Original music voice register fields");
    if((p.filter&0xc0u)||(p.filterEnvelope1&0xe0e0u)||(p.filterEnvelope2&0xe0e0u)
        ||std::any_of(p.filterLevels.begin(),p.filterLevels.end(),[](unsigned v){return v>8191;}))
        throw std::invalid_argument("Original music filter register fields");
    if(((p.lfo&7)&&((p.lfo>>3)&3)==3)||(((p.lfo>>5)&7)&&((p.lfo>>8)&3)==3))
        throw std::invalid_argument("Unimplemented random original music LFO");
    const bool newLfo=p.lfo!=parameters_.lfo||!lfoCounter_;
    parameters_=p;
    const int octave=int((p.pitch>>11)^8)-8;
    const unsigned fraction=p.pitch&1023;
    increment_=octave<0?(1024|fraction)>>(-octave):(1024|fraction)<<octave;
    // Pick the block this pitch should read. With no levels supplied this is
    // always the authored one, which is what keeps the default path identical.
    read_=sample_.pcm;
    if(!sample_.bandLimited.empty()){
        const auto level=originalMusicBandLimitLevel(increment_,unsigned(sample_.bandLimited.size()));
        const auto& chosen=sample_.bandLimited[level];
        if(chosen.size()==sample_.pcm.size())read_=chosen;
    }
    const unsigned krs=(p.envelope2>>10)&15;
    const auto rate=[&](unsigned field){return std::min(63u,field*2+(krs==15?0u:((fraction>>9)+unsigned(std::max(0,(int(krs)+octave)*2)))));};
    const auto& t=tables();
    rates_={t.attack[rate(p.envelope1&31)],t.decay[rate((p.envelope1>>6)&31)],
        t.decay[rate(p.envelope1>>11)],t.decay[rate(p.envelope2&31)]};
    // Diagnostic A/B only: IDAS3_MUSIC_NO_DECAY holds the envelope flat so the
    // decay phases can be judged by ear against the ordinary render.
    static const bool noDecay=std::getenv("IDAS3_MUSIC_NO_DECAY")!=nullptr;
    if(noDecay)rates_[1]=rates_[2]=0;
    static const bool noFilter=std::getenv("IDAS3_MUSIC_NO_FILTER")!=nullptr;
    static const bool noLfo=std::getenv("IDAS3_MUSIC_NO_LFO")!=nullptr;
    static const bool dry=std::getenv("IDAS3_MUSIC_DRY")!=nullptr;
    if(noLfo)parameters_.lfo=0;
    if(dry)parameters_.effectSend=0;
    filterRates_={t.decay[rate((p.filterEnvelope1>>8)&31)],t.decay[rate(p.filterEnvelope1&31)],
        t.decay[rate((p.filterEnvelope2>>8)&31)],t.decay[rate(p.filterEnvelope2&31)]};
    filterActive_=!(p.filter&0x20)&&((p.filter&31)!=4||std::any_of(p.filterLevels.begin(),p.filterLevels.end(),[](unsigned v){return v<8184;}));
    if(noFilter)filterActive_=false;
    if(newLfo){
        const unsigned frequency=(p.lfo>>10)&31,group=128>>(frequency>>2);
        lfoPeriod_=(group-1)*4+group*((~frequency&3)+1);lfoCounter_=lfoPeriod_;
        if(p.lfo&0x8000)lfoState_=0;
        updateLfoValue();
    }
}
void OriginalMusicVoice::release(){if(active_)phase_=filterPhase_=3;}
void OriginalMusicVoice::updateLfoValue(){
    const auto wave=[&](unsigned shape){
        if(shape==1)return lfoState_&128?255u:0u;
        if(shape==2)return ((lfoState_&127)^((lfoState_&128)?127u:0u))*2;
        return lfoState_;
    };
    lfoAmplitude_=wave((parameters_.lfo>>3)&3)>>(8-(parameters_.lfo&7));
    lfoPitch_=tables().pitchLfo[(parameters_.lfo>>5)&7][wave((parameters_.lfo>>8)&3)];
}
void OriginalMusicVoice::advanceFilter(){
    if(!filterActive_)return;
    const unsigned target=unsigned(parameters_.filterLevels[filterPhase_+1])<<16,step=filterRates_[filterPhase_];
    if(filterValue_<target)filterValue_+=std::min(step,target-filterValue_);
    else if(filterValue_>target)filterValue_-=std::min(step,filterValue_-target);
    else if(filterPhase_<2)++filterPhase_;
}
std::int32_t OriginalMusicVoice::filterSample(std::int32_t value){
    if(!filterActive_)return value;
    // The cutoff's exponent/mantissa give a second-order low-pass recurrence
    // inQ30. Keep its remainder and20-bit saturation between sample frames.
    const unsigned cutoff=filterValue_>>16,exponent=cutoff>>9,mantissa=(cutoff&511)|512;
    const std::int64_t a=std::int64_t((((std::uint64_t(mantissa)<<30)>>((15-exponent)*2))*((mantissa-1)/8))>>17);
    std::int64_t frequency=(std::int64_t(mantissa)<<exponent)*32;
    frequency+=std::int64_t(filterResonance[parameters_.filter&31])*frequency/4096;
    const std::int64_t b=(std::int64_t(2)<<30)-frequency-a,c=(std::int64_t(1)<<30)-frequency;
    if(!exponent)filterRemainder_=0;
    // The reference emulator writes this numerator as -a. That gives the
    // section a DC gain of -1: since 2^30 - b + c reduces to a, H(1) = -a/a.
    // A voice whose filter is switched on therefore comes out inverted
    // against every voice whose filter is off, however far open the cutoff is.
    //
    // The selection cue is where that shows. Its channel3 is the only filtered
    // voice in the score, and the driver programs its cutoff at 0x1ff4 with a
    // flat envelope and every filter rate at zero -- one step under the 0x1ff8
    // value that switches the section off. That is a patch left fully open,
    // meant to pass straight through, so the only thing the section does to it
    // is flip its sign against the rest of the mix. Unity DC gain is what makes
    // a fully open low-pass transparent, and the magnitude response is the same
    // either way, so this is the sign alone. IDAS3_MUSIC_REFERENCE_FILTER
    // restores the reference numerator for comparison.
    static const bool referenceSign=std::getenv("IDAS3_MUSIC_REFERENCE_FILTER")!=nullptr;
    const auto sum=(referenceSign?-a:a)*value+b*filterPrevious1_-c*filterPrevious2_-filterRemainder_;
    const auto whole=sum>>30;filterRemainder_=whole*(std::int64_t(1)<<30)-sum;
    filterPrevious2_=filterPrevious1_;filterPrevious1_=std::int32_t(std::clamp<std::int64_t>(whole,-524288,524287));
    return filterPrevious1_;
}
void OriginalMusicVoice::advanceEnvelope(){
    constexpr int end=1023<<16;
    if(phase_==0){
        if(rates_[0]){
            envelope_-=std::int32_t((std::uint64_t(envelope_)<<16)/rates_[0]+1);
            if(envelope_<65536){envelope_=0;if(!(parameters_.envelope2&0x4000))phase_=1;}
        }
    }else{
        envelope_+=std::int32_t(rates_[phase_]);
        if(phase_==1){if(unsigned(envelope_>>16)>=((parameters_.envelope2>>5)&31)*32)phase_=2;}
        else if(envelope_>=end){envelope_=end;if(phase_==3)active_=false;else phase_=3;}
    }
}
OriginalIcsMixFrame OriginalMusicVoice::renderFrame(){
    OriginalIcsMixFrame result;if(!active_)return result;
    const auto& p=parameters_;
    // The original chip interpolates its two held samples separately inQ10.
    // Even a one-shot's last interpolation partner is the source loop start.
    const unsigned next=position_+1>=sample_.loopEnd?sample_.loopStart:position_+1;
    const int value=(int(read_[position_])*int(1024-fraction_)>>10)
        +(int(read_[next])*int(fraction_)>>10);
    const auto filtered=filterSample(value*16);
    const unsigned envelopeAttenuation=std::min(255u,(unsigned(envelope_)>>18)+lfoAmplitude_);
    const auto gain=[&](unsigned attenuation){return tables().gain[std::min(255u,attenuation+envelopeAttenuation)];};
    const unsigned direct=p.totalLevel+sendAttenuation(p.directLevel);
    const unsigned panned=direct+sendAttenuation((~p.pan)&15);
    result.dry[0]=std::int32_t((std::int64_t(filtered)*gain((p.pan&16)?direct:panned))>>19);
    result.dry[1]=std::int32_t((std::int64_t(filtered)*gain((p.pan&16)?panned:direct))>>19);
    result.effects[p.effectSend&15]=std::int32_t((std::int64_t(filtered)*gain(p.totalLevel+sendAttenuation(p.effectSend>>4)))>>15);
    advanceEnvelope();if(!active_)return result;
    advanceFilter();
    fraction_+=modulatedIncrement();const unsigned advance=fraction_>>10;fraction_&=1023;
    position_+=advance;
    if(advance&&(p.envelope2&0x4000)&&phase_==0&&position_>=sample_.loopStart)phase_=1;
    if(position_>=sample_.loopEnd){
        if(sample_.looping)position_=sample_.loopStart+(position_-sample_.loopEnd)%(sample_.loopEnd-sample_.loopStart);
        else active_=false;
    }
    if(!--lfoCounter_){lfoState_=(lfoState_+1)&255;lfoCounter_=lfoPeriod_;updateLfoValue();}
    return result;
}
void OriginalMusicVoicePool::reset(){slots_={};starts_=0;peak_=0;}
unsigned OriginalMusicVoicePool::activeVoices()const{
    return unsigned(std::count_if(slots_.begin(),slots_.end(),[](const auto& slot){return slot.voice.active();}));
}
bool OriginalMusicVoicePool::active(std::uint64_t id)const{
    return std::any_of(slots_.begin(),slots_.end(),[&](const auto& slot){return slot.noteId==id&&slot.voice.active();});
}
bool OriginalMusicVoicePool::releasing(std::uint64_t id)const{
    return std::any_of(slots_.begin(),slots_.end(),[&](const auto& slot){return slot.noteId==id&&slot.voice.active()&&(slot.sourceFlags0&0x20);});
}
void OriginalMusicVoicePool::pollDriver(){
    for(auto& s:slots_)if(s.driverManaged&&s.voice.active()&&s.looping){
        const auto ca=s.voice.position(),phase=s.voice.phase();
        // B04..B58: a source one-shot ignores MIDI note-off, then hardware
        // key-offs at its first nonzero CA>=LSA. This sets status13 only;
        // it does not set source flags20 or move into the release list.
        if((s.sourceFlags0&8)&&!s.oneShotKeyoff){
            if(ca){if(ca>=s.loopStart){s.voice.release();s.oneShotKeyoff=true;}continue;}
            if(phase==0)continue;
        }
        // AD4..B00: source default ip+6=32, strict attenuation >32*16.
        if(phase!=0&&(s.voice.envelope()>>16)>512)s.voice.stop();
    }
}
void OriginalMusicVoicePool::start(std::uint64_t id,OriginalMusicPcm sample,const OriginalMusicVoiceParameters& parameters,
        bool driverManaged,std::uint8_t sourceFlags0,std::uint8_t sourceFlags1){
    for(const auto& slot:slots_)if(slot.voice.active()&&slot.noteId==id)throw std::logic_error("Repeated active original music note identity");
    const auto available=std::find_if(slots_.begin(),slots_.end(),[](const auto& slot){return !slot.voice.active();});
    if(available==slots_.end())throw std::runtime_error("Original music exceeded64 native voices");
    available->noteId=id;available->loopStart=sample.loopStart;available->looping=sample.looping;
    available->driverManaged=driverManaged;available->sourceFlags0=sourceFlags0;available->sourceFlags1=sourceFlags1;available->oneShotKeyoff=false;
    available->dspSendCleared=false;available->voice.start(sample,parameters);++starts_;peak_=std::max(peak_,activeVoices());
}
void OriginalMusicVoicePool::release(std::uint64_t id){for(auto& slot:slots_)if(slot.noteId==id){slot.voice.release();slot.sourceFlags0|=0x20;}}
void OriginalMusicVoicePool::keyOffHardware(std::uint64_t id){for(auto& slot:slots_)if(slot.noteId==id)slot.voice.release();}
void OriginalMusicVoicePool::configure(std::uint64_t id,const OriginalMusicVoiceParameters& p,bool writesEffectSend){
    for(auto& slot:slots_)if(slot.voice.active()&&slot.noteId==id){
        if(writesEffectSend)slot.dspSendCleared=false;
        auto hardware=p;if(slot.dspSendCleared)hardware.effectSend=0;slot.voice.configure(hardware);
    }
}
void OriginalMusicVoicePool::clearDspSends(){for(auto& slot:slots_)if(slot.voice.active()){slot.voice.clearDspSend();slot.dspSendCleared=true;}}
void OriginalMusicVoicePool::releaseAll(){for(auto& slot:slots_)slot.voice.release();}
OriginalIcsMixFrame OriginalMusicVoicePool::renderFrame(){
    OriginalIcsMixFrame result;
    for(auto& slot:slots_)if(slot.voice.active()){
        const auto frame=slot.voice.renderFrame();
        for(unsigned i=0;i<2;++i)result.dry[i]+=frame.dry[i];
        for(unsigned i=0;i<16;++i)result.effects[i]+=frame.effects[i];
    }
    return result;
}
}
