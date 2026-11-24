#include "original_time_attack_telemetry.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3::original;using namespace idas3::reference;
int main(int argc,char** argv)try{
    if(argc!=2)throw std::runtime_error("Canonical image required");RefMemory memory(argv[1]);
    constexpr unsigned owner=0xd000000,drive=0xd001000,recorder=0xd002000,clock=0xd003000,
        stack=0xd004000,wallXYZ=0xd005000,wallIndex=0xd006000,wallMagnitude=0xd007000,
        ditchXYZ=0xd008000,ditchIndex=0xd009000,nop=0xff0000;
    unsigned checks=0,instructions=0;
    const auto check=[&](bool good,const char* why){++checks;if(!good)throw std::runtime_error(why);};
    for(unsigned mode:{0u,1u,2u})for(unsigned course:{0u,1u,2u,8u})for(unsigned direction:{0u,1u})
    for(unsigned initial:{0u,71u,0xfffffffbu}){
        memory.clear();memory.zeroRegion(owner,0xb000);memory.zeroRegion(0xc91fb0c,0x100);
        memory.write32(0xc92de30,initial);memory.write32(0xc31c9a0,course);memory.write32(0xc31c9a8,direction);
        memory.write32(owner+0x668,mode);memory.write32(owner+0x6d0,recorder);
        memory.write32(owner+0x49c,clock);memory.write32(clock+8,12000);memory.write32(owner+0x61c,0);
        memory.writeFloat(drive,-130);memory.writeFloat(drive+4,17);memory.writeFloat(drive+8,-285);
        memory.write32(0xc91fb0c+20,wallXYZ);memory.write32(0xc91fb0c+24,wallMagnitude);memory.write32(0xc91fb0c+28,wallIndex);
        memory.write32(0xc91fb0c+36,ditchXYZ);memory.write32(0xc91fb0c+40,ditchIndex);
        OriginalTimeAttackTelemetry nativeRecorder;
        unsigned prior=initial,recordCalls=0;
        for(unsigned raw:{1300u,1600u,1630u,1631u}){
            memory.write32(owner+0x5b4,raw);std::vector<unsigned> order;
            RefCpu cpu(memory);cpu.r[12]=owner;cpu.r[13]=nop;cpu.r[15]=stack+0xf00;
            cpu.callHooks[nop]=[](auto&){};
            for(unsigned target:{0xc063ce0,0xc066840,0xc069020,0xc069e40})cpu.callHooks[target]=[](auto&){};
            // Link status is an explicit non-ready boundary; kind1 never
            // calls the TA recorder. Rendering/audio/rules are out of scope.
            cpu.callHooks[0xc05a300]=[&](auto& c){c.r[0]=clock;};
            cpu.callHooks[0xc161be0]=[](auto& c){c.r[0]=0;};
            cpu.callHooks[0xc1641e0]=[](auto& c){c.r[0]=0;};
            cpu.callHooks[0xc191b20]=[](auto& c){c.r[0]=5000;};
            cpu.callHooks[0xc190d80]=[](auto&){}; // start-interval owner is independently tested
            cpu.callHooks[0xc190dc0]=[&](auto& c){++recordCalls;order.push_back(3);check(c.r[5]==memory.read32(0xc92de30),"Recorder publisher missed updated progress");};
            cpu.callHooks[0xc062de0]=[&](auto&){
                order.push_back(1);check(memory.read32(0xc92de30)==prior,"Parent updated recorder before solver");
                // Execute the actual source append blocks. Admission is a
                // synthetic wall/ditch event; geometry/forces are separate.
                RefCpu wall(memory);wall.r[9]=drive;wall.fr[12]=std::bit_cast<unsigned>(0.04f);
                instructions+=unsigned(wall.run(0xc1580b4,0xc158104,100));
                RefCpu ditch(memory);ditch.r[9]=drive;
                instructions+=unsigned(ditch.run(0xc157dd0,0xc157e10,100));
                order.push_back(2);
            };
            instructions+=unsigned(cpu.run(0xc05ffae,0xc060080,500));
            const auto n=memory.read32(drive+0x3f4)-1;
            check(memory.read32(wallIndex+n*4)==prior&&memory.read32(ditchIndex+n*4)==prior,"Contact history used new recorder value");
            if(mode==2){
                prior=nativeRecorder.recordProgress(course,direction,raw,5000,12000);
                check(order==std::vector<unsigned>{1,2,3},"Source mode2 parent ordering changed");
            }else check(order==std::vector<unsigned>{1,2},"Source non-mode2 wrote recorder");
            check(memory.read32(0xc92de30)==prior,"Native direction-normalized recorder differs");
        }
        check(recordCalls==(mode==2?4u:0u),"Source recorder mode gate differs");
    }
    std::cout<<"PASS "<<checks<<" recording-progress source checks, "<<instructions
        <<" original instructions. Real parent tail, contact append blocks and190D00 normalization; event admission, pose/effects, record length/start interval/publisher are explicit boundaries. No clock claim.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
