#include "online_race_simulation.h"
#include "online_race_timeline.h"
#include "original_host_input.h"
#include "audio.h"
#include <algorithm>
#include <iostream>
using namespace idas3;using namespace idas3::original;
namespace {
unsigned checks=0;
void require(bool b,const char* why){++checks;if(!b)throw std::runtime_error(why);}
OnlineRaceSetup setup(){OnlineRaceSetup s;s.condition=0;s.collisions=false;for(auto& p:s.profiles)p=makeOriginalFreshBattleProfile();return s;}
OriginalVehicleInputs neutral(){OriginalHostInputState s;return adaptOriginalHostInput(s,{},true,false,0);}
bool sourceStops(const std::vector<OriginalEngineCommand>& cmds){
 if(cmds.size()!=4)return false;for(unsigned i=0;i<4;++i)if(cmds[i]!=OriginalEngineCommand{OriginalEngineCommandTarget::Continuous,i,0xa6,0})return false;return true;
}
void compareOutputs(const OnlineRaceFrame&a,const OnlineRaceFrame&b){for(unsigned i=0;i<2;++i){require(a.engine[i]==b.engine[i],"Rollback changed continuous engine/one-shot outputs");require(a.tires[i]==b.tires[i],"Rollback changed tire outputs");}}
void boundary(const std::filesystem::path& root,unsigned winner){
 OnlineRaceSimulation sim(root,setup());const auto idle=neutral();
 for(unsigned i=0;i<300;++i)sim.step({idle,idle});
 // Isolated rule boundary, not an end-to-end completed course claim. No
 // vehicle state or random seed is injected; real rules observe the goal.
 auto& rules=const_cast<OriginalRaceRules&>(sim.rules(winner));auto state=rules.state();state.progress.index=rules.rules().goalIndex;rules.restoreNumericalState(state);
 const auto before=sim.checkpoint();const auto crossed=sim.step({idle,idle});const auto crossedHash=sim.digest();const auto afterCross=sim.checkpoint();
 require(crossed.rules[winner].finished&&resolveOnlineRaceWinner(crossed.states)==int(winner),"First finish did not settle winner");
 for(unsigned i=0;i<2;++i)require(!crossed.engine[i].empty()&&!sourceStops(crossed.engine[i]),"Terminal frame stopped engine before next child Init");
 const std::array<unsigned,2> seed{sim.car(0).sharedRandomSeed(),sim.car(1).sharedRandomSeed()};
 const auto stopped=sim.step({idle,idle});const auto stoppedHash=sim.digest();const auto afterStop=sim.checkpoint();
 for(unsigned i=0;i<2;++i){require(sourceStops(stopped.engine[i]),"Next terminal frame did not emit exact four source stop commands");
  require(stopped.tires[i]==std::vector<OriginalTireCommand>{{OriginalTireCommandType::Stop,0}},"Terminal tires failed to use source disabled path");
  require(sim.car(i).sharedRandomSeed()==seed[i]*1103515245u+12345u,"Stopped idle engine/tire consumed an extra RNG draw");}
 const auto later=sim.step({idle,idle});const auto laterHash=sim.digest();
 for(unsigned i=0;i<2;++i)require(later.engine[i].empty(),"Stopped voices were stopped or stepped twice");
 sim.restore(afterStop);compareOutputs(later,sim.step({idle,idle}));require(sim.digest()==laterHash,"Post-stop checkpoint lost stopped latch");
 sim.restore(afterCross);compareOutputs(stopped,sim.step({idle,idle}));require(sim.digest()==stoppedHash,"Pending stop checkpoint did not regenerate commands/digest");
 sim.restore(before);compareOutputs(crossed,sim.step({idle,idle}));require(sim.digest()==crossedHash,"Pre-terminal rollback did not restore enabled engine");
 compareOutputs(stopped,sim.step({idle,idle}));require(sim.digest()==stoppedHash,"Terminal replay lost sound latch state");
 OnlineRaceSimulation restarted(root,setup());const auto first=restarted.step({idle,idle});for(unsigned i=0;i<2;++i)require(!first.engine[i].empty()&&!sourceStops(first.engine[i]),"Fresh race retained disabled state");
}
std::array<OriginalVehicleInputs,2> tape(unsigned frame){auto a=neutral(),b=a;b.analog.steering=std::uint16_t((frame/7)%2?100u<<8:156u<<8);return {a,b};}
void confirmedNaturalTimeout(const std::filesystem::path& root){
 const auto selected=setup();OnlineRaceSimulation baseline(root,selected);
 std::vector<std::uint64_t> hashes;std::vector<OnlineRaceFrame> events;unsigned terminal=0;
 for(unsigned f=0;f<20000;++f){events.push_back(baseline.step(tape(f)));hashes.push_back(baseline.digest());
  if(!terminal&&resolveOnlineRaceWinner(events.back().states)!=-3)terminal=f+1;
  if(terminal&&f+1>=terminal+24)break;}
 require(terminal>5000,"Natural timeout fixture did not exercise the original timer");
 for(unsigned i=0;i<2;++i){require(!sourceStops(events[terminal-1].engine[i]),"Natural timeout stopped one frame early");require(sourceStops(events[terminal].engine[i]),"Natural timeout missed next-tick gate");}
 OnlineRaceSimulation sim(root,selected);EngineAudio output;output.enabled=false;output.selectOriginalEngine(root,selected.profiles[0]);
 unsigned emitted=0,stopBatches=0;OnlineRaceTimeline timeline(sim,39281,true,[&](const OnlineRaceFrame&e,std::uint64_t hash){
  require(e.frame==emitted++,"Confirmed output repeated/skipped a frame");require(hash==hashes.at(e.frame),"Confirmed terminal state differs from canonical baseline");compareOutputs(e,events.at(e.frame));
  if(sourceStops(e.engine[0]))++stopBatches;output.applyConfirmedOnlineAudio(e.engine[0],e.tires[0],e.driving[0].acceptedContactCues);
 });
 unsigned delivered=0;
 for(unsigned f=0;f<events.size();++f){const auto inputs=tape(f);require(timeline.localInput(inputs[0])&&timeline.advance(),"Timeline unexpectedly stalled");
  if(f%8==7||f+1==events.size()){
   std::vector<OnlineInputRecord> batch;for(unsigned j=delivered;j<=f;++j)batch.push_back({39281,j,1,tape(j)[1]});std::reverse(batch.begin(),batch.end());
   require(timeline.receive(batch),"Reordered terminal inputs rejected");timeline.reconcile();require(timeline.receive(batch),"Duplicate confirmed batch rejected");timeline.reconcile();delivered=f+1;
  }else require(emitted==delivered,"Unconfirmed predicted sound escaped to mixer");
 }
 require(timeline.metrics().rollbacks>0,"Delayed input never exercised rollback");require(emitted==events.size()&&stopBatches==1,"Terminal stop was not confirmed exactly once");
 require(output.engineStatistics().simulationFrames==emitted,"Mixer received speculative or duplicate audio");require(sim.digest()==hashes.back(),"Final agreed state differs");
 // Confirmed source stop commands are consumed by the real mixer. Rendering
 // only advances voices; it receives no mutable simulation seed or car state.
 const auto digest=sim.digest();for(unsigned i=0;i<44100;++i)output.renderStereo(0,0,0,0,true);require(sim.digest()==digest,"Confirmed audio rendering changed authority state");
 std::cout<<"Natural timeout terminal tick="<<terminal-1<<", confirmed="<<emitted<<", rollback_count="<<timeline.metrics().rollbacks<<", stop_batches="<<stopBatches<<"\n";
}
}
int main(int argc,char**argv)try{if(argc!=2)throw std::runtime_error("Native root required");for(unsigned i=0;i<2;++i)boundary(argv[1],i);confirmedNaturalTimeout(argv[1]);
 std::cout<<"PASS "<<checks<<" shared finish sound/RNG/checkpoint/confirmed-mixer checks; in-process only, no EOS or linked-cabinet transport claim.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
