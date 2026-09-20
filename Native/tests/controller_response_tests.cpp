#include "original_host_input.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace idas3;
using namespace idas3::original;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
OriginalControls conditioned(const OriginalVehicleInputs& input){
    return conditionOriginalInputs(input.analog,input.calibration,{},verifiedGds0033InputConstants(),0);
}
OriginalControls host(int x,int y,int throttle,int brake,ControllerResponse response){
    OriginalHostInputState state;
    return conditioned(adaptOriginalHostInput(state,controllerHostControls(
        std::int16_t(x),std::int16_t(y),std::uint8_t(throttle),std::uint8_t(brake),response),true,true,0));
}
// Reference: Flycast v2.5 gamepad_device.cpp:294-307 and maple_jvs.cpp:1906,
// 2155-2184. Construct guest ADC words independently; do not call the new
// response helper or its calibrated inverse. Host trigger resolution is 8bit.
OriginalControls reference(int x,int y,int t,int b,bool wheel){
    int value=x;
    if(!wheel){
        float nv=std::abs(x)/32768.f;
        float r2=nv*nv+y*y/32768.f/32768.f;
        if(r2<.1f*.1f||r2==0)value=0;
        else{
            float pdz=nv*.1f/std::sqrt(r2);
            value=std::clamp(int(std::round((nv-pdz)/(1-pdz)*32768.f))*(x<0?-1:1),-32768,32767);
        }
    }
    auto half=[](int physical){
        int raw=int(std::round(float(physical)/255.f*32767.f))*2;
        if(raw>=0x8000&&raw<0x8100)raw=0x8100;
        return std::uint16_t(std::min(raw,0xff7f));
    };
    OriginalVehicleInputs input;input.calibration={128,32,32};
    input.analog={std::uint16_t(std::min(value+32768,0xff7f)),half(t),half(b)};
    return conditioned(input);
}
int main()try{
    unsigned long long comparisons=0;
    for(bool wheel:{false,true})for(int y:{-32768,-16384,-3277,0,3277,16384,32767})
        for(int x=-32768;x<=32767;++x){
            int t=(x+32768)%256,b=255-t;
            auto actual=host(x,y,t,b,wheel?ControllerResponse::FlycastWheel:ControllerResponse::FlycastGamepad);
            auto expected=reference(x,y,t,b,wheel);
            require(actual.steering==expected.steering&&actual.throttle==expected.throttle&&actual.brake==expected.brake,
                "Flycast reference differs after original conditioning");comparisons+=3;
        }
    auto neutral=host(0,0,0,0,ControllerResponse::FlycastGamepad);
    require(neutral.steering==0&&neutral.throttle==0&&neutral.brake==0,"Neutral controller moves pedals/steering");
    auto halfway=host(16384,0,128,128,ControllerResponse::FlycastGamepad);
    require(std::abs(halfway.steering+.7f)<.000001f&&std::abs(halfway.throttle-65.f/107)<.000001f,
        "Half-stick/trigger Flycast calibration fixture differs");
    require(host(0,0,64,64,ControllerResponse::FlycastGamepad).throttle==0,"Quarter trigger should be at original pedal dead zone");
    for(int x=-32768;x<=32767;++x){
        float old=x/32767.f;
        old=std::abs(old)<.13f?0:std::copysign((std::abs(old)-.13f)/.87f,old);
        auto previous=controllerHostControls(std::int16_t(x),12345,127,255,ControllerResponse::Previous);
        require(previous.steering==std::clamp(old,-1.f,1.f)&&previous.throttle==127/255.f&&previous.brake==1,
            "Previous controller response changed");
    }
    for(auto profile:{ControllerResponse::FlycastGamepad,ControllerResponse::Previous,ControllerResponse::FlycastWheel}){
        require(host(32767,32767,255,255,profile).steering==-1&&host(-32768,0,255,255,profile).steering==1,"Full lock lost");
        require(host(0,0,255,255,profile).throttle==1&&host(0,0,255,255,profile).brake==1,"Full pedal lost");
    }
    bool rejected=false;try{controllerHostControls(0,0,0,0,static_cast<ControllerResponse>(9));}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"Invalid controller response accepted");
    for(auto profile:{ControllerResponse::FlycastGamepad,ControllerResponse::Previous,ControllerResponse::FlycastWheel}){
        for(float zone:{0.f,.05f,.1f,.2f,.3f}){
            float last=-2;
            for(int x=-32768;x<=32767;x+=97){
                const auto value=controllerHostControls(std::int16_t(x),0,117,204,profile,zone);
                const auto baseline=controllerHostControls(std::int16_t(x),0,117,204,profile);
                require(std::isfinite(value.steering)&&value.steering>=last,"Custom deadzone steering must be finite/monotonic");
                require(value.throttle==baseline.throttle&&value.brake==baseline.brake,"Steering deadzone changed pedals");
                last=value.steering;
            }
            const auto zero=controllerHostControls(0,0,0,0,profile,zone);
            require(zero.steering==0,"Custom deadzone moved neutral steering");
            require(controllerHostControls(32767,0,0,0,profile,zone).steering>.999f,"Custom deadzone lost right endpoint");
            require(controllerHostControls(-32768,0,0,0,profile,zone).steering<=-1.f,"Custom deadzone lost left endpoint");
            const auto center=controllerHostControls(3276,0,0,0,profile,.2f);
            require(center.steering==0,"Custom deadzone did not suppress center noise");
        }
    }
    for(float bad:{-.1f,.31f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
        rejected=false;try{controllerHostControls(0,0,0,0,ControllerResponse::FlycastWheel,bad);}catch(const std::invalid_argument&){rejected=true;}
        require(rejected,"Invalid steering deadzone accepted");
    }
    std::cout<<"PASS "<<comparisons<<" conditioned channel comparisons against independent Flycast ADC model; all signed axis values at seven paired-axis values, all trigger codes, legacy response, custom steering-only deadzones, neutral/full endpoints and invalid values.\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
