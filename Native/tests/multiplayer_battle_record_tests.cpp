#include "multiplayer_battle_record.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3;
using namespace idas3::reference;
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("canonical-image required");
    RefMemory memory(argv[1]);std::size_t checks=0,cases=0,instructions=0;
    constexpr unsigned profile=0x0C31C99C,record=profile+464,stack=0x0D100000,stop=0x00FF0000;
    if(memory.read32(0x0C16E330)!=record)throw std::runtime_error("Source record identity mismatch");
    const auto equal=[&](unsigned a,unsigned b){++checks;if(a!=b)throw std::runtime_error("Battle progression mismatch "+std::to_string(cases)+": "+hex(a)+" vs "+hex(b));};
    for(unsigned level=1;level<=99;++level)for(int difference=-4;difference<=4;++difference)
    for(unsigned experience:{0u,1u,9u,79u,89u,94u,98u,99u})for(bool won:{false,true}){
        const unsigned other=unsigned(std::clamp(int(level)+difference,1,99));
        const Idas3BattleRecord before{400,217,level,unsigned(cases%100)},opponent{99,85,other,3};
        const auto result=advanceBattleRecord(before,won,opponent,experience);
        memory.clear();memory.zeroRegion(stack,0x10000);memory.write32(profile,3);
        const std::array<unsigned,8> input{217,183,level,std::min(level,30u),experience,other,before.streak,before.streak};
        for(unsigned i=0;i<input.size();++i)memory.write32(record+i*4,input[i]);
        RefCpu cpu(memory);cpu.r[4]=won?1:0;cpu.r[15]=stack+0xf000;cpu.pr=stop;
        // Explicit boundaries: network flag writers and debug logger. None
        // supplies arithmetic, comparison, level, experience or streak data.
        cpu.callHooks[0x0C16D440]=[](RefCpu&){};
        cpu.callHooks[0x0C16D500]=[](RefCpu&){};
        cpu.callHooks[0x0C055D60]=[](RefCpu&){};
        instructions+=cpu.run(0x0C16E020,stop,3000);
        equal(result.record.level,memory.read32(record+8));
        equal(result.experience,memory.read32(record+16));
        equal(result.record.streak,memory.read32(record+24));
        equal(result.record.wins,memory.read32(record));
        equal(result.record.battles,memory.read32(record)+memory.read32(record+4));
        ++cases;
    }
    const Idas3BattleRecord fresh{0,0,1,0};auto current=fresh;unsigned experience=0;
    for(unsigned i=0;i<50;++i){auto next=advanceBattleRecord(current,true,current,experience);current=next.record;experience=next.experience;}
    equal(current.level,11);equal(current.wins,50);equal(current.battles,50);
    for(auto invalid:{Idas3BattleRecord{0,1,1,0},Idas3BattleRecord{100,99,100,1},Idas3BattleRecord{100,99,0,1},Idas3BattleRecord{100,99,11,100}}){
        bool rejected=false;try{advanceBattleRecord(invalid,true,fresh,0);}catch(const std::invalid_argument&){rejected=true;}equal(rejected,1);
    }
    std::cout<<"Original versus progression: "<<cases<<" opcode cases, "<<checks<<" checks, "<<instructions
        <<" instructions. Only network-flag/debug-log calls stubbed; full level/experience/streak arithmetic executed.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
