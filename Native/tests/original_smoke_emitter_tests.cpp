#include "original_smoke_emitter.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <limits>
using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned emitter=0xc800000,particles=0xc810000,stack=0xc830000,tls=0xc840000;
constexpr unsigned sentinel=0xc850000,stop=0xcfffffc;
std::uint64_t checks=0,cases=0,instructions=0;
void check(bool ok,const char* label){++checks;if(!ok)throw std::runtime_error(label);}
void equal(unsigned source,unsigned host,const char* label){++checks;if(source!=host){
    std::cerr<<label<<" source="<<std::hex<<source<<" host="<<host<<std::dec<<'\n';
    throw std::runtime_error("Smoke emitter reference mismatch");}}
struct Random {
    unsigned state=0x2a631778,calls=0;
    float operator()(){state^=state<<13;state^=state>>17;state^=state<<5;++calls;return float(state&0xffffu)/65536.f;}
};
void writeParticle(RefMemory& m,unsigned at,const OriginalSmokeParticle& p){
    m.zeroRegion(at,60);m.write32(at,0xc38a764);m.write32(at+20,emitter);
    for(unsigned i=0;i<3;++i){m.writeFloat(at+24+i*4,p.position[i]);m.writeFloat(at+36+i*4,p.velocity[i]);}
    m.writeFloat(at+48,p.size);m.write32(at+52,p.remaining);m.write32(at+56,p.flags);
}
void compareParticle(RefMemory& m,unsigned at,const OriginalSmokeParticle& p){
    for(unsigned i=0;i<3;++i){equal(m.read32(at+24+i*4),std::bit_cast<unsigned>(p.position[i]),"position");equal(m.read32(at+36+i*4),std::bit_cast<unsigned>(p.velocity[i]),"velocity");}
    equal(m.read32(at+48),std::bit_cast<unsigned>(p.size),"size");equal(m.read32(at+52),p.remaining,"remaining");equal(m.read32(at+56),p.flags,"particle flags");
}
struct Oracle {
    RefMemory& m;
    std::vector<unsigned> members;
    unsigned nextParticle=particles;
    Random random;
    explicit Oracle(RefMemory& memory):m(memory){}
    void link(){
        m.write32(emitter+36,sentinel);m.write32(emitter+40,members.empty()?sentinel:members.front()+4);
        for(unsigned i=0;i<members.size();++i)m.write32(members[i]+8,i+1<members.size()?members[i+1]+4:sentinel);
    }
    void load(const OriginalSmokeEmitter& e,const OriginalSmokeParameters& p,unsigned free){
        m.clear();m.zeroRegion(emitter,224);m.zeroRegion(stack,8192);m.zeroRegion(tls,32);m.zeroRegion(sentinel,16);
        m.write32(tls+4,tls+8);members.clear();nextParticle=particles;random={};
        for(const auto& particle:e.particles){writeParticle(m,nextParticle,particle);members.push_back(nextParticle);nextParticle+=64;}
        link();
        for(unsigned i=0;i<3;++i){m.writeFloat(emitter+108+i*4,e.spread[i]);m.writeFloat(emitter+124+i*4,e.position[i]);m.writeFloat(emitter+140+i*4,e.velocity[i]);m.writeFloat(emitter+164+i*4,e.previousPosition[i]);}
        m.writeFloat(emitter+28,e.scale);m.writeFloat(emitter+32,e.force);m.writeFloat(emitter+72,e.lifetimeScale);
        m.write32(emitter+176,e.stopped);m.write32(emitter+180,e.frame);m.write32(emitter+220,e.flags);
        for(unsigned i=0;i<9;++i)m.writeFloat(0xca9badc+i*4,p.values[i]);
        m.write32(0xca9bfb0,p.alternateMode);m.write32(0xc8ff1ac,free);
    }
    void attach(const std::array<float,3>& position,float scale){
        RefCpu cpu(m);cpu.r[4]=emitter;for(unsigned i=0;i<3;++i)cpu.r[5+i]=std::bit_cast<unsigned>(position[i]);
        cpu.setFloat(4,scale);cpu.r[15]=stack+4096;cpu.pr=stop;
        instructions+=cpu.run(0xc13e160,stop,1500); // all math helpers execute, no hooks
    }
    void tick(){
        RefCpu cpu(m);cpu.r[4]=emitter;cpu.r[15]=stack+4096;cpu.pr=stop;
        // Dependency hooks are restricted to RNG, C++ exception TLS, allocator,
        // initial object/vtable setup and intrusive-list bookkeeping. The whole
        //13E240 controller,13D800 motion and13D780 initialization execute.
        cpu.callHooks[0xc221fc0]=[&](RefCpu& c){c.r[0]=tls;};
        cpu.callHooks[0xc1f9ea0]=[&](RefCpu& c){c.setFloat(0,random());};
        cpu.callHooks[0xc021960]=[&](RefCpu& c){equal(c.r[5],60,"allocation size");c.r[0]=nextParticle;nextParticle+=64;};
        cpu.callHooks[0xc13d5c0]=[&](RefCpu& c){writeParticle(m,c.r[4],{});c.r[0]=c.r[4];};
        cpu.callHooks[0xc022380]=[&](RefCpu& c){equal(c.r[4],emitter+36,"list append owner");members.push_back(c.r[5]-4);link();};
        cpu.callHooks[0xc022400]=[&](RefCpu& c){equal(c.r[4],emitter+36,"list erase owner");std::erase(members,c.r[5]-4);link();};
        instructions+=cpu.run(0xc13e240,stop,20000);
    }
    void display(){
        for(auto at:members){
            RefCpu cpu(m);cpu.r[10]=at;cpu.r[14]=stack+512;cpu.r[15]=stack+4096;cpu.pr=stop;
            cpu.callHooks[0xc13fcc0]=[&](RefCpu& c){c.r[0]=0;c.pr=stop;};
            cpu.callHooks[0xc13fcce]=[&](RefCpu& c){c.r[0]=1;c.pr=stop;};
            instructions+=cpu.run(0xc13fcae,stop,80);
        }
    }
    void compare(const OriginalSmokeEmitter& e,unsigned free,const Random& rng){
        equal(m.read32(emitter+220),e.flags,"emitter flags");equal(m.read32(emitter+180),e.frame,"frame");
        equal(m.read32(emitter+176),e.stopped,"stopped");equal(m.read32(emitter+32),std::bit_cast<unsigned>(e.force),"force");
        equal(m.read32(emitter+28),std::bit_cast<unsigned>(e.scale),"scale");equal(m.read32(emitter+72),std::bit_cast<unsigned>(e.lifetimeScale),"lifetime scale");
        for(unsigned i=0;i<3;++i){equal(m.read32(emitter+108+i*4),std::bit_cast<unsigned>(e.spread[i]),"spread");equal(m.read32(emitter+124+i*4),std::bit_cast<unsigned>(e.position[i]),"emitter position");equal(m.read32(emitter+140+i*4),std::bit_cast<unsigned>(e.velocity[i]),"emitter velocity");equal(m.read32(emitter+164+i*4),std::bit_cast<unsigned>(e.previousPosition[i]),"previous position");}
        equal(m.read32(0xc8ff1ac),free,"shared free count");equal(random.calls,rng.calls,"random calls");equal(random.state,rng.state,"random state");
        equal(unsigned(members.size()),unsigned(e.particles.size()),"particle count");
        for(unsigned i=0;i<members.size();++i)compareParticle(m,members[i],e.particles[i]);
        ++cases;
    }
};
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Canonical image required");
    RefMemory memory(argv[1]);Oracle source(memory);
    for(const auto delta:std::array<std::array<float,3>,10>{{{0,0,0},{1,0,0},{0,1,0},{0,0,-1},{1.0001f,0,0},{0,-1.0001f,0},{.7f,.7f,0},{-.75f,-.75f,0},{-9,3,8},{.01f,35,.02f}}}){
        auto p=originalSmokeParameters(0);OriginalSmokeEmitter e;e.spread={.25f,-.5f,.75f};e.flags=0x30;
        e.previousPosition={12.5f,128.f,-350.f};source.load(e,p,3);Random random;
        for(unsigned step=0;step<40;++step){
            auto position=e.previousPosition;for(unsigned i=0;i<3;++i)position[i]+=delta[i];
            const float life=float(step%4)*.2f;source.attach(position,life);attachOriginalSmokeEmitter(e,position,life);source.compare(e,3,random);
        }
    }
    for(unsigned table=0;table<3;++table)for(unsigned phase=0;phase<16;++phase)
    for(unsigned frame:{0u,1u,2u,3u,4u,5u,0xfffffffeu,0xffffffffu})
    for(unsigned stopped:{0u,1u,0xffffffffu})for(unsigned capacity:{0u,1u,8u,0xffffffffu})for(unsigned population=0;population<4;++population){
        auto p=originalSmokeParameters(table==2?0:table,table==2);
        OriginalSmokeEmitter e;e.flags=0x972f050bu|(phase<<4);e.frame=frame;e.stopped=stopped;e.scale=.2f;e.force=23.25f;e.lifetimeScale=.713f;
        e.position={149.25f,-98.75f,93.125f};e.velocity={-.25f,.03125f,.125f};e.spread={.2f,-1.75f,3.125f};
        for(unsigned i=0;i<population;++i){OriginalSmokeParticle child;child.flags=i==0?0x110u:i==1?0x10u:0x100u;
            child.remaining=i==0?1u:7u;child.position={12.5f,-1.f,3.5f};child.velocity={-.125f,4.f,1.f};child.size=.125f;e.particles.push_back(child);}
        source.load(e,p,capacity);Random random;unsigned free=capacity;source.tick();advanceOriginalSmokeEmitter(e,p,free,random);source.compare(e,free,random);
        source.display();for(auto& child:e.particles)prepareOriginalSmokeParticleDisplay(child);source.compare(e,free,random);
    }
    // Real multi-tick runs prove scheduling, shared budget exhaustion, draining
    // after stop and the display-to-removal handoff rather than single calls.
    for(unsigned table=0;table<3;++table)for(unsigned stopFrame:{0u,1u,2u,3u,7u,30u,99u}){
        auto p=originalSmokeParameters(table==2?0:table,table==2);
        OriginalSmokeEmitter e;e.flags=0x30;e.scale=.2f;e.lifetimeScale=.4f;e.spread={1.f,2.f,.5f};e.position={3.f,4.f,5.f};
        unsigned free=4;Random random;source.load(e,p,free);
        for(unsigned tick=0;tick<140;++tick){
            if(tick==stopFrame){stopOriginalSmokeEmitter(e);RefCpu cpu(memory);cpu.r[4]=emitter;cpu.r[15]=stack+4096;cpu.pr=stop;instructions+=cpu.run(0xc13e940,stop,30);}
            source.tick();advanceOriginalSmokeEmitter(e,p,free,random);source.compare(e,free,random);
            source.display();for(auto& child:e.particles)prepareOriginalSmokeParticleDisplay(child);source.compare(e,free,random);
        }
        check(e.particles.empty()&&(e.flags&0xf0u)==0&&free==4,"Stopped emitter must fully drain and return its budget");
    }
    std::cout<<"PASS "<<cases<<" emitter/display states, "<<checks<<" bitwise checks, "<<instructions<<" original instructions. All phases, frame rollover, stop, shared pool exhaustion, all source presets, attachment motion/retained basis, particle motion/initialization and multi-tick display cleanup. Hooks: RNG, exception TLS, allocation/object setup, intrusive-list bookkeeping and display-block exits. Rendering/projection and car-owner integration remain separate.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
