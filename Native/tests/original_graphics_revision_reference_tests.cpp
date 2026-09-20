#include "sh4_scalar_reference.h"
#include <iostream>
#include <stdexcept>
using namespace idas3::reference;
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Canonical 4MiB game image required");
    RefMemory memory(argv[1]);unsigned checks=0;std::size_t instructions=0;
    auto check=[&](bool okay,const char* message){++checks;if(!okay)throw std::runtime_error(message);};
    std::vector<std::uint32_t> revisions;for(unsigned i=0;i<256;++i)revisions.push_back(i);
    for(auto word:{0x10001u,0xffffffffu,0x80000001u,0x10000010u})revisions.push_back(word);
    for(auto revision:revisions){
        memory.clear();
        // These are isolated RAM words standing in for the two original
        // register reads. No hardware/API/device access occurs in this test.
        memory.write32(0xa8800000,0xe1ad0000);memory.write32(0xa8800004,revision);
        memory.write32(0x0c99a168,0x12345678);memory.write32(0x0c99a16c,0xaaaaaaaa);
        memory.write32(0x0c99a170,0xbbbbbbbb);memory.write32(0x0c99a174,0x87654321);
        RefCpu cpu(memory);instructions+=cpu.run(0x0c209b4a,0x0c209b6c,32);
        check(memory.read32(0x0c99a16c)==(revision==1?31u:32u),"Original ELAN revision flags differ");
        check(memory.read32(0x0c99a170)==(revision==1?2u:3u),"Original ELAN command-queue threshold differs");
        check(bool(memory.read32(0x0c99a16c)&8)==(revision==1),"Original direction-width mode differs");
        check(memory.read32(0x0c99a168)==0x12345678&&memory.read32(0x0c99a174)==0x87654321,"Revision selection modified neighboring state");
    }
    std::cout<<"PASS "<<revisions.size()<<" revision inputs / "<<checks<<" checks / "<<instructions<<" actual original instructions. Revision1:flags31,queue2,8-bit light direction; other revisions:flags32,queue3,12-bit direction. No hooks or device access; register values are explicit isolated inputs.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
