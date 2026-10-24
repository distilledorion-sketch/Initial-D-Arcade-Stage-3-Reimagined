#include "online_scored_driver.h"
#include <iostream>

using namespace idas3::original;
namespace {
void require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
void scoredFinish(const std::filesystem::path& root,unsigned driver,bool movingOpponent){
    OnlineRaceSetup setup;setup.condition=0;setup.boost=false;setup.collisions=false;
    for(unsigned slot=0;slot<2;++slot){
        auto& p=setup.profiles[slot];p=makeOriginalFreshBattleProfile();p.setu(16,slot?8:0);p.setByte(164,63);
    }
    OnlineRaceSimulation sim(root,setup);std::array<OriginalHostInputState,2> states;
    int winner=-3;unsigned frames=0;std::array<unsigned,2> runningFrames{};
    while(frames<24000&&winner==-3){
        std::array<OriginalVehicleInputs,2> inputs;
        for(unsigned slot=0;slot<2;++slot){
            const auto before=sim.digest();const auto controls=onlineScoredDriver(sim,slot);
            require(sim.digest()==before,"Diagnostic guidance mutated the race");
            const int adc=128+int(std::lround(controls.steering*80));
            const auto axis=std::int16_t(std::clamp((adc<<8)+128-32768,-32768,32767));
            const auto wheel=controllerHostControls(axis,0,255,0,ControllerResponse::FlycastWheel,0);
            if(sim.rules(slot).state().phase==OriginalRacePhase::Running)++runningFrames[slot];
            // Delay the trailing driver's launch after GO, not during the
            // countdown. Both cars must remain subject to the original timer.
            const bool driving=slot==driver||(movingOpponent&&runningFrames[slot]>=180);
            inputs[slot]=adaptOriginalHostInput(states[slot],driving?wheel:OriginalHostControls{},true,false,frames);
        }
        winner=resolveOnlineRaceWinner(sim.step(inputs).states);++frames;
    }
    require(winner==int(driver),"Ordinary input did not produce the expected scored winner");
    require(sim.rules(driver).state().phase==OriginalRacePhase::Finished,"Winner did not cross the actual finish");
    if(movingOpponent){
        require(sim.rules(1-driver).state().phase==OriginalRacePhase::Running,"Trailing driver was not still racing at the leader's finish");
        require(sim.rules(1-driver).state().progress.index>sim.rules(1-driver).rules().goalIndex*9/10,"Trailing driver did not traverse the course");
    }else require(sim.rules(1-driver).state().phase==OriginalRacePhase::TimeUp,"Stationary opponent fixture changed");
    require(frames>10000&&sim.rules(driver).state().progress.index>=sim.rules(driver).rules().goalIndex,"Finish bypassed the full course");
    std::cout<<"PASS Myogi slot "<<driver<<" finish after "<<frames<<" original frames; "
        <<(movingOpponent?"opponent still racing":"opponent timed out")<<"; guidance read-only, wheel ADC, no rule injection\n";
}
}
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("Expected native data root");
    for(bool movingOpponent:{false,true})for(unsigned driver=0;driver<2;++driver)
        scoredFinish(argv[1],driver,movingOpponent);
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
