#include "host_steering_smoothing.h"
#include <array>
#include <bit>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
unsigned checks=0;
void require(bool condition,const char* label){++checks;if(!condition)throw std::runtime_error(label);}
bool close(float a,float b,float tolerance=2e-6f){return std::abs(a-b)<=tolerance;}
bool bits(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
constexpr float tick=1.f/60.f;
}
int main()try{
    using idas3::HostSteeringSmoothing;
    float keyboard=0;
    for(int tickIndex=0;tickIndex<10;++tickIndex)keyboard=idas3::advanceHostKeyboardSteering(keyboard,1,tick);
    require(close(keyboard,1),"Keyboard must reach full lock within 167ms");
    for(int tickIndex=0;tickIndex<16;++tickIndex)keyboard=idas3::advanceHostKeyboardSteering(keyboard,-1,tick);
    require(close(keyboard,-1),"Keyboard countersteer must complete within 267ms");
    for(int tickIndex=0;tickIndex<7;++tickIndex)keyboard=idas3::advanceHostKeyboardSteering(keyboard,0,tick);
    require(close(keyboard,0),"Released keyboard steering must center within 117ms");
    for(float direction:{-1.f,1.f}){float key=idas3::advanceHostKeyboardSteering(0,direction,tick);require(close(key,direction*.1f),"First key tick must respond immediately without full-lock snapping");}
    // Connecting an idle pad cannot change a keyboard steering sequence.
    for(bool original:{false,true})for(float direction:{-1.f,1.f}){
        idas3::HostSteeringSource alone,idlePad;float a=0,b=0;
        for(unsigned frame=0;frame<90;++frame){
            const float key=frame<12?direction:frame<25?0:frame<45?-direction:0;
            a=alone.advance(a,key,0,false,original,tick);
            b=idlePad.advance(b,key,0,true,original,tick);
            require(bits(a,b),"Idle controller changed keyboard press, countersteer or release");
        }
    }
    idas3::HostSteeringSource source;
    float controlled=0;
    for(unsigned i=0;i<12;++i)controlled=source.advance(controlled,1,0,true,true,tick);
    require(source.advance(controlled,0,0,true,true,tick)>.8f,"Keyboard release snapped with an idle controller");
    controlled=source.advance(controlled,1,-.5f,true,true,tick);
    require(bits(controlled,-.5f),"Active stick failed to take immediate ownership");
    require(source.advance(controlled,0,0,true,true,tick)==0,"Real stick release inherited the keyboard return ramp");
    for(unsigned i=0;i<12;++i)controlled=source.advance(controlled,-1,0,true,true,tick);
    source.reset();
    require(source.advance(controlled,0,0,true,true,tick)==0,"Lifecycle reset retained keyboard ownership");
    HostSteeringSmoothing bypass;
    require(bypass.amount()==0.f,"Default must preserve existing steering");
    for(int axis=-32768;axis<=32767;++axis){
        const float target=float(axis)/32768.f;
        require(bits(bypass.advance(target,tick),target),"Zero smoothing altered conditioned steering bits");
    }
    for(float value:{0.f,-0.f,1.f,-1.f,std::numeric_limits<float>::denorm_min()})
        require(bits(bypass.advance(value,tick),value),"Bypass altered endpoint or signed zero");

    HostSteeringSmoothing slow;
    require(slow.setAmount(1.f),"Maximum amount rejected");
    require(close(slow.advance(1.f,.2f),float(1-std::exp(-1.0))),"100 percent is a 0.2 second time constant");
    float previousStep=1.f;
    for(unsigned percent=1;percent<=100;++percent){
        const float amount=float(percent)/100.f;
        HostSteeringSmoothing filter;filter.setAmount(amount);
        const float first=filter.advance(1.f,tick);
        require(first>0.f&&first<=previousStep,"Increasing smoothing must not speed up response");previousStep=first;
        float last=first;
        for(unsigned frame=0;frame<180;++frame){
            const float value=filter.advance(1.f,tick);
            require(value>=last&&value<=1.f,"Step overshoots or moves backwards");last=value;
        }
        for(unsigned frame=0;frame<180;++frame){
            const float value=filter.advance(-1.f,tick);
            require(value<=last&&value>=-1.f,"Reversal overshoots or moves backwards");last=value;
        }
        filter.reset();
        require(filter.advance(0.f,tick)==0.f,"Reset retains a turn");
        HostSteeringSmoothing fresh;fresh.setAmount(amount);
        require(bits(filter.advance(1.f,tick),fresh.advance(1.f,tick)),"Reset differs from a fresh filter");

        HostSteeringSmoothing thirty,sixty,oneTwenty,partition;
        for(auto* f:{&thirty,&sixty,&oneTwenty,&partition})f->setAmount(amount);
        float at30=0,at60=0,at120=0;
        for(unsigned frame=0;frame<15;++frame)at30=thirty.advance(.7f,1.f/30.f);
        for(unsigned frame=0;frame<30;++frame)at60=sixty.advance(.7f,tick);
        for(unsigned frame=0;frame<60;++frame)at120=oneTwenty.advance(.7f,1.f/120.f);
        partition.advance(.7f,.037f);partition.advance(.7f,.111f);
        const float partitioned=partition.advance(.7f,.352f);
        const float analytic=float(.7*(1-std::exp(-.5/(.2*double(amount)))));
        require(close(at30,analytic)&&close(at60,analytic)&&close(at120,analytic)&&close(partitioned,analytic),
                "Response depends on tick partition instead of elapsed time");
    }

    HostSteeringSmoothing center;center.setAmount(1.f);
    for(unsigned i=0;i<60;++i)center.advance(1.f,tick);
    const float towardCenter=center.advance(0.f,tick);
    require(towardCenter>0.f&&towardCenter<1.f,"Ordinary centered input must decay rather than reset");
    const float afterSameSetting=center.advance(0.f,0.f);
    require(center.setAmount(1.f)&&bits(center.advance(0.f,0.f),afterSameSetting),"Repeated setting resets in-flight state");
    require(center.advance(0.f,-1.f)==afterSameSetting,"Negative dt moved state");
    require(center.advance(0.f,std::numeric_limits<float>::infinity())==afterSameSetting,"Infinite dt moved state");
    require(center.advance(0.f,std::numeric_limits<float>::quiet_NaN())==afterSameSetting,"NaN dt moved state");
    for(float invalid:{-.01f,1.01f,std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
        require(!center.setAmount(invalid),"Invalid preference accepted");
        require(center.amount()==1.f&&bits(center.advance(0.f,0.f),afterSameSetting),"Rejected preference mutated state");
    }
    require(center.setAmount(.5f)&&center.advance(0.f,0.f)==0.f,"Changed preference did not neutralize state");
    center.advance(1.f,tick);
    require(center.advance(std::numeric_limits<float>::quiet_NaN(),tick)==0.f,"NaN target did not fail neutral");
    require(center.advance(0.f,tick)==0.f,"Invalid target left stale steering");
    require(center.advance(std::numeric_limits<float>::infinity(),tick)==0.f,"Infinite target did not fail neutral");
    require(center.advance(100.f,10.f)==1.f,"Positive target bound failed");
    require(center.advance(-100.f,10.f)==-1.f,"Negative target bound failed");

    for(float amount:{0.f,.01f,.5f,1.f}){
        HostSteeringSmoothing blocked;blocked.setAmount(amount);
        for(unsigned i=0;i<60;++i)blocked.advance(-1.f,tick);
        require(blocked.advance(-1.f,tick,true)==0.f,"Blocked driving packet must neutralize immediately");
        require(blocked.advance(0.f,tick)==0.f,"Releasing menu block retained prior steering");
        require(blocked.advance(1.f,0.f,true)==0.f,"Block must clear even without a physics tick");
    }
    HostSteeringSmoothing tiny;tiny.setAmount(std::numeric_limits<float>::denorm_min());
    require(tiny.advance(1.f,tick)==1.f,"Tiny positive setting must remain finite and approach bypass");

    // PSP Street Stage digital steering (0x11C0C4). Golden words come from an
    // independent float32 model of the routine: 12 frames held, then 6 released.
    using idas3::DigitalSteeringPreset;using idas3::PspDigitalSteering;
    constexpr std::int32_t golden[5][18]{
        {1115,2264,3443,4647,5814,6944,8039,9100,10128,11124,12088,13022,10851,9042,7535,6279,5232,4360},
        {1275,2584,3921,5280,6591,7855,9074,10249,11382,12475,13529,14545,12120,10100,8416,7013,5844,4870},
        {1487,3006,4549,6109,7604,9037,10410,11726,12987,14195,15353,16463,13719,11432,9526,7938,6615,5512},
        {1878,3778,5689,7600,9410,11125,12750,14289,15747,17128,18437,19677,16397,13664,11386,9488,7906,6588},
        {2550,5086,7590,10047,12329,14448,16416,18243,19939,21514,22977,24335,20279,16899,14082,11735,9779,8149}};
    constexpr int fullLockFrames[5]{22,19,17,13,10};
    for(int level=0;level<5;++level){
        PspDigitalSteering psp;
        for(int frame=0;frame<18;++frame){
            psp.frame(frame<12?1:0,level,0.f);
            require(psp.value==golden[level][frame],"PSP sensitivity trace differs from the recovered routine");
        }
        PspDigitalSteering mirrored;
        for(int frame=0;frame<18;++frame)mirrored.frame(frame<12?-1:0,level,0.f);
        require(mirrored.value==-golden[level][17],"PSP left steering is not symmetric");
        PspDigitalSteering lock;int frames=0;
        while(lock.steering()<1.f){lock.frame(1,level,0.f);++frames;}
        require(frames==fullLockFrames[level],"PSP full-lock time changed");
        for(int frame=0;frame<60;++frame)lock.frame(1,level,0.f);
        require(lock.value==0x7FFF,"PSP steering must clamp to 0x7FFF");
        for(int frame=0;frame<60;++frame)lock.frame(0,level,0.f);
        require(lock.value==0&&lock.steering()==0.f,"PSP release must settle at centre");
    }
    require(PspDigitalSteering{1115}.steering()==.05f,"PSP word must read as the (byte-128)/80 ADC sample");
    require(PspDigitalSteering{20480}.steering()==1.f&&PspDigitalSteering{-20480}.steering()==-1.f,"PSP full lock is byte 128±80");
    // Counter-steer uses the fixed fast divisor only while the car slides.
    constexpr std::int32_t gripReverse[6]{27119,24239,21479,18834,16299,13869};
    constexpr std::int32_t slideReverse[6]{24114,18604,13553,8923,4679,789};
    for(float slip:{0.f,.5f}){
        PspDigitalSteering psp;for(int frame=0;frame<30;++frame)psp.frame(1,2,slip);
        for(int frame=0;frame<6;++frame){
            psp.frame(-1,2,slip);
            require(psp.value==(slip>0.f?slideReverse:gripReverse)[frame],"PSP counter-steer trace differs");
        }
    }
    { PspDigitalSteering psp;for(int frame=0;frame<30;++frame)psp.frame(1,2,.2f);psp.frame(-1,2,.2f);
      require(psp.value==gripReverse[0],"Slip equal to 0.2 must not use the fast counter-steer"); }
    // The PSP rule runs per 30 Hz race frame regardless of host tick length:
    // 0.75 s with the key held for the first half is 15 held + 8 released frames.
    for(float hz:{60.f,120.f,144.f,240.f}){
        PspDigitalSteering fixed,fine;
        for(int frame=0;frame<23;++frame)fixed.frame(frame<15?1:0,4,0.f);
        for(int t=0;t<int(hz*.75f);++t)fine.advance(float(t)<hz*.5f?1:0,4,0.f,1.f/hz);
        require(fine.value==fixed.value,"PSP preset depends on the host tick rate");
    }
    { PspDigitalSteering psp;
      require(psp.advance(1,0,0.f,tick)==PspDigitalSteering{golden[0][0]}.steering(),"PSP preset must respond on the first tick");
      require(psp.advance(1,0,0.f,tick)==PspDigitalSteering{golden[0][0]}.steering(),"PSP preset stepped faster than 30 Hz");
      require(psp.advance(1,0,0.f,tick)==PspDigitalSteering{golden[0][1]}.steering(),"PSP preset stepped slower than 30 Hz"); }
    { PspDigitalSteering psp;psp.advance(1,0,0.f,0.f);psp.advance(1,0,0.f,std::numeric_limits<float>::quiet_NaN());
      require(psp.value==0,"Zero or invalid dt advanced the PSP preset"); }
    // Source integration: Default is unchanged, D-pad ramps, analog sticks bypass the preset.
    for(bool original:{false,true}){
        idas3::HostSteeringSource legacy,explicitDefault;float a=0,b=0;
        for(unsigned frame=0;frame<60;++frame){
            const float key=frame<12?1.f:frame<30?-1.f:0.f;
            a=legacy.advance(a,key,0,true,original,tick);
            b=explicitDefault.advance(b,key,0,true,original,tick,DigitalSteeringPreset::Default,false,.5f);
            require(bits(a,b),"Default preset changed the existing keyboard ramp");
        }
        for(int level=1;level<=5;++level){
            const auto preset=DigitalSteeringPreset(level);
            idas3::HostSteeringSource keys,dpad,stick,stickDefault;float k=0,d=0,s=0,sd=0;
            // Two 60 Hz physics ticks per PSP frame.
            for(int t=0;t<36;++t){
                const float held=t<24?1.f:0.f;
                k=keys.advance(k,held,0,false,original,tick,preset);
                d=dpad.advance(d,0,held,true,original,tick,preset,true);
                const float expected=PspDigitalSteering{golden[level-1][t/2]}.steering();
                require(bits(k,expected),"Keyboard did not follow the PSP preset");
                require(bits(d,expected),"D-pad did not follow the PSP preset");
                const float analog=.4f*held;
                s=stick.advance(s,0,analog,true,original,tick,preset,false);
                sd=stickDefault.advance(sd,0,analog,true,original,tick);
                require(bits(s,sd),"Analog stick steering changed with a digital preset");
            }
        }
    }
    { idas3::HostSteeringSource handover;float value=0;
      for(int frame=0;frame<30;++frame)value=handover.advance(value,0,.6f,true,true,tick,DigitalSteeringPreset::Psp3);
      value=handover.advance(value,1,0,true,true,tick,DigitalSteeringPreset::Psp3);
      require(value>.6f&&value<.75f,"Switching from stick to keys must continue from the current angle"); }
    std::cout<<"PASS "<<checks<<" host steering smoothing checks: exact zero bypass, step/reversal bounds, elapsed-time invariance, neutral/reset, unchanged settings, blocked controls, invalid values and PSP sensitivity 1-5 presets.\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
