#include "online_race_timeline.h"
#include "online_race_link.h"
#include "original_host_input.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace idas3::original;
namespace {
unsigned checks=0;
void require(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
OriginalVehicleInputs input(unsigned pressed=0,unsigned gear=0){
    OriginalHostInputState state;auto v=adaptOriginalHostInput(state,{},false,true,0);
    v.pressedByte=std::uint8_t(pressed);v.requestedGear=std::uint8_t(gear);return v;
}
OnlineRaceSetup setup(){
    OnlineRaceSetup s;s.condition=6;
    for(unsigned i=0;i<2;++i){s.profiles[i]=makeOriginalFreshBattleProfile();s.automatic[i]=false;}
    return s;
}
void run(const std::filesystem::path& root){
    const auto selected=setup();
    {
        OnlineRaceSimulation sim(root,selected);OnlineRaceTimeline timeline(sim,1,true);
        require(timeline.localInput(input(0x10)),"First local down edge rejected");require(timeline.advance(),"First local frame did not advance");
        const auto digest=sim.digest();
        require(!timeline.localInput(input(0x10)),"Consecutive local down edges bypass held-button acquisition");
        require(sim.digest()==digest&&timeline.frame()==1,"Rejected local edge modified simulation");
        require(timeline.localInput(input(0x20)),"Adjacent opposite-direction edge rejected");require(timeline.advance(),"Opposite edge did not advance");
        require(timeline.localInput(input(0x10)),"Down edge after an intervening up edge rejected");require(timeline.advance(),"Alternating edge did not advance");
    }
    for(bool reverseBatch:{false,true}){
        OnlineRaceSimulation sim(root,selected);OnlineRaceTimeline timeline(sim,2,true);
        std::array<OnlineInputRecord,2> bad{{{2,0,1,input(0x10)},{2,1,1,input(0x10)}}};
        if(reverseBatch)std::swap(bad[0],bad[1]);
        const auto digest=sim.digest();require(!timeline.receive(bad),"Invalid same-button batch accepted");
        require(timeline.outgoing(0).empty()&&sim.digest()==digest,"Rejected batch partly mutated history");
        // Corrected retransmission must work: an invalid batch cannot poison
        // either frame's immutable received record.
        const std::array<OnlineInputRecord,3> good{{{2,2,1,input(0x10)},{2,0,1,input(0x10)},{2,1,1,input()}}};
        require(timeline.receive(good),"Release-between edges or out-of-order good batch rejected");
        require(timeline.receive(good),"Duplicate good batch rejected");
        for(unsigned frame=0;frame<3;++frame){require(timeline.localInput(input()),"Good local neutral rejected");require(timeline.advance(),"Good frame stalled");}
        require(timeline.confirmed()==3,"Good reordered frames did not confirm");
    }
    for(bool latePredecessor:{false,true}){
        OnlineRaceSimulation sim(root,selected);OnlineRaceTimeline timeline(sim,3,true);
        OnlineInputRecord known{3,latePredecessor?1u:0u,1,input(0x20)};
        require(timeline.receive({&known,1}),"First isolated edge rejected");
        OnlineInputRecord bad{3,latePredecessor?0u:1u,1,input(0x20)};
        require(!timeline.receive({&bad,1}),"Adjacent edge across separate packets accepted");
        bad.input=input();require(timeline.receive({&bad,1}),"Valid replacement adjacent to known edge rejected");
    }
    {
        OnlineRaceSimulation sim(root,selected);OnlineRaceTimeline timeline(sim,6,true);
        OnlineInputRecord first{6,0,1,input(0x10)};
        require(timeline.receive({&first,1})&&timeline.localInput(input())&&timeline.advance()&&timeline.confirmed()==1,"Confirmed predecessor setup failed");
        for(unsigned frame=1;frame<65;++frame)require(timeline.localInput(input())&&timeline.advance(),"Ignored-old fixture did not fill retransmit ring");
        // Old records are ignored by the transport. They must not override the
        // immutable confirmed predecessor when validating a new adjacent edge.
        std::array<OnlineInputRecord,2> records{{{6,0,1,input()},{6,1,1,input(0x10)}}};
        require(!timeline.receive(records),"Ignored old record bypassed confirmed-boundary edge validation");
        records[0].input=input(0x20);records[1].input=input(0x20);
        require(timeline.receive(records),"Ignored old record poisoned valid confirmed-boundary input");
    }
    {
        OnlineRaceSimulation sim(root,selected);OnlineRaceTimeline timeline(sim,7,false);
        const OnlineInputRecord successor{7,1,1,input(0x10)};
        require(timeline.receive({&successor,1}),"Future authoritative local input rejected");
        require(!timeline.localInput(input(0x10)),"Local predecessor bypassed known authoritative successor validation");
        require(timeline.localInput(input(0x20)),"Legal local predecessor rejected");
    }
    {
        OnlineRaceSimulation sim(root,selected);OnlineRaceTimeline timeline(sim,4,true);
        // Fill the ring until the immediately preceding confirmed frame's
        // entry is overwritten; previousConfirmed still owns its input.
        for(unsigned frame=0;frame<65;++frame){
            const auto remote=input(frame==0?0x10:0);OnlineInputRecord r{4,frame,1,remote};
            if(frame==0)require(timeline.receive({&r,1}),"Initial confirmed edge rejected");
            require(timeline.localInput(input()),"Ring local input rejected");require(timeline.advance(),"Ring filled too early");
        }
        require(timeline.confirmed()==1,"Ring fixture confirmation boundary wrong");
        OnlineInputRecord bad{4,1,1,input(0x10)};require(!timeline.receive({&bad,1}),"Confirmed-boundary repeated edge accepted");
        bad.input=input(0x20);require(timeline.receive({&bad,1}),"Confirmed-boundary opposite edge rejected");
        timeline.reconcile();require(timeline.confirmed()==2,"Valid late edge did not reconcile");
    }
    {
        OnlineRaceSimulation a(root,selected),b(root,selected);OnlineRaceLink host(a,5,true),client(b,5,false);
        // Preserve simultaneous source edges and direct-gear host extensions.
        for(unsigned frame=0;frame<180;++frame){
            const auto v=input(frame%3==0?0x30:0,1+(frame%6));
            require(host.step(v)&&client.step(v),"Legal simultaneous/direct-gear sequence rejected");
            auto h=host.packet(),c=client.packet();host.receive(c);client.receive(h);host.reconcile();client.reconcile();
        }
        for(unsigned i=0;i<3;++i){auto h=host.packet(),c=client.packet();host.receive(c);client.receive(h);host.reconcile();client.reconcile();}
        require(host.verifiedPeerFrames()==180&&client.verifiedPeerFrames()==180&&a.digest()==b.digest(),"Legal wire history failed verification");
    }
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::runtime_error("Usage: online_input_edges_tests native_root");run(argv[1]);
    std::cout<<"PASS "<<checks<<" production timeline/wire checks: local/remote press edges, reordered/duplicate/atomic batches, confirmed boundary and intentional direct gears.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
