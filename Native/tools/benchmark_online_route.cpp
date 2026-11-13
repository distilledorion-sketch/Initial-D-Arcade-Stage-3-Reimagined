#include "../tests/online_scored_driver.h"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

// Read-only route guidance and normal controls. Measure the three operations
// performed by OnlineRaceTimeline::simulate; no renderer, transport, audio,
// ordinary save, altered physics, or diagnostic timer extension is involved.
int main(int argc,char** argv)try{
    using namespace idas3::original;
    if(argc!=5)throw std::runtime_error("Native root, NEW output directory, condition, wet required");
    const std::filesystem::path root=argv[1],out=argv[2];
    if(std::filesystem::exists(out))throw std::runtime_error("Preserve evidence");
    std::filesystem::create_directories(out);
    OnlineRaceSetup setup;setup.condition=unsigned(std::stoul(argv[3]));setup.wet=std::stoul(argv[4])!=0;
    setup.night=true;setup.boost=true;
    for(unsigned slot=0;slot<2;++slot){auto& p=setup.profiles[slot];p=makeOriginalFreshBattleProfile();
        p.setu(16,slot?19:0);p.setu(4,setup.condition/2);p.setu(8,1);p.setu(32,setup.wet);p.setByte(164,63);}
    OnlineRaceSimulation sim(root,setup);std::array<OriginalHostInputState,2> host{};
    std::array<OnlineRaceSimulation::Checkpoint,64> history;
    struct Sample {unsigned tick,section0,section1,particles;float progress0,progress1;double step,copy,digest;std::size_t bytes;};
    std::vector<Sample> samples;samples.reserve(20000);std::uint64_t finalDigest=0;
    using Clock=std::chrono::steady_clock;
    const auto ms=[](auto a,auto b){return std::chrono::duration<double,std::milli>(b-a).count();};
    const auto row=originalRaceRuleRow(originalRaceRuleRowIndex(setup.condition,2));
    for(unsigned tick=0;tick<20000;++tick){
        std::array<OriginalVehicleInputs,2> controls;
        for(unsigned slot=0;slot<2;++slot)controls[slot]=adaptOriginalHostInput(host[slot],onlineScoredDriver(sim,slot),true,false,tick);
        const auto before=Clock::now();history[tick%64]=sim.checkpoint();const auto copied=Clock::now();
        sim.step(controls);const auto stepped=Clock::now();finalDigest=sim.digest();const auto digested=Clock::now();
        const auto& a=sim.rules(0).state();const auto& b=sim.rules(1).state();
        samples.push_back({tick,a.sectionCounter,b.sectionCounter,
            unsigned(sim.smoke(0).system().particleCount()+sim.smoke(1).system().particleCount()),
            float(a.progress.index)+a.progress.fraction,float(b.progress.index)+b.progress.fraction,
            ms(copied,stepped),ms(before,copied),ms(stepped,digested),history[tick%64].memoryBytes()});
        if(resolveOnlineRaceWinner(sim.lastFrame().states)!=-3)break;
    }
    std::ofstream csv(out/"samples.csv");csv<<"tick,section0,section1,progress0,progress1,particles,step_ms,checkpoint_ms,digest_ms,checkpoint_bytes\n"<<std::setprecision(9);
    for(const auto& s:samples)csv<<s.tick<<','<<s.section0<<','<<s.section1<<','<<s.progress0<<','<<s.progress1<<','<<s.particles<<','<<s.step<<','<<s.copy<<','<<s.digest<<','<<s.bytes<<'\n';
    std::ofstream summary(out/"result.json");
    summary<<"{\"condition\":"<<setup.condition<<",\"wet\":"<<(setup.wet?"true":"false")<<",\"ticks\":"<<samples.size()
        <<",\"winner\":"<<resolveOnlineRaceWinner(sim.lastFrame().states)<<",\"goalIndex\":"<<row.goalIndex
        <<",\"progress0\":"<<samples.back().progress0<<",\"progress1\":"<<samples.back().progress1
        <<",\"section0\":"<<samples.back().section0<<",\"section1\":"<<samples.back().section1
        <<",\"phase0\":"<<unsigned(sim.rules(0).state().phase)<<",\"phase1\":"<<unsigned(sim.rules(1).state().phase)
        <<",\"finalDigest\":\""<<std::hex<<finalDigest<<"\"}\n";
    std::cout<<"Completed "<<samples.size()<<" ticks; sections "<<samples.back().section0<<'/'<<samples.back().section1
        <<"; winner "<<resolveOnlineRaceWinner(sim.lastFrame().states)<<'\n';
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
