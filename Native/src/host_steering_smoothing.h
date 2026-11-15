#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace idas3 {
// Digital steering needs a short ramp for taps, not the old 0.67-second lag.
// This host-only response does not change pad/wheel axes or the car solver.
inline float advanceHostKeyboardSteering(float current,float target,float dt){
    const float rate=target==0.f?10.f:current*target<0.f?12.f:6.f;
    return current+std::clamp(target-current,-rate*dt,rate*dt);
}
// Optional digital steering presets recovered from Initial D Street Stage
// (PSP, ULJM05093) BOOT.BIN. Its "steering sensitivity" option 1..5 indexes
// a five-row table at 0x1EAC0C (36-byte rows) that 0x182C50 copies into the
// globals read by the digital steering routine at 0x11C0C4. Only the hold
// divisor differs between rows.
enum class DigitalSteeringPreset { Default=0, Psp1, Psp2, Psp3, Psp4, Psp5 };
inline constexpr int digitalSteeringPresetCount=6;
struct PspDigitalSteering {
    static constexpr float target=42000.f;      // 0x47241000, past the 0x7FFF clamp
    static constexpr float centreDivisor=6.f;   // 0x40C00000
    static constexpr float reverseDivisor=12.f; // 0x41400000
    static constexpr float easeStart=.8f;       // 0x3F4CCCCD
    static constexpr int easeFrames=4;
    static constexpr float reverseSlip=.2f;     // 0x3E4CCCCD, against vehicle +0x27C
    static constexpr float holdDivisor[5]{32.f,28.f,24.f,19.f,14.f}; // 0x42000000..0x41600000
    // The race scene (0x8C0CC) sets the frame limiter to 30 fps and the main
    // loop (0x9779C) runs this routine once per frame.
    static constexpr double frameRate=30.0;
    std::int32_t value=0;  // signed word; the PSP stores value+0x8000
    std::int32_t held=0;   // frames since the D-pad left neutral
    double untilFrame=0;   // PSP frames until the next step
    void reset(){*this={};}
    // One 30 Hz PSP frame. direction is -1, 0 or 1. slip is the normalized
    // heading error (vehicle +0x27C, the same quantity the PSP reads at
    // 0x4AFB10), which enables the fast counter-steer only while sliding.
    void frame(int direction,int level,float slip){
        const float aim=target*float(direction);
        const float now=float(value);
        float next;
        if(aim==0.f){held=0;next=now+(aim-now)/centreDivisor;}
        else{
            ++held;
            if(aim*now<0.f&&slip>reverseSlip)next=now+(aim-now)/reverseDivisor;
            else{
                float step=(aim-now)/holdDivisor[level];
                if(held<easeFrames)step*=easeStart+(1.f-easeStart)*float(held)/float(easeFrames);
                next=now+step;
            }
        }
        value=std::clamp(std::int32_t(next),-0x7FFF,0x7FFF);
    }
    // The PSP car reads the high byte as an arcade-style ADC sample: (byte-128)/80.
    float steering()const{
        return std::clamp(float(((value+0x8000)>>8)-128)/80.f,-1.f,1.f);
    }
    float advance(int direction,int level,float slip,float dt){
        if(!std::isfinite(dt)||dt<=0.f)return steering();
        // Step on the first tick, then hold each value for a whole PSP frame.
        for(;untilFrame<=1e-6;untilFrame+=1.0)frame(direction,level,slip);
        untilFrame-=double(dt)*frameRate;
        return steering();
    }
};
// Keep a keyboard release on the keyboard's existing return ramp even when
// an unused controller is connected. A real stick still takes ownership
// immediately, including its next neutral sample.
class HostSteeringSource {
    bool keyboardOwns_=false,pspOwns_=false;
    PspDigitalSteering psp_;
public:
    void reset(){keyboardOwns_=pspOwns_=false;psp_.reset();}
    // digitalPad marks a D-pad binding that reaches native as a full-scale
    // stick value. With a PSP preset it ramps like the keyboard.
    float advance(float current,float keyboard,float analog,bool connected,
                  bool originalHandling,float dt,
                  DigitalSteeringPreset preset=DigitalSteeringPreset::Default,
                  bool digitalPad=false,float slip=0.f){
        if(preset!=DigitalSteeringPreset::Default){
            // Digital input takes the PSP ramp, which keeps ownership while it
            // recentres. A stick returning to neutral stays on its own path.
            const bool stick=connected&&std::abs(analog)>.01f;
            if(digitalPad||(!stick&&(keyboard!=0.f||pspOwns_))){
                // Continue from the previous source's position, in PSP units.
                if(!pspOwns_){psp_.reset();psp_.value=std::int32_t(std::clamp(current,-1.f,1.f)*80.f)*256;pspOwns_=true;}
                keyboardOwns_=false;
                const float input=stick?analog:keyboard;
                const int direction=input>.5f?1:input<-.5f?-1:0;
                return psp_.advance(direction,int(preset)-1,slip,dt);
            }
            pspOwns_=false;
        }
        const bool useStick=connected&&std::abs(analog)>.01f;
        if(useStick)keyboardOwns_=false;
        else if(keyboard!=0.f)keyboardOwns_=true;
        const float target=useStick?std::clamp(analog,-1.f,1.f):keyboard;
        if(originalHandling&&connected&&(useStick||(keyboard==0.f&&!keyboardOwns_)))current=target;
        else if(!useStick)current=advanceHostKeyboardSteering(current,target,dt);
        else current+=std::clamp(target-current,-12.f*dt,12.f*dt);
        if(!useStick&&keyboard==0.f&&current==0.f)keyboardOwns_=false;
        return current;
    }
};
// Optional host preference after the existing input conditioning. This is not
// part of the recovered car solver. The bypass returns its input unchanged.
class HostSteeringSmoothing {
    float amount_=0;
    double filtered_=0;
public:
    float amount()const{return amount_;}
    bool setAmount(float value){
        if(!std::isfinite(value)||value<0.f||value>1.f)return false;
        if(amount_!=value){amount_=value;reset();}
        return true;
    }
    void reset(){filtered_=0;}
    float advance(float target,float dt,bool blocked=false){
        if(blocked){reset();return 0.f;}
        if(amount_==0.f){filtered_=target;return target;}
        if(!std::isfinite(target)){reset();return 0.f;}
        target=std::clamp(target,-1.f,1.f);
        if(!std::isfinite(dt)||dt<=0.f)return float(filtered_);
        const double alpha=-std::expm1(-double(dt)/(.2*double(amount_)));
        filtered_=std::clamp(filtered_+(double(target)-filtered_)*alpha,
                             std::min(filtered_,double(target)),std::max(filtered_,double(target)));
        return float(filtered_);
    }
};
}
