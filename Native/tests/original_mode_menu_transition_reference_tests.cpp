#include "original_mode_menu_transition.h"
#include "sh4_scalar_reference.h"
#include <array>
#include <iostream>

using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned owner=0x0cd00000,child=0x0cd01000,timer=0x0cd02000,
    selector=0x0cd03000,vtable=0x0cd05000,fade=0x0cd07000,dialog=0x0cd08000,
    stack=0x0cfff000,stop=0x00ff0000,profile=0x0c31c99c;
std::uint64_t cases=0,checks=0,steps=0,hooks=0;
void equal(unsigned a,unsigned b,const char* why){++checks;if(a!=b){std::cerr<<why<<": "<<hex(a)<<" != "<<hex(b)<<'\n';throw std::runtime_error(why);}}
void run(RefCpu& c,unsigned start,unsigned end,unsigned budget=5000){try{steps+=c.run(start,end,budget);}catch(const std::exception& e){throw std::runtime_error(hex(start)+" at "+hex(c.pc)+": "+e.what());}}
void seed(RefMemory& m){
    m.clear();m.zeroRegion(owner,0x10000);m.zeroRegion(stack-0x4000,0x5000);
    m.write32(owner+432,vtable);m.write16(vtable+48,0);m.write32(vtable+52,0x00ed0000);
    m.write32(owner+580,child);m.write32(child+412,timer);m.write32(owner+576,selector);
    m.write32(owner+548,selector);m.write32(owner+464,fade);m.write8(0x0c92ed00,0);
    m.write32(owner+436,dialog);m.write32(owner+440,dialog);m.write32(dialog,vtable);
    m.write16(vtable+40,0);m.write32(vtable+44,0x00ed0010);
}
auto fields(const OriginalModeMenuTransition& s){
    return std::array<std::pair<unsigned,unsigned>,12>{{
        {444,s.drawFrame444},{448,s.fade448},{452,s.confirmationFrame452},{456,s.overlayEnabled456},
        {460,s.committed460},{468,s.phase468},{520,s.pointsWarning520},{524,s.pointsWarningAge524},
        {528,s.cardWarning528},{532,s.cardWarningAge532},{564,s.selected564},{64,s.parentEvent64}}};
}
void put(RefMemory& m,const OriginalModeMenuTransition& s){
    for(auto [offset,value]:fields(s))m.write32(owner+offset,value);
    m.write8(owner+572,s.timedOut572);m.write32(profile+1176,s.sharedCountdown1176);
    m.write32(profile+1180,s.profileFlags1180);m.write32(profile,s.profileMode0);
    m.write32(profile+72,s.profilePoints72);m.write8(profile+1191,s.profileByte1191);
    m.write32(child+432,s.selected564);m.writeFloat(child+436,-9.f);
}
void compare(RefMemory& m,const OriginalModeMenuTransition& s){
    for(auto [offset,value]:fields(s))equal(m.read32(owner+offset),value,"mode owner field");
    equal(m.read8(owner+572),s.timedOut572,"timeout byte");equal(m.read32(profile+1176),s.sharedCountdown1176,"mode countdown");
    equal(m.read32(profile+1180),s.profileFlags1180,"profile flags");equal(m.read32(profile),s.profileMode0,"profile game mode");
    equal(m.read32(profile+72),s.profilePoints72,"earned points");equal(m.read8(profile+1191),s.profileByte1191,"profile byte1191");
}
OriginalModeMenuTransitionEvents tick(RefMemory& m,OriginalModeMenuTransition& s,
        OriginalModeMenuTransitionInput input,std::uint8_t cabinet=0){
    seed(m);put(m,s);m.write8(0x0c92ed00,cabinet);
    RefCpu c(m);c.r[4]=owner;c.r[15]=stack;c.pr=stop;
    unsigned changed=0,committed=0,rejected=0,ready=0;
    c.callHooks[0x0c0d43a0]=[](auto& x){++hooks;x.setFloat(0,0.f);};
    c.callHooks[0x0c0d4300]=[&](auto& x){++hooks;equal(x.r[5],1,"mode polls confirm only");x.r[0]=input.confirmPressed;};
    c.callHooks[0x0c1fa9e0]=[](auto&){++hooks;}; //Unused diagnostic output.
    c.callHooks[0x0c09c580]=[&](auto& x){++hooks;equal(x.r[4],selector,"mode analog selector");x.r[0]=input.selectedIndex;};
    c.callHooks[0x0c141f80]=[&](auto& x){++hooks;equal(x.r[5],1,"mode audio channel");
        if(x.r[4]==2)++changed;else if(x.r[4]==3)++committed;else if(x.r[4]==8)++rejected;else throw std::runtime_error("Unexpected mode sound");};
    c.callHooks[0x0c16d440]=[&](auto& x){++hooks;equal(x.r[4],1,"mode ready flag");++ready;};
    for(unsigned fn:{0x00ed0000u,0x0c1bbc60u})c.callHooks[fn]=[](auto&){++hooks;};
    c.callHooks[0x0c2223b8]=[](auto& x){++hooks;x.r[0]=std::uint32_t(signed32(x.r[4])/signed32(x.r[5]));x.fpul=x.r[0];};
    c.callHooks[0x0c16de80]=[&](auto& x){++hooks;x.r[0]=input.hiddenExitRequested;};
    run(c,0x0c125f40,stop);input.confirmPressed|=(cabinet&0x80)!=0;
    const auto out=tickOriginalModeMenuTransition(s,input);compare(m,s);
    equal(changed,out.selectionChanged,"mode change sound");equal(committed,out.selectionCommitted,"mode commit sound");
    equal(rejected,unsigned(out.pointsRejected)+unsigned(out.cardRejected),"mode eligibility rejection sound");equal(ready,out.readyRequested,"mode entry event");
    equal(m.read32(timer+32),std::uint32_t(out.timerDisplayValue),"source displayed timer quotient");
    equal(m.read32(child+432),s.selected564,"source selected artwork");
    equal(m.read32(child+436),std::bit_cast<unsigned>(out.confirmationPhaseWritten?out.confirmationPhase:-9.f),"source mode confirmation fraction");
    ++cases;return out;
}
void initTests(RefMemory& m){
    for(unsigned mode:{0u,1u,2u})for(unsigned flags:{0u,1u,2u,0xffffffffu}){
        seed(m);OriginalModeMenuTransition s;s.drawFrame444=47;s.phase468=3;s.fade448=9;
        s.confirmationFrame452=0x87654321;s.overlayEnabled456=0;s.selected564=2;s.committed460=6;
        s.pointsWarning520=s.cardWarning528=1;s.pointsWarningAge524=75;s.cardWarningAge532=121;
        s.sharedCountdown1176=999;s.profileMode0=mode;s.profileFlags1180=flags;s.profilePoints72=123456;
        s.parentEvent64=0x842;s.timedOut572=7;s.profileByte1191=0x91;put(m,s);
        RefCpu c(m);c.r[14]=stack-512;c.r[15]=stack-512;m.write32(c.r[14]+96,owner);c.r[7]=64;
        c.callHooks[0x0c021960]=[](auto& x){++hooks;x.r[0]=selector;};
        run(c,0x0c125a04,0x0c125a36);run(c,0x0c125aa8,0x0c125ab8);run(c,0x0c125b34,0x0c125b50);
        c.r[0]=96;run(c,0x0c125c4a,0x0c125c62);
        initializeOriginalModeMenuTransition(s);compare(m,s);++cases;
    }
}
void drawTests(RefMemory& m){
    for(unsigned counter:{0u,1u,7u,14u,15u,16u,255u,0x7fffffffu,0x80000000u,0xffffffffu})
    for(unsigned enabled:{0u,1u})for(unsigned warning:{0u,1u,2u}){
        seed(m);OriginalModeMenuTransition s;s.fade448=counter;s.overlayEnabled456=enabled;s.drawFrame444=counter;
        s.pointsWarning520=s.cardWarning528=warning;put(m,s);
        RefCpu c(m);c.r[4]=owner;c.r[15]=stack;c.pr=stop;unsigned fades=0,argb=0;
        c.callHooks[0x0c0c5200]=[&](auto& x){++hooks;++fades;argb=x.r[5];equal(x.r[4],fade,"mode fade resource");equal(x.r[6],0,"mode fade unscaled");equal(x.fr[4],0,"mode fade zeroZ");};
        std::array<int,2> warningChunks={-1,-1};
        c.callHooks[0x0c128f60]=[&](auto& x){++hooks;warningChunks[0]=int(x.r[5]);};
        c.callHooks[0x00ed0010]=[&](auto& x){++hooks;warningChunks[1]=int(x.r[5]);};
        for(unsigned fn:{0x0c1d09e0u,0x0c1d0880u,0x0c129040u,0x0c1bbcc0u})c.callHooks[fn]=[](auto&){++hooks;};
        run(c,0x0c1261c0,stop);equal(fades,enabled,"mode overlay enabled");
        const auto nativeChunks=originalModeWarningChunks(s);
        for(unsigned bank=0;bank<2;++bank)equal(unsigned(warningChunks[bank]),unsigned(nativeChunks[bank]),"source warning bank/chunk");
        if(enabled)equal(argb,originalModeMenuFadeArgb(s),"source mode fade ARGB");
        advanceOriginalModeMenuDraw(s);compare(m,s);++cases;
    }
}
void casesTests(RefMemory& m){
    for(unsigned phase=0;phase<5;++phase)for(unsigned mode=0;mode<3;++mode)for(unsigned next=0;next<3;++next)
    for(unsigned points:{0u,3999u,4000u,999999u,0xffffffffu})for(unsigned flags:{0u,1u,2u,3u})
    for(unsigned input=0;input<4;++input){
        OriginalModeMenuTransition s;s.phase468=phase;s.fade448=phase==0||phase==3?15:0;
        s.confirmationFrame452=120;s.sharedCountdown1176=(input&2)?1:1279;s.selected564=mode;
        s.profileMode0=1;s.profilePoints72=points;s.profileFlags1180=flags;
        tick(m,s,{bool(input&1),next});
    }
    for(unsigned phase=0;phase<5;++phase)for(unsigned frame:{0u,1u,119u,120u,121u,0x7fffffffu,0xffffffffu})
    for(unsigned fadeCounter:{0u,1u,14u,15u,16u,0x7fffffffu,0x80000000u,0xffffffffu}){
        OriginalModeMenuTransition s;s.phase468=phase;s.confirmationFrame452=frame;s.fade448=fadeCounter;
        s.sharedCountdown1176=100;s.selected564=0;s.profileMode0=2;s.parentEvent64=0x40;
        tick(m,s,{false,0});
    }
    for(unsigned count:{0u,1u,80u,1279u,0x80000000u,0xffffffffu})for(unsigned timeout:{0u,1u,255u}){
        OriginalModeMenuTransition s;s.phase468=1;s.sharedCountdown1176=count;s.timedOut572=std::uint8_t(timeout);
        s.selected564=2;s.profilePoints72=3999;tick(m,s,{false,2});
    }
    for(unsigned warning:{0u,1u,2u})for(unsigned age:{0u,1u,119u,120u,121u,0x7fffffffu,0xffffffffu})
    for(unsigned mode=0;mode<3;++mode)for(unsigned next=0;next<3;++next){
        OriginalModeMenuTransition s;s.phase468=1;s.sharedCountdown1176=999;s.selected564=mode;
        s.pointsWarning520=s.cardWarning528=warning;s.pointsWarningAge524=s.cardWarningAge532=age;
        tick(m,s,{false,next});
    }
    for(unsigned phase=0;phase<4;++phase)for(unsigned cabinet:{0u,0x10u,0x80u,0x90u}){
        OriginalModeMenuTransition s;s.phase468=phase;s.sharedCountdown1176=999;
        auto event=tick(m,s,{false,0},std::uint8_t(cabinet));equal(event.selectionCommitted,phase==1&&bool(cabinet&0x80),"only active confirm commits");
    }
    for(unsigned phase=0;phase<5;++phase){OriginalModeMenuTransition s;s.phase468=phase;s.sharedCountdown1176=999;
        equal(tick(m,s,{false,0,true}).parentRequested,1,"external hidden exit requests parent");equal(s.parentEvent64,8,"external exit uses group event8");}
}
void sequenceTests(RefMemory& m){
    for(unsigned selected=0;selected<3;++selected){
        OriginalModeMenuTransition s;s.profilePoints72=8000;s.profileFlags1180=1;initializeOriginalModeMenuTransition(s);
        for(unsigned n=0;n<16;++n){auto e=tick(m,s,{true,selected});equal(e.readyRequested,n==15,"sixteen-update entry");equal(s.sharedCountdown1176,1279,"entry preserves mode timer");equal(s.selected564,0,"entry retains initial Legend selection");}
        tick(m,s,{false,selected});auto accepted=tick(m,s,{true,selected});
        equal(accepted.selectionCommitted,1,"mode accepted confirmation");equal(std::bit_cast<unsigned>(accepted.confirmationPhase),0,"mode first confirmation phase iszero");
        const auto countdown=s.sharedCountdown1176;
        for(unsigned n=1;n<138;++n){const auto e=tick(m,s,{false,(selected+1)%3});
            equal(e.parentRequested,n==137,"138-update normal completion");equal(s.sharedCountdown1176,countdown,"confirmation preserves countdown");
            equal(s.selected564,selected,"confirmation locks mode selection");
            if(n==120){equal(s.phase468,2,"frame120 remains in confirmation");equal(std::bit_cast<unsigned>(e.confirmationPhase),0x3f800000,"last source confirmation phase isone");}
        }
        equal(s.profileMode0,selected,"confirmed mode persisted");equal(s.profileByte1191,2,"source commit byte");equal(s.parentEvent64,8,"normal completion uses parent8");
    }
    // The source checks the old selected mode. A simultaneous move and
    // confirmation therefore differs from applying host validation afterward.
    OriginalModeMenuTransition s;s.phase468=1;s.sharedCountdown1176=999;s.selected564=0;
    equal(tick(m,s,{true,2}).selectionCommitted,1,"old-selection input ordering");equal(s.profileMode0,2,"same-tick selection commit preserves source ordering");
    s={};s.phase468=1;s.sharedCountdown1176=1;s.selected564=2;s.profilePoints72=3999;
    equal(tick(m,s,{false,2}).selectionCommitted,1,"timeout commits fallback");equal(s.profileMode0,1,"ineligible Bunta timeout chooses TimeAttack");
    s={};s.phase468=1;s.sharedCountdown1176=1279;s.selected564=2;
    tick(m,s,{true,2});equal(s.pointsWarning520,1,"ineligible Bunta shows points notice");equal(s.pointsWarningAge524,1,"rejection frame advances notice age");
    for(unsigned n=1;n<122;++n){tick(m,s,{false,2});equal(s.pointsWarning520,n<121,"points notice persists122updates including rejection");}
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("canonical-image required");RefMemory memory(argv[1]);
    initTests(memory);drawTests(memory);casesTests(memory);sequenceTests(memory);
    std::cout<<"PASS "<<cases<<" original Mode lifecycle cases, "<<checks<<" comparisons, "<<steps<<" actual instructions, "<<hooks<<" explicit dependency hooks. Full125E20/125F40, sparse Init, full Draw with graphics hooks;16-frame entry,138-frame confirmation, eligibility ordering and timeout routing.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
