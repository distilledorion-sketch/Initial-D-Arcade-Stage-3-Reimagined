#include "original_number_plate.h"
#include "car_presentation.h"
#include "original_rival_appearance_catalog.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <random>
using namespace idas3;
using namespace idas3::original;
using namespace idas3::reference;
int main(int argc,char**argv){try{
    if(argc!=3)throw std::runtime_error("project-root canonical-image required");const std::filesystem::path root=argv[1];RefMemory m(argv[2]);
    constexpr unsigned profile=0x0c31c99c,race=0x0d000000,car=0x0d010000,frame=0x0d020000,stop=0x00ff0000;
    std::uint64_t checks=0,instructions=0,cases=0;
    const auto equal=[&](unsigned a,unsigned b,const char*why){++checks;if(a!=b)throw std::runtime_error(std::string(why)+" source="+hex(a)+" native="+hex(b));};
    const auto verify=[&](const OriginalBattleProfile&p){
        m.clear();m.zeroRegion(race,0x40000);m.zeroRegion(profile,sizeof(p.words));for(unsigned i=0;i<p.words.size();++i)m.write32(profile+i*4,p.words[i]);
        m.write32(race+1048,car);m.write32(frame+40,stop);
        RefCpu c(m);c.r[12]=profile+64;c.r[13]=race;c.r[14]=frame;c.r[15]=frame;
        // The compiler unsigned divide ABI returns FPUL and preserves all
        // source integer operands. No plate/name logic is replaced by a hook.
        c.callHooks[0x0c2223b8]=[](RefCpu&v){if(!v.r[5])throw std::runtime_error("Source divide zero");v.fpul=v.r[4]/v.r[5];};
        instructions+=c.run(0x0c0631a8,stop,10000);const auto native=OriginalNumberPlate::playerDigits(p);
        for(unsigned n=0;n<5;++n){equal(m.read8(car+1716+n),native[n],"Player plate byte");equal(m.read32(frame+20+n*4),std::int32_t(n)<signed32(p.u(76))?p.u(44+n*4):0,"Source name adapter");}
        for(unsigned n=0;n<p.words.size();++n)equal(m.read32(profile+n*4),p.words[n],"Profile must remain unchanged");++cases;
    };
    auto p=makeOriginalFreshBattleProfile();verify(p);for(unsigned n=0;n<5;++n)equal(OriginalNumberPlate::playerDigits(p)[n],OriginalNumberPlate::freshDigits()[n],"Unnamed22936");
    // Every glyph in every significant position and every length boundary.
    for(unsigned position=0;position<5;++position)for(unsigned glyph=0;glyph<=220;++glyph)for(unsigned length=0;length<=6;++length){p=makeOriginalFreshBattleProfile();p.setu(76,length);p.setu(12,0xffffffff);for(unsigned n=0;n<5;++n)p.setu(44+n*4,n==position?glyph:220);verify(p);}
    std::mt19937 random(0x0631a8);for(unsigned n=0;n<256;++n){p=makeOriginalFreshBattleProfile();p.setu(76,n%7==0?0xffffffff:n%8);for(unsigned i=0;i<5;++i)p.setu(44+i*4,random());verify(p);}
    p=makeOriginalFreshBattleProfile();p.setu(76,4);for(unsigned n=0;n<4;++n)p.setu(44+n*4,std::array<unsigned,4>{180,166,168,162}[n]);verify(p);
    const auto named=OriginalNumberPlate::playerDigits(p);unsigned assemblyWords=0;
    for(unsigned carId=0;carId<35;++carId){auto plate=OriginalNumberPlate::load(root,carId);const auto old=plate.assembly().instances;plate.setPlayerProfile(p);
        for(unsigned side=0;side<2;++side){equal(plate.assembly().instances[side*6].chunk,10,"Plate backing model");for(unsigned n=0;n<5;++n)equal(plate.assembly().instances[side*6+n+1].chunk,named[n],"Bound profile glyph model");}
        for(unsigned i=0;i<old.size();++i)for(unsigned n=0;n<16;++n){equal(std::bit_cast<unsigned>(plate.assembly().instances[i].transform[n]),std::bit_cast<unsigned>(old[i].transform[n]),"Digit binding preserves placement");++assemblyWords;}
        auto namedProfile=p;namedProfile.setu(16,carId);auto presentation=CarPresentation::loadPlayerProfile(root,namedProfile);
        for(bool lights:{false,true}){CarWheelPose wheels;wheels.steeringRadians=.21f;wheels.rotationRadians={1,2,3,4};presentation.pose(wheels,lights,true);const auto& rendered=presentation.profilePlateAssembly();equal(unsigned(rendered.instances.size()),12,"Actual player plate draw count");for(unsigned side=0;side<2;++side){equal(rendered.instances[side*6].chunk,10,"Actual player backing model");for(unsigned n=0;n<5;++n)equal(rendered.instances[side*6+n+1].chunk,named[n],"Actual player profile digits");}}
        plate.setPlayerProfile(makeOriginalFreshBattleProfile());for(unsigned side=0;side<2;++side)for(unsigned n=0;n<5;++n)equal(plate.assembly().instances[side*6+n+1].chunk,OriginalNumberPlate::freshDigits()[n],"Restore unnamed appearance");
    }
    for(unsigned enemy=0;enemy<originalRivalAppearances.size();++enemy){const auto&a=originalRivalAppearances[enemy];auto rival=OriginalNumberPlate::loadRival(root,a.car,enemy);for(unsigned side=0;side<2;++side)for(unsigned n=0;n<5;++n)equal(rival.assembly().instances[side*6+n+1].chunk,a.digits[n],"Authored rival digits unchanged");}
    std::cout<<"PASS "<<checks<<" comparisons / "<<cases<<" actual-player source cases / "<<instructions<<" bounded instructions / "<<assemblyWords<<" unchanged placement words /35 player and31 rival assemblies; SEGA=";for(auto d:named)std::cout<<unsigned(d);std::cout<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
