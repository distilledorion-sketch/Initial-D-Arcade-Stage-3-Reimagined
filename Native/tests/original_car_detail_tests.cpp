#include "car_presentation.h"
#include "car_catalog.h"
#include "original_car_color_catalog.h"
#include "original_rival_appearance_catalog.h"
#include "sh4_scalar_reference.h"
#include <iostream>

using namespace idas3;using namespace idas3::reference;
int main(int argc,char** argv)try{
    if(argc!=3)throw std::invalid_argument("game-root canonical-image required");
    const std::filesystem::path root=argv[1];RefMemory m(argv[2]);
    constexpr unsigned object=0x0d000000,table=object+0x4000,stack=0x0d100000,stop=0x00ff0000;
    m.zeroRegion(object,0x10000);m.zeroRegion(stack,0x10000);
    std::size_t instructions=0,checks=0,slotCases=0,tailSlots=0,appearances=0,states=0;
    const auto require=[&](bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);};
    const auto handle=[](unsigned chunk){return 0x0d800000+chunk*256;};
    for(unsigned car=0;car<35;++car){const std::string folder(originalCarFolders[car]);
        const auto model=NativeModel::load(root/"data/original_models"/folder/(folder+".idasmesh"));
        const unsigned map=m.read32(0x0c33b250+car*4);m.write32(object+4,table);m.write32(object+0x354,map);
        for(unsigned chunk=0;chunk<model.chunks.size();++chunk)m.write32(table+chunk*4,handle(chunk));
        for(unsigned slot=0;slot<212;++slot){m.write32(object+0x358+slot*4,slot);
            const auto chunk=std::int32_t(m.read32(map+slot*4));
            if(slot>=140&&slot<=186)m.write32(object+0x898+(slot-140)*4,chunk<0?0:handle(unsigned(chunk)));
        }
        for(unsigned slot=0;slot<212;++slot)for(unsigned method:{0x0c025fe0u,0x0c026040u}){
            unsigned draws=0,drawHandle=0;RefCpu c(m);c.r[4]=object;c.r[5]=slot;c.r[15]=stack+0xf000;c.pr=stop;
            const auto submit=[&](auto& cpu){++draws;drawHandle=cpu.r[4];};c.callHooks[0x0c1d7900]=submit;c.callHooks[0x0c1d7120]=submit;
            instructions+=c.run(method,stop,1000);const auto chunk=std::int32_t(m.read32(map+slot*4));
            require(draws==unsigned(chunk>=0),"Original semantic slot draw presence");
            if(chunk>=0){require(std::size_t(chunk)<model.chunks.size(),"Source slot references absent model chunk");require(drawHandle==handle(unsigned(chunk)),"Captured slot differs from original026100 resolver");if(slot>=187)++tailSlots;}
            ++slotCases;
        }
    }
    for(unsigned entry=0;entry<66;++entry){const bool rival=entry>=35;const unsigned enemy=rival?entry-35:0,car=rival?originalRivalAppearances[enemy].car:entry;
        const std::string folder(originalCarFolders[car]);const auto base=root/"data/original_models"/folder;
        const auto model=NativeModel::load(base/(folder+".idasmesh"));
        for(unsigned color=0;color<(rival?1:original::originalCarColorCounts[car]);++color){
            const auto path=rival?root/"data/original_models/rivals_v2"/("enemy_"+std::string(enemy<10?"0":"")+std::to_string(enemy)):base/"appearance_v2"/("color_0"+std::to_string(color));
            auto presentation=rival?CarPresentation::loadRival(root,car,enemy,model.chunks.size()):CarPresentation::load(root,car,model.chunks.size(),color);
            for(unsigned state=0;state<4;++state){presentation.resetHeadlights();const auto expected=NativeAssembly::load(path/("state_"+std::to_string(state)+".idasasm"),model.chunks.size());
                const auto actual=presentation.pose({},bool(state&2),bool(state&1));
                require(actual.instances.size()==expected.instances.size(),"Restored source lighting draw count");
                for(unsigned i=0;i<actual.instances.size();++i){
                    if(actual.instances[i].chunk!=expected.instances[i].chunk){std::cerr<<"car "<<car<<" enemy "<<enemy<<" color "<<color<<" state "<<state<<" draw "<<i<<'\n';throw std::runtime_error("Restored source lighting draw order/chunk");}++checks;
                    for(unsigned w=0;w<16;++w)require(std::abs(actual.instances[i].transform[w]-expected.instances[i].transform[w])<=0.000002f,"Restored source lighting matrix (captured float helper boundary)");
                }
                const auto repeated=presentation.pose({},bool(state&2),bool(state&1));
                require(repeated.instances.size()==actual.instances.size(),"Repeated pose duplicates restored parts");
                for(unsigned i=0;i<actual.instances.size();++i)require(repeated.instances[i].chunk==actual.instances[i].chunk&&repeated.instances[i].transform==actual.instances[i].transform,"Repeated restored pose changes source assembly");
                ++states;
            }++appearances;
        }
    }
    require(appearances==212&&states==848&&tailSlots>0,"Restored detail coverage incomplete");
    std::cout<<"PASS "<<slotCases<<" actual original slot resolver cases ("<<tailSlots<<" valid tail-slot submissions), "<<appearances<<" car appearances, "<<states<<" complete source lighting-state orders, "<<checks<<" checks, "<<instructions<<" original resolver instructions. Only final graphics submission hooked; captured matrix inputs reconstructed within2e-6.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
