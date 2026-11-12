// Included by the smoke original-instruction test to reuse its allocator,
// particle/list fixture and complete field comparison. This executes the
// original outer dispatch and race owner, rather than assuming their order.
#include "car_smoke_state.h"
namespace {
void checkOriginalSmokeSchedule(RefMemory& m,const OriginalCarParts& parts){
    constexpr unsigned root=0xc938000,courseObject=0xc939000,courseVtable=0xc93a000;
    constexpr unsigned child=0xc93b000,courseUpdate=0xf100000,courseReference=0xf100100;
    constexpr unsigned soundManager=0xc93c000;
    const auto origins=originalCarEffectOrigins(parts);
    unsigned coupled=0,changedByOrder=0,commands=0;
    for(unsigned weather:{0u,1u})for(unsigned mode:{0u,2u})for(bool requested:{false,true}){
        Oracle source(m);source.reset(parts,7,0,weather,false);
        m.zeroRegion(root,0x6000);
        m.write32(root,root+128);m.write32(root+36,race);
        m.write32(race+12,0xc3831c4);m.write32(race+20,2);
        m.write32(race+32,race+128);m.write32(race+36,child);m.write32(race+68,child);
        m.write32(race+0x40c,courseObject);m.write32(courseObject,courseVtable);
        m.write32(courseVtable+44,courseUpdate);m.write32(courseVtable+84,courseReference);
        m.write32(race+0x49c,child);
        // Scene queues are absent in this fixture; the default queue's tick
        // still executes. Stream service is an explicit device boundary.
        m.write32(0xc31de08,0);m.write32(0xc31de0c,0);m.zeroRegion(0xc31de10,64);
        m.zeroRegion(0xc8ff1c0,48);m.write32(0xc8ff1d0,soundManager);
        m.write32(0xc31de5c,0);m.write32(0xc31de60,0);m.write8(0xc31de55,0);
        OriginalTireAudioState tire;tire.volume=127;m.writeFloat(0xc8ff1c4,tire.volume);
        idas3::CarSmokeState state;state.reset(weather);const auto& system=state.system();Random rng;
        std::array<OriginalMatrix,2> worlds{originalIdentityMatrix(),originalIdentityMatrix()};
        for(unsigned frame=0;frame<600;++frame){
            std::array<OriginalCarSmokeInput,2> cars;
            for(unsigned car=0;car<2;++car){
                const std::array<float,3> delta{.125f,0,1.25f};
                worlds[car].elements[12]+=delta[0];worlds[car].elements[14]+=delta[2];
                cars[car].available=true;cars[car].owner=originalRoadsideDustOwner(worlds[car],origins,delta);
                for(unsigned w=0;w<4;++w){
                    cars[car].wheelSurfaces[w]=frame%100<70&&w%2==car?0x20:0;
                    cars[car].intensity[w]=frame%100<80?255:0;
                }
                source.car(car,worlds[car],delta,cars[car].wheelSurfaces,cars[car].intensity);
            }
            std::vector<unsigned> order;
            std::vector<OriginalTireCommand> sourceCommands;
            // Stand-in solver callback has a visible, variable RNG cost and
            // requests tire sound through the unchanged original request.
            // This checks boundary ordering, not a complete physics frame.
            const unsigned solverCalls=frame%7;
            const float strength=frame%250<220?.7f:0.f;
            m.writeFloat(0xc901138,25.f);
            source.tick(mode,requested,[&](RefCpu& cpu){
                cpu.r[11]=root;
                cpu.callHooks[0xc156700]=[&](RefCpu& c){order.push_back(c.r[4]);};
                cpu.callHooks[courseUpdate]=[&](RefCpu&){order.push_back(100);};
                cpu.callHooks[courseReference]=[](RefCpu&){};
                for(unsigned at:{0xc061660u,0xc0654e0u,0xc075d60u})cpu.callHooks[at]=[](RefCpu&){};
                cpu.callHooks[0xc062de0]=[&](RefCpu&){
                    order.push_back(101);
                    for(unsigned i=0;i<solverCalls;++i)source.unrelatedRandom();
                    RefCpu request(m);request.r[15]=stack+0x1000;request.pr=stop;
                    request.r[4]=0;request.r[5]=weather;request.setFloat(4,strength);
                    instructions+=request.run(0xc142520,stop,200);
                };
                cpu.callHooks[0xc063ce0]=[&](RefCpu&){order.push_back(102);};
                cpu.callHooks[0xc0610c0]=[&](RefCpu&){order.push_back(103);};
                cpu.callHooks[0xc069020]=[&](RefCpu&){order.push_back(104);};
                cpu.callHooks[0xc160160]=[](RefCpu& c){c.r[0]=0;};
                cpu.callHooks[0xc191b20]=[](RefCpu& c){c.r[0]=0;};
                cpu.callHooks[0xc190d00]=[](RefCpu&){};
                cpu.callHooks[0xc142040]=[&](RefCpu& c){
                    equal(c.r[5],1,"outer forced tire start");
                    sourceCommands.push_back({OriginalTireCommandType::Play,signed32(c.r[4])});
                };
                cpu.callHooks[0xc1433c0]=[&](RefCpu&){sourceCommands.push_back({OriginalTireCommandType::Stop,0});};
                cpu.callHooks[0xc143400]=[&](RefCpu& c){sourceCommands.push_back({OriginalTireCommandType::Volume,signed32(c.r[5])});};
                cpu.callHooks[0xc1cf620]=[&](RefCpu&){order.push_back(105);};
            },0xc056ec0,0xc056ed2);
            for(unsigned i=0;i<solverCalls;++i)rng();
            requestOriginalTireAudio(tire,0,25.f*3.6f,0,weather,strength);
            auto wrongOrderTire=tire;unsigned wrongOrderSeed=rng.state;
            stepOriginalTireAudio(wrongOrderTire,0,wrongOrderSeed);
            auto wrongOrderSystem=system;
            const auto scene=OriginalCarSmokeScene{7,0,weather,7,mode,requested};
            state.advanceShared(scene,cars,rng.state);
            const auto nativeCommands=stepOriginalTireAudio(tire,0,rng.state);
            wrongOrderSystem.advance(scene,cars,[&](){wrongOrderSeed=wrongOrderSeed*1103515245u+12345u;return float((wrongOrderSeed>>16)&32767u)/32768.f;});
            // Even if both orders consume the same number of random values,
            // the selected sound or particle samples must reveal the swap.
            bool differs=wrongOrderTire.kind!=tire.kind;
            if(!system.order().empty()&&!wrongOrderSystem.order().empty()){
                const auto& a=system.cloud(system.order().front()).emitter.particles;
                const auto& b=wrongOrderSystem.cloud(wrongOrderSystem.order().front()).emitter.particles;
                if(!a.empty()&&!b.empty())differs|=a.back().velocity!=b.back().velocity||a.back().position!=b.back().position;
            }
            changedByOrder+=differs;
            source.compare(system,rng);
            if(nativeCommands!=sourceCommands)throw std::runtime_error("Outer source tire command sequence differs");
            equal(std::bit_cast<unsigned>(tire.volume),m.read32(0xc8ff1c4),"outer tire volume");
            equal(tire.phase,m.read32(0xc8ff1e4),"outer tire phase");
            equal(tire.frames,m.read32(0xc8ff1e8),"outer tire frame count");
            equal(tire.kind,m.read32(0xc31de5c),"outer tire kind");
            std::vector<unsigned> expected{0,100,1,2,3,4,5,101,102,6};
            if(weather!=1)expected.push_back(103);
            for(unsigned label:{7u,104u,8u,9u,0u,105u})expected.push_back(label);
            if(order!=expected){for(auto value:order)std::cerr<<value<<' ';throw std::runtime_error("Original outer owner order differs");}
            ++checks;++coupled;commands+=unsigned(nativeCommands.size());
        }
    }
    if(!changedByOrder)throw std::runtime_error("Ordering mutation was not detected");
    std::cout<<"PASS outer scene dispatch: "<<coupled<<" coupled smoke/tire frames; "<<commands
        <<" audio commands; "<<changedByOrder<<" frames discriminate sound-before-smoke. "
        <<"056EC0/023480/05FDC0/066840/060860/1415E0/142120 and RNG execute original instructions. "
        <<"Solver, course, HUD/rules, publication, dry marks, lighting and stream device remain explicit boundaries.\n";
}
}
