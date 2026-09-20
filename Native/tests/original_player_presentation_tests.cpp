#include "car_presentation.h"
#include "car_catalog.h"
#include "original_car_color_catalog.h"
#include "original_number_plate.h"
#include "original_tuning.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace idas3;
using namespace idas3::original;
namespace {
std::size_t comparisons=0,draws=0;
void compare(const NativeAssembly& actual,const NativeAssembly& expected,const NativeModel* model=nullptr,const NativeModel* captured=nullptr){
    if(actual.instances.size()!=expected.instances.size())throw std::runtime_error("Captured assembly count: native="+std::to_string(actual.instances.size())+" captured="+std::to_string(expected.instances.size()));
    for(unsigned index=0;index<actual.instances.size();++index){const auto& a=actual.instances[index];const auto& b=expected.instances[index];++draws;
        if(a.chunk!=b.chunk)throw std::runtime_error("Captured chunk order at draw "+std::to_string(index)+": native="+std::to_string(a.chunk)+" captured="+std::to_string(b.chunk));
        for(unsigned word=0;word<16;++word){++comparisons;
            if(!std::isfinite(a.transform[word])||std::abs(a.transform[word]-b.transform[word])>0.000001f)
                throw std::runtime_error("Captured transform at draw "+std::to_string(index)+" word "+std::to_string(word)+": native="+std::to_string(a.transform[word])+" captured="+std::to_string(b.transform[word]));}
        if(model){const auto& ac=model->chunks.at(a.chunk);const auto& bc=captured->chunks.at(b.chunk);
            if(ac.batches.size()!=bc.batches.size())throw std::runtime_error("Captured batch count");
            for(unsigned n=0;n<ac.batches.size();++n){comparisons+=24;
                if(ac.batches[n].material!=bc.batches[n].material||ac.batches[n].ich!=bc.batches[n].ich)throw std::runtime_error("Captured drawn material at chunk "+std::to_string(a.chunk));}
        }
    }
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("game-root required");const std::filesystem::path root=argv[1];
    const auto tuning=OriginalTuningData::load(root);
    unsigned appearances=0,frames=0,tuned=0;
    for(unsigned car=0;car<35;++car){
        const auto folder=root/"data/original_models"/originalCarFolders[car];
        const auto authored=NativeModel::load(folder/(std::string(originalCarFolders[car])+".idasmesh"));
        auto fresh=makeOriginalFreshBattleProfile();fresh.setu(16,car);
        for(unsigned color=0;color<originalCarColorCounts[car];++color)try{
            auto profile=fresh;profile.setu(64,color);
            auto native=CarPresentation::loadPlayerProfile(root,profile);
            auto legacy=CarPresentation::load(root,car,authored.chunks.size(),color);
            auto model=authored,captured=authored;native.applyMaterials(model);legacy.applyMaterials(captured);
            const auto plate=OriginalNumberPlate::load(root,car,color);
            if(!native.usesPlayerProfile())throw std::runtime_error("Saved profile consumer inactive");
            for(unsigned state=0;state<4;++state){const bool lights=bool(state&2),braking=bool(state&1);
                native.resetHeadlights();legacy.resetHeadlights();
                for(unsigned tick=0;tick<44;++tick){
                    const bool on=tick<22?lights:!lights;
                    native.advanceOriginalFrame(on);legacy.advanceOriginalFrame(on);
                    CarWheelPose wheels;wheels.steeringRadians=std::sin(float(tick)*.5f)*.4f;
                    wheels.suspensionY={.013f,-.024f,.009f,-.007f};
                    wheels.rotationRadians={float(tick)*.18f,-float(tick)*.12f,float(tick)*.37f,-float(tick)*.21f};
                    const auto assembly=native.pose(wheels,on,braking);
                    compare(assembly,legacy.pose(wheels,on,braking),&model,&captured);
                    compare(native.profilePlateAssembly(),plate.assembly());
                    const auto before=native.headlightState();
                    compare(native.pose(wheels,on,braking),assembly);
                    const auto after=native.headlightState();
                    if(before.counter!=after.counter||before.phase!=after.phase||before.fraction!=after.fraction||before.visible!=after.visible)throw std::runtime_error("Host render advanced popup state");
                    ++frames;
                }
            }
            ++appearances;
        }catch(const std::exception& e){throw std::runtime_error("car "+std::to_string(car)+" color "+std::to_string(color)+": "+e.what());}
        const auto checkTuned=[&](const OriginalBattleProfile& profile){
            auto native=CarPresentation::loadPlayerProfile(root,profile);auto model=authored;native.applyMaterials(model);
            for(bool lights:{false,true}){native.resetHeadlights();const auto& assembly=native.pose({},lights,true);
                if(assembly.instances.empty()||native.profilePlateAssembly().instances.size()!=12)throw std::runtime_error("Tuned body/plate missing");
                for(const auto& instance:assembly.instances){
                    if(instance.chunk>=model.chunks.size())throw std::runtime_error("Tuned chunk outside private model");
                    for(float value:instance.transform)if(!std::isfinite(value))throw std::runtime_error("Tuned transform non-finite");
                }
            }
            ++tuned;
        };
        for(unsigned package=0;package<tuning.car(car).packages.size();++package){auto p=fresh;p.setByte(152,std::uint8_t(package));p.setByte(153,0);p.setu(1180,0);
            for(unsigned visit=0;!(p.u(1180)&0x400);++visit){
                if(visit>=tuning.car(car).packages[package].steps.size())throw std::runtime_error("Tuning package did not end");
                applyOriginalTuningCommand(p,tuning,1);checkTuned(p);
            }
        }
        for(unsigned choice=0;choice<tuning.car(car).optional.size();++choice){auto p=fresh;p.setByte(154,std::uint8_t(choice));p.setu(72,1000000);applyOriginalTuningCommand(p,tuning,3);checkTuned(p);}
        std::cout<<"car "<<car<<": factory captures and saved parts passed"<<std::endl;
    }
    std::cout<<"PASS "<<appearances<<" factory appearances, "<<frames<<" dynamic frames, "<<draws<<" compared body/plate draws, "<<comparisons<<" comparisons, "<<tuned<<" authored tuned profiles. Repeated rendering preserves popup state.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
