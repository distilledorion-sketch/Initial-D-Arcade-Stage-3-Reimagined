#include "original_car_visibility.h"
#include "sh4_scalar_reference.h"
#include <iostream>

using namespace idas3::original;
using namespace idas3::reference;
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("canonical-image required");
    RefMemory m(argv[1]);constexpr unsigned object=0x0d000000,stack=0x0d100000,stop=0x00ff0000;
    m.zeroRegion(object,0x2000);m.zeroRegion(stack,0x10000);
    std::size_t cases=0,checks=0,instructions=0;unsigned car=0,word=0;int enemy=-1;
    const auto equal=[&](unsigned a,unsigned b,const char* label){++checks;if(a!=b)throw std::runtime_error(std::string(label)+" car="+std::to_string(car)+" config="+hex(word)+" enemy="+std::to_string(enemy)+" source="+hex(a)+" native="+hex(b));};
    const auto run=[&](){
        m.write32(object+0x2d4,word);m.write32(object+0x34c,car);m.write32(object+0x350,unsigned(enemy));
        m.write32(object+0x6c4,0xfeedface);m.write32(object+0x6c8,0xaabbccdd);
        m.write8(0x0c31c99c+164,std::uint8_t(cases));
        RefCpu c(m);c.r[4]=object;c.r[15]=stack+0xf000;c.pr=stop;
        instructions+=c.run(0x0c029040,0x0c02988e,10000);
        OriginalCarAppearanceConfig config(car);config.word=word;const auto native=originalCarVisibility(config,enemy);
        for(unsigned i=0;i<212;++i){try{equal(m.read32(object+0x358+i*4),native.slots[i],"Semantic slot");}catch(...){std::cerr<<"slot "<<i<<'\n';throw;}}
        equal(m.read32(object+0x6c4),native.popupMotorEnabled?1:0,"Popup motor");
        equal(m.read32(object+0x6c8),0xaabbccdd,"Preserved popup counter");
        equal(m.read32(object+0x2d4),word,"Preserved config");
        c.r[4]=car;c.r[15]=stack+0xf000;c.pr=stop;instructions+=c.run(0x0c191440,stop);
        equal(c.r[0],native.maximumPopupPhase,"Popup phase table");++cases;
    };
    // Every combination of the fields read by each source car branch. Other
    // bits are tested both clear and set, including bit31 and wheel variants.
    constexpr std::array<unsigned,35> masks{0x0e000387,0x4e000007,0x1e000000,0,0x40000000,7,7,0,0,0x0e00fc07,0x30000007,7,0,0x10070000,7,7,7,7,0x4038e000,0,0x0e000000,0,0x40000007,0,0xe000,0x40000380,0x40000000,7,7,0,0x40000000,0x10000000,0,0x0e00e387,0};
    for(car=0;car<35;++car){
        const auto mask=masks[car];std::vector<unsigned> bits;for(unsigned b=0;b<32;++b)if(mask&(1u<<b))bits.push_back(b);
        std::vector<int> enemies{-1,0};if(car==0){enemies.push_back(13);enemies.push_back(29);}if(car==22){enemies.push_back(7);enemies.push_back(8);enemies.push_back(28);}if(car==24)enemies.push_back(12);
        for(unsigned pattern=0;pattern<(1u<<bits.size());++pattern){
            unsigned fields=0;for(unsigned b=0;b<bits.size();++b)if(pattern&(1u<<b))fields|=1u<<bits[b];
            for(bool remainder:{false,true})for(int id:enemies){enemy=id;word=fields|(remainder?~mask:0);run();}
        }
    }
    // Independent full-word samples across the complete rival ID range.
    unsigned random=0x298320;
    for(car=0;car<35;++car)for(enemy=-1;enemy<31;++enemy)for(unsigned sample=0;sample<8;++sample){random^=random<<13;random^=random>>17;random^=random<<5;word=random;run();}
    std::cout<<"PASS "<<cases<<" original car visibility cases, "<<checks<<" exact comparisons, "<<instructions<<" original instructions, no call hooks. All212slots and popup state; material/draw pipeline separate.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
