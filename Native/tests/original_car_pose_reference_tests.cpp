#include "original_matrix.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <regex>

// Development-only bounded placement audit. This does not claim that the
// complete dynamic wheel/body assembly has been ported.
using namespace idas3::original;
using namespace idas3::reference;
int main(int argc,char** argv)try{
    if(argc!=4)throw std::invalid_argument("Usage: original_car_pose_reference_tests canonical_program primary_fsca_header exported_fsca_table");
    RefMemory memory{std::filesystem::path(argv[1])};
    std::ifstream source(argv[2],std::ios::binary);if(!source)throw std::runtime_error("Primary FSCA table unavailable");
    const std::string text{std::istreambuf_iterator<char>(source),{}};
    const std::regex pattern("0x([0-9A-Fa-f]{8})");std::vector<std::uint32_t> halfWave;
    for(std::sregex_iterator it(text.begin(),text.end(),pattern),end;it!=end;++it)halfWave.push_back(std::stoul((*it)[1].str(),nullptr,16));
    if(halfWave.size()!=32768)throw std::runtime_error("Primary FSCA source has wrong word count");
    const auto table=OriginalFscaTable::load(argv[3]);std::size_t comparisons=0,instructions=0;
    auto equal=[&](std::uint32_t actual,std::uint32_t expected,const char* kind){++comparisons;if(actual!=expected){std::cerr<<kind<<" expected="<<std::hex<<expected<<" actual="<<actual<<std::dec<<'\n';throw std::runtime_error("Original car pose differential failed");}};
    constexpr std::uint32_t car=0x0CF00000,scene=0x0CF02000,actor=0x0C9008A4,alternate=0x0C4007EC;
    constexpr std::uint32_t stack=0x0CFFF000,matrixStack=0x0CFD0000;
    std::uint32_t randomState=0x58207619u;
    auto random=[&](){randomState^=randomState<<13;randomState^=randomState>>17;randomState^=randomState<<5;return randomState;};
    auto number=[&](){return (float(random()%65537)/65536.0f-.5f)*20.0f;};
    for(std::uint32_t sample=0;sample<600;++sample){
        memory.clear();memory.zeroRegion(stack-0x1000,0x2000);memory.zeroRegion(car,0x3000);
        memory.zeroRegion(actor,168);memory.zeroRegion(alternate,168);memory.zeroRegion(matrixStack,64*32);
        // Constructor033A00 has cleared both pose pointers. The original
        // ordinary-player attachment then reads the active actor identity.
        memory.write32(0x0C900954,actor);memory.write32(scene+1640,sample%3);
        RefCpu attach(memory);attach.r[13]=scene;attach.r[0]=car;attach.r[15]=stack;
        instructions+=attach.run(0x0C062FC8,0x0C062FEE,100);
        equal(memory.read32(scene+1048),car,"scene car pointer");
        equal(memory.read32(car+0x95C),actor,"live actor attachment");
        equal(memory.read32(car+0x960),sample%3==1?alternate:0u,"alternate pose selection");
        std::array<std::array<float,3>,2> positions{},angles{};
        for(std::size_t pose=0;pose<2;++pose){
            positions[pose]={number()*500.0f,number()*100.0f,number()*500.0f};
            angles[pose]={number(),number()*1000.0f,number()};
            if(sample%17==0)angles[pose]={0,0,0};
            const auto address=pose==0?actor:alternate;
            for(std::size_t i=0;i<3;++i){memory.writeFloat(address+std::uint32_t(i*4),positions[pose][i]);memory.writeFloat(address+24+std::uint32_t(i*4),angles[pose][i]);}
        }
        // The original CAMatrix constructor installs this vtable. Its +24
        // method228AA0 returns the embedded matrix address, and is executed.
        memory.write32(car+0x964+64,0x0C380B8C);
        memory.write32(0x0C98AD0C,0x00200000u);memory.write32(0x0C98AD10,matrixStack);memory.write32(0x0C98AD14,matrixStack);
        RefCpu cpu(memory);cpu.fscaHalfWave=halfWave;cpu.r[13]=car;cpu.r[14]=stack;cpu.r[15]=stack;cpu.r[0]=0x12345678;
        // Skip only the unrelated thread-ID getter/prologue. Execute pointer
        // choice, translation, all rotations, matrix copy and stack pop.
        instructions+=cpu.run(0x0C03485C,0x0C0348DC,2000);
        const std::size_t selected=sample%3==1?1:0;
        auto position=positions[selected];position[1]-=std::bit_cast<float>(0x3CA3D70Au);
        auto expected=originalActorMatrix(position,angles[selected],table);
        //0348B4 uses the integer-angle helper with phase8000, exactly pi.
        rotateOriginalMatrixY(expected,std::bit_cast<float>(0x40490FDBu),table);
        for(std::size_t i=0;i<16;++i)equal(memory.read32(car+0x964+std::uint32_t(i*4)),std::bit_cast<std::uint32_t>(expected.elements[i]),"published model matrix");
        equal(memory.read32(0x0C98AD0C),0x00200000u,"matrix stack depth restored");
        equal(memory.read32(0x0C98AD14),matrixStack,"matrix stack pointer restored");
        for(std::size_t i=0;i<3;++i)equal(memory.read32((selected==0?actor:alternate)+std::uint32_t(i*4)),std::bit_cast<std::uint32_t>(positions[selected][i]),"source pose retained");
    }
    std::cout<<"PASS 600 original actor attachments and bounded car placement sequences, "<<comparisons<<" bit-exact comparisons, "<<instructions<<" actual original instructions, zero hooks. Model matrix uses live actorY minus0.02; full dynamic assembly grounding remains separate.\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
