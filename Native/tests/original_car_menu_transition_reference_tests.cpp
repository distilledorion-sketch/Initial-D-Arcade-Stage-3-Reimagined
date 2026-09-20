#include "original_car_menu_transition.h"
#include "sh4_scalar_reference.h"
#include <array>
#include <iostream>

using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned owner=0x0cd00000,child=0x0cd01000,timer=0x0cd02000,
    selector=0x0cd03000,variant=0x0cd04000,vtable=0x0cd05000,showroom=0x0cd06000,
    fade=0x0cd07000,stack=0x0cfff000,stop=0x00ff0000,profile=0x0c31c99c;
std::uint64_t cases=0,checks=0,steps=0,hooks=0;
void equal(unsigned a,unsigned b,const char* why){++checks;if(a!=b){std::cerr<<why<<": "<<hex(a)<<" != "<<hex(b)<<'\n';throw std::runtime_error(why);}}
void run(RefCpu& c,unsigned start,unsigned end,unsigned budget=5000){try{steps+=c.run(start,end,budget);}catch(const std::exception& e){throw std::runtime_error(hex(start)+" at "+hex(c.pc)+": "+e.what());}}
void seed(RefMemory& m){
    m.clear();m.zeroRegion(owner,0x10000);m.zeroRegion(stack-0x4000,0x5000);
    m.write32(owner+432,vtable);m.write16(vtable+48,0);m.write32(vtable+52,0x00ed0000);
    m.write32(owner+428,showroom);m.write32(owner+696,child);m.write32(child+412,timer);
    m.write32(child+416,selector);m.write32(child+420,variant);m.write32(owner+500,selector);
    m.write32(owner+488,1);m.write32(owner+460,fade);m.write32(owner+448,0x12345678);
    m.write8(0x0c92ed00,0);m.write8(0x0c92ed40,0);
}
void put(RefMemory& m,const OriginalCarMenuTransition& s){
    for(auto [offset,value]:std::array<std::pair<unsigned,unsigned>,8>{{{452,s.frame452},{456,s.phase456},
        {464,s.fade464},{468,s.fadeRange468},{472,s.overlayEnabled472},{476,s.frame476},
        {64,s.parentEvent64},{76,s.previousScreen76}}})m.write32(owner+offset,value);
    m.write8(owner+672,s.timedOut672);m.write32(profile+1176,s.sharedCountdown1176);m.write32(profile+1180,s.profileFlags1180);
}
void compare(RefMemory& m,const OriginalCarMenuTransition& s){
    for(auto [offset,value]:std::array<std::pair<unsigned,unsigned>,8>{{{452,s.frame452},{456,s.phase456},
        {464,s.fade464},{468,s.fadeRange468},{472,s.overlayEnabled472},{476,s.frame476},
        {64,s.parentEvent64},{76,s.previousScreen76}}})equal(m.read32(owner+offset),value,"car timing field");
    equal(m.read8(owner+672),s.timedOut672,"car timeout byte");equal(m.read32(profile+1176),s.sharedCountdown1176,"shared countdown");
    equal(m.read32(profile+1180),s.profileFlags1180,"profile flags");
}
OriginalCarMenuTransitionEvents tick(RefMemory& m,OriginalCarMenuTransition& state,const OriginalCarMenuTransitionInput& input){
    seed(m);put(m,state);m.writeFloat(child+456,-9.f);
    RefCpu c(m);c.r[4]=owner;c.r[15]=stack;c.pr=stop;
    unsigned committed=0,cancelled=0,published=0,captured=0;
    c.callHooks[0x0c0d43a0]=[](auto& x){++hooks;x.setFloat(0,0.f);};
    c.callHooks[0x0c0d4300]=[&](auto& x){++hooks;x.r[0]=x.r[5]==1?input.confirmPressed:input.cancelPressed;};
    c.callHooks[0x0c1b39c0]=[](auto& x){++hooks;x.r[0]=0;};
    c.callHooks[0x0c141f80]=[&](auto& x){++hooks;if(x.r[4]==3)++committed;else if(x.r[4]==8)++cancelled;};
    c.callHooks[0x0c133a60]=[](auto&){++hooks;}; // Explicit selected-car/profile setter.
    c.callHooks[0x0c0a94c0]=[&](auto& x){++hooks;++captured;x.r[0]=0x12345678;};
    c.callHooks[0x0c0a9880]=[&](auto& x){++hooks;++published;equal(x.r[4],showroom,"showroom owner");equal(x.r[5],0x12345678,"captured selected object");equal(x.r[6],0,"selected local index");};
    // These owners do not feed the transition: sound/hidden code, selection
    // rendering, timer digits and the base update. Original1341A0 countdown,
    //192560/192620 durations and1B59C0 confirmation clamp execute directly.
    for(unsigned fn:{0x0c16db80u,0x0c1ba260u,0x0c1b5880u,0x0c1bbc60u,0x0c10f420u,0x00ed0000u})
        c.callHooks[fn]=[](auto&){++hooks;};
    c.callHooks[0x0c2223b8]=[](auto& x){++hooks;x.r[0]=std::uint32_t(signed32(x.r[4])/signed32(x.r[5]));x.fpul=x.r[0];};
    run(c,0x0c12e520,stop);
    const auto out=tickOriginalCarMenuTransition(state,input);compare(m,state);
    equal(committed,out.selectionCommitted,"selection commit event");equal(cancelled,out.cancelAccepted,"cancel accepted event");
    equal(captured,out.selectionCommitted,"capture selected showroom object");equal(published,out.showroomCommitRequested,"publish selected showroom object");
    equal(m.read32(child+456),std::bit_cast<unsigned>(out.confirmationPhaseWritten?out.confirmationPhase:-9.f),"car confirmation fraction");
    ++cases;return out;
}
void initTest(RefMemory& m){
    seed(m);OriginalCarMenuTransition s;s.frame452=99;s.phase456=5;s.fade464=42;s.fadeRange468=42;
    s.frame476=0xffffffff;s.timedOut672=7;s.sharedCountdown1176=2479;s.profileFlags1180=0x81;s.parentEvent64=7;s.previousScreen76=4;
    put(m,s);RefCpu c(m);c.r[14]=stack-512;c.r[15]=stack-512;m.write32(c.r[14]+144,owner);
    run(c,0x0c12dd28,0x0c12dd7c);
    run(c,0x0c12e1ce,0x0c12e460); // Sparse timeout byte store +branch only.
    initializeOriginalCarMenuTransition(s);compare(m,s);++cases;
}
void fadeTests(RefMemory& m){
    for(unsigned range:{7u,15u})for(unsigned counter:{0u,1u,2u,3u,4u,5u,6u,7u,8u,14u,15u,16u,255u,0x7fffffffu,0x80000000u,0xffffffffu})
    for(unsigned enabled:{0u,1u}){
        seed(m);OriginalCarMenuTransition s;s.fade464=counter;s.fadeRange468=range;s.overlayEnabled472=enabled;put(m,s);
        RefCpu c(m);c.r[10]=owner+444;c.r[15]=stack;c.setFloat(12,0.f);unsigned calls=0,argb=0;
        c.callHooks[0x0c0c5200]=[&](auto& x){++hooks;++calls;argb=x.r[5];equal(x.r[4],fade,"car fade resource");equal(x.r[6],0,"car fade unscaled");equal(x.fr[4],0,"car fade zeroZ");};
        run(c,0x0c12ec34,0x0c12ec62);equal(calls,enabled,"car overlay enabled");if(enabled)equal(argb,originalCarMenuFadeArgb(s),"car exact alpha");++cases;
    }
}
void timingTests(RefMemory& m){
    for(unsigned phase=0;phase<=6;++phase)for(unsigned frame:{0u,1u,35u,36u,49u,50u,51u,120u,121u,0x7fffffffu,0xffffffffu})
    for(unsigned countdown:{0u,1u,879u,2479u,0xffffffffu})for(unsigned inputs=0;inputs<4;++inputs)for(unsigned flags:{0u,8u}){
        OriginalCarMenuTransition s;s.phase456=phase;s.frame452=frame;s.fade464=phase==0?7:phase>=4?15:0;
        s.fadeRange468=phase>=3?15:7;s.overlayEnabled472=phase==0||phase>=4;s.frame476=0xfffffffe;
        s.sharedCountdown1176=countdown;s.profileFlags1180=flags;s.previousScreen76=4;
        tick(m,s,{bool(inputs&1),bool(inputs&2)});
    }
    for(unsigned phase:{0u,4u,5u})for(unsigned counter:{0u,1u,6u,7u,14u,15u,16u,0x7fffffffu,0xffffffffu}){
        OriginalCarMenuTransition s;s.phase456=phase;s.fade464=counter;s.sharedCountdown1176=2479;s.fadeRange468=15;
        s.previousScreen76=0xabcd1234;tick(m,s,{});
    }
    OriginalCarMenuTransition s;s.sharedCountdown1176=2479;initializeOriginalCarMenuTransition(s);
    for(unsigned n=0;n<8;++n){tick(m,s,{true,true});equal(s.sharedCountdown1176,2479,"entry ignores input and countdown");}
    equal(s.phase456,1,"entry lasts8updates");auto first=tick(m,s,{true,true});equal(first.selectionCommitted,1,"confirm has priority overcancel");
    equal(std::bit_cast<unsigned>(first.confirmationPhase),std::bit_cast<unsigned>(1.f/36.f),"car first shrink1/36");
    unsigned total=1;while(!s.parentEvent64&&total<300){tick(m,s,{});++total;}
    equal(total,164,"confirmation toparent164updates including confirm");equal(s.fade464,15,"terminal fullblack");equal(s.parentEvent64,32,"parent advances");
    s={};s.sharedCountdown1176=2479;s.previousScreen76=4;initializeOriginalCarMenuTransition(s);
    for(unsigned n=0;n<8;++n)tick(m,s,{});tick(m,s,{false,true});equal(s.phase456,5,"cancel enters5");
    for(unsigned n=0;n<15;++n){tick(m,s,{});equal(s.parentEvent64,0,"cancel waits strict greater15");}
    tick(m,s,{});equal(s.parentEvent64,0x00040004,"cancel returns previousscreen");
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("canonical original image required");RefMemory m(argv[1]);initTest(m);fadeTests(m);timingTests(m);
    std::cout<<"PASS original Car menu transition: "<<cases<<" cases, "<<checks<<" comparisons, "<<steps<<" actual instructions, "<<hooks
        <<" explicit input/profile/showroom/draw hooks. Full12E520 timing executes; selection/color/model ownership is external.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
