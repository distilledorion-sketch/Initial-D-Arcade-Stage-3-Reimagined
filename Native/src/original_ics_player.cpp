#include "original_ics_player.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace idas3 {
namespace {
unsigned sendAttenuation(unsigned level){return level==0?255:(15-(level&15))*8;}
// This bank format initializes KRS, D1R, D2R and DL to0, with AR/RR31.
// Its effective decay rate is0..15 and release rate62..63. Hardware equations
// were checked against Flycast core/hw/aica/sgc_if.cpp; cabinet DSP is separate.
unsigned decayRate(unsigned effective){
    constexpr std::array<double,16> milliseconds{0,0,118200,101300,88600,70900,59100,50700,44300,35500,29600,25300,22200,17700,14800,12700};
    if(effective<2)return 0;
    return unsigned(std::lround(67108863.0/(44.1*milliseconds.at(effective))));
}
}
OriginalIcsPlayer::OriginalIcsPlayer(OriginalIcsVoiceTables tables):tables_(std::move(tables)){
    for(unsigned i=0;i<255;++i)gain_[i]=std::int32_t(32768.0*std::exp2(-double(i)/16));
    gain_[255]=0;
}
void OriginalIcsPlayer::release(int index){if(index>=0&&voices_[index].active)voices_[index].envelopePhase=3;}
void OriginalIcsPlayer::stop(unsigned channel){
    auto& c=channels_.at(channel);c.value=0;
    for(auto& slot:c.voices){release(slot);slot=-1;}
}
void OriginalIcsPlayer::reset(){for(auto& c:channels_)c=Channel{};for(auto& v:voices_)v=Voice{};starts_=0;}
void OriginalIcsPlayer::clearDspSends(){for(auto& v:voices_)v.parameters.effectSend=0;}
void OriginalIcsPlayer::select(unsigned channel,std::shared_ptr<const OriginalIcsBank> bank,unsigned program){
    if(!bank||program>=bank->programs.size())throw std::out_of_range("ICS program selection");
    const auto& p=bank->programs[program];
    if(p.layers.size()>16)throw std::runtime_error("ICS layer limit");
    for(const auto& l:p.layers)if(l.flags!=0||l.header[6]!=31||l.header[7]!=31||(l.header[9]&1)==0)
        throw std::runtime_error("Unverified ICS voice envelope/filter/activation format");
    stop(channel);auto& c=channels_.at(channel);c.bank=std::move(bank);c.program=program;
    c.controls={};c.controls.volume=p.header[2];
}
void OriginalIcsPlayer::configure(Voice& v,const OriginalIcsVoiceParameters& p,bool effectRegisterWrite){
    const auto hardwareSend=v.parameters.effectSend;
    v.parameters=p;if(!effectRegisterWrite)v.parameters.effectSend=hardwareSend;
    const unsigned oct=(p.pitch>>11)&15,fraction=p.pitch&1023;
    const int signedOct=oct<8?int(oct):int(oct)-16;
    v.increment=signedOct<0?(1024|fraction)>>(-signedOct):(1024|fraction)<<signedOct;
    const unsigned keyRate=(fraction>>9)+unsigned(std::max(0,signedOct*2));
    v.decayStep=decayRate(keyRate);
    const float milliseconds=keyRate?3.1f:3.6f;
    v.releaseStep=unsigned(std::lround(67108863.0/(44.1*double(milliseconds))));
}
void OriginalIcsPlayer::update(unsigned channel,bool effectRegisterWrite){
    auto& c=channels_.at(channel);if(!c.bank)return;
    const auto& program=c.bank->programs[c.program];
    for(unsigned i=0;i<program.layers.size();++i){
        const auto& layer=program.layers[i];auto& slot=c.voices[i];
        if(!c.value||c.value<layer.first||c.value>layer.last){release(slot);slot=-1;continue;}
        const auto parameters=originalIcsVoiceParameters(*c.bank,layer,tables_,c.controls,c.value,master_);
        bool newVoice=false;
        if(slot<0||!voices_[slot].active){
            const auto available=std::find_if(voices_.begin(),voices_.end(),[](const auto& v){return !v.active;});
            if(available==voices_.end())throw std::runtime_error("ICS physical voice allocation exhausted");
            const int index=int(available-voices_.begin());
            for(auto& owner:channels_)for(auto& previous:owner.voices)if(previous==index)previous=-1;
            slot=index;auto& v=*available;v=Voice{};
            v.bank=c.bank;v.sample=layer.sample-256;v.envelope=640u<<16;v.active=true;++starts_;
            newVoice=true;
        }
        configure(voices_[slot],parameters,effectRegisterWrite||newVoice);
    }
}
void OriginalIcsPlayer::setValue(unsigned channel,unsigned value){
    if(value>255)throw std::out_of_range("ICS control value");channels_.at(channel).value=value;update(channel);
}
void OriginalIcsPlayer::setControls(unsigned channel,const OriginalIcsVoiceControls& controls,bool effectRegisterWrite){
    auto& c=channels_.at(channel);
    effectRegisterWrite|=c.controls.effectSend!=controls.effectSend||c.controls.effectBus!=controls.effectBus;
    c.controls=controls;update(channel,effectRegisterWrite);
}
void OriginalIcsPlayer::setMasterVolume(unsigned value){
    if(value>127)throw std::out_of_range("ICS master volume");master_=value;
    for(unsigned i=0;i<channels_.size();++i)update(i);
}
unsigned OriginalIcsPlayer::activeVoices()const{return unsigned(std::count_if(voices_.begin(),voices_.end(),[](const auto& v){return v.active;}));}
OriginalIcsMixFrame OriginalIcsPlayer::renderFrame(){
    OriginalIcsMixFrame mixed;
    for(auto& v:voices_)if(v.active){
        const auto& sample=v.bank->samples[v.sample];const auto& p=v.parameters;
        const auto next=v.position+1>=sample.loopEnd&&sample.loopEnd>sample.loopStart?sample.loopStart:v.position+1;
        const int interpolated=(int(sample.pcm.at(v.position))*int(1024-v.fraction)>>10)+(int(sample.pcm.at(next))*int(v.fraction)>>10);
        const auto gain=[&](unsigned attenuation){return gain_[std::min(255u,attenuation+(v.envelope>>18))];};
        const unsigned full=p.totalLevel+sendAttenuation(p.directLevel),pan=full+sendAttenuation((~p.pan)&15);
        mixed.dry[0]+=std::int32_t((std::int64_t(interpolated)*gain((p.pan&16)?full:pan))>>15);
        mixed.dry[1]+=std::int32_t((std::int64_t(interpolated)*gain((p.pan&16)?pan:full))>>15);
        mixed.effects[p.effectSend&15]+=std::int32_t((std::int64_t(interpolated)*gain(p.totalLevel+sendAttenuation(p.effectSend>>4)))>>11);
        // AR31 is instantaneous at both effective rates62/63. D1/D2 are
        // zero-rate source fields, with the hardware's pitch key scaling.
        if(v.envelopePhase==0){v.envelope=0;v.envelopePhase=1;}
        else if(v.envelopePhase==3){v.envelope+=v.releaseStep;if(v.envelope>=1023u<<16){v.active=false;continue;}}
        else{
            v.envelope+=v.decayStep;v.envelopePhase=2;
            if(v.envelope>=1023u<<16){v.envelope=1023u<<16;v.envelopePhase=3;}
        }
        v.fraction+=v.increment;v.position+=v.fraction>>10;v.fraction&=1023;
        if(v.position>=sample.loopEnd){
            if(sample.looping)v.position=sample.loopStart+(v.position-sample.loopEnd)%(sample.loopEnd-sample.loopStart);
            else v.active=false;
        }
    }
    return mixed;
}
}
