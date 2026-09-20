#include "original_bunta_setup.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <limits>

using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned profile=0x0c31c99c,owner=0x0cd00000,race=0x0cd10000,
    tls=0x0cd20000,frame=0x0cfe0000,stack=0x0cfff000,stop=0xff0000,
    courseObject=0x0cd30000,vtable=0x0cd31000,paths=0x0cd32000,timer=0x0cd40000;
std::uint64_t checks=0,steps=0,hooks=0,cases=0;
std::size_t run(RefCpu& c,unsigned start,unsigned end,unsigned maximum){
    try{return c.run(start,end,maximum);}
    catch(const std::exception& e){throw std::runtime_error(hex(start)+" at "+hex(c.pc)+": "+e.what());}
}
void equal(unsigned a,unsigned b,const char* label){++checks;if(a!=b){
    std::cerr<<label<<" actual="<<a<<" expected="<<b<<'\n';throw std::runtime_error(label);}}
void seed(RefMemory& m,const OriginalBattleProfile& p){
    m.clear();m.zeroRegion(0x0cd00000,0x50000);m.zeroRegion(frame,0x20000);
    for(unsigned i=0;i<p.words.size();++i)m.write32(profile+i*4,p.words[i]);
    m.write32(tls+4,tls+8);m.write32(tls+8,tls+8);
}
void eligibility(RefMemory& m){
    for(unsigned mode=0;mode<4;++mode)for(bool restricted:{false,true})
    for(int points:{std::numeric_limits<int>::min(),-1,0,3999,4000,10000,std::numeric_limits<int>::max()})
    for(unsigned flags:{0u,1u,2u,3u,0x80000000u,0x80000002u}){
        auto p=makeOriginalFreshBattleProfile();p.setu(72,unsigned(points));seed(m,p);
        m.write32(owner+564,mode);m.write8(owner+572,restricted);m.write32(0x0c31ce38,flags);
        RefCpu c(m);c.r[8]=owner+508;c.r[9]=owner;c.r[15]=stack;
        c.callHooks[0x0c141f80]=[](auto&){++hooks;};
        steps+=run(c,0x0c126006,0x0c12606c,200);
        const auto selected=originalModeAfterBuntaEligibility(mode,restricted,points,flags);
        equal(m.read32(profile),selected,"profile mode after eligibility");
        equal(m.read32(owner+564),selected,"menu mode after eligibility");++cases;
    }
    // Profile3 belongs to the separately named iSelVsMain initializer.
    auto p=makeOriginalFreshBattleProfile();p.setu(0,2);seed(m,p);RefCpu c(m);c.r[14]=frame;
    steps+=run(c,0x0c135a4a,0x0c135a54,20);equal(m.read32(profile),3,"Versus profile mode");++cases;
}
void manualEligibility(RefMemory& m){
    for(unsigned mode=0;mode<4;++mode)for(bool pressed:{false,true})
    for(int points:{std::numeric_limits<int>::min(),-1,0,3999,4000,10000,std::numeric_limits<int>::max()})
    for(unsigned flags:{0u,1u,2u,3u,0x80u,0x80000002u}){
        auto p=makeOriginalFreshBattleProfile();p.setu(72,unsigned(points));p.setu(1180,flags);seed(m,p);
        m.write32(owner+564,mode);m.write8(0x0c92ed00,0);
        RefCpu c(m);c.r[4]=owner;c.r[15]=stack;c.pr=stop;unsigned sounds=0;
        c.callHooks[0x0c0d4300]=[&](auto& cpu){++hooks;cpu.r[0]=pressed;};
        c.callHooks[0x0c1fa9e0]=[](auto&){++hooks;};
        c.callHooks[0x0c141f80]=[&](auto& cpu){++hooks;++sounds;equal(cpu.r[4],8,"ineligible confirm sound");};
        steps+=run(c,0x0c125e20,stop,400);
        const auto why=originalBuntaEligibility(p);const bool rejected=mode==2&&pressed&&why!=OriginalBuntaEligibility::Eligible;
        equal(c.r[0],pressed&&!rejected,"manual confirm eligibility");
        equal(m.read32(owner+520),rejected&&why==OriginalBuntaEligibility::InsufficientPoints,"points warning");
        equal(m.read32(owner+528),rejected&&why==OriginalBuntaEligibility::CardRequired,"card warning");
        equal(sounds,rejected,"rejected confirm sound count");
        equal(m.read32(owner+564),mode,"manual rejection retains selection");++cases;
    }
    // Actual192820 constant ->1346A0 setter ->1341A0 countdown ->134680
    // getter establishes that menu572 is a timeout, not a card/config flag.
    auto p=makeOriginalFreshBattleProfile();seed(m,p);RefCpu c(m);c.r[15]=stack;c.pr=stop;
    steps+=run(c,0x0c192820,stop,30);equal(c.r[0],1279,"original menu countdown");
    c.r[4]=c.r[0];c.pr=stop;steps+=run(c,0x0c1346a0,stop,30);
    for(unsigned frameIndex=0;frameIndex<1281;++frameIndex){c.pr=stop;steps+=run(c,0x0c1341a0,stop,40);
        c.pr=stop;steps+=run(c,0x0c134680,stop,30);equal(c.r[0],frameIndex<1278?1278-frameIndex:0,"menu countdown clamps at zero");}
    ++cases;
}
void acceptedCardFlag(RefMemory& m){
    for(unsigned flags:{0u,1u,2u,3u,0x80u,0x4000u,0x80000000u,0xffffffffu}){
        auto p=makeOriginalFreshBattleProfile();p.setu(1180,flags);p.setu(72,3499);seed(m,p);
        RefCpu c(m);steps+=run(c,0x0c0fecae,0x0c0fecb8,20);applyOriginalAcceptedCardFlag(p);
        for(unsigned i=0;i<p.words.size();++i)equal(m.read32(profile+i*4),p.words[i],"accepted card preserves profile");
        equal(unsigned(originalBuntaEligibility(p)),unsigned(OriginalBuntaEligibility::InsufficientPoints),"accepted card does not grant points");
        // The separate physical-card purchase path sets bit2. It is not the
        // accepted existing-card operation used by a native persisted owner.
        seed(m,p);RefCpu purchase(m);steps+=run(purchase,0x0c102cea,0x0c102cf2,20);
        equal(m.read32(profile+1180),p.u(1180)|2u,"card purchase bit2");++cases;
    }
}
void commonLaunch(RefMemory& m,OriginalBattleProfile p,unsigned difficulty,bool live=false){
    seed(m,p);const OriginalBuntaCommonRaceFields native=live?makeOriginalBuntaRaceSetup(p,difficulty):originalLegacyBuntaCommonRaceSetup(p,difficulty);
    unsigned createdMode=~0u;
    RefCpu create(m);create.r[4]=owner;create.r[15]=stack;create.pr=stop;
    create.callHooks[0x0c221fc0]=[](auto& c){++hooks;c.r[0]=tls;};
    create.callHooks[0x0c055d60]=[](auto&){++hooks;};
    create.callHooks[0x0c021ee0]=[](auto& c){++hooks;equal(c.r[4],1748,"common race allocation extent");c.r[0]=race;};
    create.callHooks[0x0c05d240]=[&](auto& c){++hooks;createdMode=c.r[6];equal(c.r[4],race,"common race memory");equal(c.r[5],owner+(live?84:88),"common race listener");c.r[0]=race;};
    steps+=run(create,live?0x0c0907e0:0x0c1871c0,stop,1000);
    equal(createdMode,native.numericRaceMode1640,"HRaceBunta constructor mode");
    equal(m.read32(owner+(live?88:92)),race,"common race attachment");
    for(unsigned i=0;i<p.words.size();++i)equal(m.read32(profile+i*4),p.words[i],"HRaceBunta preserves profile");
    // Actual common constructor's input-to-field store, including the sparse
    // neighboring defaults. No conversion of mode2 into Legend0 takes place.
    m.write32(frame+1396,owner+88);m.write32(frame+1400,createdMode);
    RefCpu ctor(m);ctor.r[0]=race;ctor.r[14]=frame;
    steps+=run(ctor,0x0c05db62,0x0c05db8c,100);
    equal(m.read32(race+1640),native.numericRaceMode1640,"common race mode field");
    RefCpu selection(m);selection.r[9]=race;selection.r[10]=race+1596;
    selection.t=createdMode==3;
    steps+=run(selection,0x0c062500,0x0c062534,100);
    for(auto pair:std::array<std::pair<unsigned,unsigned>,5>{{{1652,native.course1652},{1656,native.scene1656},
        {1660,native.direction1660},{1664,native.night1664},{1668,native.weather1668}}})
        equal(m.read32(race+pair.first),pair.second,"common course field");
    m.write32(frame+196,race);RefCpu rows(m);rows.r[14]=frame;
    steps+=run(rows,0x0c061b5c,0x0c061c04,120);
    equal(m.read32(race+1564),native.ruleRow1564,"common authored rule row");
    equal(m.read32(race+1568),native.condition1568,"common physics condition");
    m.write32(race+1036,courseObject);m.write32(courseObject,vtable);
    m.write16(vtable+128,0);m.write32(vtable+132,0x00ed0000);
    std::vector<unsigned> slots;
    RefCpu grid(m);grid.r[13]=race;grid.r[14]=frame;grid.r[15]=stack;grid.t=createdMode==2;
    grid.callHooks[0x00ed0000]=[&](auto& c){++hooks;slots.push_back(c.r[6]);};
    steps+=run(grid,0x0c062960,0x0c0629f2,180);
    equal(unsigned(slots.size()),2,"grid calls");equal(slots[0],native.playerGridSlot,"player grid");equal(slots[1],native.rivalGridSlot,"unused rival grid");
    m.write32(courseObject+28,paths);m.write32(courseObject+32,0x0cd50000);
    m.write32(paths+24,p.u(12));for(unsigned k:{8u,12u,16u,20u})m.write32(paths+k,0x0cd60000+k*256);
    unsigned rivalControl=0,calls=0;
    RefCpu physics(m);physics.r[13]=race;physics.r[14]=frame;physics.r[15]=stack;
    physics.callHooks[0x0c159720]=[&](auto& c){++hooks;++calls;rivalControl=m.read32(c.r[15]+12);equal(c.r[4],native.condition1568,"physics condition argument");};
    steps+=run(physics,0x0c062a44,0x0c062c20,500);
    equal(calls,1,"physics initialization call");equal(rivalControl,unsigned(native.ordinaryRivalControl),"ordinary rival disabled");
    // No numerical hooks: profile override, original timer/bonus switch,
    // getters and timer stores all execute their original instructions.
    m.write32(frame+144,race);m.write32(race+1176,timer);m.write32(race+1180,timer+32);
    RefCpu budget(m);budget.r[14]=frame;budget.r[15]=stack;budget.r[2]=difficulty&15u;
    steps+=run(budget,0x0c06744c,0x0c067686,1000);
    equal(m.read32(frame+192),native.timerMode,"profile timer selector override");
    equal(m.read32(timer+8),native.initialTime6000,"initial countdown");
    for(unsigned i=0;i<6;++i)equal(m.read32(0x0c2f4bd8+i*4),unsigned(native.extensionSeconds[i]),"checkpoint extension");
    ++cases;
}
void liveOwnerBinding(RefMemory& m){
    auto p=makeOriginalFreshBattleProfile();p.setu(0,2);seed(m,p);
    m.write32(owner+12,vtable);m.write16(vtable+56,0);m.write32(vtable+60,0x00ed0000);
    RefCpu dispatch(m);dispatch.r[4]=owner;dispatch.r[15]=stack;dispatch.pr=stop;
    unsigned selected=0;
    dispatch.callHooks[0x0c055d60]=[](auto&){++hooks;};
    dispatch.callHooks[0x00ed0000]=[&](auto& c){++hooks;equal(c.r[4],owner,"Bunta dispatcher owner");selected=c.r[5];};
    steps+=run(dispatch,0x0c076b80,stop,500);equal(selected,9,"profile2 selects top-level child9");

    // Matching registration in the top-level constructor, including actual
    //076B60. Only HBunta allocation construction/list attachment is captured.
    m.write32(frame+336,tls+4);m.write32(frame+324,owner+0x800);m.write32(frame+332,tls);m.write32(frame+288,owner);
    RefCpu registerChild(m);registerChild.r[14]=frame;registerChild.r[15]=stack;
    registerChild.callHooks[0x0c1848c0]=[&](auto& c){++hooks;equal(c.r[4],owner+0x800,"actual HBunta constructor target");c.r[0]=c.r[4];};
    registerChild.callHooks[0x0c0233e0]=[&](auto& c){++hooks;equal(m.read32(c.r[5]+16),9,"actual HBunta top-level registration");};
    steps+=run(registerChild,0x0c076480,0x0c0764b6,300);

    seed(m,p);unsigned allocation=0x0cd50000,hrace=0;
    std::vector<std::pair<unsigned,unsigned>> attached;
    RefCpu constructor(m);constructor.r[4]=owner;constructor.r[15]=stack;constructor.pr=stop;
    constructor.callHooks[0x0c221fc0]=[](auto& c){++hooks;c.r[0]=tls;};
    constructor.callHooks[0x0c021ee0]=[&](auto& c){++hooks;const unsigned size=c.r[4];c.r[0]=allocation;m.zeroRegion(allocation,size+64);allocation+=(size+255)&~255u;};
    for(unsigned target:{0x0c0510a0u,0x0c228e80u,0x0c023220u,0x0c055d60u})
        constructor.callHooks[target]=[](auto& c){++hooks;c.r[0]=c.r[4];};
    for(unsigned target:{0x0c1861e0u,0x0c08f920u,0x0c090b20u,0x0c187a80u,0x0c182620u,0x0c182920u,0x0c095220u})
        constructor.callHooks[target]=[](auto& c){++hooks;c.r[0]=c.r[4];};
    constructor.callHooks[0x0c0233e0]=[&](auto& c){++hooks;attached.push_back({m.read32(c.r[5]+16),c.r[5]});c.r[0]=c.r[5];};
    // Execute actual0903C0, including its installed HRace vtables; the
    // surrounding allocation/base-list/TLS constructors are explicit hooks.
    steps+=run(constructor,0x0c1848c0,stop,5000);
    equal(unsigned(attached.size()),10,"HBunta ten children");
    for(const auto& [id,address]:attached)if(id==3)hrace=address;
    if(!hrace)throw std::runtime_error("HBunta did not attach shared HRace child3");
    equal(m.read32(hrace+12),0x0c385e4c,"actual shared HRace vtable");
    equal(m.read32(0x0c385e4c+20),0x0c090960,"actual shared HRace init");
    // Its Init invokes actual0907E0 and then common Init. Capture only the
    // common constructor/Init, retaining this exact live caller chain.
    m.write32(race+12,vtable);m.write16(vtable+16,0);m.write32(vtable+20,0x00ed0000);
    RefCpu init(m);init.r[4]=hrace;init.r[15]=stack;init.pr=stop;unsigned mode=99,initialized=0;
    init.callHooks[0x0c221fc0]=[](auto& c){++hooks;c.r[0]=tls;};
    init.callHooks[0x0c055d60]=[](auto&){++hooks;};
    init.callHooks[0x0c021ee0]=[](auto& c){++hooks;equal(c.r[4],1748,"live common race size");c.r[0]=race;};
    init.callHooks[0x0c05d240]=[&](auto& c){++hooks;mode=c.r[6];c.r[0]=race;};
    init.callHooks[0x00ed0000]=[&](auto& c){++hooks;equal(c.r[4],race,"live common Init owner");++initialized;};
    steps+=run(init,0x0c090960,stop,1500);equal(mode,0,"LIVE Bunta numeric common mode0");equal(initialized,1,"live common Init called");
    equal(m.read32(profile),2,"live Bunta retains profile2");++cases;
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("canonical program image required");RefMemory m(argv[1]);eligibility(m);manualEligibility(m);acceptedCardFlag(m);liveOwnerBinding(m);
    for(unsigned menu=0;menu<8;++menu)for(unsigned level:{0u,5u,6u,10u,11u,15u})for(unsigned difficulty:{0u,2u,4u,6u}){
        auto p=makeOriginalFreshBattleProfile();p.setu(0,2);
        for(unsigned i=0;i<8;++i)p.setu(1080+i*4,level);
        selectOriginalBuntaCourse(p,menu);commonLaunch(m,p,difficulty);commonLaunch(m,p,difficulty,true);
    }
    for(unsigned condition=0;condition<18;++condition){
        auto p=makeOriginalFreshBattleProfile();p.setu(0,2);p.setu(4,9);p.setu(28,condition);p.setu(12,condition&1);p.setu(8,1);p.setu(32,condition&1);
        commonLaunch(m,p,2);
    }
    std::cout<<"PASS Bunta eligibility/common launch: "<<cases<<" cases, "<<checks<<" comparisons, "<<steps<<" original instructions, "<<hooks
        <<" explicit allocation/TLS/diagnostic/menu/grid/physics-constructor capture hooks. Live HBunta/shared HRace binding and distinct legacy HRaceBunta branch are verified separately.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
