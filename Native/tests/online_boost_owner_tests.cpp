#include "online_race_simulation.h"
#include "original_battle_metrics.h"
#include "original_host_input.h"
#include "online_race_test_driver.h"
#include <bit>
#include <iostream>
#include <stdexcept>
using namespace idas3::original;
namespace {
unsigned checks=0,changedGaps=0,cases=0;
void require(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
void run(const std::filesystem::path& root){
    // The independently executed original outer owner orders 0680C0 path,
    // gates/outcome/timers, then 062DE0 -> 06A420 gap -> 159920 solver.
    // Here real driving changes both poses; no progress/pose mutation or
    // replacement projection is allowed to hide a stale online owner value.
    for(unsigned condition:{0u,1u,6u,7u,14u,15u})for(bool wet:{false,true})for(bool boost:{false,true}){
        OnlineRaceSetup setup;setup.condition=condition;setup.wet=wet;setup.boost=boost;
        for(unsigned slot=0;slot<2;++slot){setup.profiles[slot]=makeOriginalFreshBattleProfile();setup.profiles[slot].setu(16,slot?18:0);}
        OnlineRaceSimulation sim(root,setup);const auto metrics=OriginalBattleMetrics::load(root,condition);
        std::array<OriginalHostInputState,2> adapters{};
        for(unsigned frame=0;frame<360;++frame){
            const float prior=metrics.advantage(sim.rules(0).state().progress,sim.rules(1).state().progress);
            std::array<OriginalVehicleInputs,2> controls;
            for(unsigned slot=0;slot<2;++slot){auto held=onlineRaceTestControls(sim,slot,frame);
                if(slot)held.throttle*=.65f;
                controls[slot]=adaptOriginalHostInput(adapters[slot],held,true,false,frame);
            }
            const auto out=sim.step(controls);
            const float current=metrics.advantage(out.states[0].progress,out.states[1].progress);
            if(out.start.runRules&&current!=prior)++changedGaps;
            for(unsigned slot=0;slot<2;++slot){
                const float expected=boost?(slot?-current:current):0.f;
                const float actual=sim.car(slot).parameters().frame.global0C901650;
                if(std::bit_cast<std::uint32_t>(actual)!=std::bit_cast<std::uint32_t>(expected)){
                    std::cerr<<"condition="<<condition<<" wet="<<wet<<" boost="<<boost<<" frame="<<frame<<" slot="<<slot
                        <<" prior_gap="<<prior<<" current_gap="<<current<<" solver_gap="<<actual<<'\n';
                    require(false,"Online solver used stale/mixed-slot progress instead of both current rule coordinates");
                }
                require(sim.car(slot).selection().physics.progressEnabled0C9015E4==unsigned(boost),"Online boost selection lost its explicit lobby setting");
                ++checks;
            }
        }
        ++cases;
    }
    require(changedGaps>1000,"Owner-order fixture did not distinguish current and previous gaps");
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::runtime_error("Usage: online_boost_owner_tests native_root");run(argv[1]);
    std::cout<<"PASS "<<cases<<" forward/reverse, dry/wet, boost on/off cases; "<<changedGaps<<" changing gaps; "<<checks<<" checks. Both solvers receive this frame's projected/rules progress.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
