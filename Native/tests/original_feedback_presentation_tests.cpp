#include "original_feedback_presentation.h"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
using namespace idas3ffb;
int main()try {
    unsigned checks=0;auto check=[&](bool ok){++checks;if(!ok)throw std::runtime_error("feedback presentation check "+std::to_string(checks));};
    idas3::original::OriginalVehicleState vehicle;
    for(unsigned offset:{0x1CCu,0x27Cu,0x24Cu,0x3B4u,0x3B8u,0x3C4u,0x3C8u,0x158u,0x258u,0x25Cu,0x260u,0x264u,0x248u})vehicle.drive.setf(offset,float(offset)*.001f);
    for(unsigned offset:{0x150u,0x154u,0x434u,0x438u,0x140u,0x1A8u,0x164u,0x168u,0x14Cu})vehicle.drive.setu(offset,offset);
    vehicle.transmission.filtered18=1234;vehicle.transmission.gear00=3;vehicle.transmission.snapshot04=2;vehicle.transmission.normalized20=.7f;
    const auto mapped=feedbackInput(vehicle,30,17,8);
    check(mapped.carIndex==30&&mapped.condition==17&&mapped.cardSteeringLevel==8&&mapped.globalSetting==5);
    check(mapped.steering1CC==vehicle.drive.f(0x1CC)&&mapped.headingFactor27C==vehicle.drive.f(0x27C)&&mapped.acceleration24C==vehicle.drive.f(0x24C));
    check(mapped.frontContact3B4==vehicle.drive.f(0x3B4)&&mapped.frontContact3B8==vehicle.drive.f(0x3B8));
    check(mapped.normalizedFrontContact3C4==vehicle.drive.f(0x3C4)&&mapped.normalizedFrontContact3C8==vehicle.drive.f(0x3C8));
    check(mapped.wallContact150==0x150&&mapped.wallCorner154==0x154&&mapped.wallMagnitude158==vehicle.drive.f(0x158));
    check(mapped.wet434==0x434&&mapped.snow438==0x438&&mapped.oil140==0x140&&mapped.automaticBrake1A8==0x1A8);
    check(mapped.drive258==vehicle.drive.f(0x258)&&mapped.drive25C==vehicle.drive.f(0x25C)&&mapped.drive260==vehicle.drive.f(0x260)&&mapped.drive264==vehicle.drive.f(0x264));
    check(mapped.normalizedSpeed248==vehicle.drive.f(0x248)&&mapped.surface164==0x164&&mapped.surface168==0x168&&mapped.roadFlag14C==0x14C);
    check(mapped.filteredRpm==1234&&mapped.gear==3&&mapped.gearSnapshot==2&&mapped.transmissionNormalized==.7f);
    OriginalFeedbackPresentation fast,slow;OriginalInput input;input.steering1CC=.8f;
    for(unsigned tick=1;tick<=300;++tick){
        // One original tick of car-contact vibration, then complete silence.
        input.drive260=tick==61?20.f:0.f;
        fast.advance(input,tick);slow.advance(input,tick);
        const auto before=fast.producerState();fast.advance(input,tick);
        check(fast.producerState().ramp==before.ramp); // Polling cannot advance ramp.
        const auto peak=fast.consumePeak();
        check(peak==(tick==61?20:0));check(fast.consumePeak()==0);
        if(tick%2==0){check(slow.consumePeak()==(tick==62?20:0));check(slow.latest().rumbleAmplitude==0);}
        check(fast.latest().torqueCode==slow.latest().torqueCode&&fast.producerState().ramp==slow.producerState().ramp);
    }
    check(fast.producerState().ramp==256&&fast.latest().limit==50);
    fast.stop();check(!fast.active()&&fast.consumePeak()==0&&fast.producerState().ramp==0);
    fast.advance(input,301);check(fast.active()&&fast.latest().torqueCode==128&&fast.producerState().ramp==1);
    input.steering1CC=std::numeric_limits<float>::quiet_NaN();fast.advance(input,302);
    check(!fast.active()&&fast.consumePeak()==0&&fast.producerState().ramp==0);
    input={};input.drive260=20;fast.advance(input,303);input.drive260=0;
    for(unsigned tick=304;tick<312;++tick)fast.advance(input,tick);
    check(fast.consumePeak()==0); // An absent reader cannot replay an old hit.
    // Online catch-up can advance more than six ticks before either output
    // reads. Equal repeated events keep their own age, not the first peak's.
    fast.stop();input={};input.drive260=20;fast.advance(input,1);
    input.drive260=0;for(unsigned tick=2;tick<7;++tick)fast.advance(input,tick);
    input.drive260=20;fast.advance(input,7);input.drive260=0;fast.advance(input,8);
    check(fast.consumePeak()==20);check(fast.consumePeak()==0);
    // When the larger old event expires, the smaller recent one still exists.
    fast.stop();input.drive260=20;fast.advance(input,20);
    input.drive260=0;for(unsigned tick=21;tick<26;++tick)fast.advance(input,tick);
    input.drive260=12;fast.advance(input,26);input.drive260=0;fast.advance(input,27);
    check(fast.consumePeak()==12);check(fast.consumePeak()==0);
    // The inclusive six-tick boundary and a skipped-tick expiry are explicit.
    input.drive260=20;fast.advance(input,100);input.drive260=0;fast.advance(input,106);
    check(fast.consumePeak()==20);check(fast.consumePeak()==0);
    input.drive260=20;fast.advance(input,200);input.drive260=0;fast.advance(input,207);
    check(fast.consumePeak()==0);
    // A timeline rewind clears event history and restarts the producer ramp.
    input.drive260=20;fast.advance(input,400);input.drive260=0;fast.advance(input,399);
    check(fast.consumePeak()==0&&fast.producerState().ramp==1);
    std::cout<<"PASS "<<checks<<" mapping/lifecycle/event checks; no hardware output\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
