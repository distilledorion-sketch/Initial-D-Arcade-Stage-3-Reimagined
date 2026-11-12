#include "online_race_test_driver.h"
#include "online_race_link.h"
#include "replay_car_smoke_playback.h"
#include <iostream>
#include <vector>
using namespace idas3;
using namespace idas3::original;
namespace {
unsigned checks=0;
void check(bool condition,const char* why){++checks;if(!condition)throw std::runtime_error(why);}
OnlineRaceSetup setup(unsigned scenario){
    OnlineRaceSetup out;out.condition=std::array{6u,6u,14u,15u,14u,16u}.at(scenario);
    out.night=scenario==1;out.wet=scenario>=4;out.boost=true;out.showcaseFrames=31;
    for(unsigned slot=0;slot<2;++slot){auto& p=out.profiles[slot];p=makeOriginalFreshBattleProfile();
        p.setu(16,slot?19:scenario%2?3:0);p.setu(4,out.condition/2);p.setu(8,out.night);p.setu(32,out.wet);
        p.setu(1180,(scenario+slot)%2?128:0);p.setByte(164,63);}
    return out;
}
constexpr unsigned frames=600,scenarios=6;
struct Sample {std::array<OriginalVehicleInputs,2> inputs;std::uint64_t physics;std::array<std::uint64_t,2> smoke;std::array<ReplayCarSmokeFrame,2> smokeInputs;};
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::runtime_error("Native root required");
    const std::filesystem::path root=argv[1];
    unsigned emitting=0,rollbacks=0,sharedRandomTicks=0,digestMutations=0;std::size_t maxHistory=0;
    for(unsigned scenario=0;scenario<scenarios;++scenario){
        const auto selected=setup(scenario);OnlineRaceSimulation sim(root,selected);
        std::array<OriginalHostInputState,2> host{};std::vector<Sample> samples;
        std::array<OriginalTireAudioState,2> expectedTires{};
        OnlineRaceSimulation::Checkpoint saved;
        for(unsigned tick=0;tick<frames;++tick){Sample sample;
            for(unsigned slot=0;slot<2;++slot){auto controls=onlineRaceTestControls(sim,slot,tick);
                if(tick>240&&tick%90<20){controls.throttle=0;controls.brake=1;}
                sample.inputs[slot]=adaptOriginalHostInput(host[slot],controls,true,false,tick);}
            sim.step(sample.inputs);sample.physics=sim.digest();
            check(sim.lastFrame().smokeAvailable,"Original race omitted recorded smoke inputs");
            sample.smokeInputs=sim.lastFrame().smokeInputs;
            for(unsigned view=0;view<2;++view){const auto& smoke=sim.smoke(view);sample.smoke[view]=smoke.digest();
                sharedRandomTicks+=smoke.randomState()!=sample.smokeInputs[view].randomBefore;
                const auto& angular=sim.lastFrame().driving[view].vehicle.feedback;
                requestOriginalTireAudio(expectedTires[view],0,angular.feedbackSpeed,0,angular.feedbackArgument,angular.feedbackStrength);
                auto expectedSeed=smoke.randomState();
                const auto tireCommands=stepOriginalTireAudio(expectedTires[view],0,expectedSeed);
                check(tireCommands==sim.lastFrame().tires[view],"Tire sound did not consume the post-smoke stream");
                check(expectedSeed==sim.car(view).contactCompletion().randomSeed0C37C778,"Smoke/tire random state did not return to the driving checkpoint");
                check(smoke.frames()==sim.frame(),"Smoke clock skipped or duplicated a simulation tick");
                check(smoke.system().order().size()<=4&&smoke.system().particleCount()<=60,"Smoke pool escaped source bounds");
                emitting+=smoke.system().particleCount()>0;
                for(auto index:smoke.system().order()){
                    check(smoke.system().cloud(index).car==originalRoadsideDustOwnerSelection(0,(selected.profiles[view].u(1180)&128)!=0).car,"Online view chose the wrong effect owner");
                    if(selected.night||selected.wet)check(smoke.system().cloud(index).color!=0xff8b5439,"Night/wet view emitted roadside dust");
                }
            }
            if(tick==399){
                const auto current=sim.checkpoint();
                for(unsigned view=0;view<2;++view){
                    // Deliberately corrupt ONLY visual lifetime/seed state.
                    // The online digest must detect it before a future solver
                    // frame happens to consume a different random value.
                    const_cast<CarSmokeState&>(sim.smoke(view)).reset(selected.wet);
                    check(sim.digest()!=sample.physics,"Network digest ignored smoke state affecting future RNG");
                    sim.restore(current);check(sim.digest()==sample.physics,"Smoke digest mutation did not restore");++digestMutations;
                }
            }
            if(tick==199)saved=sim.checkpoint();
            samples.push_back(sample);
        }
        sim.restore(saved);
        for(unsigned view=0;view<2;++view)check(sim.smoke(view).digest()==samples[199].smoke[view],"Checkpoint did not restore smoke/RNG immediately");
        for(unsigned tick=200;tick<frames;++tick){sim.step(samples[tick].inputs);
            check(sim.digest()==samples[tick].physics,"Replayed checkpoint changed physics");
            for(unsigned view=0;view<2;++view)check(sim.smoke(view).digest()==samples[tick].smoke[view],"Replayed checkpoint changed smoke particles/owners/RNG");}
        std::array<std::array<std::vector<ReplayCarSmokeFrame>,2>,2> confirmedSmoke;
        const auto record=[&](unsigned peer,const OnlineRaceFrame& frame){
            check(frame.smokeAvailable&&frame.frame==confirmedSmoke[peer][0].size(),"Confirmation duplicated/skipped an effect tick");
            for(unsigned view=0;view<2;++view)confirmedSmoke[peer][view].push_back(frame.smokeInputs[view]);
        };
        OnlineRaceSimulation a(root,selected),b(root,selected);
        OnlineRaceLink first(a,91,true,[&](const auto& frame,auto){record(0,frame);}),
            second(b,91,false,[&](const auto& frame,auto){record(1,frame);});
        for(unsigned tick=0;tick<frames;++tick){
            check(first.step(samples[tick].inputs[0])&&second.step(samples[tick].inputs[1]),"Delayed smoke fixture stalled unexpectedly");
            if(tick%17==16||tick+1==frames){
                for(unsigned settle=0;settle<3;++settle){auto pa=first.packet(),pb=second.packet();first.receive(pb);second.receive(pa);first.reconcile();second.reconcile();}
                check(a.digest()==samples[tick].physics&&b.digest()==samples[tick].physics,"Corrected peer physics differs");
                for(unsigned view=0;view<2;++view){
                    check(a.smoke(view).digest()==samples[tick].smoke[view],"Host retained speculative smoke");
                    check(b.smoke(view).digest()==samples[tick].smoke[view],"Client retained speculative smoke");}
            }
            maxHistory=std::max(maxHistory,first.timeline().memoryBytes()+second.timeline().memoryBytes());
        }
        rollbacks+=unsigned(first.timeline().metrics().rollbacks+second.timeline().metrics().rollbacks);
        check(first.verifiedPeerFrames()==frames&&second.verifiedPeerFrames()==frames,"Smoke fixture did not verify all peer frames");
        for(unsigned peer=0;peer<2;++peer)for(unsigned view=0;view<2;++view){
            const auto& recorded=confirmedSmoke[peer][view];check(recorded.size()==frames,"Confirmed smoke recording is incomplete");
            CarSmokeState playback;ReplayCarSmokePlayback timeline;
            timeline.load({reinterpret_cast<const std::uint8_t*>(recorded.data()),recorded.size()*sizeof(ReplayCarSmokeFrame)},playback);
            for(unsigned tick=0;tick<frames;++tick){
                check(std::memcmp(&recorded[tick],&samples[tick].smokeInputs[view],sizeof(ReplayCarSmokeFrame))==0,"Recording retained a speculative contact/seed/owner");
                timeline.seek(tick+1,playback);check(playback.digest()==samples[tick].smoke[view],"Confirmed replay differs from canonical live smoke");
            }
            for(unsigned tick:{0u,399u,299u,599u,300u,20u}){timeline.seek(tick+1,playback);check(playback.digest()==samples[tick].smoke[view],"Corrected recording seek differs");}
        }
        unsigned unconfirmed=0;while(first.step(samples.back().inputs[0])){++unconfirmed;check(unconfirmed<=OnlineRaceTimeline::capacity,"Missing peer did not bound prediction");}
        for(unsigned repeat=0;repeat<5;++repeat)first.reconcile();
        check(confirmedSmoke[0][0].size()==frames&&confirmedSmoke[0][1].size()==frames,"Stall persisted speculative smoke");
        std::cout<<"PASS scenario="<<scenario<<" frames="<<frames<<'\n'<<std::flush;
    }
    check(emitting>0&&rollbacks>0&&sharedRandomTicks>0&&digestMutations==scenarios*2,"Fixture missed emission, rollback or source RNG coupling");
    std::cout<<"PASS checks="<<checks<<" emitting_views="<<emitting<<" rollbacks="<<rollbacks<<" max_history_bytes_both_peers="<<maxHistory
        <<" shared_random_ticks="<<sharedRandomTicks<<" digest_mutations="<<digestMutations
        <<"; original courses/day/night/rain/snow, per-view owner, checkpoint replay, late input, shared smoke-to-tire RNG and confirmed replay seeds.\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
