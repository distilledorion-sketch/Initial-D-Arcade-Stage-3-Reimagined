#include "flycast_keyboard_steering.h"
#include "online_race_link.h"
#include "online_replay_recording.h"
#include "original_host_input.h"
#include "original_driving_clock.h"
#include <array>
#include <iostream>
#include <stdexcept>
using namespace idas3;
using namespace idas3::original;
namespace {
std::uint64_t checks=0,wireRecords=0,replayFrames=0,stalls=0,multi=0;
void require(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
std::uint64_t read(std::span<const std::uint8_t> bytes,std::size_t offset,unsigned count){
    std::uint64_t v=0;for(unsigned i=0;i<count;++i)v|=std::uint64_t(bytes[offset+i])<<(i*8);return v;
}
void same(const OnlineInputRecord& record,const std::array<std::vector<OriginalVehicleInputs>,2>& expected){
    require(record.frame<expected[record.slot].size(),"Wire record has no accepted source input");
    const auto& input=expected[record.slot][record.frame];
    require(record.input.analog.steering==input.analog.steering,"Wire/authority truncated full raw steering");
    require(record.input.analog.throttle==input.analog.throttle&&record.input.analog.brake==input.analog.brake,"Wire/authority truncated a precise pedal ADC");
    require(record.input.pressedByte==input.pressedByte&&record.input.requestedGear==input.requestedGear,"Raw steering affected shift edges/direct gear");
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::runtime_error("Native asset root required");
    for(const auto rates:std::array<std::array<unsigned,2>,5>{{{20,30},{30,240},{144,20},{240,144},{60,60}}}){
        OnlineRaceSetup setup;setup.condition=6;setup.boost=true;setup.automatic={false,false};
        for(auto& profile:setup.profiles)profile=makeOriginalFreshBattleProfile();
        OnlineRaceSimulation a(argv[1],setup),b(argv[1],setup);
        std::array<std::vector<OriginalVehicleInputs>,2> expected;
        std::array<Replay,2> replays;OnlineReplayRecording recorder;recorder.begin(a,0);
        recorder.setMetrics(OriginalBattleMetrics::load(argv[1],setup.condition));
        OnlineRaceLink host(a,88,true,[&](const OnlineRaceFrame& frame,std::uint64_t){
            recorder.confirm(frame,a,replays[0],replays[1]);
            if(!frame.start.runRules)return;
            for(unsigned slot=0;slot<2;++slot){
                const auto& stored=replays[slot].frames.back();const auto& wanted=frame.replayPoses[slot];
                require(stored.detail.steering==wanted.detail.steering,"Confirmed replay contains a different steering sample");
                require(stored.detail.throttle==wanted.detail.throttle&&stored.detail.brake==wanted.detail.brake,"Confirmed replay lost precise pedal telemetry");
                require(stored.position.x==wanted.position.x&&stored.position.y==wanted.position.y&&stored.position.z==wanted.position.z,
                    "Confirmed replay contains predicted rather than committed motion");
                const auto playback=replays[slot].sample(double(stored.tick));
                require(playback.detail.steering==stored.detail.steering&&playback.yaw==stored.yaw,"Playback reinterpreted keyboard input rather than its recorded pose");
                if(slot==0)require((stored.telemetry.flags&6u)==(wanted.telemetry.flags&6u),"Replay lost original shift edges");
                ++replayFrames;
            }
        }),client(b,88,false);
        std::array<OnlineRaceLink*,2> links{&host,&client};std::array<OriginalDrivingClock,2> clocks;
        std::array<FlycastKeyboardSteering,2> keyboard;std::array<OriginalHostInputState,2> committed;
        std::array<double,2> nextRender{};std::array<unsigned,2> lastPress{};
        // Explicit wire boundaries precede ordinary timestamp-driven input.
        constexpr std::array<int,12> boundaries{0,0x7f,0xff,0x7fff,0x8000,0x8001,0x807f,0x8080,0x80ff,0xff00,0xff7e,0xff7f};
        for(unsigned wall=0;wall<14400&&(host.timeline().confirmed()<360||client.timeline().confirmed()<360);++wall){
            for(unsigned peer=0;peer<2;++peer){
                if(double(wall)+1e-9<nextRender[peer])continue;
                nextRender[peer]+=1200.0/rates[peer];
                const unsigned phase=(wall/90+peer)%4;
                keyboard[peer].sample(1000+wall*1000/1200,(phase&1)!=0,(phase&2)!=0);
                const auto raw=keyboard[peer].raw();const auto sampled=keyboard[peer].analog();
                unsigned advanced=0;
                clocks[peer].advance(1.0/rates[peer],[&]{
                    const auto frame=links[peer]->timeline().frame();if(frame>=360)return;
                    OriginalHostControls controls;controls.throttle=.65f;
                    controls.steering=keyboard[peer].steering();controls.steeringAnalog=frame<boundaries.size()?boundaries[frame]:sampled;
                    controls.shiftDown=(wall/300+peer)%8==2;controls.shiftUp=(wall/300+peer)%8==4;
                    controls.requestedGear=frame<boundaries.size()?std::uint8_t(frame%6+1):0;
                    const auto pedal=[](unsigned value){unsigned raw=value*2;if(raw>=0x8000&&raw<0x8100)raw=0x8100;return int(std::min(raw,0xff7fu));};
                    controls.throttleAnalog=frame<boundaries.size()?boundaries[frame]:pedal(12000+(wall*31+peer*19)%10000);
                    controls.brakeAnalog=frame<boundaries.size()?boundaries[boundaries.size()-1-frame]:pedal((wall/400)%3==2?9000:0);
                    auto candidate=committed[peer];const auto input=adaptOriginalHostInput(candidate,controls,false,true,0);
                    require(input.analog.throttle==controls.throttleAnalog&&input.analog.brake==controls.brakeAnalog,"Production adapter truncated precise pedals before transport");
                    if(peer==0)recorder.sampleLamps(frame,{}, {1,0,0,0,unsigned(controls.shiftDown)|(unsigned(controls.shiftUp)<<1)});
                    expected[peer].push_back(input);
                    if(!links[peer]->step(input)){expected[peer].pop_back();++stalls;}
                    else{
                        require(!(lastPress[peer]&input.pressedByte),"Held catch-up input repeated its pressed edge");
                        lastPress[peer]=input.pressedByte;committed[peer]=candidate;++advanced;
                    }
                    require(keyboard[peer].raw()==raw&&keyboard[peer].analog()==sampled,"Physics/rollback retry advanced host keyboard owner");
                });
                if(advanced>1)++multi;
            }
            if(wall%12||(wall>=1200&&wall<3000))continue;
            const auto hp=host.packet(),cp=client.packet();
            for(const auto* bytes:{&hp,&cp})for(unsigned i=0;i<read(*bytes,32,4);++i){
                const auto offset=40+i*16;OnlineInputRecord record;record.frame=read(*bytes,offset,8);record.slot=(*bytes)[offset+8];
                const auto buttons=(*bytes)[offset+9];record.input.pressedByte=buttons&~7u;record.input.requestedGear=buttons&7u;
                record.input.analog={std::uint16_t(read(*bytes,offset+10,2)),std::uint16_t(read(*bytes,offset+12,2)),std::uint16_t(read(*bytes,offset+14,2))};
                same(record,expected);++wireRecords;
            }
            host.receive(cp);client.receive(hp);host.reconcile();client.reconcile();
            const auto next=host.timeline().frame(),from=next>64?next-64:0;
            for(const auto& decoded:host.timeline().outgoing(from,64))same(decoded,expected);
        }
        require(host.timeline().confirmed()==360&&client.timeline().confirmed()==360,"Raw keyboard peers failed to recover after outage");
        require(a.digest()==b.digest(),"Raw input transport/rollback changed numerical state");
        require(recorder.confirmed()==360&&replays[0].frames.size()>100,"Confirmed keyboard replay incomplete");
        for(std::uint64_t frame=296;frame<360;++frame)require(host.timeline().digestAfter(frame)==client.timeline().digestAfter(frame),"Confirmed keyboard history desynchronized");
    }
    require(stalls>0&&multi>0,"Fixture failed to cover stalls/multiple catch-up ticks");
    std::cout<<"PASS "<<checks<<" raw keyboard online/replay checks, "<<wireRecords<<" wire records, "<<replayFrames<<" confirmed replay/playback poses, "<<stalls<<" stalled attempts, "<<multi<<" multi-tick updates; 3600 confirmed peer frames. In-process transport only; no EOS/network/device timing claim.\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
