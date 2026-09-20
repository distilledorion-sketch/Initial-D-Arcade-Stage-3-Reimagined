#include "experimental_rollback.h"
#include "original_host_input.h"
#include "original_start_grid.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <queue>
#include <random>
#include <vector>
using namespace idas3::original;
namespace {
void require(bool b,const char* m){if(!b)throw std::runtime_error(m);}
OriginalVehicleInputs neutral(bool automatic=true){OriginalHostInputState h;return adaptOriginalHostInput(h,{},automatic,true,0);}
OriginalDrivingSelection selection(unsigned car,unsigned condition,unsigned wet,unsigned tune){
    OriginalDrivingSelection s;s.physics=makeOriginalFreshTimeAttackSelection(car,condition,wet?OriginalWeather::Wet:OriginalWeather::Dry);
    s.physics.upgradeIndex0C9015F0=tune;s.collisionVariant=condition&1;return s;
}
void reset(OriginalDrivingSession& s,const std::filesystem::path& root,const OriginalDrivingSelection& selected,unsigned slot){
    const auto grid=originalStartPose(selected.physics.conditionCode,slot);s.reset(root,selected,grid.position,grid.angles);
}
OriginalVehicleInputs driving(OriginalDrivingSession& s,OriginalHostInputState& h,unsigned frame,bool automatic){
    const auto& d=s.vehicle().drive;const auto& points=s.path().points;
    std::size_t nearest=0;float best=1e30f;
    for(std::size_t i=0;i<points.size();++i){float dx=points[i][0]-d.f(0),dz=points[i][2]-d.f(8),distance=dx*dx+dz*dz;if(distance<best){best=distance;nearest=i;}}
    auto target=nearest;float length=0;const float look=std::max(10.f,d.f(0x238)*.65f);
    while(length<look&&target+1<points.size()) {float x=points[target+1][0]-points[target][0],z=points[target+1][2]-points[target][2];length+=std::sqrt(x*x+z*z);++target;}
    const auto& p=points[target];const float desired=std::atan2(-(p[0]-d.f(0)),-(p[2]-d.f(8)));
    const float error=std::remainder(desired-d.f(0x10),6.28318530718f);
    // Positive host steering is camera-right; source yaw has the opposite sign.
    float steer=std::clamp(-error*.85f,-.8f,.8f);
    float gas=1,brake=0;
    // Sudden lift/brake/steer pulses deliberately exercise wrong prediction,
    // wall penalties and one-shot manual shifts, rather than only straight runs.
    const auto cycle=frame%360;
    if(cycle>=200&&cycle<220){gas=0;brake=.7f;}
    if(cycle>=240&&cycle<252)steer=(frame/360)%2?-.9f:.9f;
    if(cycle>=280&&cycle<295)gas=0;
    const bool up=frame%110==90,down=frame%360==205;
    return adaptOriginalHostInput(h,{steer,gas,brake,down,up},automatic,true,frame);
}
struct Baseline {std::vector<OriginalVehicleInputs> inputs;std::vector<std::uint64_t> hashes;double maxSpeed=0;std::uint64_t impacts=0;};
Baseline record(const std::filesystem::path& root,const OriginalDrivingSelection& selected,unsigned slot,unsigned frames,bool automatic){
    OriginalDrivingSession s;reset(s,root,selected,slot);OriginalHostInputState h;Baseline b;
    b.inputs.reserve(frames);b.hashes.reserve(frames);
    for(unsigned f=0;f<frames;++f){auto input=driving(s,h,f,automatic);b.inputs.push_back(input);const auto out=s.tick(input);b.hashes.push_back(s.rollbackDigest());
        b.impacts+=out.newImpactRecords.size();b.maxSpeed=std::max(b.maxSpeed,double(s.vehicle().drive.f(0x238))*3.6);
        require(std::isfinite(s.actor().f(0))&&std::isfinite(s.actor().f(4))&&std::isfinite(s.actor().f(8)),"Baseline non-finite pose");}
    require(b.maxSpeed>10,"Baseline failed to move");return b;
}
void contracts(const std::filesystem::path& root){
    const auto selected=selection(0,6,0,0);OriginalDrivingSession s;reset(s,root,selected,0);
    const auto saved=s.checkpoint();const auto initial=s.rollbackDigest();OriginalHostInputState h;
    for(unsigned i=0;i<300;++i)s.tick(driving(s,h,i,false));const auto end=s.rollbackDigest();
    s.restore(saved);require(s.rollbackDigest()==initial,"Checkpoint did not restore hidden state");h={};
    for(unsigned i=0;i<300;++i)s.tick(driving(s,h,i,false));require(end==s.rollbackDigest(),"Checkpoint replay changed physics");
    OriginalDrivingSession other;reset(other,root,selected,0);bool rejected=false;
    try{other.restore(saved);}catch(const std::invalid_argument&){rejected=true;}require(rejected,"Cross-owner checkpoint accepted");
    auto moved=std::move(s);moved.restore(saved);require(moved.rollbackDigest()==initial,"Move invalidated checkpoint");
    reset(moved,root,selected,0);rejected=false;try{moved.restore(saved);}catch(const std::invalid_argument&){rejected=true;}require(rejected,"Previous-race checkpoint accepted");
    moved.setEngineOutput([](const auto&,auto&){});rejected=false;try{moved.checkpoint();}catch(const std::logic_error&){rejected=true;}require(rejected,"External audio callback allowed during replay");
    moved.setEngineOutput({});
    ExperimentalRollback rb(moved,123,neutral());
    RollbackInput packet{124,0,neutral()};require(!rb.receive({&packet,1}),"Wrong race accepted");
    packet.race=123;packet.frame=64;require(!rb.receive({&packet,1}),"Unbounded future input accepted");
    packet.frame=0;require(rb.receive({&packet,1}),"Initial packet rejected");
    packet.controls.pressedByte=16;require(!rb.receive({&packet,1}),"Conflicting packet accepted");
    for(unsigned f=0;f<65;++f)require(rb.advance(),"Unexpected early history stall");
    require(!rb.advance()&&rb.frame()==65,"Loss beyond history did not stop safely");
    const auto frozen=moved.rollbackDigest();for(unsigned f=0;f<5;++f)require(!rb.advance(),"Exhausted history advanced");require(moved.rollbackDigest()==frozen,"Stalled state mutated");
    // Burst loss recovery: fill outstanding window, replay once, resume.
    std::vector<RollbackInput> batch;for(unsigned f=1;f<65;++f)batch.push_back({123,f,neutral()});
    require(rb.receive(batch),"Recovery bundle rejected");rb.reconcile();require(rb.confirmed()==65&&rb.advance(),"History did not recover after late bundle");
    std::cout<<"PASS checkpoint/move/reset/side-effect guards, stale/future/conflicting packets, bounded loss stall/recovery\n";
}
struct Delivery {double due;std::uint64_t order;std::vector<RollbackInput> inputs;};
struct Later {bool operator()(const Delivery& a,const Delivery& b)const{return a.due>b.due||(a.due==b.due&&a.order>b.order);}};
struct Ack {double due;std::uint64_t frame;};
struct AckLater {bool operator()(const Ack& a,const Ack& b)const{return a.due>b.due;}};
struct Result {RollbackMetrics metrics;std::size_t bytes=0;double p95=0,p99=0,max=0;std::uint64_t checked=0,packets=0,dropped=0,duplicates=0,reordered=0;};
Result run(const std::filesystem::path& root,const OriginalDrivingSelection& selected,unsigned slot,const Baseline& source,unsigned ping,bool impaired,unsigned seed){
    OriginalDrivingSession s;reset(s,root,selected,slot);ExperimentalRollback rb(s,42,neutral(source.inputs[0].automaticMode));
    std::mt19937 random(seed);std::uniform_real_distribution<double> u(0,1);
    auto transit=[&](){return std::max(0.0,ping*.5+(impaired?(u(random)*40-20):0));};
    std::priority_queue<Delivery,std::vector<Delivery>,Later> wire;std::priority_queue<Ack,std::vector<Ack>,AckLater> ackWire;
    Result out;std::vector<double> costs;std::uint64_t acknowledged=0,order=0,lastOrder=0;const auto frames=source.inputs.size();
    for(std::uint64_t wall=0;wall<frames+600&&rb.confirmed()<frames;++wall){
        const double now=wall*(1000.0/60);
        while(!ackWire.empty()&&ackWire.top().due<=now){acknowledged=std::max(acknowledged,ackWire.top().frame);ackWire.pop();}
        const auto produced=std::min(wall+1,std::uint64_t(frames));
        if(wall%2==0&&acknowledged<produced){
            std::vector<RollbackInput> payload;for(auto f=acknowledged;f<std::min(produced,acknowledged+32);++f)payload.push_back({42,f,source.inputs[f]});
            ++out.packets;
            if(impaired&&u(random)<.03)++out.dropped;
            else {Delivery d{now+transit(),++order,std::move(payload)};wire.push(d);if(impaired&&u(random)<.02){d.due+=transit();wire.push(d);++out.duplicates;}}
        }
        const auto start=std::chrono::steady_clock::now();
        while(!wire.empty()&&wire.top().due<=now){auto d=wire.top();wire.pop();if(d.order<lastOrder)++out.reordered;lastOrder=std::max(lastOrder,d.order);
            require(rb.receive(d.inputs),"Valid network bundle rejected");}
        rb.reconcile();
        if(rb.frame()<frames)require(rb.advance(),"Nominal ping test exhausted rollback history");
        const auto cost=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();costs.push_back(cost);
        // Compare every confirmed historical tick, not just a final position.
        while(out.checked<rb.confirmed()){
            if(rb.digestAfter(out.checked)!=source.hashes[out.checked])throw std::runtime_error("DESYNC at frame "+std::to_string(out.checked)+" ping "+std::to_string(ping)+" condition "+std::to_string(selected.physics.conditionCode));
            ++out.checked;
        }
        if(wall%2==0&&(!impaired||u(random)>=.03))ackWire.push({now+transit(),rb.confirmed()});
        out.bytes=std::max(out.bytes,rb.memoryBytes());
    }
    require(rb.confirmed()==frames,"Final inputs did not converge after drain");require(s.rollbackDigest()==source.hashes.back(),"Final state mismatch");
    out.metrics=rb.metrics();std::sort(costs.begin(),costs.end());out.p95=costs[std::size_t((costs.size()-1)*.95)];out.p99=costs[std::size_t((costs.size()-1)*.99)];out.max=costs.back();return out;
}
}
int main(int argc,char** argv)try{
    if(argc<2)throw std::invalid_argument("Usage: rollback_physics_tests native_root [report.csv] [frames=1800] [cases=36]");
    const std::filesystem::path root=argv[1];const unsigned frames=argc>3?std::stoul(argv[3]):1800,cases=argc>4?std::stoul(argv[4]):36;
    contracts(root);std::ofstream csv;if(argc>2)csv.open(argv[2]);
    if(csv)csv<<"case,condition,car,wet,tune,manual,slot,ping_rtt_ms,jitter_ms,loss_pct,frames,desyncs,rollbacks,replayed_frames,max_depth,max_correction_units,max_yaw_rad,p95_step_ms,p99_step_ms,max_step_ms,max_replay_ms,history_bytes,stalls,packets,dropped,duplicates,reordered,source_max_kmh,source_wall_impacts\n";
    std::uint64_t checked=0,replayed=0;double maxCorrection=0,maxCost=0;
    for(unsigned c=0;c<cases;++c){
        const unsigned car=c%35,condition=c%18,wet=(c/18)%2,tune=c%3==0?75:0,slot=c%2;const bool automatic=(c/2)%2==0;
        const auto selected=selection(car,condition,wet,tune);const auto source=record(root,selected,slot,frames,automatic);
        for(unsigned ping:{50u,100u,200u,250u})for(unsigned impaired=0;impaired<2;++impaired){
            auto r=run(root,selected,slot,source,ping,impaired!=0,1009+c*101+ping+impaired*17);const auto& m=r.metrics;
            if(csv)csv<<c<<','<<condition<<','<<car<<','<<wet<<','<<tune<<','<<!automatic<<','<<slot<<','<<ping<<','<<(impaired?20:0)<<','<<(impaired?3:0)<<','<<r.checked<<",0,"<<m.rollbacks<<','<<m.replayedFrames<<','<<m.deepestReplay<<','<<m.maxCorrection<<','<<m.maxYawCorrection<<','<<r.p95<<','<<r.p99<<','<<r.max<<','<<m.maxReplayMs<<','<<r.bytes<<','<<m.stalls<<','<<r.packets<<','<<r.dropped<<','<<r.duplicates<<','<<r.reordered<<','<<source.maxSpeed<<','<<source.impacts<<'\n';
            checked+=r.checked;replayed+=m.replayedFrames;maxCorrection=std::max(maxCorrection,m.maxCorrection);maxCost=std::max(maxCost,r.max);
        }
        std::cout<<"PASS case "<<c<<" condition="<<condition<<" car="<<car<<" wet="<<wet<<" tune="<<tune<<" manual="<<!automatic<<" across 50/100/200/250ms, clean + jitter/loss; source max="<<source.maxSpeed<<" km/h impacts="<<source.impacts<<std::endl;
        if(csv)csv.flush();
    }
    std::cout<<"PASS confirmed_frames="<<checked<<" replayed_frames="<<replayed<<" max_correction_units="<<maxCorrection<<" max_step_ms="<<maxCost<<"\n";
    std::cout<<"Scope: headless source physics with virtual 60Hz network time, no Unity renderer, Steam, real internet, race rules or shared car collisions. Ping is RTT, input transit is half RTT.\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
