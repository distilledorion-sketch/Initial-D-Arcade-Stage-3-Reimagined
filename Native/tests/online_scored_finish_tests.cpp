#include "online_scored_driver.h"
#include <iostream>

using namespace idas3::original;
namespace {
void require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
void scoredFinish(const std::filesystem::path& root,unsigned driver){
    OnlineRaceSetup setup;setup.condition=0;setup.boost=false;setup.collisions=false;
    for(unsigned slot=0;slot<2;++slot){
        auto& p=setup.profiles[slot];p=makeOriginalFreshBattleProfile();p.setu(16,slot?8:0);p.setByte(164,63);
    }
    OnlineRaceSimulation sim(root,setup);std::array<OriginalHostInputState,2> states;
    int winner=-3;unsigned frames=0;
    while(frames<24000&&winner==-3){
        const auto before=sim.digest();const auto controls=onlineScoredDriver(sim,driver);
        require(sim.digest()==before,"Diagnostic guidance mutated the race");
        const int adc=128+int(std::lround(controls.steering*80));
        const auto axis=std::int16_t(std::clamp((adc<<8)+128-32768,-32768,32767));
        const auto wheel=controllerHostControls(axis,0,255,0,ControllerResponse::FlycastWheel,0);
        std::array<OriginalVehicleInputs,2> inputs;
        for(unsigned slot=0;slot<2;++slot)
            inputs[slot]=adaptOriginalHostInput(states[slot],slot==driver?wheel:OriginalHostControls{},true,false,frames);
        winner=resolveOnlineRaceWinner(sim.step(inputs).states);++frames;
    }
    require(winner==int(driver),"Ordinary input did not produce the expected scored winner");
    require(sim.rules(driver).state().phase==OriginalRacePhase::Finished,"Winner did not cross the actual finish");
    require(sim.rules(1-driver).state().phase==OriginalRacePhase::TimeUp,"Stationary opponent fixture changed");
    require(frames>10000&&sim.rules(driver).state().progress.index>=sim.rules(driver).rules().goalIndex,"Finish bypassed the full course");
    std::cout<<"PASS Myogi slot "<<driver<<" finish after "<<frames<<" original frames; guidance read-only, wheel ADC, no rule injection\n";
}
}
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("Expected native data root");
    scoredFinish(argv[1],0);scoredFinish(argv[1],1);return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
