#include "original_net_password.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <random>
using namespace idas3::original;
using namespace idas3::reference;
int main(int argc,char** argv)try{
    if(argc!=2)throw std::runtime_error("Canonical image required");
    RefMemory m(argv[1]);std::mt19937 rng(345345);unsigned cases=0;
    for(unsigned course=0;course<9;++course)for(unsigned car=0;car<35;++car)for(unsigned flags=0;flags<16;++flags){
        OriginalNetPasswordInput in{course,flags&1,(flags>>1)&1,(flags>>2)&1,100000u+rng()%1700000u,car,(flags>>3)&1};
        for(auto& n:in.name)n=std::uint8_t(rng()%222);in.salt=rng();
        m.clear();m.zeroRegion(0xd000000,0x30000);constexpr unsigned stack=0xd010000,out=0xd020000,stop=0xd020200;
        const unsigned values[]{in.course,in.direction,in.weather,in.night,in.ticks6000,in.car,in.manual,1};
        for(unsigned i=0;i<8;++i)m.write32(stack+i*4,values[i]);
        for(unsigned i=0;i<5;++i)m.write8(stack+32+i,in.name[i]);m.write32(stack+40,in.salt);m.write32(stack+44,out);
        RefCpu cpu(m);cpu.r[15]=stack;cpu.pr=stop;
        cpu.callHooks[0xc2223e0]=[](auto& c){c.fpul=c.r[4]/c.r[5];}; // unsigned integer division helper
        cpu.run(0xc0de100,stop,100000);
        const auto actual=originalNetPassword(in);
        for(unsigned i=0;i<14;++i)if(actual[i]!=m.read32(out+i*4))throw std::runtime_error("Password mismatch case "+std::to_string(cases)+" symbol "+std::to_string(i));
        ++cases;
    }
    std::cout<<"PASS "<<cases<<" passwords / "<<cases*14<<" symbols match original instructions\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
