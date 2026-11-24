#include "original_host_input.h"
#include "sh4_scalar_reference.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace idas3::original;
using namespace idas3::reference;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::runtime_error("Usage: original_shift_input_reference_tests canonical_image");
    RefMemory memory{std::filesystem::path(argv[1])};
    std::uint64_t instructions=0,checks=0,polls=0;
    // Execute the complete original edge publisher. Only the hardware/JVS
    // arrival boundary is hooked; its returned bytes are explicit fixtures.
    // This proves one acquisition's contract, not its whole-game scheduling.
    for(unsigned previous=0;previous<256;++previous)for(unsigned current=0;current<256;++current){
        memory.clear();memory.zeroRegion(0x0C92ECA0,0x480);memory.zeroRegion(0x0CAE83D0,0x160);
        memory.zeroRegion(0x0C98B084,16);memory.zeroRegion(0x0CFFF000,0x1000);
        memory.write32(0x0CAE83D0,2);memory.write32(0x0CAE83D4,2);memory.write32(0x0CAE83D8,3);
        memory.write8(0x0C92ED20,std::uint8_t(previous));memory.write8(0x0CAE83F1,std::uint8_t(current));
        for(unsigned axis=0;axis<3;++axis)memory.write16(0x0CAE8502+2*axis,std::uint16_t(previous*256+current+axis));
        RefCpu cpu(memory);cpu.r[15]=0x0CFFFF00;cpu.pr=0x0F000000;
        cpu.callHooks[0x0C1CED00]=[&](RefCpu&){++polls;};
        instructions+=cpu.run(0x0C1CEE80,0x0F000000,2000);
        OriginalHostInputState state{std::uint8_t(previous&0x30)};
        OriginalHostControls controls;controls.shiftDown=(current&0x10)!=0;controls.shiftUp=(current&0x20)!=0;
        const auto input=adaptOriginalHostInput(state,controls,false,true,0);
        require(input.pressedByte==(memory.read8(0x0C92ED40)&0x30),"Host shift edge disagrees with original acquisition");
        require(memory.read8(0x0C92ED20)==current,"Original current held byte not published");
        require(memory.read8(0x0C92ED30)==previous,"Original previous held byte not retained");
        require(memory.read8(0x0C92ED50)==std::uint8_t(previous&~current),"Original release byte mismatch");
        require(state.previousShiftButtons==(current&0x30),"Host retained the wrong held shift bits");
        checks+=5;
        for(unsigned axis=0;axis<3;++axis){
            require(memory.read16(0x0C92F0E0+2*axis)==std::uint16_t(previous*256+current+axis),"Original raw ADC publisher mismatch");++checks;
        }
    }
    require(polls==65536,"Unexpected original hardware-poll hook count");
    std::cout<<"PASS 65536 full original digital acquisition cases; "<<checks<<" checks, "<<instructions
        <<" original instructions, "<<polls<<" explicit hardware-poll hooks. Held/pressed/released and ADC publication; caller cadence not certified.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
