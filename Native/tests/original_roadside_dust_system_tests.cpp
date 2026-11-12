#include "original_roadside_dust_system.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <map>
using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned carsAt=0xc900000,partsAt=0xc908000,actorsAt=0xc90c000,race=0xc910000;
constexpr unsigned manager=0xc912000,emitters=0xc914000,particles=0xc920000;
constexpr unsigned stack=0xc930000,tls=0xc934000,matrixStack=0xc935000,sentinels=0xc936000,stop=0xcfffffc;
std::uint64_t checks=0,ticks=0,instructions=0;
unsigned scenario=0,step=0;
void equal(unsigned source,unsigned host,const char* label){++checks;if(source!=host){
    std::cerr<<"scenario "<<scenario<<" tick "<<step<<' '<<label<<" source="<<std::hex<<source<<" native="<<host<<std::dec<<'\n';
    throw std::runtime_error("Dust lifecycle reference mismatch");}}
struct Random {
    unsigned state=0x2a631778,calls=0;
    float operator()(){state^=state<<13;state^=state>>17;state^=state<<5;++calls;return float(state&0xffffu)/65536.f;}
};
struct Oracle {
    RefMemory& m;
    std::map<unsigned,std::vector<unsigned>> lists;
    std::map<unsigned,unsigned> ends;
    Random random;
    explicit Oracle(RefMemory& memory):m(memory){}
    void link(unsigned list){
        const auto& members=lists.at(list);const auto end=ends.at(list);
        m.write32(list,end);m.write32(list+4,members.empty()?end:members.front()+4);
        for(unsigned i=0;i<members.size();++i)m.write32(members[i]+8,i+1<members.size()?members[i+1]+4:end);
    }
    void initList(unsigned list){
        if(!ends.contains(list))ends[list]=sentinels+16u*unsigned(ends.size());
        lists[list].clear();link(list);
    }
    void clearObject(unsigned at,unsigned size){for(unsigned i=0;i<size;i+=4)m.write32(at+i,0);}
    void reset(const OriginalCarParts& parts,unsigned course,unsigned night,unsigned weather){
        m.clear();m.zeroRegion(carsAt,0x38000);lists.clear();ends.clear();random={};
        m.write32(tls+4,tls+64);m.write32(tls+68,tls+128);
        m.write32(0xc98ad0c,0x00200000);m.write32(0xc98ad10,matrixStack);m.write32(0xc98ad14,matrixStack);
        m.write32(0xc8ff16c,0); // no display objects: cleanup is exercised separately below
        m.write32(0xc8ff18c,4);m.write32(0xc8ff1ac,60);m.write32(0xca9bfb0,0);
        const auto p=originalSmokeParameters(0);for(unsigned i=0;i<9;++i)m.writeFloat(0xca9badc+i*4,p.values[i]);
        m.write32(race+0x4f4,manager);m.write32(race+0x674,course);m.write32(race+0x680,night);m.write32(race+0x684,weather);
        initList(manager+72);
        for(unsigned car=0;car<2;++car){
            const auto at=carsAt+car*0x4000,part=partsAt+car*0x2000,actor=actorsAt+car*0x1000;
            m.write32(race+0x418+car*4,at);m.write32(at+0x2f0,part);
            m.write32(at+0x95c,actor);m.write32(at+0x960,actor);m.write32(at+0x964+64,0xc380b8c);
            unsigned pointer=part;for(const auto& t:parts.transforms)for(const auto* values:{&t.scale,&t.rotationDegrees,&t.translation})
                for(float value:*values){m.writeFloat(pointer,value);pointer+=4;}
        }
    }
    void car(unsigned index,const OriginalMatrix& world,const std::array<float,3>& delta,const std::array<std::uint8_t,4>& surfaces){
        const auto at=carsAt+index*0x4000,actor=actorsAt+index*0x1000;
        for(unsigned i=0;i<16;++i)m.writeFloat(at+0x964+i*4,world.elements[i]);
        for(unsigned i=0;i<3;++i)m.writeFloat(actor+12+i*4,delta[i]);
        for(unsigned i=0;i<4;++i)m.write8(actor+116+i,surfaces[i]);
    }
    void tick(unsigned mode,bool playerRequested){
        m.write32(race+0x668,mode);m.write8(race+0x494,playerRequested?1:0);
        RefCpu cpu(m);cpu.r[4]=race;cpu.r[15]=stack+8192;cpu.pr=stop;
        const auto identity=originalIdentityMatrix();for(unsigned i=0;i<16;++i)cpu.xf[i]=std::bit_cast<unsigned>(identity.elements[i]);
        cpu.callHooks[0xc221fc0]=[&](RefCpu& c){c.r[0]=tls;};
        cpu.callHooks[0xc1f9ea0]=[&](RefCpu& c){c.setFloat(0,random());};
        // Only allocation, generic object/list infrastructure and exception TLS
        // are boundaries. ARace, CarSmokeEvt construction/start/attachment,
        // SmokeMgr, emitter scheduling and particle motion execute unchanged.
        cpu.callHooks[0xc021960]=[&](RefCpu& c){
            unsigned at=0;
            if(c.r[5]==304){
                for(unsigned i=0;i<4;++i){const auto candidate=emitters+i*512;
                    if(std::find(lists.at(manager+72).begin(),lists.at(manager+72).end(),candidate)==lists.at(manager+72).end()){at=candidate;break;}}
            }else if(c.r[5]==60){
                for(unsigned i=0;i<60&&!at;++i){const auto candidate=particles+i*64;bool used=false;
                    for(const auto& [owner,members]:lists){(void)owner;if(std::find(members.begin(),members.end(),candidate)!=members.end()){used=true;break;}}
                    if(!used)at=candidate;}
            }else throw std::runtime_error("Unexpected source allocation size "+std::to_string(c.r[5]));
            if(!at)throw std::runtime_error("Source exceeded its shared allocation budget");
            clearObject(at,c.r[5]);c.r[0]=at;
        };
        cpu.callHooks[0xc0219a0]=[](RefCpu& c){c.r[0]=c.r[4];};
        cpu.callHooks[0xc0220a0]=[&](RefCpu& c){initList(c.r[4]);c.r[0]=c.r[4];};
        cpu.callHooks[0xc13d5c0]=[&](RefCpu& c){m.write32(c.r[4],0xc38a764);c.r[0]=c.r[4];};
        cpu.callHooks[0xc022380]=[&](RefCpu& c){lists.at(c.r[4]).push_back(c.r[5]-4);link(c.r[4]);};
        cpu.callHooks[0xc022400]=[&](RefCpu& c){std::erase(lists.at(c.r[4]),c.r[5]-4);link(c.r[4]);};
        instructions+=cpu.run(0xc060860,stop,100000);
        for(auto e:lists.at(manager+72))for(auto p:lists.at(e+36)){
            RefCpu display(m);display.r[10]=p;display.r[14]=stack+512;display.r[15]=stack+8192;display.pr=stop;
            display.callHooks[0xc13fcc0]=[](RefCpu& c){c.r[0]=0;c.pr=stop;};
            display.callHooks[0xc13fcce]=[](RefCpu& c){c.r[0]=1;c.pr=stop;};
            instructions+=display.run(0xc13fcae,stop,80);
        }
    }
    void compare(const OriginalRoadsideDustSystem& system,const Random& rng){
        const auto& members=lists.at(manager+72);
        equal(unsigned(members.size()),unsigned(system.order().size()),"emitter count");
        equal(m.read32(0xc8ff18c),4u-unsigned(system.order().size()),"free emitters");
        equal(m.read32(0xc8ff1ac),system.freeParticles(),"free particles");
        equal(random.calls,rng.calls,"RNG calls");equal(random.state,rng.state,"RNG state");
        equal(unsigned(system.particleCount())+system.freeParticles(),60,"conserved particle pool");
        for(unsigned i=0;i<members.size();++i){
            const auto at=members[i];const auto& cloud=system.cloud(system.order()[i]);const auto& e=cloud.emitter;
            for(const auto [offset,value]:std::array<std::pair<unsigned,unsigned>,3>{{{220,e.flags},{176,e.stopped},{180,e.frame}}})
                equal(m.read32(at+offset),value,"emitter state");
            for(const auto [offset,value]:std::array<std::pair<unsigned,float>,3>{{{28,e.scale},{32,e.force},{72,e.lifetimeScale}}})
                equal(m.read32(at+offset),std::bit_cast<unsigned>(value),"emitter scalar");
            for(const auto& [offset,values]:std::array<std::pair<unsigned,std::array<float,3>>,4>{{{108,e.spread},{124,e.position},{140,e.velocity},{164,e.previousPosition}}})
                for(unsigned axis=0;axis<3;++axis)equal(m.read32(at+offset+axis*4),std::bit_cast<unsigned>(values[axis]),"emitter vector");
            for(unsigned j=0;j<16;++j)equal(m.read32(at+240+j*4),std::bit_cast<unsigned>(cloud.footprint.elements[j]),"footprint");
            const auto& children=lists.at(at+36);equal(unsigned(children.size()),unsigned(e.particles.size()),"ordered particle count");
            for(unsigned j=0;j<children.size();++j){const auto p=children[j];const auto& child=e.particles[j];
                for(unsigned axis=0;axis<3;++axis){equal(m.read32(p+24+axis*4),std::bit_cast<unsigned>(child.position[axis]),"particle position");equal(m.read32(p+36+axis*4),std::bit_cast<unsigned>(child.velocity[axis]),"particle velocity");}
                equal(m.read32(p+48),std::bit_cast<unsigned>(child.size),"particle size");equal(m.read32(p+52),child.remaining,"particle life");equal(m.read32(p+56),child.flags,"particle flags");
            }
            const auto attached=m.read32(race+0x4f8+(cloud.car*4+cloud.effect)*4)==at;
            equal(unsigned(attached),unsigned(e.stopped==0),"owner attachment/stop");
        }
        ++ticks;
    }
};
}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::invalid_argument("canonical-image native-root required");
    RefMemory memory(argv[1]);Oracle source(memory);
    const auto parts=OriginalCarParts::load(std::filesystem::path(argv[2])/"data/original_models/toyota_ae86t/assembly_parts.bin");
    const auto origins=originalCarEffectOrigins(parts);
    for(scenario=0;scenario<16;++scenario){
        OriginalRoadsideDustSystem system;Random rng;source.reset(parts,7,0,0);
        std::array<OriginalMatrix,2> worlds{originalIdentityMatrix(),originalIdentityMatrix()};
        for(step=0;step<240;++step){
            std::array<OriginalRoadsideDustCar,2> cars;
            for(unsigned car=0;car<2;++car){
                const auto mask=step>=180?0u:step<60?scenario:((step/(3+scenario)+car*5)&15u);
                const std::array<float,3> delta=step%37<6?std::array<float,3>{0,0,0}:
                    step%37==6?std::array<float,3>{.1f,0,0}:std::array<float,3>{.125f*float(car+1),.03125f,1.25f};
                for(unsigned axis=0;axis<3;++axis)worlds[car].elements[12+axis]+=delta[axis];
                cars[car].available=true;cars[car].owner=originalRoadsideDustOwner(worlds[car],origins,delta);
                for(unsigned wheel=0;wheel<4;++wheel)cars[car].wheelSurfaces[wheel]=std::uint8_t((mask&(1u<<wheel))?0x26:0x06);
                source.car(car,worlds[car],delta,cars[car].wheelSurfaces);
            }
            const unsigned mode=step<90?2u:step<120?3u:1u;
            const bool requested=step<140||step%19<8;
            source.tick(mode,requested);system.advance(7,0,0,mode,requested,cars,rng);source.compare(system,rng);
        }
        equal(unsigned(system.order().size()),0,"all clouds drained");equal(system.freeParticles(),60,"full pool restored");
    }
    std::cout<<"PASS "<<ticks<<" full ARace/SmokeMgr lifecycle ticks, "<<checks<<" bitwise checks, "<<instructions
        <<" original instructions. Four-wheel admission, owner switches, stationary/moving attachment, shared budgets, ordered particles, stop, drain and reuse. Source constructors/start/attachment/manager/motion run; boundaries are allocator, base object/list infrastructure, RNG, exception TLS and display-block exits. No rendering or gameplay integration claim.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
