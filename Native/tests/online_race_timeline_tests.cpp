#include "online_race_timeline.h"
#include "online_race_link.h"
#include "imported_course.h"
#include "original_host_input.h"
#include "original_battle_metrics.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <queue>
#include <random>
using namespace idas3::original;
namespace {
void require(bool b,const char* why){if(!b)throw std::runtime_error(why);}
OriginalVehicleInputs neutral(){OriginalHostInputState state;return adaptOriginalHostInput(state,{},true,false,0);}
OnlineRaceSetup setup(unsigned scenario){
    OnlineRaceSetup s;s.condition=scenario%18;s.wet=s.condition>=16||scenario%2;s.boost=(scenario%2)!=0;
    for(unsigned i=0;i<2;++i){auto& p=s.profiles[i];p=makeOriginalFreshBattleProfile();p.setu(16,(scenario*2+i)%35);
        if(scenario%3==0)p.setByte(164,5);
        s.automatic[i]=(scenario+i)%2==0;}
    return s;
}
std::array<OriginalVehicleInputs,2> driving(const OnlineRaceSimulation& sim,std::array<OriginalHostInputState,2>& host,unsigned frame){
    std::array<OriginalVehicleInputs,2> out;
    for(unsigned slot=0;slot<2;++slot){
        const auto& car=sim.car(slot);const auto& d=car.vehicle().drive;const auto& points=car.path().points;
        std::size_t nearest=0;float best=1e30f;
        for(std::size_t i=0;i<points.size();++i){float dx=points[i][0]-d.f(0),dz=points[i][2]-d.f(8),dist=dx*dx+dz*dz;if(dist<best){best=dist;nearest=i;}}
        auto target=nearest;float length=0;const float look=std::max(10.f,d.f(0x238)*.65f);
        while(length<look&&target+1<points.size()){float x=points[target+1][0]-points[target][0],z=points[target+1][2]-points[target][2];length+=std::sqrt(x*x+z*z);++target;}
        const auto& p=points[target];const float wanted=std::atan2(-(p[0]-d.f(0)),-(p[2]-d.f(8)));
        float steer=std::clamp(-std::remainder(wanted-d.f(0x10),6.28318530718f)*.85f,-.8f,.8f),gas=1,brake=0;
        const auto cycle=(frame+slot*60)%360;
        if(cycle>=200&&cycle<220){gas=0;brake=.7f;}
        if(cycle>=240&&cycle<252)steer=(frame/360)%2?-.9f:.9f;
        if(cycle>=280&&cycle<295)gas=0;
        out[slot]=adaptOriginalHostInput(host[slot],{steer,gas,brake,frame%360==205,frame%110==90},sim.setup().automatic[slot],false,frame);
    }
    return out;
}
struct Baseline {std::vector<std::array<OriginalVehicleInputs,2>> inputs;std::vector<std::uint64_t> hashes;std::uint64_t contacts=0,impacts=0;};
Baseline record(const std::filesystem::path& root,const OnlineRaceSetup& selected,unsigned frames){
    OnlineRaceSimulation sim(root,selected);std::array<OriginalHostInputState,2> inputState{};Baseline result;
    for(unsigned frame=0;frame<frames;++frame){auto input=driving(sim,inputState,frame);result.inputs.push_back(input);
        const auto effect=sim.step(input);result.hashes.push_back(sim.digest());
        for(unsigned slot=0;slot<2;++slot){result.impacts+=effect.driving[slot].newImpactRecords.size();const auto& actor=sim.car(slot).actor();
            for(unsigned offset:{0u,4u,8u,24u,28u,32u})require(std::isfinite(actor.f(offset)),"Non-finite baseline pose");}
        require(effect.driving[0].bodyCollision.active==effect.driving[1].bodyCollision.active,"Collision active flag differs between cars");
        if(effect.driving[0].bodyCollision.active){
            require(effect.driving[0].bodyCollision.x==-effect.driving[1].bodyCollision.x&&effect.driving[0].bodyCollision.z==-effect.driving[1].bodyCollision.z,"Pair responses are not opposite");}
    }
    result.contacts=sim.contactFrames();return result;
}
void wireContracts(const std::filesystem::path& root){
    std::array<OriginalRaceRuleState,2> result;
    require(resolveOnlineRaceWinner(result)==-3,"Unfinished race awarded a result");
    for(auto& state:result)state.phase=OriginalRacePhase::Finished;
    result[0].times.finishTime=10000;result[1].times.finishTime=10004;
    require(resolveOnlineRaceWinner(result)==0,"Sub-frame host win became a draw");
    std::swap(result[0],result[1]);require(resolveOnlineRaceWinner(result)==1,"Sub-frame client win became a draw");
    result[1].times.finishTime=result[0].times.finishTime;require(resolveOnlineRaceWinner(result)==2,"Exact tie is not a draw");
    result[0].phase=OriginalRacePhase::TimeUp;require(resolveOnlineRaceWinner(result)==1,"Timed-out host won");
    result[1].phase=OriginalRacePhase::TimeUp;require(resolveOnlineRaceWinner(result)==-1,"Double timeout has a winner");
    result[0].phase=OriginalRacePhase::Finished;require(resolveOnlineRaceWinner(result)==0,"Timed-out client won");

    auto selected=setup(3);OnlineRaceSimulation a(root,selected),b(root,selected);
    OnlineRaceLink host(a,91,true),client(b,91,false);auto input=neutral();
    for(unsigned f=0;f<600;++f){
        require(host.step(input)&&client.step(input),"Wire stalled on a healthy link");
        if(f%2==0){auto h=host.packet(),c=client.packet();host.receive(c);client.receive(h);host.reconcile();client.reconcile();}
    }
    for(unsigned i=0;i<3;++i){auto h=host.packet(),c=client.packet();host.receive(c);client.receive(h);host.reconcile();client.reconcile();}
    require(host.verifiedPeerFrames()==600&&client.verifiedPeerFrames()==600&&a.digest()==b.digest(),"Wire peers did not verify their complete history");
    const auto bytes=host.packet();
    auto reject=[&](std::vector<std::uint8_t> bad){bool failed=false;try{client.receive(bad);client.reconcile();}catch(const std::exception&){failed=true;}require(failed,"Malformed wire packet accepted");};
    auto bad=bytes;bad.pop_back();reject(bad);bad=bytes;bad[0]^=1;reject(bad);bad=bytes;bad[8]^=1;reject(bad);bad=bytes;bad[36]=1;reject(bad);
    // A forged newer digest is rejected even when the input payload is valid.
    require(host.step(input)&&client.step(input),"Digest setup stalled");auto c=client.packet();host.receive(c);host.reconcile();bad=host.packet();bad[24]^=1;reject(bad);
    std::cout<<"PASS actual wire codec, confirmed hash exchange, truncation/version/race/reserved/digest rejection"<<std::endl;
}
void contracts(const std::filesystem::path& root){
    {
        OnlineRaceSimulation ended(root,setup(6));const auto idle=neutral();
        for(unsigned f=0;f<20000&&ended.rules(0).state().phase!=OriginalRacePhase::TimeUp;++f)ended.step({idle,idle});
        require(ended.rules(0).state().phase==OriginalRacePhase::TimeUp&&ended.rules(1).state().phase==OriginalRacePhase::TimeUp,"Online natural timeout fixture");
        const auto saved=ended.checkpoint();std::array<OriginalHostInputState,2> inputs{};
        std::array<OriginalVehicleInputs,2> held;
        for(unsigned slot=0;slot<2;++slot)held[slot]=adaptOriginalHostInput(inputs[slot],{slot?-.75f:.75f,1,0,false,true},true,true,0);
        ended.step(held);
        for(unsigned slot=0;slot<2;++slot){const auto& controls=ended.car(slot).vehicle().controls;
            require(controls.steeringHighByte==(slot?68u:188u)&&std::abs(controls.steering)>.4f,"Online ended race lost steering");
            require(controls.throttle==0.f&&controls.brake>.9f&&ended.car(slot).raceAutomaticBrakeByte()!=0,"Online ended race accepted acceleration or lost braking");
        }
        const auto digest=ended.digest();ended.restore(saved);ended.step(held);
        require(ended.digest()==digest,"Post-race steering changed across rollback replay");
        std::cout<<"PASS online natural timeout: both steering directions, blocked accelerator, automatic brake, exact checkpoint replay\n";
    }

    for(bool enabled:{false,true}){
        auto configured=setup(0);configured.boost=enabled;
        OnlineRaceSimulation boosted(root,configured);
        const auto checkpoint=boosted.checkpoint();
        const auto metrics=OriginalBattleMetrics::load(root,configured.condition);
        std::array<OriginalHostInputState,2> adapters{};
        std::vector<std::array<OriginalVehicleInputs,2>> replay;
        bool observed=false;
        for(unsigned frame=0;frame<600;++frame){
            const float gap=metrics.advantage(boosted.rules(0).state().progress,boosted.rules(1).state().progress);
            std::array<OriginalVehicleInputs,2> inputs;
            for(unsigned slot=0;slot<2;++slot)inputs[slot]=adaptOriginalHostInput(adapters[slot],{0,slot==0?1.f:0.f,0,false,false},true,false,0);
            replay.push_back(inputs);boosted.step(inputs);
            for(unsigned slot=0;slot<2;++slot){
                const bool trailing=(slot==0?gap:-gap)<0;
                require((boosted.car(slot).vehicle().drive.u(0x43C)!=0)==(enabled&&trailing),"Boost affected leader/off mode or missed trailing car");
                observed|=enabled&&trailing;
            }
        }
        require(!enabled||observed,"Boost test never exercised a trailing car");
        const auto digest=boosted.digest();boosted.restore(checkpoint);
        for(const auto& input:replay)boosted.step(input);
        require(boosted.digest()==digest,"Boost changed across checkpoint replay");
    }
    auto selected=setup(6);OnlineRaceSimulation a(root,selected),b(root,selected);
    const auto initial=a.checkpoint();std::array<OriginalHostInputState,2> host{};std::vector<std::array<OriginalVehicleInputs,2>> inputs;
    for(unsigned f=0;f<900;++f){auto input=driving(a,host,f);inputs.push_back(input);a.step(input);}
    const auto final=a.digest();a.restore(initial);for(const auto& input:inputs)a.step(input);require(a.digest()==final,"Pair restore/replay changed state");
    bool failed=false;try{b.restore(initial);}catch(const std::invalid_argument&){failed=true;}require(failed,"Foreign pair checkpoint accepted");
    OnlineRaceTimeline timeline(b,444,true);auto n=neutral();OnlineInputRecord bad{444,0,0,n};
    require(!timeline.receive({&bad,1}),"Client can forge host input");bad.slot=1;bad.race=445;require(!timeline.receive({&bad,1}),"Stale race accepted");
    bad.race=444;bad.frame=64;require(!timeline.receive({&bad,1}),"Unbounded future accepted");
    bad.frame=0;bad.input.calibration.steeringWord=0;require(!timeline.receive({&bad,1}),"Remote calibration override accepted");
    for(unsigned f=0;f<64;++f){require(timeline.localInput(n),"Local input rejected");require(timeline.advance(),"Early history stall");}
    auto frozen=b.digest();require(!timeline.advance()&&b.digest()==frozen,"Exhausted history modified race");
    std::vector<OnlineInputRecord> fill;for(unsigned f=0;f<64;++f)fill.push_back({444,f,1,n});require(timeline.receive(fill),"Delayed recovery batch rejected");timeline.reconcile();
    require(timeline.confirmed()==64&&timeline.metrics().confirmedEffects==64,"Confirmation/effect count incorrect after outage");
    require(timeline.receive(fill),"Duplicate batch rejected");timeline.reconcile();require(timeline.metrics().confirmedEffects==64,"Duplicate packet replayed effects");
    std::cout<<"PASS paired state restore, countdown/rule/audio state, role/race/window validation, bounded outage recovery and one-time confirmation\n";
}
struct Packet {double due;unsigned receiver;std::uint64_t sequence,ack;std::vector<OnlineInputRecord> inputs;};
struct Later {bool operator()(const Packet& a,const Packet& b)const{return a.due>b.due||(a.due==b.due&&a.sequence>b.sequence);}};
struct Report {std::array<OnlineTimelineMetrics,2> metrics;std::uint64_t checked=0,packets=0,lost=0,duplicate=0;double p99=0,maxMs=0;std::size_t memory=0;};
Report run(const std::filesystem::path& root,const OnlineRaceSetup& selected,const Baseline& baseline,unsigned ping,bool impaired,unsigned seed){
    OnlineRaceSimulation host(root,selected),client(root,selected);std::array<std::uint64_t,2> emitted{};
    auto output=[&](unsigned slot,const OnlineRaceFrame& effect,std::uint64_t digest){
        require(effect.frame==emitted[slot],"Confirmed effects repeated or skipped");
        if(digest!=baseline.hashes.at(effect.frame))throw std::runtime_error("DESYNC frame="+std::to_string(effect.frame)+" peer="+std::to_string(slot)+" ping="+std::to_string(ping));
        ++emitted[slot];};
    OnlineRaceTimeline h(host,54321,true,[&](const auto& e,auto digest){output(0,e,digest);});
    OnlineRaceTimeline c(client,54321,false,[&](const auto& e,auto digest){output(1,e,digest);});
    std::array<OnlineRaceTimeline*,2> peers{&h,&c};std::array<std::uint64_t,2> ack{};
    std::priority_queue<Packet,std::vector<Packet>,Later> wire;std::mt19937 random(seed);std::uniform_real_distribution<double> u(0,1);
    auto latency=[&](){return std::max(0.0,ping*.5+(impaired?(u(random)*40-20):0));};
    Report report;std::vector<double> costs;const auto frames=baseline.inputs.size();std::uint64_t sequence=0;
    for(std::size_t wall=0;wall<frames+600&&(h.confirmed()<frames||c.confirmed()<frames);++wall){
        double now=wall*(1000.0/60);const auto start=std::chrono::steady_clock::now();
        while(!wire.empty()&&wire.top().due<=now){const auto p=wire.top();wire.pop();require(peers[p.receiver]->receive(p.inputs),"Valid input bundle rejected");ack[p.receiver]=std::max(ack[p.receiver],p.ack);}
        for(unsigned peer=0;peer<2;++peer){auto& timeline=*peers[peer];timeline.reconcile();
            if(timeline.frame()<frames){require(timeline.localInput(baseline.inputs[timeline.frame()][peer]),"Local input invalid");require(timeline.advance(),"Normal network exhausted history");}}
        costs.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
        if(wall%2==0)for(unsigned peer=0;peer<2;++peer){auto& sender=*peers[peer];++report.packets;
            if(impaired&&u(random)<.03){++report.lost;continue;}
            Packet p{now+latency(),1-peer,++sequence,sender.confirmed(),sender.outgoing(ack[peer])};wire.push(p);
            if(impaired&&u(random)<.02){p.due+=latency();wire.push(p);++report.duplicate;}}
        report.memory=std::max(report.memory,h.memoryBytes()+c.memoryBytes());
    }
    require(h.confirmed()==frames&&c.confirmed()==frames,"Race failed to converge after network drain");
    require(host.digest()==client.digest()&&host.digest()==baseline.hashes.back(),"Final host/client state disagrees");
    report.checked=emitted[0]+emitted[1];report.metrics={h.metrics(),c.metrics()};std::sort(costs.begin(),costs.end());report.p99=costs[std::size_t((costs.size()-1)*.99)];report.maxMs=costs.back();return report;
}
}
int main(int argc,char** argv)try{
    if(argc<2)throw std::invalid_argument("online_race_timeline_tests native_root [report.csv] [frames=1800] [cases=18]");
    const std::filesystem::path root=argv[1];unsigned frames=argc>3?std::stoul(argv[3]):1800,cases=argc>4?std::stoul(argv[4]):18;
    contracts(root);wireContracts(root);std::ofstream csv;if(argc>2)csv.open(argv[2]);
    if(csv)csv<<"case,condition,car0,car1,ping_ms,jitter_ms,loss_pct,confirmed_peer_frames,contacts,wall_impacts,host_rollbacks,client_rollbacks,replayed_frames,max_depth,p99_both_peers_ms,max_both_peers_ms,max_correction_units,history_bytes_both_peers,packets,lost,duplicate,desyncs\n";
    std::uint64_t checked=0,contacts=0;
    for(unsigned scenario=0;scenario<cases;++scenario){auto selected=setup(scenario);
        if(argc>5){
            selected.condition=6+(scenario&1);selected.wet=(scenario&2)!=0;
            selected.imported=std::make_shared<const idas3::ImportedCourse>(idas3::ImportedCourse::load(argv[5]));
            const auto a=selected.imported->onlineSpawn(scenario&1,0),b=selected.imported->onlineSpawn(scenario&1,1);
            const float dx=a.position[0]-b.position[0],dz=a.position[2]-b.position[2];
            require(dx*dx+dz*dz>7,"Imported online grids overlap");
            OnlineRaceSimulation sim(root,selected);
            for(unsigned slot=0;slot<2;++slot){
                const auto grid=selected.imported->onlineSpawn(scenario&1,slot);
                require(sim.car(slot).selection().physics.conditionCode==selected.imported->handlingCondition(scenario&1),"Both online cars use the imported course handling selection");
                require(std::abs(sim.car(slot).actor().f(0)-grid.position[0])<1&&std::abs(sim.car(slot).actor().f(8)-grid.position[2])<1,"Online car initialized on wrong track");
                require(sim.rules(slot).rules().goalIndex==selected.imported->rules(scenario&1).goalIndex,"Online finish gate is not Hakone");
            }
        }
        const auto baseline=record(root,selected,frames);contacts+=baseline.contacts;
        for(unsigned ping:{50u,100u,200u,250u})for(unsigned impaired=0;impaired<2;++impaired){auto r=run(root,selected,baseline,ping,impaired!=0,123+scenario*133+ping+impaired*47);
            checked+=r.checked;const auto& a=r.metrics[0];const auto& b=r.metrics[1];
            if(csv)csv<<scenario<<','<<selected.condition<<','<<selected.profiles[0].u(16)<<','<<selected.profiles[1].u(16)<<','<<ping<<','<<(impaired?20:0)<<','<<(impaired?3:0)<<','<<r.checked<<','<<baseline.contacts<<','<<baseline.impacts<<','<<a.rollbacks<<','<<b.rollbacks<<','<<a.replayedFrames+b.replayedFrames<<','<<std::max(a.maxDepth,b.maxDepth)<<','<<r.p99<<','<<r.maxMs<<','<<std::max(a.maxCorrection,b.maxCorrection)<<','<<r.memory<<','<<r.packets<<','<<r.lost<<','<<r.duplicate<<",0\n";
        }
        if(csv)csv.flush();std::cout<<"PASS case="<<scenario<<" contact_frames="<<baseline.contacts<<" wall_impacts="<<baseline.impacts<<"; both peers 50/100/200/250ms + jitter/loss"<<std::endl;
    }
    if(cases>=18&&frames>=1800)require(contacts>0,"No car-to-car contact was exercised");
    std::cout<<"PASS peer_frames="<<checked<<" contact_frames_in_baselines="<<contacts<<". Virtual network; two numerical peers, no Unity/Steam transport yet.\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
