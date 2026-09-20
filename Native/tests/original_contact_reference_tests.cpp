#include "original_contact.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3::original;
using namespace idas3::reference;
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Pass canonical original program image");
    RefMemory memory{std::filesystem::path(argv[1])};std::size_t comparisons=0,instructions=0;
    std::uint32_t rng=0xBAD73194u;auto random=[&](){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;};
    auto number=[&](){return float(random()%65537)/65536.0f;};
    for(std::uint32_t sample=0;sample<2400;++sample){
        memory.clear();memory.zeroRegion(0x0C900F00,0x500);memory.zeroRegion(0x0CFC0000,0x100);
        OriginalDriveState d;OriginalActorState a;OriginalWheelHistory history;
        for(auto& word:d.words)word=std::bit_cast<std::uint32_t>(number()*8-4);
        for(auto& word:a.words)word=random();
        for(const auto offset:{0x0,0x4,0x8,0xC,0x10,0x14}){d.setf(offset,number()*5000-2500);a.setf(offset,number()*5000-2500);}
        d.setf(0x238,number()*80);d.setf(0x3DC,number());d.setf(0x3E0,number());d.setf(0x1B8,number());d.setf(0x1C4,number());
        for(auto& counter:history.rotationCounters0CAA94B4)counter=random();
        for(std::size_t i=0;i<d.words.size();++i)memory.write32(0x0C900F00+std::uint32_t(i*4),d.words[i]);
        for(std::size_t i=0;i<a.words.size();++i)memory.write32(0x0CFC0000+std::uint32_t(i*4),a.words[i]);
        for(std::size_t i=0;i<4;++i)memory.write32(0x0CAA94B4+std::uint32_t(i*4),history.rotationCounters0CAA94B4[i]);
        RefCpu cpu(memory);cpu.r[8]=0x0CFC0000;cpu.r[14]=cpu.r[15]=0x0CFFF100;
        // Stop at the actual158200 call target, after its delay-slot actor
        // store. No contact query or math helper is substituted by a hook.
        instructions+=cpu.run(0x0C157AF4,0x0C158200,3000);
        prepareOriginalContactFrame(d,a,history);
        const auto equal=[&](std::uint32_t address,std::uint32_t actual){++comparisons;const auto expected=memory.read32(address);if(expected!=actual){std::cerr<<"Pre-contact mismatch sample="<<sample<<" address="<<std::hex<<address<<" expected="<<expected<<" actual="<<actual<<std::dec<<'\n';throw std::runtime_error("Original pre-contact frame mismatch");}};
        for(std::size_t i=0;i<d.words.size();++i)equal(0x0C900F00+std::uint32_t(i*4),d.words[i]);
        for(std::size_t i=0;i<a.words.size();++i)equal(0x0CFC0000+std::uint32_t(i*4),a.words[i]);
        for(std::size_t i=0;i<4;++i)equal(0x0CAA94B4+std::uint32_t(i*4),history.rotationCounters0CAA94B4[i]);
    }
    std::cout<<"PASS 2,400 original actor/wheel/contact-feedback cases, "<<comparisons<<" bit-exact comparisons, "<<instructions<<" original instructions; zero hooks. Boundary stops before158200 contact query.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
