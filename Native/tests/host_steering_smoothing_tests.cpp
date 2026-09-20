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
    std::cout<<"PASS "<<checks<<" host steering smoothing checks: exact zero bypass, step/reversal bounds, elapsed-time invariance, neutral/reset, unchanged settings, blocked controls and invalid values.\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
