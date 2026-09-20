#include "original_race_rules.h"
#include "sh4_scalar_reference.h"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr std::uint32_t race=0x0CF00000,course=0x0CF02000,path=0x0CF03000,
    points=0x0CF10000,leftPoints=0x0CF20000,rightPoints=0x0CF30000,
    context=0x0CF40000,config=0x0CF41000,car=0x0CF42000,actor=0x0CF44000,
    timerA=0x0CF45000,timerB=0x0CF45100,record=0x0CF46000,
    scratch=0x0CF47000,stack=0x0CFFE000,stop=0x0F000000;
std::size_t comparisons{},instructions{},platformHooks{},dispatchHooks{},integerDivisionHooks{};
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void equal(std::uint32_t a,std::uint32_t b,const char* what){++comparisons;if(a!=b){std::cerr<<what<<": "<<std::hex<<a<<" != "<<b<<std::dec<<'\n';throw std::runtime_error("Original race comparison failed");}}
void seed(RefMemory& m){m.clear();m.zeroRegion(race,0x50000);m.zeroRegion(0x0CFF0000,0x10000);m.write32(context+4,context+0x100);}
void cpuSetup(RefCpu& c){c.r[15]=stack;c.pr=stop;c.callHooks[0x0C221FC0]=[](RefCpu& c){++platformHooks;c.r[0]=context;};c.callHooks[0x0C04FCC0]=[](RefCpu& c){++platformHooks;c.r[0]=config;};c.callHooks[0x0C055D60]=[](RefCpu&){++platformHooks;};}
std::vector<OriginalRacePoint> read(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);std::uint32_t n{},c{};f.read(reinterpret_cast<char*>(&n),4);f.read(reinterpret_cast<char*>(&c),4);check(f&&c==3&&n>1,"Invalid test path");std::vector<OriginalRacePoint> out(n);f.read(reinterpret_cast<char*>(out.data()),n*12);check(bool(f),"Truncated test path");return out;}
void writePoint(RefMemory& m,std::uint32_t a,OriginalRacePoint p){for(std::size_t i=0;i<3;++i)m.writeFloat(a+std::uint32_t(i)*4,p[i]);}
void seedPath(RefMemory& m,const std::vector<OriginalRacePoint>& p,const std::vector<OriginalRacePoint>& l,const std::vector<OriginalRacePoint>& r,bool reverse){
    m.write32(race+1036,course);m.write32(course+24,path);m.write32(path,0x0C386194);m.write32(path+28,reverse);
    for(auto [off,address]:{std::pair{12u,points},std::pair{16u,leftPoints},std::pair{20u,rightPoints}}){m.write32(path+off,path+0x100+off*4);m.write32(path+0x100+off*4,std::uint32_t(p.size()));m.write32(path+0x104+off*4,address);}
    for(std::size_t i=0;i<p.size();++i){writePoint(m,points+std::uint32_t(i)*12,p[i]);writePoint(m,leftPoints+std::uint32_t(i)*12,l[i]);writePoint(m,rightPoints+std::uint32_t(i)*12,r[i]);}
}
void tables(RefMemory& m){
    for(std::uint32_t i=0;i<30;++i){const auto& r=originalRaceRuleRow(i);for(auto [entry,expected]:{std::pair{0x0C191B00u,r.startIndex},std::pair{0x0C191B20u,r.goalIndex}}){RefCpu c(m);cpuSetup(c);c.r[4]=i;instructions+=c.run(entry,stop,200);equal(c.r[0],std::uint32_t(expected),"original start/goal table");}
        for(std::uint32_t j=0;j<8;++j)for(auto entry:{0x0C191B40u,0x0C191B80u,0x0C191BC0u}){RefCpu c(m);cpuSetup(c);c.r[4]=i;c.r[5]=j;instructions+=c.run(entry,stop,200);const auto expected=entry==0x0C191BC0u?r.sectionIndices[j>3?0:j]:(entry==0x0C191B40u?r.lapIndices[j>5?0:j]:r.extensionIndices[j>5?0:j]);equal(c.r[0],std::uint32_t(expected),"original event table");}
    }
    for(std::uint32_t condition=0;condition<20;++condition)for(std::uint32_t difficulty=0;difficulty<7;++difficulty){RefCpu c(m);cpuSetup(c);c.r[4]=difficulty;c.r[5]=condition;instructions+=c.run(0x0C191E80,stop,200);equal(c.r[0],std::uint32_t(originalTimeAttackInitialSeconds(condition,difficulty)),"original initial time table");}
    for(std::uint32_t condition=0;condition<20;++condition){RefCpu c(m);cpuSetup(c);c.r[4]=condition;instructions+=c.run(0x0C192040,stop,200);for(std::uint32_t i=0;i<6;++i)equal(m.read32(c.r[0]+i*4),std::uint32_t(originalTimeAttackBonusSeconds(condition)[i]),"original bonus table");}
}
void timersAndRecords(RefMemory& m){
    std::mt19937 random(0x600028);
    for(std::uint32_t i=0;i<1000;++i){seed(m);OriginalRaceTimer timer{std::int32_t(i%3==0?-100:i%3==1?100:0),i<12?(i%2?0x7fffffceu:0x80000032u):random(),random()};
        m.write32(race+1180,timerB);m.write32(timerB+4,std::uint32_t(timer.step));m.write32(timerB+8,timer.value);m.write32(timerB+12,timer.status);
        RefCpu c(m);cpuSetup(c);c.r[9]=race;instructions+=c.run(0x0C068028,0x0C06806A,100);advanceOriginalRaceTimer(timer);equal(timer.value,m.read32(timerB+8),"elapsed timer value");equal(timer.status,m.read32(timerB+12),"elapsed timer overflow flags");
        OriginalRaceTimeRecord times{};times.lapCount=i%5;times.sectionCount=(i/5)%5;for(std::uint32_t j=0;j<4;++j){times.lapTimes[j]=random();times.sectionTimes[j]=random();m.write32(record+j*4,times.lapTimes[j]);m.write32(record+20+j*4,times.sectionTimes[j]);}m.write32(record+16,times.lapCount);m.write32(record+36,times.sectionCount);
        auto time=random();c.r[4]=record;c.r[5]=time;c.pr=stop;instructions+=c.run(0x0C09B5A0,stop,200);equal(c.r[0],appendOriginalLapTime(times,time),"lap append result");
        time=random();c.r[4]=record;c.r[5]=time;c.pr=stop;instructions+=c.run(0x0C09B600,stop,200);equal(c.r[0],appendOriginalSectionTime(times,time),"section append result");
        time=random();c.r[4]=record;c.r[5]=time;c.pr=stop;instructions+=c.run(0x0C09B680,stop,100);recordOriginalFinishTime(times,time);
        for(std::uint32_t j=0;j<4;++j){equal(times.lapTimes[j],m.read32(record+j*4),"lap timestamp");equal(times.sectionTimes[j],m.read32(record+20+j*4),"section timestamp");}equal(times.lapCount,m.read32(record+16),"lap count");equal(times.sectionCount,m.read32(record+36),"section count");equal(times.finishTime,m.read32(record+40),"finish timestamp");
    }
}
void triggerDispatch(RefMemory& m){
    for(std::uint32_t row=0;row<27;++row)for(std::uint32_t counter=0;counter<4;++counter){
        const auto& data=originalRaceRuleRow(row);
        for(const auto target:{data.sectionIndices[counter],data.lapIndices[counter],data.extensionIndices[counter],data.goalIndex})for(int delta:{-1,0,1}){
            seed(m);const auto progress=target+delta;m.write32(race+1460,std::uint32_t(progress));m.write32(race+1564,row);m.write32(race+1640,2);m.write32(race+1552,counter);m.write32(race+1556,counter);m.write32(race+1560,counter);
            RefCpu c(m);cpuSetup(c);std::vector<std::uint32_t> observed;
            for(auto entry:{0x0C0688A0u,0x0C068620u,0x0C068680u,0x0C068520u})c.callHooks[entry]=[&,entry](RefCpu&){++dispatchHooks;observed.push_back(entry);};
            c.r[4]=race;instructions+=c.run(0x0C067A80,stop,5000);
            const auto expected=originalRaceTriggers(data,progress,counter,counter,counter);std::vector<std::uint32_t> calls;
            if(expected.section)calls.push_back(0x0C0688A0);if(expected.lap)calls.push_back(0x0C068620);if(expected.timeExtension)calls.push_back(0x0C068680);if(expected.finished)calls.push_back(0x0C068520);
            ++comparisons;check(calls==observed,"Original per-frame checkpoint/finish dispatch differs");
        }
    }
}
void progressWrap(RefMemory& m){
    std::mt19937 random(0x61460);
    for(const auto period:{1068,1400,4088})for(std::uint32_t i=0;i<400;++i){
        seed(m);m.write32(race+1036,course);m.write32(course+24,path);m.write32(path,0x0C386194);m.write32(path+12,path+0x100);m.write32(path+0x100,std::uint32_t(period+1));
        const OriginalPathCoordinate previous{std::int32_t(random()%period),.25f},current{std::int32_t(random()%period),float(i%100)/100.0f};OriginalPathCoordinate accumulated{std::int32_t(i)*9,0};
        m.write32(scratch,std::uint32_t(previous.index));m.writeFloat(scratch+4,previous.fraction);m.write32(scratch+16,std::uint32_t(current.index));m.writeFloat(scratch+20,current.fraction);m.write32(scratch+32,std::uint32_t(accumulated.index));
        RefCpu c(m);cpuSetup(c);c.r[4]=race;c.r[5]=scratch+32;c.r[6]=scratch;c.r[7]=scratch+16;
        // This independent wrap test has one explicit arithmetic boundary:
        // the C-runtime helper enters FPSCR.PR=1, outside this scalar oracle.
        // All operands here are positive exact integers below2*period.
        c.callHooks[0x0C2223B8]=[](RefCpu& c){++integerDivisionHooks;c.fpul=std::uint32_t(std::bit_cast<std::int32_t>(c.r[4])/std::bit_cast<std::int32_t>(c.r[5]));};
        instructions+=c.run(0x0C061460,stop,2000);advanceOriginalRaceProgress(accumulated,previous,current,period);equal(std::uint32_t(accumulated.index),m.read32(scratch+32),"original circuit unwrapped progress");equal(std::bit_cast<std::uint32_t>(accumulated.fraction),m.read32(scratch+36),"original progress fraction copy");
    }
}
void crossing(RefMemory& m,const std::filesystem::path& root){
    const auto p=read(root/"data/courses/k_df_path.bin"),l=read(root/"data/courses/k_df_path_l.bin"),r=read(root/"data/courses/k_df_path_r.bin");
    std::mt19937 random(0x61780);std::uniform_real_distribution<float> distance(-2,2);
    for(std::uint32_t condition:{6u,7u}){
        seed(m);seedPath(m,p,l,r,(condition&1)!=0);OriginalRaceRules rules;rules.reset(root,{condition,2,0},{0,0},{0,0,0});
        for(std::uint32_t j=0;j<400;++j){const auto relative=j%4==3?rules.rules().goalIndex:rules.rules().sectionIndices[j%4];const auto gate=rules.gate(relative);OriginalRacePoint a=gate.center,b=gate.center;for(std::size_t k=0;k<3;++k){a[k]+=distance(random);b[k]+=distance(random);}if(j%11==0)a=b=gate.center;
            const auto previous=60000u+j*100,current=previous+100;const auto active=j&1;
            m.write32(race+1408,active);writePoint(m,race+1508+(active?0:16),a);writePoint(m,race+1508+active*16,b);m.write32(race+1540+(active?0:4),previous);m.write32(race+1540+active*4,current);
            RefCpu c(m);cpuSetup(c);c.r[4]=race;c.r[5]=rules.rules().startIndex+relative;
            instructions+=c.run(0x0C061780,stop,20000);equal(c.r[0],originalRaceCrossingTime(gate,a,b,previous,current),"full original crossing timestamp");
        }
    }
}
void timeUpBoundary(RefMemory& m){
    for(const auto remaining:{100u,1u,0u,0xffffffffu,0xffff8ad0u})for(bool stopped:{false,true}){
        seed(m);m.write32(race+1640,2);m.write32(race+1176,timerA);m.write32(race+1180,timerB);m.write32(timerA+4,std::uint32_t(-100));m.write32(timerA+8,remaining);m.write32(timerB+4,100);m.write32(timerB+8,30000);m.write32(race+1048,car);m.write32(car+2396,actor);m.write32(actor+80,stopped?0x4000:0);
        RefCpu c(m);cpuSetup(c);c.callHooks[0x0C1420C0]=[](RefCpu&){++platformHooks;};c.r[4]=race;
        instructions+=c.run(0x0C067D00,stop,10000);
        equal(m.read32(timerA+8),std::bit_cast<std::int32_t>(remaining)>0?remaining-100:remaining,"deadline intentionally freezes at zero");
        const bool expired=std::bit_cast<std::int32_t>(remaining)<=0;
        equal(m.read32(race+1608),expired&&(stopped||std::bit_cast<std::int32_t>(remaining)<=-30000),"original timeup waits for stopped actor or negative fallback");
        equal((m.read32(actor+80)>>13)&1,expired,"original automatic brake flag");equal(m.read32(timerB+8),30100,"elapsed continues through deadline transition");
    }
}
void integrated(const std::filesystem::path& root){
    constexpr std::array<const char*,9> folders{"k_ez","s_nm","h_hd","k_df","s_vh","s_uh","n_sy","k_tu","k_df"};
    std::size_t frames=0;
    for(std::uint32_t condition=0;condition<18;++condition){auto p=read(root/"data/courses"/(std::string(folders[condition/2])+"_path.bin"));if(condition&1)std::reverse(p.begin(),p.end());
        OriginalRaceRules rules;const auto& row=originalRaceRuleRow(originalRaceRuleRowIndex(condition,2));const auto period=std::int32_t(p.size()-1);
        const auto start=(row.startIndex+period-2)%period;rules.reset(root,{condition,2,0},{start,0},p[start]);rules.start();
        std::uint32_t sections=0,laps=0,extensions=0;std::int32_t index=start;bool done=false;
        for(std::size_t i=0;i<15000;++i){const auto events=rules.tick({index,0},p[index]);++frames;sections+=events.section;laps+=events.lap;extensions+=events.timeExtension;if(events.finished){done=true;break;}check(!events.timeUp,"Unexpected timeup on controlled authoring path traversal");index=(index+1)%period;}
        check(done,"Original race did not naturally finish");equal(rules.state().progress.index,row.goalIndex,"authored finish threshold");equal(sections,std::uint32_t(std::count_if(row.sectionIndices.begin(),row.sectionIndices.end(),[](int x){return x>=0;})),"all authored sections");equal(laps,std::uint32_t(std::count_if(row.lapIndices.begin(),row.lapIndices.end(),[](int x){return x>=0;})),"all authored lap splits");equal(extensions,std::uint32_t(std::count_if(row.extensionIndices.begin(),row.extensionIndices.end(),[](int x){return x>=0;})),"all authored extensions");check(rules.displayedElapsed()>100000,"Race finished prematurely");
        const auto time=rules.displayedElapsed();check(!rules.tick({index,0},p[index]).finished,"Finish event repeated");equal(time,rules.displayedElapsed(),"finish freezes result");
        if(condition<2)equal(rules.state().progress.index,3u*std::uint32_t(period),"Myogi three-lap finish");if(condition>=2&&condition<4)equal(rules.state().progress.index,2u*std::uint32_t(period),"Usui two-lap finish");
    }
    std::cout<<"Controlled native authoring-path traversal: "<<frames<<" frames across18 conditions; not a vehicle/projection fidelity claim.\n";
}
void integratedBunta(const std::filesystem::path& root){
    constexpr std::array<const char*,9> folders{"k_ez","s_nm","h_hd","k_df","s_vh","s_uh","n_sy","k_tu","k_df"};
    std::size_t frames=0;
    for(unsigned condition=0;condition<18;++condition)for(unsigned weather:{0u,1u}){
        auto p=read(root/"data/courses"/(std::string(folders[condition/2])+"_path.bin"));if(condition&1)std::reverse(p.begin(),p.end());
        const auto& row=originalRaceRuleRow(originalRaceRuleRowIndex(condition,0));const auto period=std::int32_t(p.size()-1);
        const auto start=(row.startIndex+period-2)%period;
        OriginalRaceRules bunta,ta;bunta.resetBunta(root,{condition,2,weather},{start,0},p[start]);ta.reset(root,{condition,2,weather},{start,0},p[start]);
        equal(bunta.state().remaining.value,ta.state().remaining.value,"Bunta selects TA initial clock");
        bunta.start();ta.start();bool finished=false;
        for(unsigned tick=0;tick<15000;++tick){
            const int i=(start+int(tick))%period,j=(i+period-3)%period;
            const auto battle=bunta.tickBattle({i,0},p[i],{j,0});const auto solo=ta.tick({i,0},p[i]);++frames;
            equal(bunta.state().remaining.value,ta.state().remaining.value,"Bunta checkpoint clock uses profile2 table");
            equal(bunta.state().elapsed.value,ta.state().elapsed.value,"Bunta common elapsed timer");
            equal(battle.secondsAdded,solo.secondsAdded,"Bunta profile2 checkpoint extension");
            if(battle.finished){finished=true;equal(bunta.state().battleResult.outcomeCode,0,"Bunta uses actual player/rival progress for win");break;}
        }
        check(finished,"Bunta controlled traversal did not finish");
        // Switching back to solo clears the battle mode; repeated resets must
        // not retain either the Legend timer selector or Bunta rival state.
        bunta.reset(root,{condition,2,weather},{start,0},p[start]);bunta.start();bunta.tick({start,0},p[start]);
    }
    std::cout<<"Bunta native rule integration: "<<frames<<" controlled path frames across18 conditions and dry/wet; separate original-byte tests prove launch/timer selection.\n";
}
}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::invalid_argument("Usage: original_race_rules_tests canonical_program native_project_root");RefMemory memory{std::filesystem::path(argv[1])};
    seed(memory);tables(memory);timersAndRecords(memory);triggerDispatch(memory);progressWrap(memory);crossing(memory,argv[2]);timeUpBoundary(memory);integrated(argv[2]);integratedBunta(argv[2]);
    std::cout<<"PASS original race rules: "<<comparisons<<" comparisons, "<<instructions<<" actual instructions; "<<platformHooks<<" TLS/config/diagnostic/audio boundary calls and "<<dispatchHooks<<" explicit handler captures. No table, crossing-math, timer or result-record hooks.\n";
    std::cout<<"Progress-wrap test only: "<<integerDivisionHooks<<" explicit bounded signed-division helper hooks; PR1 arithmetic helper remains outside oracle coverage.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
