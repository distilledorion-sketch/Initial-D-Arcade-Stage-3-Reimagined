#include "original_stream_gain.h"
#include "sh4_scalar_reference.h"
#include <fstream>
#include <iostream>
#include <sstream>
using namespace idas3::original;
using namespace idas3::reference;
namespace {std::size_t checks=0,instructions=0;
void eq(std::uint32_t a,std::uint32_t b,const char* what){++checks;if(a!=b)throw std::runtime_error(std::string(what)+": "+hex(a)+" != "+hex(b));}}
int main(int argc,char**argv)try{
    if(argc!=4)throw std::invalid_argument("canonical SH4 image, AICADRV.bin, ARM fixture required");
    std::ifstream driver(argv[2],std::ios::binary);driver.seekg(0x7eb0);std::array<unsigned char,128> bytes{};driver.read(reinterpret_cast<char*>(bytes.data()),bytes.size());if(!driver)throw std::runtime_error("Driver table missing");
    for(unsigned v=0;v<128;++v)eq(originalStreamTotalLevel(v),bytes[v]^255,"exact original stream table");
    std::ifstream fixture(argv[3]);if(!fixture)throw std::runtime_error("ARM fixture missing");unsigned slot,master,active,right,volume,expected,got;std::size_t cases=0;
    while(fixture>>slot>>master>>active>>right>>volume>>expected>>got){eq(originalStreamTotalLevel(volume,master),expected,"full ARM A2 TL");eq(got,active?expected:0x5a,"ARM active-slot gate");++cases;}
    eq(unsigned(cases),24576,"complete ARM fixture");
    eq(originalStreamGainQ15(0),0,"source mute");eq(originalStreamGainQ15(127),31378,"full source stream gain");eq(originalStreamGainQ15(125),31378,"opening explicit125 gain");eq(originalStreamGainQ15(100),17109,"opening descriptor100 gain");
    // SH4 public volume wrapper through queued stream-manager flush and the
    // actual FIFO word packer. Only final FIFO readiness/submission is hooked.
    RefMemory m(argv[1]);constexpr auto owner=0x0d000000u,stack=0x0d010000u,stop=0x0f000000u;
    for(unsigned s=0;s<8;++s)for(unsigned channels:{1u,2u})for(int value:{-1,0,70,100,125,127,128,255}){
        m.clear();m.zeroRegion(stack,0x10000);m.zeroRegion(owner,64);m.zeroRegion(0x0cae85f8,192);m.zeroRegion(0x0c98443c,148*8);
        m.write32(0x0c31de0c,owner);m.write32(owner,s);m.write32(owner+12,70);
        const auto manager=0x0cae85f8+s*24,descriptor=0x0c98443c+s*148;
        m.write32(manager,70);m.write32(manager+16,1);m.write16(descriptor+64,0);m.write16(descriptor+66,channels);m.write16(descriptor+68,s);
        RefCpu c(m);c.r[15]=stack+0xf000;c.r[4]=std::uint32_t(value);c.pr=stop;
        instructions+=c.run(0x0c141e40,stop,300);
        const unsigned queued=value<0?70:unsigned(value)&127;
        eq(m.read32(owner+12),std::uint32_t(value),"public requested volume");eq(m.read32(manager),queued,"queued platform7-bit volume");eq(m.read32(manager+4),1,"queued volume dirty");
        std::vector<unsigned> fifo;c.callHooks[0x0c200b40]=[](auto& cpu){cpu.r[0]=1;};c.callHooks[0x0c1ed9c0]=[&](auto& cpu){fifo.push_back(cpu.r[4]);};
        c.r[15]=stack+0xf000;c.pr=stop;instructions+=c.run(0x0c1cffe0,stop,5000);
        eq(unsigned(fifo.size()),channels,"stereo FIFO submission count");
        for(unsigned side=0;side<channels;++side)eq(fifo[side],0xa2u|(s<<12)|(side<<11)|(queued<<16),"actual FIFO stream volume word");
        eq(m.read32(manager+4),0,"volume dirty consumed");
    }
    std::cout<<"PASS "<<cases<<" full ARM stream fixtures +128 SH4 public-wrapper/queued-flush/stereo-FIFO cases; "<<checks<<" comparisons, "<<instructions<<" SH4 instructions. Final FIFO readiness/submission hooks only. Exact sourceTL plus shared native integer attenuation policy.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
