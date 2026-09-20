#include "original_time_attack_background.h"
#include "sh4_scalar_reference.h"
#include <bit>
#include <iostream>

using namespace idas3::original;
using namespace idas3::reference;
int main(int argc,char** argv)try{
    if(argc!=2)throw std::runtime_error("usage: original_time_attack_background_tests canonical-image");
    RefMemory memory(argv[1]);std::uint64_t checks=0,steps=0;
    const auto equal=[&](unsigned a,unsigned b,const char* what){++checks;if(a!=b)throw std::runtime_error(what);};
    constexpr unsigned owner=0x0cd00000,bank=0x0cd01000,positions=0x0cd02000,wrapper=0x0cd03000,stack=0x0cfff000,stop=0x00ff0000;
    memory.zeroRegion(owner,0x10000);memory.zeroRegion(stack-0x10000,0x11000);
    memory.write32(wrapper+44,bank);
    RefCpu init(memory);init.r[4]=owner;init.r[5]=wrapper;init.r[6]=45;init.r[15]=stack;init.pr=stop;
    steps+=init.run(0x0c112dc0,stop,100);
    equal(memory.read32(owner),bank,"Original background bank");equal(memory.read32(owner+4),45,"Original background tile45");
    equal(memory.read32(owner+8),3,"Original columns");equal(memory.read32(owner+12),2,"Original rows");
    equal(memory.read32(owner+16),0,"Original initial frame");equal(memory.read32(owner+20),0x3c020821,"Original speed");
    memory.write32(bank,0x0c38a8ec);memory.write32(bank+56,positions);
    memory.write32(positions+45*12,0xc04ccccc);memory.write32(positions+45*12+4,0x40199999);memory.write32(positions+45*12+8,0xbe19999a);
    unsigned phase=0;
    for(unsigned tick=0;tick<642;++tick){
        const auto expected=originalTimeAttackBackgroundDraws(phase);unsigned count=0;
        RefCpu cpu(memory);cpu.r[4]=owner;cpu.r[15]=stack;cpu.pr=stop;
        // Only the terminal polygon submission is observed. Original getter,
        // setter, tiling arithmetic, loop/reset and restored position execute.
        cpu.callHooks[0x0c145920]=[&](auto& q){
            if(count>=expected.size())throw std::runtime_error("Original tile count overflow");
            equal(q.r[4],bank,"Original draw bank");equal(q.r[5],45,"Original draw tile");
            const auto p=positions+45*12;const auto& draw=expected[count++];
            equal(std::bit_cast<unsigned>(memory.readFloat(p)-std::bit_cast<float>(0xc04cccccu)),std::bit_cast<unsigned>(draw.position.x),"Original tile x");
            equal(std::bit_cast<unsigned>(memory.readFloat(p+4)-std::bit_cast<float>(0x40199999u)),std::bit_cast<unsigned>(draw.position.y),"Original tile y");
            equal(std::bit_cast<unsigned>(memory.readFloat(p+8)-std::bit_cast<float>(0xbe19999au)),std::bit_cast<unsigned>(draw.position.z),"Original tile z");
        };
        steps+=cpu.run(0x0c112e60,stop,5000);equal(count,12,"Original twelve tile draws");
        phase=advanceOriginalTimeAttackBackground(phase);equal(memory.read32(owner+16),phase,"Original phase and wrap");
        equal(memory.read32(positions+45*12),0xc04ccccc,"Original x restored");
        equal(memory.read32(positions+45*12+4),0x40199999,"Original y restored");
        equal(memory.read32(positions+45*12+8),0xbe19999a,"Original z restored");
    }
    equal(phase,0,"Two complete background cycles");
    std::cout<<"PASS "<<checks<<" comparisons, "<<steps<<" bounded original instructions; 642 frames, two321-frame cycles, 7704 tile submissions. Only145920 polygon submission hooked.\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
