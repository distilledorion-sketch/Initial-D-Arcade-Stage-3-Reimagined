#include "original_number_plate.h"
#include "original_car_dimensions.h"
#include "car_catalog.h"
#include "original_rival_appearance_catalog.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3;
using namespace idas3::reference;
int main(int argc,char**argv)try{
    if(argc!=4)throw std::runtime_error("project-root original-image HOSTFS required");
    const std::filesystem::path root=argv[1],hostfs=argv[3];RefMemory m(argv[2]);
    constexpr unsigned object=0x0d000000,parts=0x0d010000,stack=0x0d0f0000,frame=0x0d0e0000,stop=0x00ff0000;
    std::vector<unsigned> fsca(32768);std::ifstream table(root/"data/original_physics/fsca_table.bin",std::ios::binary);table.seekg(16);table.read(reinterpret_cast<char*>(fsca.data()),131072);
    std::size_t comparisons=0,instructions=0;
    auto check=[&](bool v,const char*why){if(!v)throw std::runtime_error(why);};
    // Full fresh profile establishes name-length76==0. The original caller
    // supplies five zeros to057280 in this case, not five default-space220s.
    m.zeroRegion(stack-0x2000,0x2000);RefCpu fresh(m);fresh.r[15]=stack;fresh.pr=stop;
    instructions+=fresh.run(0xc134a60,stop,100000);check(m.read32(0xc31c99c+76)==0,"Fresh profile name length");
    m.zeroRegion(parts,0x400);fresh.r[4]=parts;fresh.r[5]=parts+64;fresh.r[15]=stack;fresh.pr=stop;
    // Compiler unsigned-division service is an explicit bounded boundary;
    // every input here is positive and below100000, with positive divisor.
    fresh.callHooks[0xc2223b8]=[](auto&c){if(!c.r[5])throw std::runtime_error("Plate divisor zero");c.fpul=c.r[4]/c.r[5];};
    instructions+=fresh.run(0xc057280,stop,10000);
    const auto digits=OriginalNumberPlate::freshDigits();for(unsigned i=0;i<5;++i)check(m.read32(parts+64+i*4)==digits[i],"Fresh plate conversion");
    for(unsigned appearance=0;appearance<66;++appearance){
        const bool rival=appearance>=35;const unsigned enemy=rival?appearance-35:0;
        const unsigned car=rival?originalRivalAppearances[enemy].car:appearance;
        auto plate=rival?OriginalNumberPlate::loadRival(root,car,enemy):OriginalNumberPlate::load(root,car);check(plate.assembly().instances.size()==12,"Plate draw count");
        check(std::bit_cast<unsigned>(originalCarRideHeight(car))==m.read32(0xc2f4ed8+car*4),"Ride height source word");
        m.clear();m.zeroRegion(object,0x100000);m.zeroRegion(0xc98ad0c,12);m.zeroRegion(0xce00000,0x10000);
        m.write16(0xc98ad0e,32);m.write32(0xc98ad10,0xce00000);m.write32(0xc98ad14,0xce00000);
        std::ifstream source(hostfs/"parts"/(std::string(originalCarFolders[car])+".bin"),std::ios::binary);
        std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(source),{}};check(bytes.size()==828,"Original parts size");
        for(unsigned i=0;i<bytes.size();++i)m.write8(parts+i,bytes[i]);
        const auto appearancePath=rival?root/"data/original_models/rivals"/("enemy_"+std::string(enemy<10?"0":"")+std::to_string(enemy)):root/"data/original_models"/originalCarFolders[car]/"fresh_player";
        std::ifstream placement(appearancePath/"plate.bin",std::ios::binary);placement.seekg(16);unsigned config;placement.read(reinterpret_cast<char*>(&config),4);
        m.write32(object+0x2f0,parts);m.write32(object+0x2d4,config);m.write32(frame+36,object+0x2bc);m.write32(object+0x330,0x0d050000);
        const auto selectedDigits=rival?originalRivalAppearances[enemy].digits:digits;
        for(unsigned i=0;i<5;++i){m.write8(object+0x6b4+i,selectedDigits[i]);if(rival)check(selectedDigits[i]==m.read32(0xc2f48cc+enemy*20+i*4),"Original enemy digits");}
        RefCpu cpu(m);cpu.r[13]=object;cpu.r[14]=frame;cpu.r[15]=stack;cpu.fscaHalfWave=fsca;
        for(unsigned i=0;i<16;++i)cpu.xf[i]=std::bit_cast<unsigned>(i%5==0?1.f:0.f);
        unsigned draws=0;
        cpu.callHooks[0xc05a8e0]=[](auto&c){c.r[0]=c.r[5];}; // original model chunk lookup only
        cpu.callHooks[0xc1d7120]=[&](auto&c){
            check(draws<12,"Unexpected source plate draw");const auto& expected=plate.assembly().instances[draws++];
            check(expected.chunk==c.r[4],"Original plate glyph mapping");
            for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col){++comparisons;
                if(std::bit_cast<unsigned>(expected.transform[row*4+col])!=c.xf[col*4+row])throw std::runtime_error("Plate matrix mismatch car="+std::to_string(car)+" draw="+std::to_string(draws-1)+" component="+std::to_string(row*4+col));}
        };
        instructions+=cpu.run(0xc0271cc,0xc0272e8,30000);check(draws==12,"Missing original plate draw");
        check(m.read16(0xc98ad0c)==0,"Unbalanced original plate matrix stack");
    }
    std::cout<<"PASS original number plates:35 fresh players+31 rivals/792 draws, "<<comparisons<<" exact matrix words; fresh22936 and31 original enemy digits; "<<instructions<<" original instructions\n";
    return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
