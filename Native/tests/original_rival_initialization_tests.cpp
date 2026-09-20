#include "original_rival_initialization.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <random>
using namespace idas3::original;
using namespace idas3::reference;
int main(int argc,char**argv){try{
    if(argc!=2)throw std::runtime_error("canonical-image required");RefMemory memory(argv[1]);std::mt19937 random(0x15ae00);
    std::size_t checks=0,steps=0,cases=0;
    auto equal=[&](std::uint32_t got,std::uint32_t expected,const std::string& label){++checks;if(got!=expected)throw std::runtime_error(label+" native="+hex(got)+" original="+hex(expected));};
    for(unsigned i=0;i<32;++i)equal(originalRivalCarIndex(i),memory.read32(0x0c31fe9c+i*4),"car table");
    constexpr unsigned actorBase=0x0c901c6c,publicBase=0x0c90172c,queriesBase=0x0caa9764,stack=0x0cfff000,positionBase=0x0cd00000,anglesBase=0x0cd00100,primaryPath=0x0cd01000,alternatePath=0x0cd02000,stop=0x0f000000;
    for(unsigned condition=0;condition<18;++condition)for(unsigned enemy=0;enemy<32;++enemy)for(unsigned variant=0;variant<6;++variant){
        memory.clear();memory.zeroRegion(0x0c8ff000,0x1c0000);memory.zeroRegion(0x0cff0000,0x10000);memory.zeroRegion(positionBase,0x1000);
        OriginalRivalState rival;OriginalActorState pub;std::array<OriginalCollisionQuery,4> queries;
        for(auto& w:rival.words)w=random();for(auto& w:pub.words)w=random();for(auto& q:queries)for(auto& w:q.words)w=random();
        constexpr std::array<std::int32_t,6> slots{-3,0,1,7,8,12};
        OriginalRivalInitializationInputs input;input.condition0C9015CC=condition;input.enemyId0C9015E0=enemy;input.profileMode0C901648=variant%3;
        input.level0C9015D0=variant==0?0xffffffffu:variant+4;input.actorSlot=slots[variant];input.field8=slots[5-variant];
        input.position={float(condition)*31.125f,-float(enemy)*2.5f,float(variant)*124.75f};input.angles={.05f*variant,-3.0f+.125f*enemy,.03f*condition};
        const auto slot=unsigned(std::clamp(input.actorSlot,0,8)),a=actorBase+slot*716,p=publicBase+slot*168;
        for(unsigned i=0;i<rival.words.size();++i)memory.write32(a+i*4,rival.words[i]);for(unsigned i=0;i<42;++i)memory.write32(p+i*4,pub.words[i]);
        for(unsigned q=0;q<4;++q)for(unsigned i=0;i<16;++i)memory.write32(queriesBase+q*64+i*4,queries[q].words[i]);
        for(unsigned i=0;i<3;++i){memory.writeFloat(positionBase+i*4,input.position[i]);memory.writeFloat(anglesBase+i*4,input.angles[i]);}
        memory.write32(0x0c9015cc,condition);memory.write32(0x0c9015e0,enemy);memory.write32(0x0c901648,input.profileMode0C901648);memory.write32(0x0c9015d0,input.level0C9015D0);
        memory.write32(stack,std::uint32_t(input.actorSlot));memory.write32(stack+4,std::uint32_t(input.field8));
        RefCpu cpu(memory);cpu.r[4]=positionBase;cpu.r[5]=anglesBase;cpu.r[6]=primaryPath;cpu.r[7]=alternatePath;cpu.r[15]=stack;cpu.pr=stop;
        steps+=cpu.run(0x0c15ae00,stop,5000);
        const auto result=initializeOriginalRival(rival,pub,queries,input);
        for(unsigned i=0;i<rival.words.size();++i)equal(rival.words[i],memory.read32(a+i*4),"rival+"+hex(i*4));
        for(unsigned i=0;i<42;++i)equal(pub.words[i],memory.read32(p+i*4),"public+"+hex(i*4));
        for(unsigned q=0;q<4;++q)for(unsigned i=0;i<16;++i)equal(queries[q].words[i],memory.read32(queriesBase+q*64+i*4),"query");
        equal(result.enemyId0C9015E0,memory.read32(0x0c9015e0),"enemy");equal(result.profile0CAA9868,memory.read32(0x0caa9868),"profile");equal(result.level0C9015D0,memory.read32(0x0c9015d0),"level");equal(result.frame0CAA986C,memory.read32(0x0caa986c),"frame");equal(result.alternatePath?alternatePath:primaryPath,memory.read32(0x0c901728),"path");++cases;
    }
    std::cout<<"PASS original rival initialization: "<<cases<<" cases, "<<checks<<" exact comparisons, "<<steps<<" original instructions, zero hooks.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
