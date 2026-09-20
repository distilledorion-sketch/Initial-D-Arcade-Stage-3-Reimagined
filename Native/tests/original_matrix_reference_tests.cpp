#include "original_matrix.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <regex>
using namespace idas3::original;
using namespace idas3::reference;
int main(int argc,char** argv)try{
    if(argc!=4)throw std::invalid_argument("Usage: original_matrix_reference_tests canonical_program primary_fsca_header exported_fsca_table");
    RefMemory memory{std::filesystem::path(argv[1])};
    std::ifstream source(argv[2],std::ios::binary);if(!source)throw std::runtime_error("Primary FSCA table unavailable");
    const std::string text{std::istreambuf_iterator<char>(source),{}};
    const std::regex pattern("0x([0-9A-Fa-f]{8})");std::vector<std::uint32_t> halfWave;
    for(std::sregex_iterator it(text.begin(),text.end(),pattern),end;it!=end;++it)halfWave.push_back(std::stoul((*it)[1].str(),nullptr,16));
    if(halfWave.size()!=32768)throw std::runtime_error("Primary FSCA source has wrong word count");
    const auto table=OriginalFscaTable::load(argv[3]);std::size_t comparisons=0,instructions=0;
    auto equal=[&](std::uint32_t actual,std::uint32_t expected,const char* kind){++comparisons;if(actual!=expected){std::cerr<<kind<<" mismatch expected="<<std::hex<<expected<<" actual="<<actual<<std::dec<<'\n';throw std::runtime_error("Original matrix differential failed");}};
    for(std::uint32_t phase=0;phase<65536;++phase){
        RefCpu cpu(memory);cpu.fscaHalfWave=halfWave;cpu.fpul=phase;cpu.pr=0x0F000000;
        // An actual FSCA at1F67E2; stop before the next instruction.
        instructions+=cpu.run(0x0C1F67E2,0x0C1F67E4,4);
        const auto values=table.sinCos(std::uint16_t(phase));
        equal(std::bit_cast<std::uint32_t>(values[0]),cpu.fr[4],"FSCA sine");equal(std::bit_cast<std::uint32_t>(values[1]),cpu.fr[5],"FSCA cosine");
    }
    std::uint32_t randomState=0xBA927139u;
    auto random=[&](){randomState^=randomState<<13;randomState^=randomState>>17;randomState^=randomState<<5;return randomState;};
    auto number=[&](){return (float(random()%65537)/65536.0f-.5f)*20.0f;};
    for(std::uint32_t sample=0;sample<1800;++sample){
        memory.clear();memory.zeroRegion(0x0CFFF000,0x1000);
        RefCpu cpu(memory);cpu.fscaHalfWave=halfWave;cpu.r[15]=0x0CFFF800;
        OriginalMatrix matrix;for(std::size_t i=0;i<16;++i){matrix.elements[i]=number();cpu.xf[i]=std::bit_cast<std::uint32_t>(matrix.elements[i]);}
        const auto run=[&](std::uint32_t address){cpu.pr=0x0F000000;instructions+=cpu.run(address,0x0F000000,2000);};
        const auto compare=[&](const char* kind){for(std::size_t i=0;i<16;++i)equal(std::bit_cast<std::uint32_t>(matrix.elements[i]),cpu.xf[i],kind);if(cpu.fpscrSz)throw std::runtime_error("Matrix routine failed to restore FPSCR.SZ");};
        if(sample%2==0){run(0x0C1F6AD0);matrix=originalIdentityMatrix();compare("identity");}
        const std::array<float,3> position{number()*100,number()*100,number()*100};
        for(std::size_t i=0;i<3;++i)cpu.setFloat(4+int(i),position[i]);
        run(0x0C1F6AC0);translateOriginalMatrix(matrix,position);compare("translation");
        std::array<float,3> angles{number()*2000,number()*2000,number()*2000};
        if(sample%101==0)angles={0.0f,-0.0f,1.5707963705062866f};
        if(sample%103==0)angles={-1.0e8f,1.0e8f,3.1415927410125732f};
        cpu.setFloat(4,angles[1]);run(0x0C1F6780);rotateOriginalMatrixY(matrix,angles[1],table);compare("Y rotation");
        cpu.setFloat(4,angles[0]);run(0x0C1F6770);rotateOriginalMatrixX(matrix,angles[0],table);compare("X rotation");
        cpu.setFloat(4,angles[2]);run(0x0C1F6790);rotateOriginalMatrixZ(matrix,angles[2],table);compare("Z rotation");
        for(std::uint32_t corner=0;corner<8;++corner){
            const std::array<float,3> point{number(),number(),number()};
            for(std::size_t i=0;i<3;++i)memory.writeFloat(0x0CFD0000+std::uint32_t(i*4),point[i]);
            cpu.r[4]=0x0CFD0000;cpu.r[5]=corner%2==0?0x0CFD0000:0x0CFD0020;
            const auto output=cpu.r[5];run(0x0C1F6260);
            const auto transformed=transformOriginalPoint(matrix,point);
            for(std::size_t i=0;i<3;++i)equal(std::bit_cast<std::uint32_t>(transformed[i]),memory.read32(output+std::uint32_t(i*4)),"point");
        }
        run(0x0C1F6AF0);
        for(unsigned column=0;column<3;++column)for(unsigned row=0;row<3;++row)matrix.elements[column*4+row]=row==column?1.f:0.f;
        compare("view-space billboard rotation reset");
    }
    std::cout<<"PASS all65536 FSCA phases and1800 original matrix sequences (identity/translation/Y-X-Z/8points/billboard), "<<comparisons<<" bit-exact comparisons, "<<instructions<<" actual original instructions, zero hooks. Finite primary Flycast FTRV/FSCA numerical contract; no physical-silicon certification.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
