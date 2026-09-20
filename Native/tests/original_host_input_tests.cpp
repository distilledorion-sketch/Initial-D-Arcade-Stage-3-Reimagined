#include "original_host_input.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace idas3;
using namespace idas3::original;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
OriginalControls conditioned(const OriginalVehicleInputs& input){return conditionOriginalInputs(input.analog,input.calibration,{0,0},verifiedGds0033InputConstants(),0);}
int main()try{
    OriginalHostInputState state;
    auto input=adaptOriginalHostInput(state,{},true,true,12);auto output=conditioned(input);
    require(input.calibration.steeringWord==128&&input.calibration.throttleWord==32&&input.calibration.brakeWord==32,"Original default calibration changed");
    require(output.steering==0&&output.throttle==0&&output.brake==0,"Idle native controls must reach original neutral");
    require(input.automaticMode&&input.gearEnabled&&input.elapsedFrames0C900E84==12,"Native race state was lost");
    input=adaptOriginalHostInput(state,{1,1,1},false,true,13);output=conditioned(input);
    require(output.steering==-1&&output.throttle==1&&output.brake==1,"Physical right must reach original negative full lock");
    input=adaptOriginalHostInput(state,{-1,-1,2},false,false,14);output=conditioned(input);
    require(output.steering==1&&output.throttle==0&&output.brake==1,"Physical left and bounded pedal mapping failed");
    float priorSteering=2,priorPedal=-1;
    for(int index=0;index<=2000;++index){
        const float fraction=float(index)/2000.f;
        output=conditioned(adaptOriginalHostInput(state,{fraction*2-1,fraction,1-fraction},false,true,index));
        require(output.steering<=priorSteering&&output.throttle>=priorPedal,"ADC conditioning must be monotonic in the original axis");
        require(std::abs(output.steering+(fraction*2-1))<=.006251f,"Steering lost more than half an original ADC step");
        require(std::abs(output.throttle-fraction)<=.004674f,"Pedal lost more than half an original ADC step");
        priorSteering=output.steering;priorPedal=output.throttle;
    }
    OriginalHostControls shifts;shifts.shiftUp=true;
    require(adaptOriginalHostInput(state,shifts,false,true,0).pressedByte==0x20,"Upshift must generate the original event bit");
    for(int frame=1;frame<120;++frame)require(adaptOriginalHostInput(state,shifts,false,true,frame).pressedByte==0,"Held shift must not repeat every physics tick");
    shifts.shiftDown=true;
    require(adaptOriginalHostInput(state,shifts,false,true,120).pressedByte==0x10,"A second button must retain its independent edge");
    shifts={};require(adaptOriginalHostInput(state,shifts,false,true,121).pressedByte==0,"Releasing buttons must not emit a shift");
    shifts.shiftUp=true;shifts.shiftDown=true;
    require(adaptOriginalHostInput(state,shifts,false,true,122).pressedByte==0x30,"Simultaneous original button edges must be preserved");
    bool rejected=false;try{adaptOriginalHostInput(state,{std::numeric_limits<float>::quiet_NaN(),0,0},false,true,123);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"Nonfinite native input accepted");
    std::cout<<"PASS native-to-original input range, ADC quantization, monotonicity, gear edges and finite-input contract\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
