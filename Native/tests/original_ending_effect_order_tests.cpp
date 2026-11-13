#include "sh4_scalar_reference.h"
#include <algorithm>
#include <iostream>
#include <map>
#include <vector>
using namespace idas3::reference;
namespace {
constexpr unsigned manager=0xc900000,car1=manager+0x1000,car2=manager+0x3000,view=manager+0x5000;
constexpr unsigned context=manager+0x6000,vtable=manager+0x7000,stack=manager+0x1f000,stop=0xcfffffc;
unsigned checks=0,cases=0;std::uint64_t instructions=0;
void equal(unsigned a,unsigned b,const char* what){++checks;if(a!=b){std::cerr<<what<<" case="<<cases<<" source="<<std::hex<<a<<" host="<<b<<std::dec<<'\n';throw std::runtime_error("Ending effect order mismatch");}}
}
int main(int argc,char**argv)try{
    if(argc!=2)throw std::invalid_argument("Canonical image required");RefMemory m(argv[1]);
    for(unsigned night:{0u,1u,2u})for(unsigned weather:{0u,1u,2u})for(unsigned snow:{0u,1u})for(unsigned flame:{0u,1u,2u,3u}){
        m.clear();m.zeroRegion(manager,0x80000);m.write32(0xc31ce38,snow?0x04000000:0);
        for(unsigned i=0;i<2;++i){const unsigned car=i?car2:car1;
            m.write32(car+0x34c,19);m.write32(car+0x2d4,(flame&(1u<<i))?0x40080000u:0);}
        unsigned heap=manager+0x20000;std::map<unsigned,unsigned> nodes;std::vector<unsigned> constructed,updated;
        RefCpu cpu(m);cpu.r[4]=manager;cpu.r[5]=car1;cpu.r[6]=car2;cpu.r[7]=view;cpu.r[15]=stack;cpu.pr=stop;
        m.write32(stack,1);m.write32(stack+4,night);m.write32(stack+8,weather);m.write32(stack+12,0);m.write32(stack+16,1);
        cpu.callHooks[0xc221fc0]=[&](RefCpu& c){c.r[0]=context;};
        const auto allocate=[&](RefCpu& c){const unsigned bytes=c.r[4];if(bytes>0x10000)throw std::runtime_error("Unexpected effect allocation");
            c.r[0]=heap;heap+=(bytes+63)&~63u;if(heap>manager+0x70000)throw std::runtime_error("Effect fixture heap exhausted");};
        cpu.callHooks[0xc021ee0]=allocate;cpu.callHooks[0xc021e80]=allocate;cpu.callHooks[0xc021f00]=allocate;
        const unsigned constructors[]={0xc1ad820,0xc17e0e0,0xc17bcc0,0xc180520,0xc1a8ae0};
        for(unsigned constructor:constructors)cpu.callHooks[constructor]=[&,constructor](RefCpu& c){
            const unsigned node=c.r[4];nodes[node]=constructor;constructed.push_back(constructor);
            //Resource-owning children are boundaries; real list construction,
            //registration, linking and virtual iteration execute unhooked.
            m.write32(node+12,vtable);m.write16(vtable+24,0);m.write32(vtable+28,0xcffff00);
            c.r[0]=node;
        };
        instructions+=cpu.run(0xc17d560,stop,50000);
        equal(m.read32(manager+76),car1,"Manager primary");equal(m.read32(manager+80),car2,"Manager rival");
        equal(m.read32(manager+92),1,"Manager replay mode");
        std::vector<unsigned> expected;
        if(weather==1)expected.push_back(snow?0xc1ad820:0xc17e0e0);
        if(flame)expected.push_back(0xc17bcc0);
        if(night==0&&weather==0)expected.push_back(0xc180520);
        if(night==1||weather==1)expected.push_back(0xc1a8ae0);
        equal(unsigned(constructed.size()),unsigned(expected.size()),"Constructed effect count");
        for(unsigned i=0;i<expected.size();++i)equal(constructed[i],expected[i],"Construction order");
        RefCpu step(m);step.r[4]=manager;step.r[15]=stack;step.pr=stop;
        step.callHooks[0xc1fa9e0]=[](RefCpu&){}; //Rendering-state setup; no child traversal.
        step.callHooks[0xcffff00]=[&](RefCpu& c){updated.push_back(nodes.at(c.r[4]));};
        instructions+=step.run(0xc17dfe0,stop,10000);
        equal(unsigned(updated.size()),unsigned(expected.size()),"Updated effect count");
        for(unsigned i=0;i<expected.size();++i)equal(updated[i],expected[i],"Source list update order");
        ++cases;
    }
    // E36A4 consumes the camera manager's selected-list index (+130), not
    // the recording cursor. The ending camera file has one kind13 entry.
    // Execute real forward/wrap selection on a one-entry list, followed by
    // the ending's change detector: even a camera cycle stays at index0.
    m.clear();m.zeroRegion(manager,0x80000);
    constexpr unsigned cameraManager=car1,node=car2,head=view;
    m.write32(cameraManager+4,node);m.write32(cameraManager+8,head);
    m.write32(cameraManager+12,node+4);m.write32(cameraManager+0x12c,1);
    m.write32(node+8,head);m.write32(manager+0x284,cameraManager);
    for(unsigned tick=0;tick<6000;++tick){
        RefCpu select(m);select.r[4]=cameraManager;select.r[15]=stack;select.pr=stop;
        instructions+=select.run(0xc0a9da0,stop,1000);
        equal(m.read32(cameraManager+0x130),0,"Single-camera wrap index");
        RefCpu signal(m);signal.r[13]=manager+0x27c;
        instructions+=signal.run(0xc0e36a4,0xc0e36e2,100);
        equal(signal.r[2],0,"Single-camera ending has no cut");
    }
    unsigned previous=0;
    for(unsigned current:{0u,1u,1u,2u,0u,0u,13u,13u}){
        m.write32(cameraManager+0x130,current);
        RefCpu signal(m);signal.r[13]=manager+0x27c;
        instructions+=signal.run(0xc0e36a4,0xc0e36e2,100);
        equal(signal.r[2],current!=previous,"Changed-camera signal");
        equal(m.read32(cameraManager+0x134),current,"Consumed camera index");previous=current;
    }
    std::cout<<"PASS "<<cases<<" manager cases / "<<checks<<" checks / "<<instructions<<" original instructions. Original manager admission, list construction, linking and dispatch preserve weather, conditional engine effect, dry-day effect, then lamp order. Original single-camera wrap and ending cut detector execute unchanged for6000 cycles. Child resource constructors and updates are explicit hooks.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
