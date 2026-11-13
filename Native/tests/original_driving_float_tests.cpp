#include "original_driving_float.h"
#include "original_driving_session.h"
#include "original_start_grid.h"
#include "online_race_simulation.h"
#include <bit>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace idas3::original;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Pass native asset root");
#if defined(_M_X64) || defined(_M_IX86) || defined(__SSE2__)
    struct Restore {unsigned value=_mm_getcsr();~Restore(){_mm_setcsr(value);}} restore;
    constexpr unsigned controls[]={0x9fc0,0x1f80,0x3f80,0x5f80,0x7f80};
    std::vector<std::uint64_t> baseline;
    unsigned comparisons=0;
    for(unsigned control:controls){
        _mm_setcsr(control);
        {
            OriginalDrivingFloatScope scope;
            require((_mm_getcsr()&0xe040u)==0x8040u,"Solver did not select nearest/flush mode");
            volatile float tiny=std::bit_cast<float>(0x00800000u);
            volatile float half=.5f;
            const float result=tiny*half;
            require(std::bit_cast<unsigned>(result)==0,"Arithmetic failed to flush underflow");
        }
        require(_mm_getcsr()==control,"Scope leaked caller floating-point state");
        try{OriginalDrivingFloatScope scope;throw 1;}catch(int){}
        require(_mm_getcsr()==control,"Exception leaked caller floating-point state");
        OriginalDrivingSelection selection;selection.bodyContactEnabled=false;
        selection.physics=makeOriginalFreshTimeAttackSelection(0,6,OriginalWeather::Dry);
        const auto pose=originalStartPose(6,1);
        OriginalDrivingSession session;session.reset(argv[1],selection,pose.position,pose.angles);
        require(_mm_getcsr()==control,"Initialization changed caller floating-point state");
        for(unsigned frame=0;frame<1200;++frame){
            OriginalVehicleInputs input;input.calibration={128,32,32};input.automaticMode=true;input.gearEnabled=frame>=60;
            input.analog={std::uint16_t((frame%500<250?144:112)<<8),std::uint16_t((frame%500<400?171:64)<<8),std::uint16_t((frame%500>=400?171:64)<<8)};
            session.tick(input);
            require(_mm_getcsr()==control,"Driving changed caller floating-point state");
            const auto digest=session.rollbackDigest();
            if(control==controls[0])baseline.push_back(digest);
            else {require(digest==baseline.at(frame),"Caller floating-point mode changed driving");++comparisons;}
        }
    }
    std::vector<std::uint64_t> onlineBaseline;
    unsigned onlineComparisons=0;
    for(unsigned control:controls){
        _mm_setcsr(control);
        OnlineRaceSetup setup;setup.condition=6;setup.boost=true;setup.collisions=true;
        setup.profiles={makeOriginalFreshBattleProfile(),makeOriginalFreshBattleProfile()};
        OnlineRaceSimulation simulation(argv[1],setup);
        require(_mm_getcsr()==control,"Online initialization changed caller floating-point state");
        for(unsigned frame=0;frame<900;++frame){
            std::array<OriginalVehicleInputs,2> inputs;
            for(unsigned slot=0;slot<2;++slot){
                auto& input=inputs[slot];input.calibration={128,32,32};input.automaticMode=true;
                input.analog={std::uint16_t((frame%400<200?136:120)<<8),std::uint16_t((slot==0||frame%400<200?171:118)<<8),16384};
            }
            simulation.step(inputs);
            require(_mm_getcsr()==control,"Online step changed caller floating-point state");
            const auto digest=simulation.digest();
            if(control==controls[0])onlineBaseline.push_back(digest);
            else {require(digest==onlineBaseline.at(frame),"Caller floating-point mode changed online driving");++onlineComparisons;}
        }
    }
    std::cout<<"PASS "<<comparisons<<" driving and "<<onlineComparisons<<" two-player digests under5 host modes; real underflow, reset/tick and exception restoration.\n";
#else
    // Register-mode and captured-trace checks are run on the release platform.
    // Other targets compile the architecture-specific guard with the player.
    OriginalDrivingFloatScope scope;
    std::cout<<"PASS non-x86 scope construction; runtime parity requires device validation.\n";
#endif
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
