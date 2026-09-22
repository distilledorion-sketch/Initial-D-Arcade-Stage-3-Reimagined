#include "car_presentation.h"
#include "car_catalog.h"
#include "original_rival_appearance_catalog.h"
#include <iostream>
using namespace idas3;
int main(int argc,char** argv)try{
    const auto root=std::filesystem::path(argc>1?argv[1]:".");unsigned cases=0;
    const auto check=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    const auto verify=[&](CarPresentation& car,NativeModel& model){
        car.applyMaterials(model);
        for(bool lights:{false,true}){
            car.raceReflections(false);const auto base=car.pose({},lights,false);
            car.raceReflections(true);const auto reflection=car.pose({},lights,false);
            check(reflection.instances.size()>base.instances.size(),"Missing authored reflection geometry");
            unsigned env=0;
            for(const auto& instance:reflection.instances){check(instance.chunk<model.chunks.size(),"Reflection chunk outside car bank");
                for(const auto& batch:model.chunks[instance.chunk].batches)env+=(batch.material[2]&(1u<<11))!=0;}
            check(env>0,"Reflection geometry lacks generated environment coordinates");
            car.raceReflections(false);check(car.pose({},lights,false).instances.size()==base.instances.size(),"Race reflections leak into menu");
            const auto wheels=car.wheelOrigins();check(wheels[0].x>wheels[1].x&&wheels[2].x>wheels[3].x&&wheels[0].z>wheels[2].z,"Invalid tire footprint");++cases;
        }
    };
    for(unsigned id=0;id<35;++id){const auto folder=std::string(originalCarFolders[id]);
        auto model=NativeModel::load(root/"data/original_models"/folder/(folder+".idasmesh"));
        auto profile=original::makeOriginalFreshBattleProfile();profile.setu(16,id);
        auto car=CarPresentation::loadPlayerProfile(root,profile);try{verify(car,model);}catch(...){std::cerr<<"player "<<id<<": ";throw;}
    }
    for(unsigned enemy=0;enemy<31;++enemy){const auto id=originalRivalAppearances[enemy].car;const auto folder=std::string(originalCarFolders[id]);
        auto model=NativeModel::load(root/"data/original_models"/folder/(folder+".idasmesh"));
        auto car=CarPresentation::loadRival(root,id,enemy,model.chunks.size());try{verify(car,model);}catch(...){std::cerr<<"enemy "<<enemy<<": ";throw;}
    }
    std::cout<<"PASS "<<cases<<" race reflection/footprint cases across 35 player cars and 31 original rivals, lights off/on and menu restoration.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
