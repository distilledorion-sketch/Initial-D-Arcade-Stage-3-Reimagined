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
    // Fixed-tick keyboard response from release .56, including frames where
    // rendering samples input more frequently than the car advances.
    const auto closeEnough=[](float actual,float expected){ return std::abs(actual-expected)<1e-5f; };
    a.input.down['D']=true;
    for(int i=0;i<30;++i)a.commands(0,1000+i);
    require(a.steering==0,"Input polling alone advanced keyboard steering");
    require(closeEnough(a.driver().steer,.1f),"First right key tick");
    a.advanceHostClock(.06,[&]{a.driver();});
    require(closeEnough(a.steering,.4f),"Keyboard ramp skipped catch-up ticks");
    a.input.down['D']=false;
    require(closeEnough(a.driver().steer,.4f-10.f/60.f),"Keyboard release");
    a.input.down['A']=true;
    require(closeEnough(a.driver().steer,.4f-22.f/60.f),"Keyboard countersteer");
    // A neutral pad must let a keyboard release finish. Moving the pad
    // takes control immediately, including its first centered sample.
    prepareKeyboardApp(a);a.input.connected=true;a.input.down['D']=true;
    for(int i=0;i<12;++i)a.driver();
    a.input.down['D']=false;
    require(closeEnough(a.driver().steer,1.f-10.f/60.f),"Idle pad replaced the key release");
    a.input.pad.Gamepad.sThumbLX=16384;
    require(a.driver().steer==.7f,"Pad did not take control");
    a.input.pad.Gamepad.sThumbLY=24576;
    require(a.driver().steer==.75f,"Default pad ignored the current diagonal");
    a.input.pad.Gamepad.sThumbLY=0;
    require(a.driver().steer==.7f,"Default pad kept a released diagonal");
    a.input.pad.Gamepad.sThumbLX=0;
    require(a.driver().steer==0,"Pad release inherited the keyboard ramp");
    a.controllerResponse=idas3::original::ControllerResponse::FlycastWheel;
    a.input.pad.Gamepad.sThumbLX=16384;
    require(a.driver().steer==.8f,"Wheel horizontal response changed");
    a.input.pad.Gamepad.sThumbLY=24576;
    require(a.driver().steer==.8f,"Wheel picked up an unrelated axis");
    prepareKeyboardApp(a);a.steeringSmoothing.setAmount(.5f);a.input.down['D']=true;
    const float smoothed=a.driver().steer;
    require(smoothed>0&&smoothed<.1f,"Bypassed saved steering smoothing");
    a.setHostDrivingControlsBlocked(true);
    require(a.driver().steer==0,"Blocked input retained steering");
    std::cout<<"PASS "<<checks<<" keyboard, pad and wheel checks using the actual driver.\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
