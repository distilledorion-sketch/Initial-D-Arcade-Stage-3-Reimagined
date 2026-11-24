// Compile the actual App driver/lifecycle/clock methods. No native window,
// sound device, assets, preferences, user saves or network service is opened.
#define IDAS3_UNITY_PLUGIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "../src/main.cpp"
#include <iostream>

namespace {
unsigned checks=0;
void require(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
void prepareKeyboardApp(App& a){
    a.validationMode=true;a.active=true;a.originalHandling=true;
    a.menu=a.paused=a.loadingActive=a.legendVisitActive=a.preRaceDialogueActive=a.vsActive=false;
    a.input={};a.steering=0;a.steeringSource.reset();a.steeringSmoothing.setAmount(0);
    a.hostDrivingControlsBlocked=false;a.steeringControllerConnected=false;a.multiplayer={};a.clock.reset();
}
}
int main()try{
    auto app=std::make_unique<App>();auto& a=*app;prepareKeyboardApp(a);
    // SDL virtual stick / Flycast v2.5: X=16384, Y=24576, X=16385,
    // Y=0, X=0. Y-only events must not steer a car. In particular the
    // initial X and following Y can arrive on host frames without a tick.
    a.input.connected=true;a.input.pad.Gamepad.sThumbLX=16384;a.commands(0,1000);
    a.input.pad.Gamepad.sThumbLY=24576;a.commands(0,1010);
    require(a.driver(false).steer==.7f&&a.driver().steer==.7f,"Vertical movement altered the sampled X");
    a.input.pad.Gamepad.sThumbLX=16385;a.commands(0,1020);
    require(a.driver().steer==.75f,"New X did not use current Y");
    a.input.pad.Gamepad.sThumbLY=0;a.commands(0,1030);
    require(a.driver().steer==.75f,"Vertical release changed horizontal steering");
    a.input.pad.Gamepad.sThumbLX=0;a.commands(0,1040);
    require(a.driver().steer==0,"Stick did not return to center");
    a.controllerResponse=idas3::original::ControllerResponse::FlycastWheel;
    a.input.pad.Gamepad.sThumbLX=16384;a.commands(0,1050);
    require(a.driver().steer==.8f,"Wheel curve changed");
    a.input.pad.Gamepad.sThumbLY=24576;a.commands(0,1060);
    require(a.driver().steer==.8f,"Wheel steering picked up an unrelated axis");
    a.controllerResponse=idas3::original::ControllerResponse::FlycastGamepad;
    a.steeringDeadzone=.2f;a.input.pad.Gamepad.sThumbLY=0;a.commands(0,1070);auto custom=a.driver().steer;
    a.input.pad.Gamepad.sThumbLY=24576;a.commands(0,1080);
    require(a.driver().steer!=custom,"Explicit custom response changed to the default policy");
    a.steeringDeadzone=-1;prepareKeyboardApp(a);
    // Execute the production commands owner as well as its sampler. It must
    // service input on host frames that never reach the simulation or render.
    a.input.down['D']=true;a.commands(0,1000);a.commands(0,1016);
    require(a.driver().originalSteeringAnalog==0x947b,"Commands skipped input on zero-tick frames");
    a.paused=true;a.commands(0,3000);a.paused=false;a.commands(0,3100);
    require(a.driver().steer==0,"Paused commands retained the old clock/raw axis");
    a.commands(0,3116);require(a.driver().originalSteeringAnalog==0x947b,"Commands resume did not restart ramp");
    a.loadingActive=true;a.commands(0,5000);a.loadingActive=false;a.commands(0,5100);
    require(a.driver().steer==0,"Early-return loading commands skipped reset");
    prepareKeyboardApp(a);a.commands(0,1000);a.commands(0,5000);
    a.input.down['D']=true;a.commands(0,5100);
    require(a.driver().steer==0,"Idle commands accumulated old elapsed time into a new press");
    prepareKeyboardApp(a);
    // Source event order and read-only driver(false), also used during race
    // setup. Catch-up ticks must all read the same host sample.
    a.input.down['D']=true;a.sampleHostSteering(1000);
    require(a.driver(false).steer==0,"Race-start snapshot advanced keyboard time");
    a.sampleHostSteering(1016);const auto sampled=a.driver(false);
    require(sampled.originalSteeringAnalog==0x947b&&sampled.steer==.25f,"App did not publish the source raw 16ms sample");
    unsigned ticks=0;a.advanceHostClock(.1,[&]{const auto d=a.driver();++ticks;
        require(d.steer==sampled.steer&&d.originalSteeringAnalog==sampled.originalSteeringAnalog,"Actual App catch-up advanced host keyboard time");});
    require(ticks>=5,"Catch-up fixture did not run multiple actual App ticks");
    a.input.down['D']=false;a.sampleHostSteering(1033);
    require(a.driver().originalSteeringAnalog==43581,"App applied release before prior held interval");
    a.sampleHostSteering(1050);require(a.driver().originalSteeringAnalog==0x947b,"App release used physics dt rather than host elapsed ms");
    // Press and release on host frames without a physics step still produces
    // the exact sampled intermediate ADC at the next solver opportunity.
    prepareKeyboardApp(a);a.input.down['A']=true;a.sampleHostSteering(1000);
    a.input.down['A']=false;a.sampleHostSteering(1007);
    require(a.driver().originalSteeringAnalog==30474,"Zero-tick host samples lost short keyboard input");
    // Idle connected devices cannot alter the ramp or force a release snap.
    prepareKeyboardApp(a);a.input.connected=true;
    a.input.down['D']=true;a.sampleHostSteering(1000);a.sampleHostSteering(1101);
    require(a.driver().originalSteeringAnalog==0xff7f,"Idle pad truncated full keyboard ADC");
    a.input.down['D']=false;a.sampleHostSteering(1117);a.sampleHostSteering(1133);
    require(a.driver().steer==1,"Idle pad discarded source release plateau");
    a.input.pad.Gamepad.sThumbLX=-20000;const auto analog=a.driver();
    require(analog.steer<0&&analog.originalSteeringAnalog==-1,"Active pad failed to bypass keyboard raw ADC");
    a.input.pad.Gamepad.sThumbLX=0;require(a.driver().steer==0,"Pad release resurrected keyboard return");
    // Explicit saved smoothing remains the owner's choice, not bypassed by
    // the exact raw-keyboard channel. It does not change raw ramp scheduling.
    prepareKeyboardApp(a);a.steeringSmoothing.setAmount(.5f);
    a.input.down['D']=true;a.sampleHostSteering(1000);a.sampleHostSteering(1101);
    const auto smooth=a.driver();require(smooth.steer>0&&smooth.steer<1&&smooth.originalSteeringAnalog==-1,"Raw keyboard ADC bypassed selected smoothing");
    // Each suspension must discard old direction and elapsed time. Reading a
    // held button again after resume starts from neutral, not its prior lock.
    for(unsigned boundary=0;boundary<8;++boundary){
        prepareKeyboardApp(a);a.input.down['D']=true;a.sampleHostSteering(1000);a.sampleHostSteering(1101);
        require(a.driver().steer==1,"Lifecycle fixture did not reach full lock");
        switch(boundary){case 0:a.active=false;break;case 1:a.menu=true;break;case 2:a.paused=true;break;
            case 3:a.loadingActive=true;break;case 4:a.legendVisitActive=true;break;case 5:a.preRaceDialogueActive=true;break;
            case 6:a.setHostDrivingControlsBlocked(true);break;case 7:a.multiplayer.active=a.multiplayer.waiting=true;break;}
        a.sampleHostSteering(9000);
        a.active=true;a.menu=a.paused=a.loadingActive=a.legendVisitActive=a.preRaceDialogueActive=false;
        a.setHostDrivingControlsBlocked(false);a.multiplayer={};
        a.sampleHostSteering(9100);require(a.driver().steer==0,"Resume consumed suspended wall time or stale raw lock");
        a.sampleHostSteering(9116);require(a.driver().originalSteeringAnalog==0x947b,"Resume did not restart the source ramp");
    }
    // A disconnected controller resets its old state; keyboard restarts with
    // a new timestamp rather than inheriting the previous analog direction.
    prepareKeyboardApp(a);a.input.connected=true;a.input.pad.Gamepad.sThumbLX=25000;
    a.sampleHostSteering(1000);require(a.driver().steer>0,"Controller fixture not active");
    a.input={};a.input.down['A']=true;a.sampleHostSteering(1100);
    require(a.driver().steer==0,"Device loss retained old analog steering");
    a.sampleHostSteering(1116);require(a.driver().steer<0,"Keyboard did not resume after device loss");
    // Experimental physics retains its old response. The sampled Flycast
    // state must not leak a full raw ADC into that intentional alternate mode.
    prepareKeyboardApp(a);a.originalHandling=false;a.input.down['D']=true;
    a.sampleHostSteering(1000);a.sampleHostSteering(1200);
    const auto legacy=a.driver();require(std::abs(legacy.steer-.1f)<1e-6f&&legacy.originalSteeringAnalog==-1,"Experimental response changed");
    std::cout<<"PASS "<<checks<<" actual App keyboard and analog checks; source sampling, catch-up, zero-tick input, raw ADC, smoothing, controller arbitration and suspension. No window/audio/settings/network opened.\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
