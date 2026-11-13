// Reuses the source lifecycle oracle, changing only the replay owner layout.
// E3900, original wheel anchors/matrix math, allocation admission, start,
// attachment, manager, emitter and particles all execute in the SH4 image.
void checkOriginalReplaySmoke(RefMemory& memory,const OriginalCarParts& parts){
    Oracle source(memory);const auto origins=originalCarEffectOrigins(parts);
    unsigned white=0,brown=0,mixed=0,capacity=0,drained=0,second=0;
    const auto initialTicks=ticks,initialChecks=checks,initialInstructions=instructions;
    for(unsigned sceneCode:{0u,6u,13u,14u,15u,16u,17u})for(unsigned weather:{0u,1u})
    for(unsigned count:{0u,1u,2u,3u})for(bool alternate:{false,true}){
        ++scenario;OriginalReplayCarSmokeSystem system;system.reset(weather,alternate);
        Random rng;source.reset(parts,sceneCode/2,0,weather,alternate);
        memory.write32(race+0x78,manager);memory.write32(race+0x118,count);
        memory.write32(race+0xec,sceneCode);memory.write32(race+0xf4,weather);
        for(unsigned car=0;car<2;++car)memory.write32(race+0x2a0+4*car,carsAt+0x4000*car);
        std::array<OriginalMatrix,2> worlds{originalIdentityMatrix(),originalIdentityMatrix()};
        // Nonidentity rigid matrices expose reversed multiplication/lift order.
        worlds[0].elements={0,0,1,0, 0,1,0,0, -1,0,0,0, 5,2,8,1};
        worlds[1].elements={1,0,0,0, 0,0,1,0, 0,-1,0,0, -3,4,-7,1};
        for(step=0;step<220;++step){
            std::array<OriginalCarSmokeInput,2> cars;
            for(unsigned car=0;car<2;++car){
                const std::array<float,3> delta=step>=160||step%23<4?std::array<float,3>{0,0,0}:
                    step%23==4?std::array<float,3>{.1f,0,0}:std::array<float,3>{.125f*float(car+1),.03125f,1.25f};
                for(unsigned axis=0;axis<3;++axis)worlds[car].elements[12+axis]+=delta[axis];
                cars[car].available=true;cars[car].owner=originalRoadsideDustOwner(worlds[car],origins,delta);
                for(unsigned w=0;w<4;++w){
                    const bool dirty=step<160&&((step/9+w+car)%3==0);
                    cars[car].wheelSurfaces[w]=std::uint8_t(dirty?0x20:step%13<4?7:6);
                    cars[car].intensity[w]=step>=160?0:std::uint8_t(((step/7+car+w)%4)*85);
                }
                source.car(car,worlds[car],delta,cars[car].wheelSurfaces,cars[car].intensity);
                // Exercise the fallback actor binding as well as attached actor.
                memory.write32(carsAt+car*0x4000+0x960,step%2?0:actorsAt+car*0x1000);
            }
            if(step%17==0){source.unrelatedRandom();rng();}
            source.tick(0,false,{},0xc0e3900);
            system.advance(sceneCode,weather,count,cars,rng);source.compare(system,rng,0x7c);
            unsigned a=0,b=0;for(auto i:system.order()){
                const auto& cloud=system.cloud(i);a+=cloud.color==0xffffffff;b+=cloud.color==0xff8b5439;
                second+=cloud.car==1;equal(cloud.effect>=2,1,"replay rear wheels only");
            }
            white+=a>0;brown+=b>0;mixed+=a>0&&b>0;capacity+=system.order().size()==4;
        }
        equal(unsigned(system.order().size()),0,"replay clouds drained");equal(system.freeParticles(),60,"replay pool restored");++drained;
    }
    if(!white||!brown||!mixed||!capacity||!second)throw std::runtime_error("Missing replay smoke coverage");
    std::cout<<"PASS replay E3900/SmokeMgr: "<<ticks-initialTicks<<" ticks, "<<checks-initialChecks
        <<" bitwise checks, "<<instructions-initialInstructions<<" original instructions.\n"
        <<"Replay coverage white="<<white<<" brown="<<brown<<" mixed="<<mixed<<" full="<<capacity
        <<" second-car="<<second<<" drained="<<drained<<".\n"<<std::flush;
}
