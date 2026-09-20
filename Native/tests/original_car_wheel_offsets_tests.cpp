#include "original_car_wheel_offsets.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3::original;
using namespace idas3::reference;
int main(int argc,char**argv)try{
    if(argc!=2)throw std::invalid_argument("canonical-image required");
    RefMemory m(argv[1]);constexpr unsigned object=0xd000000,stack=0xd010000,stop=0x00ff0000;
    m.zeroRegion(object,0x2000);m.zeroRegion(stack,0x10000);
    RefCpu cpu(m);cpu.r[4]=1;cpu.r[5]=0xffff;cpu.r[15]=stack+0xf000;cpu.pr=stop;
    std::size_t instructions=cpu.run(0xc191460,stop,1000),cases=0,checks=0;
    // Full field combinations, with unused bits both clear and set. The
    // native table is independently checked by original initializer/lookups.
    for(unsigned car=0;car<35;++car)for(unsigned front=0;front<8;++front)for(unsigned middle=0;middle<8;++middle)for(unsigned rear=0;rear<8;++rear)for(bool rest:{false,true}){
        OriginalCarAppearanceConfig config(car);config.word=front|(middle<<10)|(rear<<13)|(rest?~0xfc07u:0u);
        m.write32(object+0x34c,car);m.write32(object+0x2d4,config.word);m.write32(object+0x318,0xdeadbeef);m.write32(object+0x31c,0xaabbccdd);
        cpu.r[13]=object;cpu.r[15]=stack+0xf000;cpu.pr=stop;instructions+=cpu.run(0xc029ad0,0xc029b66,1000);
        const auto out=originalCarWheelOffsets(config);
        if(m.read32(object+0x318)!=std::bit_cast<unsigned>(out.front)||m.read32(object+0x31c)!=std::bit_cast<unsigned>(out.rear))throw std::runtime_error("Wheel-fit mismatch car="+std::to_string(car)+" config="+hex(config.word));
        checks+=2;++cases;
    }
    std::cout<<"PASS "<<cases<<" wheel-fit configurations, "<<checks<<" exact F32 comparisons, "<<instructions<<" original instructions; no call hooks. All35 cars, original static initialization, front/rear and special shared selectors.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
