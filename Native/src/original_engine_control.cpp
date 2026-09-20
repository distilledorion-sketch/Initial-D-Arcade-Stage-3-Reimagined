#include "original_engine_control.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace idas3::original {
namespace {
std::int32_t truncate(float value){
    // SH4 FTRC defines these results, including the low-RPM sqrt's NaN case.
    if(std::isnan(value)||value<=-2147483648.f)return INT32_MIN;
    if(value>=2147483648.f)return INT32_MAX;
    return static_cast<std::int32_t>(value);
}
std::int32_t add(std::int32_t a,std::int32_t b){return std::bit_cast<std::int32_t>(std::uint32_t(a)+std::uint32_t(b));}
std::int32_t subtract(std::int32_t a,std::int32_t b){return std::bit_cast<std::int32_t>(std::uint32_t(a)-std::uint32_t(b));}
unsigned random(std::uint32_t& seed){seed=seed*1103515245u+12345u;return(seed>>16)&32767u;}
}
OriginalEngineTables OriginalEngineTables::load(const std::filesystem::path& root){
    std::ifstream f(root/"data/original_audio/continuous/control.bin",std::ios::binary);
    if(!f)throw std::runtime_error("Original continuous sound tables unavailable");
    const auto word=[&](){std::array<unsigned char,4>b{};if(!f.read(reinterpret_cast<char*>(b.data()),4))throw std::runtime_error("Truncated engine control tables");return unsigned(b[0])|(unsigned(b[1])<<8)|(unsigned(b[2])<<16)|(unsigned(b[3])<<24);};
    if(word()!=0x43454449||word()!=0x31303030||word()!=36)throw std::runtime_error("Unknown engine control table format");
    OriginalEngineTables result;
    for(auto& family:result.families){
        for(auto& curve:family.curves){curve.scale=std::bit_cast<int>(word());curve.mode=std::bit_cast<int>(word());curve.shiftWeight=std::bit_cast<int>(word());curve.reserved=std::bit_cast<int>(word());
            curve.constant=std::bit_cast<float>(word());curve.linear=std::bit_cast<float>(word());curve.nonlinear=std::bit_cast<float>(word());}
        family.maximumRpm=std::bit_cast<float>(word());
        for(auto& limit:family.pitchLimits)limit=std::bit_cast<int>(word());
        if(!std::isfinite(family.maximumRpm)||family.maximumRpm<=1200)throw std::runtime_error("Invalid original engine RPM table");
    }
    for(auto& pattern:result.backfirePatterns)pattern=std::uint16_t(word());
    if(f.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Unexpected engine control table tail");
    return result;
}
std::int32_t evaluateOriginalEngineCurve(const OriginalEngineCurve& c,float x){
    float value=std::fma(c.linear,x,c.constant);
    if(c.mode==1){x*=x;value=std::fma(c.nonlinear,x,value);}
    else if(c.mode==2){float cube=x*x;cube*=x;value=std::fma(c.nonlinear,cube,value);}
    else if(c.mode==3){x=std::sqrt(x);value=std::fma(c.nonlinear,x,value);}
    return truncate(float(c.scale)*value);
}
OriginalEngineConfiguration configureOriginalEngine(unsigned family,int tuningLevel,
    std::uint8_t option34,std::uint8_t option3e,std::uint8_t option42){
    if(family>=36)throw std::out_of_range("Original continuous sound family");
    OriginalEngineConfiguration c;c.family=family;c.originalCar=family==35?0:family;
    c.pitch0=.9f;c.volumePitch1=.97f;c.volume0=.96f;
    if(tuningLevel>3){c.pitch0=1.f;c.volumePitch1=1.06f;c.volume0=1.02f;}
    else if(tuningLevel>2){c.pitch0=.97f;c.volumePitch1=1.02f;c.volume0=.99f;}
    else if(tuningLevel>0){c.pitch0=.94f;c.volumePitch1=1.f;c.volume0=.97f;}
    const auto car=c.originalCar;
    if((car>=6&&car<=9)||(car>=11&&car<=14)||(car>=19&&car<=24)||(car>=27&&car<=33)){
        c.auxiliaryLoop=true;c.releaseCue=tuningLevel>0;
    }
    if(car==19&&option3e&&option42){c.backfire=true;c.releaseCue=false;}
    if(car==0&&tuningLevel>3&&option34==3)c.auxiliaryLoop=c.releaseCue=true;
    if(car==1){
        if(option34==0){if(tuningLevel>3)c.auxiliaryLoop=true;else if(tuningLevel>1)c.auxiliaryLoop=c.releaseCue=true;}
        else if(option34==3&&tuningLevel>3)c.auxiliaryLoop=c.releaseCue=true;
    }
    if((car==2||car==17)&&option34==0&&tuningLevel>3)c.auxiliaryLoop=c.releaseCue=true;
    return c;
}
void resetOriginalEngineControl(OriginalEngineControlState& state){
    const auto backfireFrames=state.backfireFrames,previousRoadFrames=state.previousRoadFrames;
    const auto pattern=state.backfirePattern;
    state={};state.backfireFrames=backfireFrames;state.previousRoadFrames=previousRoadFrames;state.backfirePattern=pattern;
}
std::vector<OriginalEngineCommand> stepOriginalEngineControl(const OriginalEngineTables& tables,
    const OriginalEngineConfiguration& c,OriginalEngineControlState& s,const OriginalEngineControlInput& in,std::uint32_t& seed,
    const std::function<void(const OriginalEngineCommand&,std::uint32_t&)>& cueOutput){
    if(c.family>=tables.families.size())throw std::out_of_range("Original engine family");
    const auto& family=tables.families[c.family];const auto& curves=family.curves;
    std::vector<OriginalEngineCommand> commands;commands.reserve(12);
    const auto send=[&](unsigned voice,unsigned command,int value){commands.push_back({OriginalEngineCommandTarget::Continuous,in.handles[voice],command,value});};
    // Non-forced cue dispatch can consume the shared RNG. Dispatch here,
    // before later controller draws, rather than replaying cues after return.
    const auto cue=[&](OriginalEngineCommandTarget target,int value){commands.push_back({target,0,0,value});if(cueOutput)cueOutput(commands.back(),seed);};
    const auto volume=[&](unsigned voice,int value){if(s.lastVolume[voice]!=value){send(voice,0x10a5,value);s.lastVolume[voice]=value;}};
    const auto pitch=[&](unsigned voice,int value){if(s.lastPitch[voice]!=value){send(voice,0xa6,value);s.lastPitch[voice]=value;}};
    float normalized=(in.rpm-800.f)/(family.maximumRpm-1200.f);
    normalized=std::clamp(normalized,0.f,1.f);
    if(in.rpm-s.previousRpm<-5.f)s.fallingRpmFrames=std::min(add(s.fallingRpmFrames,1),10);
    else s.fallingRpmFrames=std::max(subtract(s.fallingRpmFrames,1),0);
    if(in.gear!=s.previousGear){
        const bool up=subtract(in.gear,s.previousGear)>0;s.shiftFrames=up?20:10;
        s.shiftOffset=(up?-1.f:.25f)*std::fma(float(in.gear),.1f,.5f);
    }
    std::array<int,2> p{evaluateOriginalEngineCurve(curves[0],std::max(normalized,.1f)),evaluateOriginalEngineCurve(curves[2],normalized)};
    std::array<int,2> v{evaluateOriginalEngineCurve(curves[1],normalized),evaluateOriginalEngineCurve(curves[3],in.throttle)};
    if(s.shiftFrames>0){
        --s.shiftFrames;float factor;
        if(s.shiftOffset<0)factor=float(s.shiftFrames)/20.f;
        else factor=1.f-float(s.shiftFrames)/10.f;
        const auto offset=[&](unsigned curve){float result=float(curves[curve].shiftWeight)*s.shiftOffset;result*=factor;return truncate(result);};
        p[0]=add(p[0],offset(0));v[0]=add(v[0],offset(1));p[1]=add(p[1],offset(2));v[1]=add(v[1],offset(3));
    }else s.shiftOffset=0;
    const float throttleDelta=in.throttle-s.previousThrottle;
    if(throttleDelta<0||in.throttle<=.01f){
        if(!s.decayLatched){
            if(s.heldVolume==0)s.heldVolume=v[1];
            s.decayFrames=add(s.decayFrames,1);if(s.decayFrames>30)s.decayLatched=true;
            if(c.backfire){
                if(normalized>.5f&&s.backfireFrames==0&&s.decayFrames==5){s.backfireFrames=16;s.backfirePattern=tables.backfirePatterns[random(seed)%3];}
                if(s.backfireFrames>0){--s.backfireFrames;if(std::int16_t(s.backfirePattern)<0)cue(OriginalEngineCommandTarget::RaceCue1424A0,7);s.backfirePattern=std::uint16_t(s.backfirePattern*2u);}
            }
        }else if(s.decayFrames==0)s.decayLatched=false;
    }else{
        if(s.decayFrames>0&&throttleDelta>.01f){
            s.recoveryFrames=s.decayLatched?7:(s.decayFrames>>2);
            s.decayFrames=0;s.heldVolume=0;s.decayLatched=false;
        }
        s.recoveryFrames=std::max(subtract(s.recoveryFrames,1),0);
        if(s.recoveryFrames>0)v[0]=subtract(v[0],s.recoveryFrames*3);
        s.decayFrames=0;s.backfireFrames=0;s.backfirePattern=0;
    }
    if(s.decayFrames>0){
        float decay=float(s.decayFrames)/30.f;
        v[1]=add(v[1],truncate(std::sqrt(decay)*18.f));v[0]=subtract(v[0],truncate(decay*15.f));
        if(s.decayFrames>6){
            float value=family.maximumRpm-(family.maximumRpm-in.rpm)*1.2f;value=std::max(value,1000.f);
            value=(value-800.f)/(family.maximumRpm-1200.f);
            p[0]=evaluateOriginalEngineCurve(curves[0],value);p[1]=evaluateOriginalEngineCurve(curves[2],value);
        }
        volume(2,0);pitch(2,0);
    }
    if(c.originalCar>=15&&c.originalCar<=18&&normalized>.6f){p[0]=add(p[0],4);p[1]=add(p[1],4);}
    if(normalized<.05f){
        const float relative=float(p[0])/float(evaluateOriginalEngineCurve(curves[0],.1f));
        v[1]=add(v[1],truncate(std::sqrt(1.f-relative)*15.f));
        s.lastPitch[1]=255;p[1]=2+int(random(seed)%2);
    }
    for(unsigned i=0;i<2;++i){
        v[i]=truncate((i==0?c.volume0:c.volumePitch1)*float(v[i]));
        p[i]=truncate((i==0?c.pitch0:c.volumePitch1)*float(p[i]));
        if(p[i]>=family.pitchLimits[i])p[i]=255;p[i]=std::max(p[i],2);
        v[i]=std::clamp(v[i],0,120);if(in.rpm==0)v[i]=0;
        volume(i,v[i]);pitch(i,p[i]);
    }
    if(c.auxiliaryLoop){
        if(in.throttle*100.f>float(s.auxiliaryLevel))s.auxiliaryLevel=add(s.auxiliaryLevel,1);
        else s.auxiliaryLevel=subtract(s.auxiliaryLevel,1+int(random(seed)%3));
        s.auxiliaryLevel=std::clamp(s.auxiliaryLevel,0,100);
        if(!in.suppressShiftRelease){
            if(in.gear!=s.previousGear){
                if(in.gear>s.previousGear&&in.gear!=1){s.auxiliaryLevel>>=1;if(in.throttle>.8f&&c.releaseCue)cue(OriginalEngineCommandTarget::RaceCue1424E0,0);}
            }else if(s.previousThrottle-in.throttle>.2f&&s.previousThrottle>.9f&&c.releaseCue)cue(OriginalEngineCommandTarget::RaceCue1424E0,0);
        }
        float level=float(s.auxiliaryLevel)/100.f;
        int base=add(truncate(std::sqrt(level)*30.f),75);
        float scaled=float(base)*c.volume0;scaled*=c.pitch0;
        const int nextVolume=truncate(scaled);level*=level;const int nextPitch=truncate(level*255.f);
        volume(3,nextVolume);
        // Source4AE0 caches the pre-scaled volume, not the pitch sent to AICA.
        if(s.lastPitch[3]!=nextPitch){send(3,0xa6,nextPitch);s.lastPitch[3]=base;}
    }
    const unsigned surface=in.wheelSurface[0]|in.wheelSurface[1]|in.wheelSurface[2]|in.wheelSurface[3];
    if((surface&6)==6){if(s.roadFrames<=3)s.roadFrames=add(s.roadFrames,1);}
    else if(s.roadFrames>0){s.roadFrames=subtract(s.roadFrames,2);if(s.roadFrames<=0){for(unsigned i=0;i<4;++i)send(i,0x40a5,0);s.roadFrames=0;}}
    if(s.roadFrames>0){
        if(s.roadFrames==4&&s.previousRoadFrames==4)for(unsigned i=0;i<4;++i)send(i,0x40a5,30);
        s.previousRoadFrames=s.roadFrames;
    }
    s.previousThrottle=in.throttle;s.previousGear=in.gear;s.previousRpm=in.rpm;
    return commands;
}
}
