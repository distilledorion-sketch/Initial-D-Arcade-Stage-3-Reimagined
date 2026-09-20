#include "original_start_grid.h"
#include "sh4_scalar_reference.h"
#include <iomanip>
#include <iostream>
using namespace idas3::original;
using namespace idas3::reference;
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Usage: original_start_grid_reference_tests canonical_program");
    RefMemory memory{std::filesystem::path(argv[1])};
    std::size_t comparisons=0,instructions=0;
    auto equal=[&](std::uint32_t a,std::uint32_t b,const char* what){++comparisons;if(a!=b){std::cerr<<what<<" actual="<<std::hex<<a<<" expected="<<b<<std::dec<<'\n';throw std::runtime_error("Original start grid comparison failed");}};
    constexpr std::uint32_t object=0x0CF00000,race=0x0CF10000,frame=0x0CFFF000,stop=0x0F000000;
    for(std::uint32_t condition=0;condition<18;++condition)for(std::uint32_t slot=0;slot<2;++slot){
        memory.clear();memory.zeroRegion(0x0CFFE000,0x2000);
        const auto native=originalStartPose(condition,slot);
        RefCpu cpu(memory);cpu.r[15]=frame-0x100;cpu.pr=stop;
        cpu.r[4]=condition/2;cpu.r[5]=condition&1;cpu.r[6]=slot;cpu.r[2]=frame;
        instructions+=cpu.run(0x0C191C20,stop,200);
        for(std::uint32_t i=0;i<3;++i)equal(std::bit_cast<std::uint32_t>(native.position[i]),memory.read32(frame+i*4),"all-course original position getter");
        cpu.r[4]=condition/2;cpu.r[5]=condition&1;cpu.r[6]=0;cpu.r[2]=frame+56;cpu.pr=stop;
        instructions+=cpu.run(0x0C191C80,stop,200);
        for(std::uint32_t i=0;i<3;++i)equal(std::bit_cast<std::uint32_t>(native.authoredDirection[i]),memory.read32(frame+56+i*4),"all-course original direction getter");
        cpu.r[14]=frame;
        instructions+=cpu.run(0x0C062A0E,0x0C062A4A,2000);
        for(std::uint32_t i=0;i<3;++i){
            equal(std::bit_cast<std::uint32_t>(native.horizontalDirection[i]),memory.read32(frame+56+i*4),"all-course normalized direction");
            equal(std::bit_cast<std::uint32_t>(native.angles[i]),memory.read32(frame+84+i*4),"all-course original orientation");
        }
    }
    for(std::uint32_t condition=6;condition<=7;++condition){
        for(std::uint32_t slot=0;slot<2;++slot){
            const auto native=originalAkinaStartPose(condition,slot);
            for(std::uint32_t i=0;i<3;++i){
                equal(std::bit_cast<std::uint32_t>(native.position[i]),memory.read32(0x0C29EAC4+(condition-6)*24+slot*12+i*4),"raw position table");
                equal(std::bit_cast<std::uint32_t>(native.authoredDirection[i]),memory.read32(0x0C29EC74+(condition-6)*24+slot*12+i*4),"raw direction table");
            }
            std::cout<<std::setprecision(10)<<"condition="<<condition<<" slot="<<slot<<" position="<<native.position[0]<<','<<native.position[1]<<','<<native.position[2]<<" yaw="<<native.angles[1]<<" yawBits="<<std::hex<<std::bit_cast<std::uint32_t>(native.angles[1])<<std::dec<<'\n';
        }
        for(const std::uint32_t mode:{0u,2u,3u}){
            memory.clear();memory.zeroRegion(object,0x1000);memory.zeroRegion(race,0x1000);memory.zeroRegion(0x0CFFE000,0x2000);
            // Real constructor vtable; actual virtual methods and table getters execute.
            memory.write32(object,0x0C38DC9C);memory.write32(object+48,condition-6);
            memory.write32(race+0x40C,object);memory.write32(race+0x668,mode);
            RefCpu cpu(memory);cpu.r[13]=race;cpu.r[14]=frame;cpu.r[15]=frame-0x100;cpu.pr=stop;
            instructions+=cpu.run(0x0C062706,0x0C062A4A,20000);
            const auto slot=originalSoloStartGridSlot(mode);
            const auto native=originalAkinaStartPose(condition,slot),other=originalAkinaStartPose(condition,1-slot);
            for(std::uint32_t i=0;i<3;++i){
                equal(std::bit_cast<std::uint32_t>(native.position[i]),memory.read32(frame+i*4),"race player position");
                equal(std::bit_cast<std::uint32_t>(other.position[i]),memory.read32(frame+28+i*4),"race secondary position");
                equal(std::bit_cast<std::uint32_t>(native.horizontalDirection[i]),memory.read32(frame+56+i*4),"normalized direction");
                equal(std::bit_cast<std::uint32_t>(native.angles[i]),memory.read32(frame+84+i*4),"race angles");
            }
        }
    }
    // Actual course setter stores the route selector in all three records.
    for(std::uint32_t selector=0;selector<2;++selector){
        memory.clear();memory.zeroRegion(object,0x1000);memory.zeroRegion(0x0CFFE000,0x2000);
        memory.write32(object+24,object+0x100);memory.write32(object+28,object+0x200);
        RefCpu cpu(memory);cpu.r[4]=object;cpu.r[5]=selector;cpu.r[15]=frame;cpu.pr=stop;
        instructions+=cpu.run(0x0C03AA00,stop,100);
        equal(selector,memory.read32(object+48),"course selector");equal(selector,memory.read32(object+0x100+28),"route selector");equal(selector,memory.read32(object+0x200+24),"alternate route selector");
    }
    std::size_t rejected=0;
    for(const auto bad:{5u,8u,0xFFFFFFFFu})try{(void)originalAkinaStartPose(bad,0);}catch(const std::invalid_argument&){++rejected;}
    for(const auto bad:{2u,0xFFFFFFFFu})try{(void)originalAkinaStartPose(6,bad);}catch(const std::invalid_argument&){++rejected;}
    for(const auto bad:{1u,4u,0xFFFFFFFFu})try{(void)originalSoloStartGridSlot(bad);}catch(const std::invalid_argument&){++rejected;}
    if(rejected!=8)throw std::runtime_error("Invalid start-grid input accepted");
    for(const auto bad:{18u,0xFFFFFFFFu})try{(void)originalStartPose(bad,0);throw std::runtime_error("Invalid general condition accepted");}catch(const std::invalid_argument&){}
    std::cout<<"PASS original start grid: "<<comparisons<<" exact comparisons, "<<instructions<<" actual instructions, zero hooks; all36 course/direction/slot getters+orientation and all six Akina/solo-mode race branches.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
