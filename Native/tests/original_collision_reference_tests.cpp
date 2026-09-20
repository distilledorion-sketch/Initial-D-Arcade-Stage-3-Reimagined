#include "original_collision.h"
#include "sh4_scalar_reference.h"
#include <fstream>
#include <iostream>
using namespace idas3::original;
using namespace idas3::reference;
static std::vector<std::uint8_t> read(const std::filesystem::path& path){std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Original collision reference file unavailable");return {std::istreambuf_iterator<char>(f),{}};}
static std::uint32_t word(const std::vector<std::uint8_t>& b,std::size_t o){return std::uint32_t(b.at(o))|(std::uint32_t(b.at(o+1))<<8)|(std::uint32_t(b.at(o+2))<<16)|(std::uint32_t(b.at(o+3))<<24);}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::invalid_argument("Usage: original_collision_reference_tests canonical_program HOSTFS");
    RefMemory memory{std::filesystem::path(argv[1])};const std::filesystem::path hostfs=argv[2];
    std::size_t comparisons=0,instructions=0,locatedCount=0,missCount=0,coefficientCases=0,sweptHits=0,sweptMisses=0,queryCases=0;
    constexpr std::uint32_t dataBase=0x0CB00000,queryBase=0x0CFD0000;
    struct Case{const char* file;const char* path;unsigned condition;};
    constexpr std::array<Case,14> cases{{
        {"k_ez_colli","ezi",0},{"s_nm_colli","nmi",2},
        {"h_hd_colli_0","agi",4},{"h_hd_colli_1","ago",5},
        {"k_df_colli_0","dfi",6},{"k_df_colli_1","dfo",7},
        {"s_vh_colli_0","vhi",8},{"s_vh_colli_1","vho",9},
        {"s_uh_colli_0","iri",10},{"s_uh_colli_1","iro",11},
        {"n_sy_colli_0","shi",12},{"n_sy_colli_1","sho",13},
        {"k_tu_colli_1","tui",14},{"k_tu_colli_0","tuo",15}}};
    for(std::uint32_t variant=0;variant<cases.size();++variant){
        memory.clear();memory.zeroRegion(0x0CFFF000,0x1000);
        const auto& test=cases[variant];
        const bool akina=test.condition==6||test.condition==7;
        const auto source=hostfs/"binary"/(std::string(test.file)+".bin.nz");
        const auto collision=OriginalCollisionData::load(source);const auto raw=read(source);
        for(std::size_t i=0;i<raw.size();++i)memory.write8(dataBase+std::uint32_t(i),raw[i]);
        // Exact loader relocation of table offsets. The native loader decodes
        // typed records; only this development oracle has original addresses.
        for(const auto offset:{12u,20u,28u,36u,44u})memory.write32(dataBase+offset,dataBase+word(raw,offset));
        memory.write32(0x0C2EEF6C,dataBase);
        auto compareWord=[&](std::uint32_t actual,std::uint32_t expected){++comparisons;if(actual!=expected)throw std::runtime_error("RCL1 loader source-word mismatch");};
        for(std::size_t i=0;i<collision.materials.size();++i)for(std::size_t j=0;j<9;++j)compareWord(collision.materials[i][j],word(raw,word(raw,12)+i*36+j*4));
        for(std::size_t i=0;i<collision.vertices.size();++i)for(std::size_t j=0;j<8;++j)compareWord(collision.vertices[i][j],word(raw,word(raw,20)+i*32+j*4));
        for(std::size_t i=0;i<collision.triangles.size();++i)for(std::size_t j=0;j<4;++j)compareWord(std::uint16_t(collision.triangles[i][j*2])|(std::uint32_t(std::uint16_t(collision.triangles[i][j*2+1]))<<16),word(raw,word(raw,28)+i*16+j*4));
        for(std::size_t i=0;i<collision.coarseCells.size();++i)for(std::size_t j=0;j<14;++j)compareWord(collision.coarseCells[i][j],word(raw,word(raw,44)+i*56+j*4));
        const auto path=read(hostfs/"binary"/(std::string("PATH_")+test.path+"_0.bin"));
        const auto pathCount=memory.read32(0x0C283E88+test.condition*8)+1u;
        OriginalCollisionQuery q;OriginalTriangleSearchTrace trace;OriginalSurfaceScratch surface;clearOriginalCollisionQuery(q);
        for(std::size_t i=0;i<surface.words.size();++i)memory.write32(0x0C99AA98+std::uint32_t(i*4),surface.words[i]);
        for(std::size_t triangle=0;triangle<collision.triangles.size();triangle+=akina?19:113){
            RefCpu cpu(memory);cpu.r[4]=0x0C99AA98;cpu.r[5]=std::uint32_t(triangle);cpu.r[15]=0x0CFFF800;cpu.pr=0x0F000000;
            instructions+=cpu.run(0x0C023A40,0x0F000000,3000);
            buildOriginalSurfaceCoefficients(collision,std::int32_t(triangle),surface);++coefficientCases;
            for(std::size_t i=0;i<surface.words.size();++i)compareWord(surface.words[i],memory.read32(0x0C99AA98+std::uint32_t(i*4)));
        }
        for(std::uint32_t sample=0;sample<(akina?240u:64u);++sample){
            ++queryCases;
            const auto index=(akina?sample*3:sample*(pathCount/64))%pathCount;
            q.setf(32,std::bit_cast<float>(word(path,index*12))+(sample%5==0?30.0f:0.0f));
            q.setf(36,std::bit_cast<float>(word(path,index*12+4))+(sample%7==0?-10.0f:20.0f));
            q.setf(40,std::bit_cast<float>(word(path,index*12+8))+(sample%11==0?30.0f:0.0f));
            const auto previousIndex=index>0?index-1:0;
            q.setf(44,std::bit_cast<float>(word(path,previousIndex*12)));
            q.setf(48,std::bit_cast<float>(word(path,previousIndex*12+4))+20.0f);
            q.setf(52,std::bit_cast<float>(word(path,previousIndex*12+8)));
            if(sample%13==0)for(const auto offset:{0u,4u,8u})q.setu(44+offset,q.u(32+offset));
            if(sample%17==0)q.setf(44,q.f(44)+200.0f);
            if(sample%4==0){q.setu(56,0xFFFFFFFFu);q.setu(60,0xFFFFFFFFu);}
            for(std::size_t i=0;i<16;++i)memory.write32(queryBase+std::uint32_t(i*4),q.words[i]);
            for(std::size_t i=0;i<100;++i)memory.write32(0x0C99A904+std::uint32_t(i*4),std::uint32_t(trace.indices0C99A904[i]));
            memory.write32(0x0C99AA94,trace.count0C99AA94);
            RefCpu cpu(memory);cpu.r[4]=queryBase;cpu.r[15]=0x0CFFF800;cpu.pr=0x0F000000;
            instructions+=cpu.run(0x0C022B80,0x0F000000,300000);
            const bool found=locateOriginalCollision(collision,q,trace);
            if(found)++locatedCount;else ++missCount;
            const auto equal=[&](std::uint32_t address,std::uint32_t actual){++comparisons;const auto expected=memory.read32(address);if(expected!=actual){std::cerr<<"Collision mismatch variant="<<variant<<" sample="<<sample<<" address="<<std::hex<<address<<" expected="<<expected<<" actual="<<actual<<std::dec<<'\n';throw std::runtime_error("Original collision-location differential failed");}};
            if(cpu.r[0]!=std::uint32_t(found))throw std::runtime_error("Collision-location return mismatch");
            for(std::size_t i=0;i<16;++i)equal(queryBase+std::uint32_t(i*4),q.words[i]);
            for(std::size_t i=0;i<100;++i)equal(0x0C99A904+std::uint32_t(i*4),std::uint32_t(trace.indices0C99A904[i]));
            equal(0x0C99AA94,trace.count0C99AA94);
            RefCpu surfaceCpu(memory);surfaceCpu.r[4]=queryBase;surfaceCpu.r[15]=0x0CFFF800;surfaceCpu.pr=0x0F000000;
            instructions+=surfaceCpu.run(0x0C022CE0,0x0F000000,300000);
            const bool surfaceFound=queryOriginalCollisionSurface(collision,q,trace,surface);
            if(surfaceCpu.r[0]!=std::uint32_t(surfaceFound))throw std::runtime_error("Original full surface query return mismatch");
            for(std::size_t i=0;i<16;++i)equal(queryBase+std::uint32_t(i*4),q.words[i]);
            for(std::size_t i=0;i<100;++i)equal(0x0C99A904+std::uint32_t(i*4),std::uint32_t(trace.indices0C99A904[i]));
            equal(0x0C99AA94,trace.count0C99AA94);
            for(std::size_t i=0;i<surface.words.size();++i)equal(0x0C99AA98+std::uint32_t(i*4),surface.words[i]);
            RefCpu sweptCpu(memory);sweptCpu.r[4]=queryBase;sweptCpu.r[15]=0x0CFFF800;sweptCpu.pr=0x0F000000;
            instructions+=sweptCpu.run(0x0C022D20,0x0F000000,300000);
            const bool sweptFound=queryOriginalCollisionSwept(collision,q,trace,surface);
            if(sweptCpu.r[0]!=std::uint32_t(sweptFound))throw std::runtime_error("Original swept query return mismatch");
            if(sweptFound&&(q.u(28)&0x8000u))++sweptHits;
            if(!sweptFound)++sweptMisses;
            for(std::size_t i=0;i<16;++i)equal(queryBase+std::uint32_t(i*4),q.words[i]);
            for(std::size_t i=0;i<100;++i)equal(0x0C99A904+std::uint32_t(i*4),std::uint32_t(trace.indices0C99A904[i]));
            equal(0x0C99AA94,trace.count0C99AA94);
            for(std::size_t i=0;i<surface.words.size();++i)equal(0x0C99AA98+std::uint32_t(i*4),surface.words[i]);
        }
    }
    if(locatedCount==0||missCount==0)throw std::runtime_error("Collision test did not cover both success and failure");
    if(sweptHits==0||sweptMisses==0)throw std::runtime_error("Swept test lacks hits or misses");
    std::cout<<"PASS all14 course RCL1 loaders, "<<coefficientCases<<" original coefficient cases,"<<queryCases<<" location/surface/swept query triplets, "<<comparisons<<" source/bit-exact comparisons, "<<instructions<<" original instructions, zero hooks; located="<<locatedCount<<", misses="<<missCount<<", sweptHits="<<sweptHits<<", sweptMisses="<<sweptMisses<<".\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
