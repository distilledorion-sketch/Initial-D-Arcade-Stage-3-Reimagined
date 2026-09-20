#include "original_time_attack_analysis.h"
#include "sh4_scalar_reference.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>

using namespace idas3::original;
using namespace idas3::reference;
namespace {
std::uint64_t checks=0,instructions=0,divisionHooks=0;
constexpr unsigned owner=0x0cd00000,parent=0x0cd30000,stack=0x0cfff000,stop=0x00ff0000,stats=0x0c91fb0c,profile=0x0c31c99c;
void require(bool condition,const char* label){++checks;if(!condition)throw std::runtime_error(label);}
std::string text(const RefMemory& memory,unsigned p){std::string out;for(unsigned i=0;i<512;++i){auto c=memory.read8(p+i);if(!c)return out;out+=char(c);}throw std::runtime_error("Original advice string bound");}
void initialize(RefMemory& m,const OriginalTimeAttackAnalysisInput& in,unsigned seed){
    m.clear();m.zeroRegion(owner,0x40000);m.zeroRegion(0x0cfe0000,0x20000);m.zeroRegion(stats,0x100);
    m.write32(profile+4,in.course);m.write32(profile+16,in.car);m.write32(profile+68,in.manual?1:0);
    m.write32(owner+80,parent);m.write32(parent+92,in.resultStatus);m.write8(owner+400,in.previousBestTicks6000!=0);
    m.write8(owner+0x1af98,in.course<2);m.write32(owner+0x1afa0,in.convertedEventCount);
    m.writeFloat(stats+4,in.acceleratorFraction);m.writeFloat(stats+8,in.brakeFraction);m.writeFloat(stats+60,in.maxSteeringDelta);
    m.write32(stats+16,in.wallCount);m.write32(stats+32,in.ditchCount);m.write32(stats+56,in.maxGearUsed);
    m.write32(stats+72,in.finishTicks6000);m.write32(stats+92,in.previousBestTicks6000);
    for(unsigned i=0;i<4;++i){m.write32(stats+76+i*4,in.currentSections6000[i]);m.write32(stats+96+i*4,in.previousSections6000[i]);}
    m.write32(0x0c37c778,seed);
}
RefCpu cpu(RefMemory& m){RefCpu c(m);c.r[4]=owner;c.r[15]=stack;c.pr=stop;return c;}
unsigned kind(RefMemory& m,const OriginalTimeAttackAnalysisInput& in){auto c=cpu(m);c.r[5]=in.convertedEventCount;instructions+=c.run(0x0c18bb80,stop,100);return m.read32(owner+120);}
void compare(RefMemory& m,const OriginalTimeAttackAnalysisInput& in,unsigned seed,std::set<unsigned>& rows){
    initialize(m,in,seed);
    for(unsigned k=0;k<3;++k)for(unsigned i=0;i<(k==0?8u:3u);++i){
        auto c=cpu(m);instructions+=c.run(originalTimeAttackAdvicePredicateAddress(k,i),stop,160);
        if(bool(c.r[0])!=originalTimeAttackAdvicePredicate(in,k,i))throw std::runtime_error("Predicate mismatch course="+std::to_string(in.course)+" kind="+std::to_string(k)+" index="+std::to_string(i));
        ++checks;
    }
    require(kind(m,in)==originalTimeAttackAnalysisKind(in.course,in.resultStatus,in.convertedEventCount),"Original analysis kind");
    {auto c=cpu(m);instructions+=c.run(0x0c18d0e0,stop,160);require(c.r[0]==originalTimeAttackWorstSection(in.course,in.currentSections6000,in.previousSections6000),"Original worst-section selection");}
    const auto originalKind=m.read32(owner+120);
    auto c=cpu(m);c.callHooks[0x0c221fc0]=[](auto& q){q.r[0]=0;};
    // Compiler PR1 signed division contract, positive divisor3. No classifier,
    // predicates, RNG, selection branches or source text tables are hooked.
    c.callHooks[0x0c2223b8]=[](auto& q){require(q.r[5]==3,"Original praise RNG divisor");q.fpul=unsigned(signed32(q.r[4])/3);++divisionHooks;};
    instructions+=c.run(0x0c18ca60,0x0c18cf5c,3000);
    const auto out=analyzeOriginalTimeAttack(in,seed);
    require(out.kind==originalKind,"Advice retained original kind");
    require(out.adviceIndex==m.read32(owner+0x1af9c),"Original advice row selection");
    require(seed==m.read32(0x0c37c778),"Original praise RNG state/consumption");
    for(unsigned i=0;i<3;++i){require(out.lineAddresses[i]==m.read32(owner+108+i*4),"Original advice text pointer/section replacement");require(out.lines[i]==text(m,out.lineAddresses[i]),"Original advice bytes");}
    {auto timer=cpu(m);instructions+=timer.run(originalKind==2?0x0c192a20:0x0c192a00,stop,20);require(out.countdownTicks==timer.r[0],"Original kind-specific lecture timer");}
    rows.insert(out.kind*100+out.adviceIndex);
}
}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("usage: original_time_attack_analysis_tests canonical-image proof-directory");
    RefMemory m(argv[1]);const std::filesystem::path proof=argv[2];std::filesystem::create_directories(proof);
    unsigned textRows=0;
    for(unsigned k=0;k<3;++k)for(unsigned i=0;i<(k==0?8u:k==1?3u:6u);++i){const auto& row=originalTimeAttackAdviceText(k,i);
        const unsigned expected=(k==0?0x0c338aec:k==1?0x0c338cb0:0x0c338f08)+i*16;
        require(row.rowAddress==expected,"Original text row address");
        for(unsigned j=0;j<4;++j){require(row.addresses[j]==m.read32(expected+j*4),"Original text pointer table");require(row.text[j]==text(m,row.addresses[j]),"Original English advice unchanged");}
        ++textRows;
    }
    std::set<unsigned> rows;unsigned fixtures=0;
    // Cover every car/manual/course combination, including the authored
    // six-speed exclusions for Happogahara/Irohazaka/Shomaru.
    for(unsigned course=0;course<9;++course)for(unsigned car=0;car<35;++car)for(unsigned manual=0;manual<2;++manual){
        OriginalTimeAttackAnalysisInput in;in.course=course;in.car=car;in.manual=manual;in.resultStatus=2;
        in.acceleratorFraction=.95f;in.brakeFraction=.04f;in.maxSteeringDelta=.1f;
        compare(m,in,fixtures+1,rows);++fixtures;
    }
    std::uint32_t random=0x4bb572e1;
    const auto next=[&](){random=random*1664525u+1013904223u;return random;};
    for(unsigned course=0;course<9;++course)for(unsigned i=0;i<180;++i){
        OriginalTimeAttackAnalysisInput in;in.course=course;in.car=next()%35;in.manual=(next()&1)!=0;in.resultStatus=i%3;
        in.convertedEventCount=next()%101;in.wallCount=next()%71;in.ditchCount=next()%4;in.maxGearUsed=next()%2;
        in.acceleratorFraction=float(next()%101)/100.f;in.brakeFraction=float(next()%101)/100.f;in.maxSteeringDelta=float(next()%101)/1000.f;
        in.previousBestTicks6000=i%4?1000000:0;in.finishTicks6000=998000+next()%7000;
        for(unsigned s=0;s<4;++s){in.currentSections6000[s]=200000+next()%10000;in.previousSections6000[s]=200000+next()%10000;}
        compare(m,in,next(),rows);++fixtures;
    }
    // Directed threshold/equality cases: each floating compare keeps the
    // source's strictness, including IEEE non-finite comparison behavior.
    for(unsigned course=0;course<9;++course)for(unsigned boundary=0;boundary<3;++boundary){
        OriginalTimeAttackAnalysisInput in;in.course=course;in.resultStatus=2;in.acceleratorFraction=1;in.maxSteeringDelta=.1f;
        const auto belowAbove=[&](float v){return boundary==0?std::nextafter(v,-std::numeric_limits<float>::infinity()):boundary==1?v:std::nextafter(v,std::numeric_limits<float>::infinity());};
        in.maxSteeringDelta=belowAbove(m.readFloat(0x0c2909b0+course*36));compare(m,in,next(),rows);++fixtures;
        for(unsigned addr:{0x0c2909a4u,0x0c2909a8u}){in.maxSteeringDelta=.1f;in.brakeFraction=belowAbove(m.readFloat(addr+course*36)/100.f);compare(m,in,next(),rows);++fixtures;}
        for(unsigned addr:{0x0c2909b8u,0x0c2909bcu,0x0c2909c0u}){in.wallCount=m.read32(addr+course*36)+boundary-1;in.convertedEventCount=in.wallCount;in.resultStatus=boundary;compare(m,in,next(),rows);++fixtures;}
        in.resultStatus=0;in.convertedEventCount=0;in.finishTicks6000=1000000+2999+boundary;in.previousBestTicks6000=1000000;compare(m,in,next(),rows);++fixtures;
    }
    for(float value:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}){
        OriginalTimeAttackAnalysisInput in;in.resultStatus=2;in.acceleratorFraction=in.brakeFraction=in.maxSteeringDelta=value;compare(m,in,next(),rows);++fixtures;
    }
    // Strict priority and fallback cases cover every reachable authored row.
    for(unsigned course=0;course<9;++course)for(unsigned i=0;i<10;++i){
        OriginalTimeAttackAnalysisInput in;in.course=course;in.resultStatus=i<7?2:0;in.acceleratorFraction=1;in.maxSteeringDelta=.1f;
        in.brakeFraction=.04f;in.convertedEventCount=i>=7?100:0;
        if(i==0){in.car=5;in.manual=true;}if(i==1)in.maxSteeringDelta=0;if(i==2)in.acceleratorFraction=0;
        if(i==3)in.wallCount=100;if(i==4)in.brakeFraction=0;if(i==5)in.brakeFraction=.9f;
        if(i==8)in.ditchCount=1;if(i==9){in.previousBestTicks6000=1000000;in.finishTicks6000=1010000;in.currentSections6000[3]=10000;}
        compare(m,in,next(),rows);++fixtures;
    }
    require(rows.size()==16,"All16 reachable advice rows (one dormant timeout row) covered");
    for(unsigned selector:{9u,100u}){bool rejected=false;try{originalTimeAttackAnalysisKind(selector,0,0);}catch(const std::out_of_range&){rejected=true;}require(rejected,"Invalid course rejected");}
    std::ofstream csv(proof/"advice-coverage.csv");csv<<"kind,advice_index\n";for(auto row:rows)csv<<row/100<<','<<row%100<<'\n';
    std::cout<<"PASS "<<checks<<" Time Attack analysis comparisons, "<<instructions<<" bounded original instructions, "<<fixtures<<" fixtures, "<<textRows<<" authored rows,16 reachable selections;14 source predicates,9 courses,35 cars, IEEE edges, thresholds/ties, exact English/section pointers and RNG. Hooks: scheduler identity, "<<divisionHooks<<" positive signed compiler divisions. No guest game or device execution.\n";
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
