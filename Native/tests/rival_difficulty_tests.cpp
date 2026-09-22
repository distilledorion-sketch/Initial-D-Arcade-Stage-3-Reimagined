#include "original_rival.h"
#include <cmath>
#include <iostream>
using namespace idas3::original;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::runtime_error("Pass native project root");
    const auto root=std::filesystem::path(argv[1])/"data/original_rival";
    const auto data=OriginalRivalData::load(root);unsigned checks=0,cases=0;
    for(unsigned condition=0;condition<18;++condition){
        const auto path=data.loadPath(root,condition);
        for(unsigned profile:{0u,13u,26u,31u})for(int gap:{-100,0,100}){
            const unsigned index=path.inclusiveLastIndex/2;
            OriginalRivalState initial;initial.setu(0,1);initial.setu(12,index);
            initial.setf(200,path.points[index][0]);initial.setf(204,path.points[index][1]);initial.setf(208,path.points[index][2]);
            OriginalDriveState player;player.setu(0x118,index+gap);OriginalActorState actor;actor.setu(0x50,0x8000);
            const auto playerBefore=player.words;const auto actorBefore=actor.words;
            float speeds[4]{};
            for(unsigned difficulty=0;difficulty<4;++difficulty){
                auto rival=initial;OriginalActorState pub;unsigned ticks=1000;
                OriginalRivalPaceInputs inputs;inputs.condition0C9015CC=condition;inputs.profile0CAA9868=profile;
                inputs.level0C9015D0=3;inputs.progress0C901604.fill(5);inputs.aiDifficulty=difficulty;
                for(unsigned frame=0;frame<1000;++frame){
                    updateOriginalRivalPace(rival,pub,ticks,data,path,inputs,player,actor);
                    require(std::isfinite(rival.f(68))&&rival.f(68)>=0,"Invalid rival speed");
                    require(player.words==playerBefore&&actor.words==actorBefore,"Difficulty mutated player physics");
                }
                speeds[difficulty]=rival.f(68);
                actor.setu(0x50,0);updateOriginalRivalPace(rival,pub,ticks,data,path,inputs,player,actor);
                require(rival.f(68)==0,"Difficulty bypassed stopped/countdown gate");actor.setu(0x50,0x8000);++checks;
            }
            require(speeds[0]>0,"No pace measured");
            require(speeds[1]>speeds[0]&&speeds[2]>speeds[1],"Difficulty did not increase rival pace");
            require(std::abs(speeds[1]/speeds[0]-1.05f)<.001f&&std::abs(speeds[2]/speeds[0]-1.10f)<.001f,"Unexpected pace multiplier");
            require(speeds[3]==speeds[2],"Difficulty exceeded Expert cap");checks+=4;++cases;
        }
    }
    std::cout<<"PASS rival difficulty: "<<cases<<" route/profile/gap cases, "<<checks<<" checks; Normal/Hard/Expert pace, player isolation and stop gates\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
