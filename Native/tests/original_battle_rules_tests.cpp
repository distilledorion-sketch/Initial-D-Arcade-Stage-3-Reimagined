#include "original_race_rules.h"
#include "original_battle_profile.h"
#include "sh4_scalar_reference.h"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <random>
using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr std::uint32_t race=0x0cf00000,timer=0x0cf02000,scratch=0x0cf03000,stack=0x0cffe000,stop=0x0f000000,profile=0x0c31c99c;
std::size_t comparisons{},instructions{};
void check(bool v,const char* text){if(!v)throw std::runtime_error(text);}
void equal(std::uint32_t a,std::uint32_t b,const char* text){++comparisons;if(a!=b){std::cerr<<text<<": "<<std::hex<<a<<" != "<<b<<std::dec<<'\n';throw std::runtime_error("Original battle rules mismatch");}}
void seed(RefMemory& m){m.clear();m.zeroRegion(race,0x10000);m.zeroRegion(0x0cff0000,0x10000);m.zeroRegion(profile,1228);}
void setup(RefCpu& c){c.r[15]=stack;c.pr=stop;}
std::vector<OriginalRacePoint> read(const std::filesystem::path& path){std::ifstream f(path,std::ios::binary);std::uint32_t n{},k{};f.read(reinterpret_cast<char*>(&n),4);f.read(reinterpret_cast<char*>(&k),4);check(f&&k==3&&n>1,"Invalid test path");std::vector<OriginalRacePoint> v(n);f.read(reinterpret_cast<char*>(v.data()),std::streamsize(n)*12);check(bool(f),"Truncated test path");return v;}
void tables(RefMemory& m){
    for(std::uint32_t enemy=0;enemy<34;++enemy)for(std::uint32_t difficulty=0;difficulty<7;++difficulty){RefCpu c(m);setup(c);c.r[4]=difficulty;c.r[5]=enemy;instructions+=c.run(0x0c191e00,stop,200);equal(c.r[0],std::uint32_t(originalLegendInitialSeconds(enemy,difficulty)),"Legend initial seconds");}
    for(std::uint32_t enemy=0;enemy<34;++enemy){RefCpu c(m);setup(c);c.r[4]=enemy;instructions+=c.run(0x0c192000,stop,200);for(std::uint32_t j=0;j<6;++j)equal(m.read32(c.r[0]+4*j),std::uint32_t(originalLegendBonusSeconds(enemy)[j]),"Legend extension seconds");}
}
void setupSlices(RefMemory& m,const std::filesystem::path& root){
    for(std::uint32_t enemy=0;enemy<31;++enemy)for(std::uint32_t difficulty=0;difficulty<5;++difficulty)for(std::uint32_t flags:{0u,1u,15u,16u,0xffu}){
        seed(m);m.write32(profile+24,enemy);m.write8(profile+116+enemy,std::uint8_t(flags));m.write32(scratch+144,race);m.write32(race+1176,timer);
        RefCpu c(m);setup(c);c.r[14]=scratch;c.r[2]=difficulty;instructions+=c.run(0x0c06749e,0x0c0675a4,1000);
        const auto& rival=originalRival(enemy);OriginalRaceRules rules;rules.resetLegend(root,{rival.course*2+rival.direction,enemy,difficulty,std::uint8_t(flags)},{0,0},{0,0,0});
        equal(m.read32(timer+8),rules.state().remaining.value,"0671A0 Legend timer selection/high-nibble bonus");
        c.r[14]=scratch;c.r[15]=stack;instructions+=c.run(0x0c0675ea,0x0c067686,500);
        for(std::uint32_t j=0;j<6;++j)equal(m.read32(0x0c2f4bd8+4*j),std::uint32_t(originalLegendBonusSeconds(enemy)[j]),"0671A0 six extension copies");
    }
}
void result(RefMemory& m){
    std::mt19937 random(0x068ba0);
    for(std::uint32_t i=0;i<4000;++i){
        seed(m);const auto row=i%27;const auto goal=originalRaceRuleRow(row).goalIndex;
        OriginalPathCoordinate player{std::int32_t(random()%6000)-1000,float(random()%5)*.25f},rival{std::int32_t(random()%6000)-1000,float(random()%5)*.25f};
        if(i%4==0)player.index=rival.index;if(i%7==0)player=rival;if(i%11==0)rival.index=goal;if(i%13==0)rival.index=goal-1;
        if(i%17==0)player.fraction=std::bit_cast<float>(0x7fc00000u);
        m.write32(race+1460,std::uint32_t(player.index));m.writeFloat(race+1464,player.fraction);m.write32(race+1472,std::uint32_t(rival.index));m.writeFloat(race+1476,rival.fraction);m.write32(race+1564,row);m.write32(race+1640,0);
        RefCpu c(m);setup(c);c.r[4]=race;instructions+=c.run(0x0c068ba0,stop,500);equal(c.r[0],originalPlayerAhead(player,rival),"068BA0 signed/fraction strict ordering");
        OriginalBattleResult expected{i%3,(i%6)==0};m.write32(race+1644,expected.outcomeCode);m.write8(race+1573,expected.rivalFinishedLatch);c.r[4]=race;c.pr=stop;
        instructions+=c.run(0x0c068a60,stop,1000);updateOriginalBattleResult(expected,player,rival,goal);
        equal(m.read32(race+1644),expected.outcomeCode,"068A60 local outcome code");equal(m.read8(race+1573),expected.rivalFinishedLatch,"068A60 rival finish latch");
    }
}
void integrated(const std::filesystem::path& root){
    constexpr std::array<const char*,9> folders{"k_ez","s_nm","h_hd","k_df","s_vh","s_uh","n_sy","k_tu","k_df"};std::size_t frames{};
    for(std::uint32_t enemy=0;enemy<31;++enemy)for(bool rivalAhead:{false,true}){
        const auto& rival=originalRival(enemy);const auto condition=rival.course*2+rival.direction;auto p=read(root/"data/courses"/(std::string(folders[rival.course])+"_path.bin"));if(rival.direction)std::reverse(p.begin(),p.end());
        OriginalRaceRules rules;rules.resetLegend(root,{condition,enemy,2,0},{0,0},p[0]);rules.start();const auto period=rules.pathPeriod();const auto start=rules.rules().startIndex;bool completed{},sawLatchedBeforePlayer{};
        auto wrap=[&](int index){index%=period;return index<0?index+period:index;};
        for(int progress=0;progress<=rules.rules().goalIndex+3;++progress){
            const auto index=wrap(start+progress),other=wrap(start+progress+(rivalAhead?2:-2));
            const auto events=rules.tickBattle({index,.25f},p[index],{other,.25f});++frames;
            if(rules.state().battleResult.rivalFinishedLatch&&!events.finished){sawLatchedBeforePlayer=true;check(rules.state().phase==OriginalRacePhase::Running,"Rival finish ended player race early");}
            check(!events.timeUp,"Controlled Legend traversal exhausted original budget");
            if(events.finished){completed=true;break;}
        }
        check(completed,"Legend race did not finish at original goal");equal(rules.state().battleResult.outcomeCode,rivalAhead?1u:0u,"Natural Legend outcome");equal(sawLatchedBeforePlayer,rivalAhead,"Rival finish latch before player goal");
        check(rules.displayedElapsed()>100000,"Legend race finished prematurely");
    }
    std::cout<<"Controlled Legend traversal: "<<frames<<" frames across31 rivals and both outcomes; projection/vehicle motion are caller boundaries.\n";
}
}
int main(int argc,char** argv)try{if(argc!=3)throw std::invalid_argument("Usage: original_battle_rules_tests canonical_program native_project_root");RefMemory memory{std::filesystem::path(argv[1])};seed(memory);tables(memory);setupSlices(memory,argv[2]);result(memory);integrated(argv[2]);std::cout<<"PASS original battle rules: "<<comparisons<<" exact comparisons, "<<instructions<<" actual instructions, zero hooks.\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
