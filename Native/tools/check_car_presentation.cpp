#include "car_presentation.h"
#include "car_catalog.h"
#include "car_lamp_catalog.h"
#include "original_rival_appearance_catalog.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
int main(int argc,char**argv){try{
    if(argc!=2)throw std::runtime_error("Game root required");
    const std::filesystem::path root(argv[1]);unsigned changed=0;
    for(unsigned appearance=0;appearance<66;++appearance){const bool rival=appearance>=35;const unsigned enemy=rival?appearance-35:0;const unsigned car=rival?idas3::originalRivalAppearances[enemy].car:appearance;const std::string folder(idas3::originalCarFolders[car]);
        const auto model=idas3::NativeModel::load(root/"data/original_models"/folder/(folder+".idasmesh"));
        auto p=rival?idas3::CarPresentation::loadRival(root,car,enemy,model.chunks.size()):idas3::CarPresentation::load(root,car,model.chunks.size());
        const auto before=p.assembly();
        idas3::CarWheelPose pose;pose.steeringRadians=.2f;pose.suspensionY={.04f,.03f,.02f,.01f};pose.rotationRadians={.3f,.6f,.9f,1.2f};
        const auto& after=p.pose(pose);unsigned count=0;
        if(before.instances.size()!=after.instances.size())throw std::runtime_error("Changed instance count");
        for(unsigned i=0;i<after.instances.size();++i){const auto& a=after.instances[i];const auto& b=before.instances[i];
            if(a.chunk!=b.chunk)throw std::runtime_error("Changed selected source mesh");
            bool different=false;for(unsigned j=0;j<16;++j){if(!std::isfinite(a.transform[j]))throw std::runtime_error("Nonfinite wheel transform");different|=std::abs(a.transform[j]-b.transform[j])>1e-5f;}
            count+=different;
        }
        if(count!=p.animatedInstanceCount()||count<4)throw std::runtime_error("Wheel/caliper update coverage");
        const unsigned repairedChunk=car==6?48:car==29?13:car==33?72:car==34?88:0;
        if(repairedChunk&&!rival){unsigned wheels=0;for(const auto& i:after.instances)wheels+=i.chunk==repairedChunk;
            if(wheels!=4||model.chunks.at(repairedChunk).batches.empty())throw std::runtime_error("Missing original stock wheel geometry");}
        changed+=count;
        for(unsigned flags=0;flags<4;++flags){const bool night=(flags&1)!=0,brake=(flags&2)!=0;const auto& a=p.pose(pose,night,brake);
            if(a.instances.size()!=before.instances.size()+unsigned(night)+unsigned(brake))throw std::runtime_error("Original lamp selection count");
            if(p.illuminatedChunks().size()!=unsigned(night)*2+unsigned(brake))throw std::runtime_error("Original lit lamp list");
            const auto& lamps=idas3::originalCarLampChunks[car];unsigned rear=0;
            for(const auto& instance:a.instances)rear+=instance.chunk==unsigned(lamps[night?2:0]);
            if(rear!=1)throw std::runtime_error("Original rear lamp substitution");
            for(auto chunk:p.illuminatedChunks())if(model.chunks.at(chunk).batches.empty())throw std::runtime_error("Empty original illuminated lamp");
        }
    }
    std::cout<<"PASS: 35 fresh-player/31 source-rival appearances, "<<changed<<" live wheel/caliper transforms, 264 day/night/brake lamp states; retained source body instances.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
