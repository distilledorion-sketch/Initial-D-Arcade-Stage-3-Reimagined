#include "original_race_start.h"
#include "original_countdown_presentation.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <vector>
using namespace idas3::original;
using namespace idas3::reference;
namespace {
std::size_t checks{},instructions{};
void eq(std::uint32_t a,std::uint32_t b,const char* name){++checks;if(a!=b)throw std::runtime_error(std::string(name)+": "+hex(a)+" != "+hex(b));}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Usage: original_race_start_tests canonical_image");
    RefMemory m(argv[1]);
    constexpr auto owner=0x0d000000u,aux=0x0d001000u,auxList=0x0d001100u,auxSentinel=0x0d001200u;
    constexpr auto pre=0x0d002000u,go=0x0d002100u,sentinel=0x0d002200u,vt=0x0d003000u;
    constexpr auto playerCar=0x0d004000u,rivalCar=0x0d005000u,player=0x0d006000u,rival=0x0d006400u;
    constexpr auto stack=0x0d030000u,noop=0x0f001000u,remove=0x0f001100u;
    for(auto mode:{0u,2u}){
        m.clear();m.zeroRegion(owner,0x8000);m.zeroRegion(stack-0x1000,0x11000);
        m.write32(owner+12,0x0c383c74); // actual ARace owner vtable
        m.write32(owner+32,sentinel);m.write32(owner+36,pre);m.write32(owner+68,pre);
        m.write32(pre+4,go);m.write32(pre+12,vt+64);m.write32(pre+20,2);
        m.write32(go+4,sentinel);m.write32(go+12,vt+128);m.write32(go+76,owner);
        m.write32(vt+64+28,noop);m.write32(vt+64+36,noop);
        m.write32(vt+128+20,0x0c05bee0);m.write32(vt+128+28,0x0c05c040);
        m.write32(owner+1600,auxList);m.write32(auxList,auxSentinel);m.write32(auxList+4,aux);
        m.write32(auxList+32,vt+192);m.write32(vt+192+52,remove);
        m.write32(aux+4,auxSentinel);m.write32(aux+12,vt);m.write32(vt+20,0x0c05ae20);
        m.write32(aux+20,240);m.write32(aux+24,owner);
        m.write32(owner+1640,mode);m.write32(owner+1048,playerCar);m.write32(owner+1052,rivalCar);
        m.write32(playerCar+2396,player);m.write32(rivalCar+2396,rival);
        OriginalRaceStart native;native.reset(mode);
        for(unsigned frame=1;frame<=260;++frame){
            RefCpu c(m);c.r[15]=stack+0xf000;c.r[12]=owner;c.r[13]=0x0c156700;
            std::vector<unsigned> cues;std::vector<std::string> order;int digit=-1;unsigned service=0;
            c.callHooks[noop]=[](auto&){};
            c.callHooks[remove]=[&](auto& cpu){eq(cpu.r[5],aux,"aux child removed");m.write32(auxList+4,auxSentinel);};
            c.callHooks[0x0c156700]=[](auto&){};
            c.callHooks[0x0c2223b8]=[](auto& cpu){cpu.fpul=std::uint32_t(std::int32_t(cpu.r[4])/std::int32_t(cpu.r[5]));};
            c.callHooks[0x0c068500]=[&](auto& cpu){digit=int(cpu.r[5]);order.push_back("countdown");};
            c.callHooks[0x0c1420c0]=[&](auto& cpu){eq(cpu.r[5],1,"cue argument");cues.push_back(cpu.r[4]);};
            c.callHooks[0x0c16d500]=[&](auto& cpu){eq(cpu.r[4],1,"source service");++service;};
            c.callHooks[0x0c0654e0]=[&](auto&){order.push_back("HUD");};
            for(auto a:{0x0c221fc0u,0x0c05a300u,0x0c05a600u})c.callHooks[a]=[](auto& cpu){cpu.r[0]=0x0d007000;};
            c.callHooks[0x0c16db80]=[](auto& cpu){eq(cpu.r[4],13,"GO service13");};
            c.callHooks[0x0c024100]=[](auto&){}; // scene rendering, explicit boundary
            for(auto a:{0x0c069c40u,0x0c0683c0u,0x0c067000u})c.callHooks[a]=[](auto&){};
            c.callHooks[0x0c16d1e0]=[](auto& cpu){cpu.r[0]=0;};
            for(auto [a,label]:std::vector<std::pair<unsigned,const char*>>{{0x0c0680c0,"path"},{0x0c067a80,"gates"},{0x0c068a60,"outcome"},{0x0c067d00,"timers"}})
                c.callHooks[a]=[&,label](auto&){order.push_back(label);};
            c.callHooks[0x0c06a420]=[](auto& cpu){cpu.setFloat(0,0.f);};
            c.callHooks[0x0c159920]=[&](auto&){order.push_back("solver");eq(bool(m.read32(player+80)&0x8000),frame>=180,"start flag before solver");};
            c.callHooks[0x0c063ce0]=[&](auto&){order.push_back("publish");};
            // The real outer span executes real auxiliary-list iteration,
            //0759C0 phase advance,075D60/023480 dispatch, GO Init/Main and
            //062DE0 numeric-mode selection. No hand-authored scheduler hook.
            instructions+=c.run(0x0c05fe6e,0x0c05ffba,12000);
            const auto f=native.step();eq(f.countdownRemaining,m.read32(aux+20),"remaining");
            eq(std::uint32_t(f.countdownDigit),std::uint32_t(digit),"HUD digit");eq(f.requestService1,service,"service epoch");
            eq(f.go,frame==180,"GO epoch");eq(f.runRules,frame>=180,"rules epoch");eq(f.gearEnabled,frame>=180,"gear epoch");
            std::vector<unsigned> expectedCues;if(f.cue2)expectedCues.push_back(2);if(f.cue3)expectedCues.push_back(3);
            if(cues!=expectedCues)throw std::runtime_error("Source countdown/GO cue order differs");++checks;
            std::vector<std::string> expected;if(frame<240)expected.push_back("countdown");expected.push_back("HUD");
            if(frame>=180)for(const char* label:{"path","gates","outcome","timers"})expected.push_back(label);
            expected.push_back("solver");expected.push_back("publish");
            if(order!=expected){for(const auto& s:order)std::cerr<<s<<' ';throw std::runtime_error("Source owner order differs on frame"+std::to_string(frame));}++checks;
            eq(bool(m.read32(rival+80)&0x8000),mode==0&&frame>=180,"rival start flag");
        }
    }
    // Execute the original HUD scale/fade span with the real filter leaves.
    // No rendering or host timing is substituted for those instructions.
    m.clear();m.zeroRegion(owner,0x1000);m.zeroRegion(stack,0x1000);
    m.writeFloat(owner+0xb8,-59.f);m.writeFloat(owner+0xbc,1.f/61.f);
    m.writeFloat(owner+0xc0,1.f);m.writeFloat(owner+0xc4,1.f);m.write32(owner+0xc8,0);
    for(unsigned tick=1;tick<240;++tick){
        m.write32(owner+0xcc,tick);
        RefCpu c(m);c.r[9]=owner;c.r[14]=stack;c.r[15]=stack+0x800;
        instructions+=c.run(0x0c0c8adc,0x0c0c8b98,500);
        const auto a=originalCountdownPresentation(tick);
        eq(std::bit_cast<std::uint32_t>(a.scale),m.read32(owner+0xd8),"Source countdown scale");
        eq(unsigned(std::lround(a.opacity*255.f)),m.read32(stack+36),"Source countdown fade");
    }
    // Actual sixty-step loop: it neither polls input, changes platform frame,
    // advances rules nor invokes outer sound service. The leaf diagnostic
    // reset159F40 executes too and leaves solver elapsed/RNG untouched.
    m.clear();m.zeroRegion(owner,0x2000);m.zeroRegion(stack-0x1000,0x11000);
    m.write32(stack+196,owner);m.write32(0x0c92de30,87654);m.write16(0x0c92f0e2,0xab00);
    m.write32(0x0c900e84,19);m.write32(0x0c37c778,23);m.write32(owner+1640,2);
    RefCpu c(m);c.r[14]=stack;c.r[15]=stack-0x100;unsigned steps=0,publishes=0;
    c.callHooks[0x0c159920]=[&](auto& cpu){eq(publishes,steps,"warmup alternation");eq(m.read32(0x0c92de30),87654,"frozen platform frame");eq(m.read16(0x0c92f0e2),0xab00,"frozen ADC");eq(cpu.fr[4],0,"solo correction");++steps;};
    c.callHooks[0x0c063ce0]=[&](auto&){++publishes;eq(publishes,steps,"warmup publication");};
    c.callHooks[0x0c057700]=[](auto& cpu){eq(cpu.r[4],2,"warmup completion mode");};
    instructions+=c.run(0x0c0620fa,0x0c062130,6000);
    eq(steps,60,"warmup iterations");eq(publishes,60,"warmup publications");eq(m.read32(owner+1604),1,"initialized latch");
    eq(m.read32(0x0c900e84),19,"diagnostic reset leaves elapsed");eq(m.read32(0x0c37c778),23,"diagnostic reset leaves RNG");
    std::cout<<"PASS "<<checks<<" original outer/countdown/GO/warmup ordering checks, "<<instructions<<" original instructions. Numerical solver and rule leaves are explicit callbacks; their complete countdown sequence has a separate differential test.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
