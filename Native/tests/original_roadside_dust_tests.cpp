#include "original_roadside_dust.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <cmath>
#include <limits>
#include <stdexcept>
using namespace idas3::original;
using namespace idas3::reference;

int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Original canonical image required");
    RefMemory memory(argv[1]);
    constexpr unsigned frame=0xc800000,race=0xc810000,owner=0xc820000,actor=0xc830000,
        alternate=0xc831000,manager=0xc840000,emitter=0xc850000,stack=0xc860000;
    std::uint64_t cases=0,checks=0,instructions=0;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);};
    for(unsigned course=0;course<9;++course)for(unsigned night=0;night<2;++night)
    for(unsigned weather=0;weather<2;++weather)for(unsigned side=0;side<4;++side)
    for(unsigned flags=0;flags<256;++flags)for(unsigned state=0;state<4;++state)
    for(bool alternateActor:{false,true})for(float gain:{1.f,.2f}){
        OriginalRoadsideDustInput input{course,night,weather,side};
        input.displayGain=gain;
        // Opposite flags on the other wheels distinguish the source's3,2,1,0
        // permutation from simply reading a combined/first contact flag.
        input.wheelSurfaces.fill(std::uint8_t(flags^0x20u));input.wheelSurfaces[3-side]=std::uint8_t(flags);
        input.emitterPresent=state>=2;input.emitterKind=state==2?2:1;input.allocationAvailable=state!=1;
        const auto result=originalRoadsideDustDecision(input);
        memory.clear();memory.zeroRegion(frame,0x1000);memory.zeroRegion(race,0x2000);
        memory.zeroRegion(owner,0x1000);memory.zeroRegion(actor,0x2000);
        memory.zeroRegion(manager,0x1000);memory.zeroRegion(emitter,0x1000);memory.zeroRegion(stack,0x1000);
        const auto slot=2+side;
        memory.write32(frame+132,race);memory.write32(frame+140,2);memory.write32(frame+152,side);
        memory.write32(frame+184,slot*4);memory.write32(frame+136,owner);
        memory.write32(race+1652,course);memory.write32(race+1664,night);memory.write32(race+1668,weather);
        memory.write32(race+0x4bc+56,manager);memory.write32(race+0x4bc+60+slot*4,input.emitterPresent?emitter:0);
        memory.write32(manager+40+slot*4,input.emitterKind);
        memory.write32(owner+0x93c+32,actor);memory.write32(owner+0x93c+36,alternateActor?alternate:0);
        for(unsigned i=0;i<4;++i){memory.write8(actor+116+i,alternateActor?std::uint8_t(flags^0x20u):input.wheelSurfaces[i]);
            memory.write8(alternate+116+i,input.wheelSurfaces[i]);}
        RefCpu cpu(memory);cpu.r[14]=frame;cpu.r[15]=stack+0x800;
        cpu.r[11]=frame+64;cpu.r[12]=frame+76;cpu.r[13]=frame+92;cpu.setFloat(14,gain);
        unsigned allocations=0,starts=0,stops=0,scale=0,color=0;
        cpu.callHooks[0xc140320]=[&](RefCpu& c){++allocations;check(c.r[4]==manager,"Unexpected manager");c.r[0]=input.allocationAvailable?emitter:0;};
        cpu.callHooks[0xc140660]=[&](RefCpu& c){++starts;check(c.r[4]==emitter,"Unexpected emitter");scale=c.fr[4];color=memory.read32(c.r[15]+16);};
        cpu.callHooks[0xc13e940]=[&](RefCpu& c){++stops;check(c.r[4]==emitter,"Unexpected stopped emitter");};
        instructions+=cpu.run(0xc060b38,0xc060c76,200);++cases;
        const auto action=starts?OriginalRoadsideDustDecision::Action::Start:stops?OriginalRoadsideDustDecision::Action::Stop:OriginalRoadsideDustDecision::Action::None;
        check(starts<=1&&stops<=1&&!(starts&&stops),"Multiple source actions");
        check(result.action==action,"Source dust admission differs");
        check(result.replacesTireSmoke==(memory.read32(frame+164)!=0),"Source tire smoke exclusion differs");
        check(allocations==unsigned(result.replacesTireSmoke&&!input.emitterPresent),"Source allocation policy differs");
        if(starts){check(scale==std::bit_cast<unsigned>(result.displayGain),"Source display gain differs");check(color==result.color,"Source brown tint differs");
            check(memory.read32(manager+40+slot*4)==result.emitterKind,"Source emitter kind differs");}
        if(stops)check(memory.read32(race+0x4bc+60+slot*4)==0&&memory.read32(manager+40+slot*4)==0,"Source stop leaves stale emitter");
    }
    // Actual dust update branch: threshold equality/adjacent float values,
    // front/rear lifetime difference and the wheel position passed downstream.
    for(unsigned index=0;index<4;++index)
    for(float speed:{-1.f,0.f,std::nextafter(.1f,0.f),.1f,std::nextafter(.1f,1.f),1.f,35.f,std::numeric_limits<float>::quiet_NaN()}){
        memory.clear();memory.zeroRegion(frame,0x1000);memory.zeroRegion(stack,0x1000);
        memory.write32(frame+164,1);memory.write32(frame+152,index);
        memory.writeFloat(frame+92,13.25f);memory.writeFloat(frame+96,41.75f);memory.writeFloat(frame+100,-18.5f);
        RefCpu cpu(memory);cpu.r[14]=frame;cpu.r[15]=stack+0x800;
        cpu.r[11]=frame+64;cpu.r[12]=frame+76;cpu.r[13]=frame+92;cpu.r[7]=emitter;
        cpu.setFloat(12,speed);cpu.fr[13]=memory.read32(0xc060ae8);
        unsigned updates=0,scale=0;
        cpu.callHooks[0xc222372]=[&](RefCpu& c){for(unsigned i=0;i<64;i+=4)memory.write32(c.r[4]+i,memory.read32(c.r[5]+i));};
        cpu.callHooks[0xc140980]=[&](RefCpu& c){++updates;scale=c.fr[4];check(c.r[4]==emitter,"Dust update owner");
            for(unsigned i=0;i<3;++i)check(c.r[5+i]==memory.read32(frame+92+i*4),"Dust update wheel position");};
        instructions+=cpu.run(0xc060f00,0xc06103e,150);++cases;
        check(updates==1,"Dust update count");check(scale==std::bit_cast<unsigned>(originalRoadsideDustLifetimeScale(index,speed)),"Dust lifetime scale");
    }
    std::cout<<"PASS "<<cases<<" roadside dust admission/update cases, "<<checks<<" comparisons, "<<instructions
        <<" original instructions; all wheel flag bytes, nine courses, day/night, dry/wet, four source wheel indices, both owner display gains, actor fallback, allocation failure, existing emitters and stationary/threshold/front/rear lifetimes. Allocation/start/stop/update/copy calls are explicit observation hooks; particle simulation/rendering not covered.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
