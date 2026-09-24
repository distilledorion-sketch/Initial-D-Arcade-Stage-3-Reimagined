#include "hud_analog_presentation.h"
#include "race.h"
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace idas3;
unsigned checks=0;
void require(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
bool near(float a,float b,float epsilon=.002f){return std::abs(a-b)<=epsilon;}
HudAnalogSample telemetry(std::uint64_t tick){return {tick,20.f+float(tick)*.05f,1800.f+float(tick)*13.f};}
}

int main()try{
    // Identical source ticks are presented at 30..240 Hz. The target is exactly
    // one solver interval behind wall time, independent of output frame rate.
    for(unsigned rate:{30u,60u,120u,144u,240u}){
        HudAnalogPresentation display;FixedClock clock;auto current=telemetry(0),previous=current;
        unsigned intermediate=0;
        for(unsigned frame=0;frame<rate*3;++frame){
            clock.advance(1./rate,[&]{previous=current;current=telemetry(current.tick+1);});
            const auto before=current;
            const auto result=display.sample(previous,current,clock.alpha(),true);
            require(current.tick==before.tick&&current.speed==before.speed&&current.rpm==before.rpm,"Display changed source telemetry");
            require(result.tick==current.tick,"Display changed simulation clock");
            require(result.rpm>=previous.rpm&&result.rpm<=current.rpm,"RPM extrapolated past source endpoints");
            if(frame>2){
                const float expectedTick=float((frame+1)*60./rate-1.);
                require(near(result.rpm,1800.f+13.f*expectedTick),"RPM depends on rendering FPS");
                require(near(result.speed,20.f+.05f*expectedTick),"Speed depends on rendering FPS");
            }
            intermediate+=result.rpm>previous.rpm+.01f&&result.rpm<current.rpm-.01f;
            const auto repaint=display.sample(previous,current,clock.alpha(),true);
            require(result.rpm==repaint.rpm&&result.speed==repaint.speed,"Repeated draw advanced the meter");
        }
        require(rate<=60||intermediate>rate/3,"High-FPS rendering still steps only at 60 Hz");
        require(current.tick==180,"Presentation rate changed simulation tick count");
    }
    HudAnalogPresentation display;auto previous=telemetry(40),current=telemetry(41);
    display.sample(previous,current,.7f,true);previous=current;current=telemetry(42);
    const auto first=display.sample(previous,current,.25f,true);
    require(near(first.rpm,std::lerp(previous.rpm,current.rpm,.25f)),"Fractional meter sample incorrect");
    for(float alpha:{0.f,.3f,.9f,1.f}){
        require(display.sample(previous,current,alpha,false).rpm==current.rpm,"Paused/current sample changed with alpha");
    }
    require(display.sample(previous,current,0.f,true).rpm==current.rpm,"Resume replayed old interval");
    previous=current;current=telemetry(43);
    require(near(display.sample(previous,current,.4f,true).rpm,std::lerp(previous.rpm,current.rpm,.4f)),"First new tick after resume did not interpolate");
    require(display.sample(previous,current,0.f,true).rpm==current.rpm,"Clock reset rewound needle");
    previous=telemetry(3);current=telemetry(4);
    require(display.sample(previous,current,.6f,true).rpm==current.rpm,"Tick rewind blended across restart");
    display.reset();previous=telemetry(0);current=telemetry(900);
    require(display.sample(previous,current,.4f,true).rpm==current.rpm,"Discontinuous source pair was interpolated");
    previous=current;current=telemetry(901);
    display.sample(previous,current,.4f,false); // Replay/seek path uses current frame.
    require(display.sample(previous,current,.8f,false).rpm==current.rpm,"Replay added a second interpolation layer");

    // Gear changes can legitimately produce a large RPM drop. Interpolation
    // must stay between those endpoints rather than impose arbitrary slew.
    display.reset();previous={1,30,8800};current={2,30,8800};display.sample(previous,current,0,true);
    previous=current;current={3,30.1f,4900};
    require(near(display.sample(previous,current,.5f,true).rpm,6850),"Shift RPM drop did not interpolate linearly");
    require(display.sample(previous,current,5.f,true).rpm==4900,"Alpha was not clamped at latest sample");
    previous=current;current={4,-1,std::numeric_limits<float>::quiet_NaN()};
    const auto invalid=display.sample(previous,current,1,true);
    require(invalid.speed==0&&invalid.rpm==0,"Non-finite or negative current value escaped to HUD");
    previous=current;current={5,40,5000};
    require(display.sample(previous,current,.5f,true).rpm==5000,"Invalid previous RPM contaminated current value");
    std::cout<<"PASS "<<checks<<" HUD analog checks: 30/60/120/144/240 FPS, pause/resume, reset, shifts, replay, finite output, read-only source.\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
