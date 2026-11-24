#include "original_forced_quit.h"
#include "original_race_rules.h"
#include "sh4_scalar_reference.h"
#include <array>
#include <bit>
#include <iostream>
#include <random>
#include <string>
#include <vector>
using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr std::uint32_t race=0x0cf00000,config=0x0cf02000,car=0x0cf03000,
    rivalCar=0x0cf04000,live=0x0cf05000,published=0x0cf06000,
    rivalLive=0x0cf07000,rivalPublished=0x0cf08000,timerA=0x0cf09000,
    timerB=0x0cf09100,child=0x0cf0a000,network=0x0cf0b000,record=0x0cf0c000,
    stack=0x0cffe000,stop=0x0f000000,profile=0x0c31c99c,
    flagsAddress=profile+1180,cardAddress=profile+1189,brakeGlobal=0x0c2f4de0;
std::uint64_t checks{},instructions{},cases{},logs{},audio{},configCalls{},networkCalls{},division{};
std::string context;
void equal(std::uint32_t a,std::uint32_t b,const char* label){
    ++checks;if(a!=b)throw std::runtime_error(context+": "+label+" expected="+hex(a)+" original="+hex(b));
}
struct State {
    OriginalCabinetButtonCounters buttons{};
    OriginalRaceTimer remaining{-100,10000,0},elapsed{100,30000,0};
    std::uint32_t mode{2},configWord{},flags{0x15a40181},timeUp{},liveFlags{0x80010001},
        publishedFlags{},rivalPublishedFlags{},outcome{0x12345678};
    std::uint8_t card{1},brake{0x55},goal{},challenger{};
    bool preferred{true},rivalPreferred{true};
};
std::array<std::uint32_t,448> raceBefore;
std::array<std::uint8_t,1228> profileBefore;
void seed(RefMemory& m,const State& s){
    m.clear();m.zeroRegion(race,0x10000);m.zeroRegion(stack-0x1000,0x2000);
    m.zeroRegion(0x0c92ece0,0x60);
    for(std::uint32_t i=0;i<raceBefore.size();++i)m.write32(race+i*4,0xa5a00000u+i);
    for(std::uint32_t i=0;i<profileBefore.size();++i)m.write8(profile+i,std::uint8_t(i*73+5));
    m.write32(race+0x418,car);m.write32(race+0x41c,rivalCar);
    m.write32(car+0x95c,live);m.write32(car+0x960,s.preferred?published:0);
    m.write32(rivalCar+0x95c,rivalLive);m.write32(rivalCar+0x960,s.rivalPreferred?rivalPublished:0);
    m.write32(live+80,s.liveFlags);m.write32(published+80,s.publishedFlags);
    m.write32(rivalLive+80,s.rivalPublishedFlags);m.write32(rivalPublished+80,s.rivalPublishedFlags);
    m.write32(race+0x498,timerA);m.write32(race+0x49c,timerB);
    m.write32(timerA+4,std::uint32_t(s.remaining.step));m.write32(timerA+8,s.remaining.value);m.write32(timerA+12,s.remaining.status);
    m.write32(timerB+4,std::uint32_t(s.elapsed.step));m.write32(timerB+8,s.elapsed.value);m.write32(timerB+12,s.elapsed.status);
    m.write32(race+0x668,s.mode);m.write32(race+0x648,s.timeUp);m.write32(race+0x66c,s.outcome);
    m.write8(race+0x624,s.goal);m.write8(race+0x64c,s.challenger);
    m.write32(race+0x68c,s.buttons.start);m.write32(race+0x690,s.buttons.view);
    m.write32(race+0x694,s.buttons.releasedStart);m.write32(race+0x698,s.buttons.releasedView);
    m.write32(config+12,s.configWord);m.write32(flagsAddress,s.flags);m.write8(cardAddress,s.card);
    m.write8(brakeGlobal,s.brake);m.write32(network,network+0x100);
    for(std::uint32_t i=0;i<raceBefore.size();++i)raceBefore[i]=m.read32(race+i*4);
    for(std::uint32_t i=0;i<profileBefore.size();++i)profileBefore[i]=m.read8(profile+i);
}
void cpuSetup(RefCpu& c){
    c.r[15]=stack;c.pr=stop;
    c.callHooks[0x0c04fcc0]=[](RefCpu& c){++configCalls;c.r[0]=config;};
    c.callHooks[0x0c055d60]=[](RefCpu&){++logs;};
    c.callHooks[0x0c1420c0]=[](RefCpu&){++audio;};
    // Explicit linked-device boundary: status=true chooses the original no-copy
    // branch. We do NOT model remote timer synchronization or host networking.
    c.callHooks[0x0c05a300]=[](RefCpu& c){++networkCalls;c.r[0]=network;};
    c.callHooks[0x0c161be0]=[](RefCpu& c){++networkCalls;c.r[0]=network+0x200;};
    c.callHooks[0x0c1641e0]=[](RefCpu& c){++networkCalls;c.r[0]=1;};
    // 2223B8 uses a double-mode runtime outside this scalar interpreter. The
    // warning branch passes positive exact integers 6000..41999 and divisor6000.
    c.callHooks[0x0c2223b8]=[](RefCpu& c){++division;c.fpul=std::uint32_t(signed32(c.r[4])/signed32(c.r[5]));};
}
void compare(RefMemory& m,const State& s){
    auto expected=raceBefore;
    expected[0x68c/4]=s.buttons.start;expected[0x690/4]=s.buttons.view;
    expected[0x694/4]=s.buttons.releasedStart;expected[0x698/4]=s.buttons.releasedView;
    expected[0x648/4]=s.timeUp;
    for(std::uint32_t i=0;i<expected.size();++i)equal(expected[i],m.read32(race+i*4),"all race words (goal/outcome/challenger preserved)");
    for(std::uint32_t i=0;i<profileBefore.size();++i){
        const auto value=i>=1180&&i<1184?std::uint8_t(s.flags>>((i-1180)*8)):profileBefore[i];
        equal(value,m.read8(profile+i),"all profile bytes (only flag1180 owned)");
    }
    equal(std::uint32_t(s.remaining.step),m.read32(timerA+4),"remaining step");
    equal(s.remaining.value,m.read32(timerA+8),"remaining value");equal(s.remaining.status,m.read32(timerA+12),"remaining status");
    equal(std::uint32_t(s.elapsed.step),m.read32(timerB+4),"elapsed step");
    equal(s.elapsed.value,m.read32(timerB+8),"elapsed value");equal(s.elapsed.status,m.read32(timerB+12),"elapsed status");
    equal(s.liveFlags,m.read32(live+80),"live player flags");equal(s.publishedFlags,m.read32(published+80),"published player unchanged");
    equal(s.rivalPublishedFlags,m.read32(rivalLive+80),"live rival unchanged");equal(s.rivalPublishedFlags,m.read32(rivalPublished+80),"published rival unchanged");
    equal(s.brake,m.read8(brakeGlobal),"brake global");
}
// Fixture composition at the exact original owner sites. Existing timer helper
// is compiled from a hashed snapshot. Not an App scheduler or new online owner.
bool projectTimer(State& s){
    const bool positive=signed32(s.remaining.value)>0;
    bool changed=false;
    if(positive){
        const auto code=(s.configWord>>14)&3;
        if(code!=1&&code!=2)advanceOriginalRaceTimer(s.remaining);
        applyOriginalForcedQuitInPositiveBranch(s.buttons,positive,s.mode,s.configWord,s.card,s.remaining.value,s.flags);
    }else changed=applyOriginalExpiredRaceTimer(s.remaining.value,s.mode,
        s.preferred?s.publishedFlags:s.liveFlags,s.rivalPublishedFlags,s.timeUp,s.liveFlags,s.brake);
    advanceOriginalRaceTimer(s.elapsed);return changed;
}
void timerCase(RefMemory& m,State s){
    ++cases;seed(m,s);const auto previousLogs=logs;
    const bool expectedLog=projectTimer(s);
    RefCpu c(m);cpuSetup(c);c.r[4]=race;instructions+=c.run(0x0c067d00,stop,3000);
    compare(m,s);equal(expectedLog,std::uint32_t(logs-previousLogs),"exact log transition");
}
void counterCases(RefMemory& m){
    const std::array<std::uint32_t,9> values{0,1,2,3,4,0x7fffffff,0x80000000,0xfffffffe,0xffffffff};
    for(auto a:values)for(auto b:values)for(unsigned level=0;level<256;++level){
        State s;s.buttons={a,b,a^0xfeed00,b^0xbaad00};seed(m,s);++cases;
        m.write8(0x0c92ece0,std::uint8_t(level));
        RefCpu c(m);cpuSetup(c);c.r[12]=race;
        instructions+=c.run(0x0c05fddc,0x0c05fe1a,80);
        advanceOriginalCabinetButtons(s.buttons,std::uint8_t(level));
        equal(s.buttons.start,m.read32(race+0x68c),"held Start wrapping");equal(s.buttons.view,m.read32(race+0x690),"held View wrapping");
        equal(s.buttons.releasedStart,m.read32(race+0x694),"released Start");equal(s.buttons.releasedView,m.read32(race+0x698),"released View");
        equal(s.flags,m.read32(flagsAddress),"counter producer does not set profile flag");
    }
    for(std::uint32_t flags:{0u,0xffffffffu,0x20000u,0xfffdffffu,0x81234567u}){
        State s;s.flags=flags;s.buttons={1,2,3,4};seed(m,s);++cases;
        m.write32(child+48,race);RefCpu c(m);cpuSetup(c);c.r[14]=child;
        instructions+=c.run(0x0c05fc7a,0x0c05fd80,50);
        resetOriginalCabinetButtons(s.buttons,s.flags);compare(m,s);
    }
}
void chordMatrix(RefMemory& m){
    for(std::uint32_t mode:{0u,1u,2u})for(std::uint32_t code=0;code<4;++code)
    for(std::uint8_t card:{0,1,2,255})for(std::uint32_t start=0;start<=4;++start)
    for(std::uint32_t view=0;view<=4;++view){
        State s;s.mode=mode;s.configWord=(0xa5a50fffu&~0xc000u)|(code<<14);s.card=card;
        s.buttons={start,view,0xdead,0xbeef};s.goal=std::uint8_t((start+view)&1);s.challenger=std::uint8_t(start&1);
        s.outcome=(start+view)&1;s.flags=start&1?0xffffffffu:0xfffdffffu;
        context="matrix mode"+std::to_string(mode)+" C"+std::to_string(code)+" card"+std::to_string(card)+" counters"+std::to_string(start)+","+std::to_string(view);
        timerCase(m,s);
    }
    for(auto remaining:{1u,99u,100u,101u,0x7fffffffu})for(auto count:{1u,4u})
    for(std::int32_t step:{-100,0,100,2147483647}){
        State s;s.remaining={step,remaining,0x80};s.buttons={count,count,0,0};s.elapsed={100,0x7fffffceu,0x40};
        context="entry-positive/decrement/overflow";timerCase(m,s);
    }
    for(auto counter:{0x7fffffffu,0x80000000u,0xfffffffeu,0xffffffffu})for(bool start:{false,true}){
        State s;s.buttons={start?counter:1,start?1:counter,4,4};context="signed/unsigned chord rejects";timerCase(m,s);
    }
}
void positiveBrakeMatrix(RefMemory& m){
    for(auto mode:{0u,2u})for(auto remaining:{1u,99u,100u,101u,10000u})for(auto count:{0u,1u,4u}){
        State s;s.mode=mode;s.remaining.value=remaining;s.liveFlags|=0x2000u;s.brake=1;
        s.buttons={count,count,0,0};context="positive entry retains prior automatic brake";timerCase(m,s);
    }
}
void expiredMatrix(RefMemory& m){
    for(std::uint32_t mode:{0u,1u,2u})for(auto remaining:{0u,0xffffffffu,std::uint32_t(-29999),std::uint32_t(-30000),0x80000000u})
    for(unsigned stopped=0;stopped<4;++stopped)for(bool preferred:{false,true})
    for(auto previousTimeUp:{0u,1u,0xfeedu}){
        State s;s.mode=mode;s.remaining.value=remaining;s.timeUp=previousTimeUp;
        s.buttons={1,1,0xab,0xcd};s.preferred=preferred;s.rivalPreferred=!preferred;
        s.liveFlags=0x80010001u|(stopped&1?0x4000:0);
        s.publishedFlags=0x100u|(stopped&1?0x4000:0);s.rivalPublishedFlags=0x200u|(stopped&2?0x4000:0);
        s.goal=1;s.challenger=1;s.outcome=0;s.flags=0xa5a5a5a5;
        context="nonpositive entry / stopped / fallback / mode"+std::to_string(mode);timerCase(m,s);
    }
}
void viewPrefix(const RefMemory& original,const State& s){
    // Separate observational copy: camera controls cannot alter the ongoing
    // multi-update timer/counter sequence or repair its state.
    RefMemory m=original;
    // Exact camera-prefix predicate and side effects only. These leaves touch
    // camera resources; no renderer or device is exercised by this fixture.
    const bool expectedToggle=s.buttons.view==3||(s.buttons.view==0&&s.buttons.releasedView-1u<=1u);
    const auto beforeFlags=m.read32(flagsAddress);m.write8(race+0x494,0);
    unsigned toggleCalls=0;RefCpu c(m);cpuSetup(c);c.r[4]=race;
    c.callHooks[0x0c0a9da0]=[&](RefCpu&){++toggleCalls;};
    c.callHooks[0x0c064680]=[](RefCpu&){};c.callHooks[0x0c0647a0]=[](RefCpu&){};
    instructions+=c.run(0x0c0683c0,0x0c06842e,150);
    equal(expectedToggle,toggleCalls,"view consumed-counter toggle");
    equal(expectedToggle,m.read8(race+0x494),"view byte");
    equal(expectedToggle?beforeFlags|0x80u:beforeFlags,m.read32(flagsAddress),"view profile bit");
}
void preInitViewOrder(RefMemory& m){
    //Execute actual PreInit Main, not its timer/GO owner. Creation Main is
    //independently bound at source update242 by the existing handoff/camera
    //parent oracle; these cases prove its internal view-before-toggle order.
    for(std::uint32_t mode:{0u,2u})for(std::uint32_t held=0;held<=4;++held)
    for(std::uint32_t released=0;released<=4;++released){
        State s;s.mode=mode;s.buttons={3,held,0,released};seed(m,s);++cases;
        m.write32(child+0x4c,race);m.write32(0x0ca9b334,0);m.write8(race+0x494,0);
        m.write32(config+8,0);std::vector<std::uint32_t> views;unsigned toggles=0;
        RefCpu c(m);cpuSetup(c);c.r[4]=child;
        c.callHooks[0x0c024100]=[&](RefCpu& x){views.push_back(x.r[4]);};
        c.callHooks[0x0c0a9da0]=[&](RefCpu&){
            equal(race+76,views.back(),"PreInit main-view dispatch precedes View toggle");++toggles;
        };
        c.callHooks[0x0c064680]=[](RefCpu&){};c.callHooks[0x0c0647a0]=[](RefCpu&){};
        c.callHooks[0x0c068be0]=[](RefCpu&){}; // unrelated lamp/control tail
        instructions+=c.run(0x0c05be00,stop,600);
        //05BE50..5E branches past +18C when unsigned(mode-2)<=1.
        //Ordinary mode0 includes it; time-attack mode2 omits it.
        equal(mode==2?2u:3u,std::uint32_t(views.size()),"PreInit normal view dispatch count");
        equal(race+0x2cc,views.front(),"PreInit first dispatched view");
        if(mode==0)equal(race+0x18c,views[1],"PreInit ordinary optional view");
        equal(race+76,views.back(),"PreInit last dispatched main view");
        const bool wanted=held==3||(held==0&&released-1u<=1u);
        equal(wanted,toggles,"PreInit actual View predicate");equal(wanted,m.read8(race+0x494),"PreInit source view byte");
        equal(s.remaining.value,m.read32(timerA+8),"PreInit never runs GO timer");
        equal(s.flags&0x20000,m.read32(flagsAddress)&0x20000,"PreInit never applies Forced Quit marker");
        equal(s.buttons.start,m.read32(race+0x68c),"PreInit does not consume Start chord");
        equal(s.buttons.view,m.read32(race+0x690),"PreInit does not consume View chord");
    }
}
void sequences(RefMemory& m){
    for(int stagger=-4;stagger<=4;++stagger)for(unsigned preHeld:{0u,2u,4u,240u}){
        context="stagger/held before GO";State s;s.buttons={preHeld,0,11,22};s.flags=0x15a40101;
        seed(m,s);bool everAccepted=false;
        for(unsigned tick=0;tick<12;++tick){
            const bool heldStart=tick>=unsigned(std::max(0,-stagger))&&tick<7;
            const bool heldView=tick>=unsigned(std::max(0,stagger))&&tick<7;
            const auto level=std::uint8_t((heldStart?0x80:0)|(heldView?0x10:0));
            RefCpu c(m);cpuSetup(c);c.r[12]=race;m.write8(0x0c92ece0,level);
            instructions+=c.run(0x0c05fddc,0x0c05fe1a,80);advanceOriginalCabinetButtons(s.buttons,level);
            const auto before=s.flags;const auto previousLogs=logs;const bool expectedLog=projectTimer(s);
            c.r[4]=race;c.r[15]=stack;c.pr=stop;instructions+=c.run(0x0c067d00,stop,3000);
            equal(expectedLog,std::uint32_t(logs-previousLogs),"sequence exact diagnostic transition");
            compare(m,s);++cases;
            if((s.flags^before)&0x20000){everAccepted=true;equal(0,s.timeUp,"accepted chord not immediate timeup");equal(0,s.liveFlags&0x2000,"accepted chord not immediate brake");}
            if(everAccepted){
                viewPrefix(m,s);
                // The camera-prefix oracle above owns only its isolated copy.
                equal(s.flags,m.read32(flagsAddress),"accepted chord never toggles view");
            }
            if(tick==8){s.publishedFlags|=0x4000;m.write32(published+80,s.publishedFlags);}
        }
    }
    // A plain short View tap is a control, proving the camera hook can fire.
    for(auto held:{1u,2u,3u,4u}){State s;s.buttons.view=held;seed(m,s);viewPrefix(m,s);++cases;}
}
void sameGoalOrder(RefMemory& m){
    for(std::uint32_t mode:{0u,2u})for(auto outcome:{0u,1u}){
        State s;s.mode=mode;s.buttons={1,1,0,0};s.goal=0;s.outcome=0xfeed;
        s.flags=0xffffffff;s.liveFlags|=0x2000;seed(m,s);++cases;
        m.write32(race+0x61c,0);m.write32(race+0x628,record);m.write32(record+40,0xffffffff);
        std::vector<std::uint32_t> order;RefCpu c(m);cpuSetup(c);c.r[8]=child;m.write32(child+12,race);
        c.callHooks[0x0c0680c0]=[&](RefCpu&){order.push_back(0x680c0);};
        c.callHooks[0x0c069c40]=[&](RefCpu&){order.push_back(0x69c40);};
        // The trigger predicate is supplied as a goal crossing; actual goal
        // handler068520 executes through its timestamp/flags/log prefix. Only
        // crossing-time math is hooked. Later widget/network tail is outside
        // this boundary. Separate CPU preserves the caller's registers/stack.
        c.callHooks[0x0c067a80]=[&](RefCpu& x){
            order.push_back(0x67a80);RefCpu goalCpu(x.memory);cpuSetup(goalCpu);goalCpu.r[4]=race;
            goalCpu.callHooks[0x0c061780]=[](RefCpu& y){y.r[0]=0x1234567b;};
            instructions+=goalCpu.run(0x0c068520,0x0c068586,1500);
        };
        // Result arithmetic is an explicit accepted leaf; original caller and
        // timer/chord code execute unhooked around it.
        c.callHooks[0x0c068a60]=[&](RefCpu& x){
            order.push_back(0x68a60);equal(1,x.memory.read8(race+0x624),"outcome runs after same-tick goal");
            equal(0,x.memory.read32(flagsAddress)&0x20000,"goal clears forced-quit marker before chord");
            equal(0,x.memory.read32(live+80)&0x2000,"goal clears automatic brake before chord");
            x.memory.write32(race+0x66c,outcome);
        };
        instructions+=c.run(0x0c05c08e,0x0c05c0ac,4000);
        equal(4,std::uint32_t(order.size()),"GO owner call count");
        equal(0x680c0,order[0],"GO projection order");equal(0x69c40,order[1],"GO preprogress order");
        equal(0x67a80,order[2],"GO goal order");equal(0x68a60,order[3],"GO outcome order");
        equal(1,m.read8(race+0x624),"goal remains set after chord");equal(outcome,m.read32(race+0x66c),"outcome remains after chord");
        equal(0x12345678,m.read32(record+40),"goal timestamp retained after chord");
        equal(0x20000,m.read32(flagsAddress)&0x20000,"chord re-sets goal-cleared marker");
        equal(0,m.read32(timerA+8),"same-goal chord accepted");equal(4,m.read32(race+0x68c),"same-goal counter consumed");
        equal(0,m.read32(race+0x648),"same-goal not forced timeout");equal(s.liveFlags&~0x2000u,m.read32(live+80),"same-goal not forced brake");
        equal(0,m.read8(brakeGlobal),"goal-cleared brake global retained");
    }
}
}
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("usage: forced-quit-reference canonical-image");
    RefMemory memory(argv[1]);context="counter producer/reset";counterCases(memory);
    chordMatrix(memory);positiveBrakeMatrix(memory);expiredMatrix(memory);sequences(memory);context="same-tick goal/chord";sameGoalOrder(memory);
    context="PreInit View owner order";preInitViewOrder(memory);
    std::cout<<"PASS cases="<<cases<<" checks="<<checks<<" instructions="<<instructions
        <<" configHooks="<<configCalls<<" linkedNoCopyHooks="<<networkCalls
        <<" integerDivisionHooks="<<division<<" logHooks="<<logs<<" audioHooks="<<audio<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<" cases="<<cases<<" checks="<<checks<<" instructions="<<instructions<<'\n';return 1;}}
