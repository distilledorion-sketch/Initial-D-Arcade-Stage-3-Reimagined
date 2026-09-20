#include "original_attract.h"
#include "sh4_scalar_reference.h"
#include <array>
#include <iostream>
#include <vector>
using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned owner=0x0cd00000,child=0x0cd01000,vtable=0x0cd02000,
    sentinel=0x0cd03000,stack=0x0cfff000,stop=0x00ff0000,dispatch=0x00ed0000;
std::uint64_t checks{},cases{},steps{},hooks{};
void eq(unsigned a,unsigned b,const char* what){++checks;if(a!=b)throw std::runtime_error(std::string(what)+": "+hex(a)+" != "+hex(b));}
void run(RefCpu& c,unsigned start,unsigned end,unsigned limit=10000){try{steps+=c.run(start,end,limit);}catch(const std::exception& e){throw std::runtime_error(hex(start)+" at "+hex(c.pc)+": "+e.what());}}
void seed(RefMemory& m){m.clear();m.zeroRegion(owner,0x10000);m.zeroRegion(stack-0x4000,0x5000);
    m.zeroRegion(0x0c63d37c,152);
    m.write32(owner+12,vtable);m.write32(owner+32,sentinel);m.write32(owner+36,child);m.write32(owner+68,child);
    m.write16(vtable+40,0);m.write32(vtable+44,0x0c075ca0);
    m.write16(vtable+56,0);m.write32(vtable+60,dispatch);
    m.write32(child+12,vtable);m.write32(child+4,sentinel);
}
void routing(RefMemory& m){
    for(unsigned id=0;id<20;++id)for(bool overrideLink:{false,true})for(bool replay:{false,true}){
        seed(m);m.write32(child+16,id);m.write8(child+28,1);m.write8(0x0c2f4e24,replay);
        m.write32(owner+352,0xabcdef12);m.write32(owner+344,owner+256);
        RefCpu c(m);c.r[14]=stack-512;c.r[15]=stack-512;c.r[1]=child;c.r[10]=owner+64;c.r[12]=owner;c.r[8]=0xffffffff;
        m.write32(c.r[14]+152,owner+32);m.write32(owner+340,0);
        unsigned requested=99,prepared=0;
        c.callHooks[0x0c02de20]=[&](auto& x){++hooks;x.r[0]=overrideLink;};
        c.callHooks[0x0c0e1d40]=[&](auto&){++hooks;++prepared;};
        c.callHooks[dispatch]=[&](auto& x){++hooks;requested=x.r[5];};
        run(c,0x0c02caa4,0x0c02cc50);
        const auto e=originalAttractCompletion(id,overrideLink,replay);
        eq(requested,unsigned(e.requestedChild),"completed child request");eq(m.read8(child+28),0,"consumes child finish flag");
        eq(prepared,e.preparePendingReplay,"pending replay preparation");
        eq(m.read8(0x0c2f4e24),e.clearPendingReplay?0:unsigned(replay),"pending replay consumption");
        eq(m.read32(owner+352),e.clearReplayState352?0:0xabcdef12,"replay352 preservation");++cases;
    }
}
void starts(RefMemory& m){
    for(unsigned id:{0u,1u,2u,3u,8u,11u,17u,0xffffffffu})for(unsigned mode:{0u,1u,2u})
    for(unsigned switches:{0u,0x7fu,0x80u,0xffu})for(bool accepted:{false,true}){
        seed(m);m.write32(child+16,id);m.write8(0x0c400670,switches);
        RefCpu c(m);c.r[14]=stack-512;c.r[15]=stack-512;c.r[10]=owner+64;c.r[12]=owner;
        m.write32(c.r[14]+152,owner+32);unsigned checked=0,cleared=0;
        c.callHooks[0x0c202b20]=[](auto&){++hooks;};
        c.callHooks[0x0c2029e0]=[&](auto& x){++hooks;m.write32(x.r[4]+64,mode);};
        c.callHooks[0x0c2029c0]=[](auto& x){++hooks;x.r[0]=0;};
        c.callHooks[0x0c202b60]=[&](auto& x){++hooks;++checked;eq(x.r[4],0,"credit channel");eq(x.r[5],0,"credit slot");x.r[0]=accepted;};
        c.callHooks[0x0c1f7e40]=[&](auto& x){++hooks;++cleared;eq(x.r[4]|x.r[5]|x.r[6],0,"start clear background");};
        run(c,0x0c02cc50,0x0c02cd48);
        const bool allowed=originalAttractCanCheckStart(id,mode,std::uint8_t(switches));
        eq(checked,allowed,"start check gating");OriginalAttractExitState state;
        const auto e=tickOriginalAttractExit(state,allowed&&accepted);
        eq(m.read32(owner+348),state.pendingFrames348,"accepted start begins3wait");eq(cleared,e.clearBackgroundBlack,"accepted start clears black");++cases;
    }
    for(unsigned delay:{1u,2u,3u,4u,0x80000000u,0xffffffffu})for(unsigned frame:{0u,900u,0xffffffffu})for(bool priorFinish:{false,true}){
        seed(m);m.write32(owner+348,delay);m.write32(owner+84,frame);m.write8(owner+28,priorFinish);
        RefCpu c(m);c.r[4]=owner;c.r[15]=stack;c.pr=stop;
        c.callHooks[0x0c1fa9e0]=[](auto&){++hooks;};run(c,0x0c02ca00,stop);
        OriginalAttractExitState s{delay,frame,priorFinish};const auto e=tickOriginalAttractExit(s,true);
        eq(m.read32(owner+348),s.pendingFrames348,"exit wait decrement");eq(m.read32(owner+84),s.frame84,"exit parent frame");
        eq(m.read8(owner+28),s.finishFlag28,"exit finish flag");eq(e.skipChildUpdate,true,"waiting suppresses child update");
        eq(e.finishRequested,delay==1,"finish on final wait");eq(e.clearBackgroundBlack,false,"wait ignores further start");++cases;
    }
}
void singleEntry(RefMemory& m){
    for(unsigned resume=0;resume<256;++resume)for(unsigned mode:{0u,1u,2u,3u,0xffffffffu}){
        seed(m);m.write8(0x0c31ce43,resume);m.write32(0x0c31c99c,mode);
        RefCpu c(m);c.r[4]=owner;c.r[15]=stack;c.pr=stop;unsigned request=0xffffffff,count=0;
        c.callHooks[0x0c055d60]=[](auto&){++hooks;};c.callHooks[dispatch]=[&](auto& x){++hooks;++count;request=x.r[5];};
        run(c,0x0c076980,stop);const auto e=originalSingleEntryChild(std::uint8_t(resume),mode);
        eq(count,e.has_value(),"single entry issued request");if(e)eq(request,*e,"single entry route");++cases;
    }
}
void titleFades(RefMemory& m){
    for(unsigned phase:{0u,4u})for(unsigned frame=0;frame<64;++frame){
        seed(m);m.write32(owner+84,frame);m.write32(owner+88,phase);
        RefCpu c(m);c.r[12]=owner;c.r[14]=stack-512;c.r[15]=stack-512;c.r[13]=0;
        run(c,phase==0?0x0c0833a0:0x0c0835a0,0x0c083604);
        OriginalAttractTitleState s;s.phase=phase;s.frame=frame;
        stepOriginalAttractTitle(s);
        eq(s.phase,m.read32(owner+88),"title source phase");
        eq(s.frame,m.read32(owner+84)+1,"title source frame including draw tail");
        eq(s.fadeArgb,c.r[13]<<24,"title source fade");
        eq(s.completed,m.read8(owner+28)!=0,"title source finish");++cases;
    }
}
void registration(RefMemory& m){
    // Execute full02ABC0 registration using ordinary native allocation and
    // constructor callbacks. The original linked-list writes execute directly.
    for(unsigned mode:{0u,1u,4u}){
        seed(m);constexpr unsigned eh=owner+0x5000,config=owner+0x5100;
        m.write32(eh+4,eh+16);m.write32(config,mode);unsigned next=0x0cd10000;
        RefCpu c(m);c.r[4]=owner;c.r[15]=stack;c.pr=stop;
        c.callHooks[0x0c221fc0]=[&](auto& x){++hooks;x.r[0]=eh;};
        c.callHooks[0x0c228e80]=[](auto& x){++hooks;x.r[0]=x.r[4];};
        c.callHooks[0x0c202140]=[&](auto& x){++hooks;x.r[0]=config;};
        for(unsigned fn:{0x0c021ee0u,0x0c021960u})c.callHooks[fn]=[&](auto& x){++hooks;x.r[0]=next;m.zeroRegion(next,4096);next+=4096;};
        for(unsigned fn:{0x0c095bc0u,0x0c0355c0u,0x0c04e940u,0x0c1cd740u,0x0c073dc0u,0x0c0734c0u,
            0x0c039740u,0x0c082e60u,0x0c0e5740u,0x0c083740u,0x0c1cdf00u,0x0c0d00a0u,
            0x0c02e8a0u,0x0c046680u,0x0c06df00u,0x0c02fae0u,0x0c075ee0u})
            c.callHooks[fn]=[&,fn](auto& x){++hooks;x.r[0]=x.r[4];m.write32(x.r[0]+8,0x5345514e);m.write32(x.r[0]+12,0x5345514e);
                // Source caller converts these complete objects to the
                // embedded HSequence subobject at+1060/+1048 respectively.
                if(fn==0x0c0e5740)m.write32(x.r[0]+1068,0x5345514e);
                if(fn==0x0c06df00)m.write32(x.r[0]+1056,0x5345514e);};
        run(c,0x0c02abc0,stop,20000);
        std::vector<unsigned> nodes;unsigned p=m.read32(owner+36),end=m.read32(owner+32);
        for(unsigned guard=0;p!=end&&guard<30;++guard){nodes.push_back(p);p=m.read32(p+4);}
        const auto expected=originalAttractChildren(mode);
        if(nodes.size()!=expected.size()){std::cerr<<"registered:";for(auto node:nodes)std::cerr<<' '<<m.read32(node+16);std::cerr<<'\n';}
        eq(nodes.size(),expected.size(),"actual constructor child count");
        for(unsigned i=0;i<nodes.size();++i){eq(m.read32(nodes[i]+16),expected[i],"actual constructor child order");
            m.write32(owner+68,nodes[i]);m.write32(nodes[i]+12,vtable);m.write16(vtable+32,0);m.write32(vtable+36,dispatch);
            RefCpu n(m);n.r[4]=owner;n.r[5]=0xffffffff;n.r[15]=stack;n.pr=stop;
            n.callHooks[dispatch]=[](auto&){++hooks;};run(n,0x0c0759c0,stop);
            const auto native=originalAttractNextChild(expected[i],mode);eq(native.has_value(),true,"registered child resolves");
            eq(m.read32(m.read32(owner+68)+16),*native,"source next-child operation");++cases;
        }
    }
}
}
int main(int argc,char** argv){try{if(argc!=2)throw std::runtime_error("canonical image path required");RefMemory m(argv[1]);
    routing(m);starts(m);singleEntry(m);registration(m);titleFades(m);
    std::cout<<"original attract: "<<cases<<" cases, "<<checks<<" comparisons, "<<steps<<" actual instructions, "<<hooks<<" scoped hooks\n";
    return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
