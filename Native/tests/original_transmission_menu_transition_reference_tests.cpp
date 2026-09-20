#include "original_transmission_menu_transition.h"
#include "sh4_scalar_reference.h"
#include <array>
#include <iostream>

using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned owner=0x0cd00000,child=0x0cd01000,timer=0x0cd02000,
    selector=0x0cd03000,vtable=0x0cd05000,fade=0x0cd07000,
    stack=0x0cfff000,stop=0x00ff0000,profile=0x0c31c99c;
std::uint64_t cases=0,checks=0,steps=0,hooks=0;
void equal(unsigned a,unsigned b,const char* why){++checks;if(a!=b){std::cerr<<why<<": "<<hex(a)<<" != "<<hex(b)<<'\n';throw std::runtime_error(why);}}
void run(RefCpu& c,unsigned start,unsigned end,unsigned budget=5000){try{steps+=c.run(start,end,budget);}catch(const std::exception& e){throw std::runtime_error(hex(start)+" at "+hex(c.pc)+": "+e.what());}}
void seed(RefMemory& m){
    m.clear();m.zeroRegion(owner,0x10000);m.zeroRegion(stack-0x4000,0x5000);
    m.write32(owner+432,vtable);m.write16(vtable+48,0);m.write32(vtable+52,0x00ed0000);
    m.write32(owner+524,child);m.write32(child+412,timer);m.write32(child+416,selector);
    m.write32(owner+484,selector);m.write32(owner+472,fade);m.write8(0x0c92ed00,0);
}
void put(RefMemory& m,const OriginalTransmissionMenuTransition& s){
    for(auto [offset,value]:std::array<std::pair<unsigned,unsigned>,10>{{{452,s.frame452},{456,s.exitHold456},
        {460,s.fade460},{464,s.overlayEnabled464},{468,s.committed468},{476,s.phase476},{512,s.selected512},
        {64,s.parentEvent64},{76,s.previousScreen76},{80,s.alternateScreen80}}})m.write32(owner+offset,value);
    m.write8(owner+516,s.timedOut516);m.write32(profile+1176,s.sharedCountdown1176);m.write32(profile+1180,s.profileFlags1180);
    m.write32(profile+68,s.profileTransmission68);m.write32(profile+16,s.profileCar16);
    m.write8(profile+1192,s.profileByte1192);m.write8(profile+152,s.profileByte152);
}
void compare(RefMemory& m,const OriginalTransmissionMenuTransition& s){
    for(auto [offset,value]:std::array<std::pair<unsigned,unsigned>,10>{{{452,s.frame452},{456,s.exitHold456},
        {460,s.fade460},{464,s.overlayEnabled464},{468,s.committed468},{476,s.phase476},{512,s.selected512},
        {64,s.parentEvent64},{76,s.previousScreen76},{80,s.alternateScreen80}}})equal(m.read32(owner+offset),value,"transmission menu timing field");
    equal(m.read8(owner+516),s.timedOut516,"timeout byte");equal(m.read32(profile+1176),s.sharedCountdown1176,"shared countdown");
    equal(m.read32(profile+1180),s.profileFlags1180,"profile flags");equal(m.read32(profile+68),s.profileTransmission68,"profile AT/MT");
    equal(m.read32(profile+16),s.profileCar16,"profile car");equal(m.read8(profile+1192),s.profileByte1192,"profile route byte");
    equal(m.read8(profile+152),s.profileByte152,"profile reset byte");
}
OriginalTransmissionMenuTransitionEvents tick(RefMemory& m,OriginalTransmissionMenuTransition& s,
        OriginalTransmissionMenuTransitionInput input,std::uint8_t cabinetButtons=0){
    seed(m);put(m,s);m.writeFloat(child+424,-9.f);m.write8(0x0c92ed00,cabinetButtons);
    RefCpu c(m);c.r[4]=owner;c.r[15]=stack;c.pr=stop;unsigned changed=0,committed=0,secret=0;
    c.callHooks[0x0c0d43a0]=[](auto& x){++hooks;x.setFloat(0,0.f);};
    c.callHooks[0x0c0d4300]=[&](auto& x){++hooks;equal(x.r[5],1,"owner polls confirm only");x.r[0]=input.confirmPressed;};
    c.callHooks[0x0c1b39c0]=[&](auto& x){++hooks;x.r[0]=input.selectedIndex;};
    c.callHooks[0x0c141f80]=[&](auto& x){++hooks;equal(x.r[5],1,"audio channel");if(x.r[4]==2)++changed;else if(x.r[4]==3)++committed;else throw std::runtime_error("Unexpected sound");};
    c.callHooks[0x0c16db80]=[&](auto& x){++hooks;equal(x.r[4],9,"transmission hidden-code event");++secret;};
    for(unsigned fn:{0x0c10f420u,0x0c10f460u,0x00ed0000u,0x0c1ba260u,0x0c1bbc60u})
        c.callHooks[fn]=[](auto&){++hooks;};
    c.callHooks[0x0c2223b8]=[](auto& x){++hooks;x.r[0]=std::uint32_t(signed32(x.r[4])/signed32(x.r[5]));x.fpul=x.r[0];};
    const bool parentExpected=s.phase476==3&&signed32(s.fade460+1)>15&&signed32(s.exitHold456)>3;
    run(c,0x0c1249a0,stop);input.confirmPressed|=(cabinetButtons&0x80)!=0;
    const auto out=tickOriginalTransmissionMenuTransition(s,input);compare(m,s);
    equal(changed,out.selectionChanged,"selection sound");equal(committed,out.selectionCommitted,"commit sound");
    equal(secret,out.selectionCommitted,"hidden-code commit event");equal(out.parentRequested,parentExpected,"parent requested event");
    equal(m.read32(child+424),std::bit_cast<unsigned>(out.confirmationPhaseWritten?out.confirmationPhase:-9.f),"original shrink fraction");
    ++cases;return out;
}
void initTest(RefMemory& m){
    seed(m);OriginalTransmissionMenuTransition s;s.frame452=99;s.exitHold456=7;s.fade460=42;s.phase476=3;
    s.selected512=99;s.profileTransmission68=1;s.committed468=7;s.timedOut516=7;s.sharedCountdown1176=2479;
    s.profileFlags1180=0x81;s.parentEvent64=7;s.previousScreen76=6;s.alternateScreen80=7;s.profileByte152=1;s.profileByte1192=2;put(m,s);
    RefCpu c(m);c.r[14]=stack-512;c.r[15]=stack-512;m.write32(c.r[14]+156,owner);m.write32(c.r[14]+160,owner+508);
    c.callHooks[0x0c021960]=[](auto& x){++hooks;x.r[0]=selector;};
    run(c,0x0c1244dc,0x0c124510);run(c,0x0c124594,0x0c1245b0);
    c.r[0]=208;run(c,0x0c124718,0x0c124960);
    initializeOriginalTransmissionMenuTransition(s);compare(m,s);++cases;
}
void fadeTests(RefMemory& m){
    for(unsigned counter:{0u,1u,2u,3u,4u,5u,6u,7u,8u,14u,15u,16u,255u,0x7fffffffu,0x80000000u,0xffffffffu})
    for(unsigned enabled:{0u,1u}){
        seed(m);OriginalTransmissionMenuTransition s;s.fade460=counter;s.overlayEnabled464=enabled;put(m,s);
        RefCpu c(m);c.r[13]=owner;c.r[12]=444;c.r[15]=stack;c.setFloat(12,0.f);unsigned calls=0,argb=0;
        c.callHooks[0x0c0c5200]=[&](auto& x){++hooks;++calls;argb=x.r[5];equal(x.r[4],fade,"fade resource");equal(x.r[6],0,"fade unscaled");equal(x.fr[4],0,"fade zeroZ");};
        run(c,0x0c124d88,0x0c124dbe);equal(calls,enabled,"overlay enabled");if(enabled)equal(argb,originalTransmissionMenuFadeArgb(s),"exact alpha");++cases;
    }
}
void timingTests(RefMemory& m){
    for(unsigned phase=0;phase<=4;++phase)for(unsigned frame:{0u,1u,35u,36u,120u,121u,0x7fffffffu,0xffffffffu})
    for(unsigned countdown:{0u,1u,879u,2479u,0xffffffffu})for(unsigned input=0;input<4;++input){
        OriginalTransmissionMenuTransition s;s.phase476=phase;s.frame452=frame;s.fade460=phase==0||phase==3?15:0;
        s.exitHold456=4;s.overlayEnabled464=phase==0||phase==3;s.sharedCountdown1176=countdown;
        s.profileFlags1180=1;s.profileTransmission68=0;s.profileCar16=0;
        tick(m,s,{bool(input&1),input>>1});
    }
    for(unsigned flags:{0u,1u,2u,3u,0xffffffffu})for(unsigned car:{0u,29u,34u})for(unsigned route:{0u,1u,2u,255u})
    for(unsigned hold:{0u,1u,3u,4u,5u,0x7fffffffu,0xffffffffu}){
        OriginalTransmissionMenuTransition s;s.phase476=3;s.fade460=15;s.exitHold456=hold;s.profileFlags1180=flags;
        s.profileCar16=car;s.profileByte1192=std::uint8_t(route);s.profileByte152=0x81;s.parentEvent64=0x100;
        s.previousScreen76=0xabc01234;s.alternateScreen80=0xfabc5678;s.sharedCountdown1176=2000;
        tick(m,s,{});
    }
    for(unsigned phase:{0u,3u})for(unsigned counter:{0u,1u,14u,15u,16u,0x7fffffffu,0xffffffffu}){
        OriginalTransmissionMenuTransition s;s.phase476=phase;s.fade460=counter;s.exitHold456=4;s.sharedCountdown1176=2479;tick(m,s,{});
    }
    for(unsigned buttons:{0u,0x10u,0x80u,0x90u}){
        OriginalTransmissionMenuTransition s;s.phase476=1;s.sharedCountdown1176=2479;
        auto e=tick(m,s,{},std::uint8_t(buttons));equal(e.selectionCommitted,bool(buttons&0x80),"cabinet cancel bit has no branch");
    }
    OriginalTransmissionMenuTransition s;s.sharedCountdown1176=2479;initializeOriginalTransmissionMenuTransition(s);
    for(unsigned n=0;n<16;++n){tick(m,s,{true,1});equal(s.sharedCountdown1176,2479,"entry ignores timer/input");}
    equal(s.phase476,1,"entry lasts16updates");const auto e=tick(m,s,{true,1});
    equal(std::bit_cast<unsigned>(e.confirmationPhase),std::bit_cast<unsigned>(1.f/36.f),"first shrink1/36");
    unsigned total=1;while(!s.parentEvent64&&total<250){tick(m,s,{});++total;}
    equal(total,142,"confirmation toparent142updates");equal(s.parentEvent64,8,"ordinary complete event8");equal(s.profileTransmission68,1,"manual persisted");
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("canonical original image required");RefMemory m(argv[1]);initTest(m);fadeTests(m);timingTests(m);
    std::cout<<"PASS original Transmission menu transition: "<<cases<<" cases, "<<checks<<" comparisons, "<<steps<<" actual instructions, "<<hooks
        <<" explicit input/audio/showroom/draw hooks. Full1249A0 timing/profile/routing executes original bytes.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
