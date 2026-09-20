#include "original_frontend_transition.h"
#include "sh4_scalar_reference.h"
#include <array>
#include <iostream>

using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned owner=0x0cd00000,child=0x0cd01000,bank=0x0cd02000,
    maker=0x0cd03000,timer=0x0cd04000,selector=0x0cd05000,vtable=0x0cd06000,
    fade=0x0cd07000,geometry=0x0cd08000,material=0x0cd09000,stack=0x0cfff000,
    stop=0x00ff0000,profile=0x0c31c99c,sharedTimer=profile+1176,flags=profile+1180;
std::uint64_t checks=0,steps=0,hooks=0,cases=0;
void equal(unsigned a,unsigned b,const char* why){++checks;if(a!=b){std::cerr<<why<<": "<<hex(a)<<" != "<<hex(b)<<'\n';throw std::runtime_error(why);}}
void run(RefCpu& c,unsigned start,unsigned end,unsigned budget=5000){try{steps+=c.run(start,end,budget);}catch(const std::exception& e){throw std::runtime_error(hex(start)+" at "+hex(c.pc)+": "+e.what());}}
void seed(RefMemory& m){m.clear();m.zeroRegion(owner,0x10000);m.zeroRegion(stack-0x4000,0x5000);
    m.write32(owner+432,vtable);m.write16(vtable+48,0);m.write32(vtable+52,0x00ed0000);
    m.write32(owner+504,child);m.write32(child+412,timer);m.write32(child+416,selector);
    m.write32(owner+500,maker);m.write32(owner+496,bank);m.write32(owner+464,selector);
    m.write8(0x0c92ed00,0);
}
void put(RefMemory& m,const OriginalMakerTransition& s){
    for(auto [offset,value]:std::array<std::pair<unsigned,unsigned>,9>{{{444,s.frame444},{448,s.phase448},
        {456,s.fade456},{460,s.overlayEnabled460},{484,s.timedOut484},{440,s.selected440},{64,s.parentEvent64},
        {452,fade},{488,1}}})m.write32(owner+offset,value);
    m.write32(sharedTimer,s.sharedCountdown1176);m.write32(flags,s.profileFlags1180);
}
void compare(RefMemory& m,const OriginalMakerTransition& s){
    for(auto [offset,value]:std::array<std::pair<unsigned,unsigned>,7>{{{444,s.frame444},{448,s.phase448},
        {456,s.fade456},{460,s.overlayEnabled460},{484,s.timedOut484},{440,s.selected440},{64,s.parentEvent64}}})
        equal(m.read32(owner+offset),value,"maker state field");
    equal(m.read32(sharedTimer),s.sharedCountdown1176,"shared countdown");equal(m.read32(flags),s.profileFlags1180,"profile flags");
}
void makerTick(RefMemory& m,OriginalMakerTransition& state,const OriginalMakerTransitionInput& input){
    seed(m);put(m,state);m.write32(profile+40,input.profileMaker40);m.writeFloat(maker+420,-9.f);
    RefCpu c(m);c.r[4]=owner;c.r[15]=stack;c.pr=stop;
    unsigned changed=0,committed=0,reset=0;
    c.callHooks[0x0c0d43a0]=[](auto& x){++hooks;x.setFloat(0,0.f);};
    c.callHooks[0x0c0d4300]=[&](auto& x){++hooks;x.r[0]=input.confirmPressed;};
    c.callHooks[0x0c1b39c0]=[&](auto& x){++hooks;x.r[0]=input.selectedIndex;};
    // Explicit input, maker-selection/profile, display, audio and hidden-code
    // owners. Timing, original countdown and1B68C0's clamp execute directly.
    for(unsigned fn:{0x0c12cb80u,0x0c1fa9e0u,0x0c1ba260u,0x0c1b68a0u,0x0c1bbc60u,0x00ed0000u})
        c.callHooks[fn]=[](auto&){++hooks;};
    // Countdown digits only: this quotient does not feed transition state.
    c.callHooks[0x0c2223b8]=[](auto& x){++hooks;x.r[0]=std::uint32_t(signed32(x.r[4])/signed32(x.r[5]));x.fpul=x.r[0];};
    c.callHooks[0x0c141f80]=[&](auto& x){++hooks;if(x.r[4]==2)++changed;if(x.r[4]==3)++committed;};
    c.callHooks[0x0c133a60]=[&](auto&){++hooks;++reset;};
    c.callHooks[0x0c1338c0]=[](auto&){++hooks;};
    run(c,0x0c12ccc0,stop);const auto native=tickOriginalMakerTransition(state,input);compare(m,state);
    equal(changed,native.selectionChanged,"maker change event");equal(committed,native.selectionCommitted,"maker commit event");
    equal(reset,native.resetSelectedCar,"maker reset selected car event");
    equal(m.read32(maker+420),std::bit_cast<unsigned>(native.confirmationPhaseWritten?native.confirmationPhase:-9.f),"maker shrink phase");
    ++cases;
}
void title(RefMemory& m){
    std::vector<unsigned> frames;for(unsigned n=0;n<=902;++n)frames.push_back(n);
    for(unsigned n:{0x7fffffffu,0x80000000u,0xffffffffu})frames.push_back(n);
    for(unsigned frame:frames)
    for(unsigned cabinet:{0u,4u,7u}){
        seed(m);m.write32(owner+84,frame);m.write32(owner+328,bank);m.write32(owner+332,child);m.write32(owner+336,fade);
        m.write32(bank,vtable);m.write32(child,vtable);m.write16(vtable+40,0);m.write32(vtable+44,0x00ed0010);
        m.write32(owner+12,vtable+128);m.write16(vtable+168,0);m.write32(vtable+172,0x00ed0020);
        m.write32(selector,cabinet);RefCpu c(m);c.r[4]=owner;c.r[15]=stack;c.pr=stop;
        unsigned argb=0,finish=0,mark=0,draws=0;
        c.callHooks[0x0c1d0880]=[](auto&){++hooks;};c.callHooks[0x0c1458c0]=[](auto&){++hooks;};
        c.callHooks[0x0c202140]=[](auto& x){++hooks;x.r[0]=selector;};
        c.callHooks[0x00ed0010]=[&](auto& x){++hooks;if(x.r[4]==bank){equal(x.r[5],draws==0?1:0,"title authored layer order");++draws;}else{equal(x.r[5],45,"title cabinet mark selector");++mark;}};
        c.callHooks[0x00ed0020]=[&](auto&){++hooks;++finish;};
        c.callHooks[0x0c0c5200]=[&](auto& x){++hooks;argb=x.r[5];equal(x.r[6],0,"title overlay unscaled");equal(x.fr[4],0,"title overlay zero Z");};
        run(c,0x0c083dc0,stop);OriginalTitleTransition state{frame};const auto native=tickOriginalTitleTransition(state,cabinet);
        equal(m.read32(owner+84),state.frame84,"title frame increment");equal(argb,native.blackOverlayArgb,"title exact fade alpha");
        equal(finish,native.finishRequested,"title exact finish tick");equal(mark,native.drawCabinetMark45,"title cabinet mark");++cases;
    }
    OriginalTitleTransition state;unsigned finishCount=0;
    for(unsigned frame=0;frame<=900;++frame){const auto e=tickOriginalTitleTransition(state,0);finishCount+=e.finishRequested;
        if(frame<=870)equal(e.blackOverlayArgb,0,"title full visible hold");}
    equal(finishCount,1,"title one900-frame completion");
}
void fadeTests(RefMemory& m){
    // Actual12CFA0 numeric prefix, including192560 and222300, up to the
    // unrelated text/bank draw. Negative/overflow counters preserve bit math.
    for(unsigned counter:{0u,1u,2u,3u,4u,5u,6u,7u,8u,36u,255u,0x7fffffffu,0x80000000u,0xffffffffu})
    for(unsigned enabled:{0u,1u}){
        seed(m);OriginalMakerTransition state;state.fade456=counter;state.overlayEnabled460=enabled;put(m,state);
        RefCpu c(m);c.r[4]=owner;c.r[15]=stack;unsigned calls=0,argb=0;
        c.callHooks[0x0c0c5200]=[&](auto& x){++hooks;++calls;argb=x.r[5];equal(x.fr[4],0,"maker overlay zero Z");equal(x.r[6],0,"maker overlay unscaled");};
        run(c,0x0c12cfa0,0x0c12cff6);equal(calls,enabled,"maker overlay enabled");
        if(enabled)equal(argb,originalMakerFadeArgb(state),"maker exact fade alpha");++cases;
    }
    for(bool available:{false,true})for(unsigned argb:{0u,0x00ffffffu,0x01000000u,0x80000000u,0xff000000u,0xffffffffu}){
        seed(m);m.write32(fade+8,available?geometry:0);m.write32(geometry+12,material);
        m.write32(material+12,0x12345678);m.write32(material+20,0x87654321);
        RefCpu c(m);c.r[4]=fade;c.r[5]=argb;c.r[6]=0;c.setFloat(4,0);c.r[15]=stack;c.pr=stop;
        unsigned submitted=0,flagSet=0,flagClear=0;
        c.callHooks[0x0c1d5400]=[](auto& x){++hooks;x.r[0]=geometry;};
        for(unsigned fn:{0x0c1fcc60u,0x0c1f65c0u})c.callHooks[fn]=[](auto&){++hooks;};
        c.callHooks[0x0c1f6ac0]=[](auto& x){++hooks;equal(x.fr[4],0,"overlay translateX");equal(x.fr[5],0,"overlay translateY");equal(x.fr[6],0,"overlay translateZ");};
        c.callHooks[0x0c1d7120]=[&](auto&){++hooks;++submitted;};
        c.callHooks[0x0c16d500]=[&](auto& x){++hooks;++flagSet;equal(x.r[4],2,"overlay sets controlbit2");};
        c.callHooks[0x0c16d440]=[&](auto& x){++hooks;++flagClear;equal(x.r[4],2,"overlay clears controlbit2");};
        run(c,0x0c0c5200,stop);OriginalFadeOverlay state{available,0x12345678,0x87654321};const auto e=applyOriginalSimpleFadeOverlay(state,argb);
        equal(m.read32(material+12),state.diffuse0,"overlay original diffuse0");equal(m.read32(material+20),state.diffuse1,"overlay original diffuse1");
        equal(submitted,e.submitted,"overlay submit");equal(flagSet,e.submitted&&e.controlBit2Set,"overlay set flag");equal(flagClear,e.submitted&&!e.controlBit2Set,"overlay clear flag");++cases;
    }
}
void makerTests(RefMemory& m){
    // Actual sparse12C800 state stores and original15/2 initialization.
    seed(m);OriginalMakerTransition initial;initial.frame444=99;initial.phase448=4;initial.fade456=42;initial.timedOut484=1;
    initial.sharedCountdown1176=1279;initial.profileFlags1180=0x81;initial.selected440=2;put(m,initial);
    RefCpu init(m);init.r[14]=stack-256;init.r[15]=stack-256;init.t=true;m.write32(init.r[14]+96,owner);
    run(init,0x0c12c8a8,0x0c12c8d8); // targeted timing write block only
    // timedOut484 is cleared separately at12C95A..12C960; preserve unrelated stores.
    init.r[14]=stack-256;m.write32(init.r[14]+96,owner);run(init,0x0c12c954,0x0c12c964);
    initializeOriginalMakerTransition(initial);compare(m,initial);++cases;
    for(unsigned phase=0;phase<=5;++phase)for(unsigned counter:{0u,1u,3u,4u,35u,36u,120u,121u,0xffffffffu})
    for(unsigned countdown:{0u,1u,879u,880u,1279u,0xffffffffu})for(bool confirm:{false,true}){
        OriginalMakerTransition s;s.phase448=phase;s.frame444=counter;s.fade456=phase==0?0:phase==3?7:0;
        s.overlayEnabled460=phase!=1;s.sharedCountdown1176=countdown;s.selected440=2;s.profileFlags1180=0x81;
        makerTick(m,s,{confirm,confirm?3u:2u,2});
    }
    for(unsigned phase:{0u,3u})for(unsigned fadeCounter:{0u,1u,6u,7u,8u,0x7fffffffu,0xffffffffu}){
        OriginalMakerTransition s;s.phase448=phase;s.fade456=fadeCounter;s.sharedCountdown1176=1279;
        makerTick(m,s,{false,0,0});
    }
    OriginalMakerTransition s;s.sharedCountdown1176=1279;initializeOriginalMakerTransition(s);
    unsigned elapsed=0;while(s.phase448==0){makerTick(m,s,{false,0,0});++elapsed;}equal(elapsed,8,"maker initial fade exact ticks");
    makerTick(m,s,{true,0,0});equal(s.phase448,2,"maker confirmation entersphase2");
    for(unsigned n=0;n<120;++n){makerTick(m,s,{false,0,0});equal(s.phase448,2,"maker holds beyond36-frame shrink");}
    makerTick(m,s,{false,0,0});equal(s.phase448,3,"maker oldcounter121 startsfade");
    for(unsigned n=0;n<8;++n)makerTick(m,s,{false,0,0});equal(s.phase448,4,"maker fade finishesafter8updates");
    equal(s.fade456,7,"maker terminalfade remainsfullblack");
    for(unsigned n=0;n<3;++n){makerTick(m,s,{false,0,0});equal(s.parentEvent64,0,"maker parent waitsfinalhold");}
    makerTick(m,s,{false,0,0});equal(s.parentEvent64,32,"maker parent advance event");
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("canonical original image required");RefMemory m(argv[1]);title(m);fadeTests(m);makerTests(m);
    std::cout<<"PASS original title/maker transitions: "<<cases<<" cases, "<<checks<<" comparisons, "<<steps
        <<" actual instructions, "<<hooks<<" explicit input/profile/draw/camera/matrix callback hooks. Timing/fade arithmetic executes original instructions.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
