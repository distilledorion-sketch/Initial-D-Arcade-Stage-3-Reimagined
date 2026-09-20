#include "original_legend_progress.h"
#include "sh4_scalar_reference.h"
#include <array>
#include <bit>
#include <iostream>
using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned profile=0x0C31C99C,stack=0x0D100000,stop=0x00FF0000,owner=0x0D000000,race=0x0D001000;
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("canonical-image required");RefMemory memory(argv[1]);std::size_t checks=0,instructions=0,cases=0;
    const auto equal=[&](unsigned a,unsigned b){++checks;if(a!=b)throw std::runtime_error("Original Legend progress mismatch "+hex(a)+" vs "+hex(b));};
    const auto seed=[&](const OriginalBattleProfile& p){memory.clear();memory.zeroRegion(stack,0x10000);for(unsigned i=0;i<p.words.size();++i)memory.write32(profile+i*4,p.words[i]);};
    const auto compare=[&](const OriginalBattleProfile& p){for(unsigned i=0;i<p.words.size();++i)equal(memory.read32(profile+i*4),p.words[i]);};
    for(unsigned enemy=0;enemy<31;++enemy)for(unsigned value=0;value<256;++value)for(bool win:{false,true}){
        OriginalBattleProfile p;for(unsigned i=0;i<p.words.size();++i)p.words[i]=0x819245a3u^(i*0x1872321u)^(enemy*0x55u)^value;
        // Alternate all28 cleared, only current enemy missing, and a distant
        // uncleared rival. Include unrelated flags and nonzero selection data.
        for(unsigned i=0;i<31;++i)p.setByte(116+i,std::uint8_t(0x10|(i&15)));
        if(value%3==1)p.setByte(116+((enemy+7)%28),0x0f);
        p.setByte(116+enemy,std::uint8_t(value));seed(p);
        if(win)recordOriginalLegendWin(p,enemy);else recordOriginalLegendLoss(p,enemy);
        RefCpu cpu(memory);cpu.r[4]=enemy;cpu.r[15]=stack+0xF000;cpu.pr=stop;
        instructions+=cpu.run(win?0x0C1343A0:0x0C134480,stop,5000);compare(p);++cases;
    }
    for(unsigned mode=0;mode<4;++mode)for(unsigned enemy=0;enemy<31;++enemy)for(unsigned finish:{0u,1u,128u,255u})for(unsigned outcome:{0u,1u,0xffffffffu}){
        auto p=makeOriginalFreshBattleProfile();p.setu(0,mode);p.setu(24,enemy);seed(p);
        memory.zeroRegion(owner,0x2000);memory.write32(owner+80,race);memory.write8(race+1572,std::uint8_t(finish));memory.write32(race+1644,outcome);
        const auto result=recordOriginalLegendResult(p,std::uint8_t(finish),outcome);
        const auto expected=mode?OriginalLegendResult::NotLegend:finish&&outcome==0?OriginalLegendResult::Win:OriginalLegendResult::Loss;
        equal(unsigned(result),unsigned(expected));
        RefCpu cpu(memory);cpu.r[14]=stack+0xF000;cpu.r[15]=cpu.r[14];memory.write32(cpu.r[14],owner);cpu.pr=stop;
        instructions+=cpu.run(0x0C05CFAC,0x0C05D026,10000);compare(p);++cases;
    }
    const std::array<float,18> advantages{0.f,-0.f,-500.f,-.001f,.001f,.125f,1.f,12.345f,99.999f,100.f,149.999f,150.f,187.5f,1000.f,1.e20f,
        std::bit_cast<float>(0x7f800000u),std::bit_cast<float>(0xff800000u),std::bit_cast<float>(0x7fc01234u)};
    const std::array<unsigned,6> balances{0,3999,999980000,999999998,999999999,0xfffffff0};
    unsigned pointCase=0;
    for(unsigned enemy=0;enemy<31;++enemy)for(unsigned wins=0;wins<16;++wins)for(unsigned course=0;course<10;++course)for(unsigned status:{0u,1u,2u}){
        auto p=makeOriginalFreshBattleProfile();p.setu(24,enemy);p.setu(4,course);p.setByte(116+enemy,std::uint8_t((wins<<4)|7));
        p.setu(72,balances[pointCase%balances.size()]);const float advantage=advantages[(pointCase/3+status)%advantages.size()];++pointCase;seed(p);
        constexpr unsigned child=owner+0x3000;memory.zeroRegion(owner,0x4000);
        memory.write32(owner+76,race);memory.write32(race+80,status);memory.writeFloat(race+84,advantage);
        RefCpu cpu(memory);cpu.r[14]=stack+0xf000;cpu.r[15]=cpu.r[14];cpu.r[2]=profile;cpu.r[9]=child;cpu.r[10]=1;memory.write32(cpu.r[14]+4,owner+64);
        instructions+=cpu.run(0x0C091714,0x0C0917FA,10000);
        const auto points=calculateOriginalLegendPoints(p,status,advantage);
        const std::array<unsigned,5> expected{points.participation,points.win,points.advantage,points.total,points.balanceBeforeCap};
        for(unsigned i=0;i<expected.size();++i)equal(memory.read32(child+36+i*4),expected[i]);
        // Run the actual common points-screen commit, including its unsigned
        // overflow behavior and cap, after the actual scoring callback.
        cpu.r[14]=stack+0xf000;cpu.r[15]=cpu.r[14];memory.write32(cpu.r[14]+336,owner);memory.write32(owner+252,child);
        instructions+=cpu.run(0x0C06FC48,0x0C06FCA4,1000);awardOriginalLegendPoints(p,status,advantage);compare(p);++cases;
    }
    //16E020 is sometimes mistaken for a Legend points routine. Execute its
    // complete early-return paths to prevent applying Versus-only mutations.
    for(unsigned mode:{0u,1u,2u,4u,0xffffffffu})for(unsigned win:{0u,1u}){
        auto p=makeOriginalFreshBattleProfile();p.setu(0,mode);seed(p);RefCpu cpu(memory);cpu.r[4]=win;cpu.r[15]=stack+0xf000;cpu.pr=stop;
        instructions+=cpu.run(0x0C16E020,stop,1000);compare(p);++cases;
    }
    for(unsigned pattern=0;pattern<1024;++pattern){
        auto p=makeOriginalFreshBattleProfile();for(unsigned course=0;course<9;++course)p.setu(80+course*4,0x12345678+course);
        for(unsigned enemy=0;enemy<31;++enemy){
            const auto cleared=(pattern>>(enemy%10))&1;const auto count=cleared?1+((pattern+enemy)%15):0;
            p.setByte(116+enemy,std::uint8_t((count<<4)|(enemy&15)));
        }
        seed(p);refreshOriginalLegendCourseProgress(p);RefCpu cpu(memory);cpu.r[14]=stack+0xf000;cpu.r[15]=cpu.r[14];cpu.r[1]=0;
        instructions+=cpu.run(0x0C0F1AD8,0x0C0F1B34,20000);compare(p);++cases;
    }
    const std::array<unsigned,12> ranks{0,1,2,3,9,10,15,16,30,31,0x7fffffffu,0xffffffffu};
    for(unsigned pattern=0;pattern<2048;++pattern){
        auto p=makeOriginalFreshBattleProfile();for(unsigned enemy=0;enemy<31;++enemy)p.setByte(116+enemy,std::uint8_t(((pattern>>(enemy%10))&1)?0x10:pattern&1));
        p.setu(148,ranks[(pattern/8)%ranks.size()]);p.setu(476,ranks[(pattern/4)%ranks.size()]);p.setu(496,ranks[(pattern/2)%ranks.size()]);
        p.setu(1180,0x003c0080u|(pattern%4));p.setu(480,0x89213456);
        for(unsigned i=0;i<8;++i)p.setu(1080+i*4,ranks[((pattern/16)+(i==pattern%8))%ranks.size()]);
        seed(p);RefCpu cpu(memory);cpu.r[15]=stack+0xf000;cpu.pr=stop;
        instructions+=cpu.run(0x0C18A940,stop,20000);equal(cpu.r[0],calculateOriginalDriverRank(p));
        cpu.r[14]=stack+0xf000;cpu.r[15]=cpu.r[14];instructions+=cpu.run(0x0C05D026,0x0C05D084,20000);
        updateOriginalPostRaceRank(p);compare(p);++cases;
    }
    auto p=makeOriginalFreshBattleProfile();for(unsigned i=0;i<28;++i)recordOriginalLegendWin(p,i);
    if(!(p.u(1180)&0x08000000)||originalLegendChoices(p,7).count!=5)throw std::runtime_error("Base roster unlock missing");
    recordOriginalLegendWin(p,28);if(originalLegendChoices(p,7).count!=6)throw std::runtime_error("Second Tsuchisaka unlock missing");
    recordOriginalLegendWin(p,29);if(originalLegendChoices(p,3).count!=6)throw std::runtime_error("Bunta Akina rival unlock missing");
    std::cout<<"PASS "<<cases<<" original Legend result/progress cases, "<<checks<<" exact comparisons, "<<instructions<<" original instructions, zero hooks; all1228 profile bytes checked.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}

