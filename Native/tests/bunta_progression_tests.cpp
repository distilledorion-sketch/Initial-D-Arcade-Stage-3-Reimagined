#include "local_driver_profiles.h"
#include "original_bunta_setup.h"
#include "original_bunta_results.h"
#include "original_legend_progress.h"
#include <chrono>
#include <iostream>
#include <stdexcept>

using namespace idas3;
using namespace idas3::original;
namespace {
unsigned checks=0,saves=0;
void require(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
OriginalBattleProfile restart(LocalDriverProfiles& store,const std::filesystem::path& directory,
        unsigned car,const OriginalBattleProfile& profile){
    require(store.save(car,profile),"Bunta profile save failed");++saves;
    LocalDriverProfiles restarted(directory);const auto loaded=restarted.load(car);
    require(loaded.origin==LocalDriverProfiles::Origin::Saved,"restart did not load saved profile");
    require(loaded.profile.words==profile.words,"restart changed complete original profile");return loaded.profile;
}
void earnEntry(OriginalBattleProfile& p){
    require(p.u(72)==0,"fresh profile received unearned points");
    applyOriginalAcceptedCardFlag(p);
    require(originalBuntaEligibility(p)==OriginalBuntaEligibility::InsufficientPoints,"accepted card bypassed earned points");
    p.setu(0,0);selectOriginalRival(p,0);
    require(recordOriginalLegendResult(p,1,0)==OriginalLegendResult::Win,"Legend win not recorded");
    updateOriginalPostRaceRank(p);const auto first=awardOriginalLegendPoints(p,0,0.f);
    require(first.total==3000&&p.u(72)==3000,"first original Myogi win points");
    require(originalBuntaEligibility(p)==OriginalBuntaEligibility::InsufficientPoints,"3000 points admitted Bunta");
    require(recordOriginalLegendResult(p,0,1)==OriginalLegendResult::Loss,"Legend loss not recorded");
    updateOriginalPostRaceRank(p);const auto second=awardOriginalLegendPoints(p,1,0.f);
    require(second.total==1000&&p.u(72)==4000,"original participation did not reach4000");
    require(originalBuntaEligibility(p)==OriginalBuntaEligibility::Eligible,"earned4000 accepted-card entry rejected");
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("scratch parent directory required");
    const auto directory=std::filesystem::path(argv[1])/("bunta-progression-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    LocalDriverProfiles store(directory);
    for(unsigned car=0;car<35;++car){
        auto loaded=store.load(car);require(loaded.origin==LocalDriverProfiles::Origin::Fresh,"new car profile not fresh");
        auto p=loaded.profile;require(p.u(16)==car,"fresh selected car mismatch");
        earnEntry(p);p=restart(store,directory,car,p);
        p.setu(0,2);selectOriginalBuntaCourse(p,car%8);
        const unsigned course=p.u(4),levelOffset=1080+course*4;
        const auto setup=makeOriginalBuntaRaceSetup(p,2);
        require(setup.numericRaceMode1640==0&&setup.profileMode0C901648==2,"live Bunta mode binding");
        require(setup.geometryCar0C9015F8==0&&setup.enemyId0C9015E0==13,"fresh Bunta visible rival");
        require(setup.progress0C901604[course]==0,"fresh Bunta pace progression");
        require(recordOriginalBuntaResult(p,1,0),"finished Bunta win rejected");updateOriginalPostRaceRank(p);
        const auto win=awardOriginalBuntaPoints(p,0,20.f);
        require(p.u(levelOffset)==1&&win.total==3200&&p.u(72)==7200,"first Bunta level/points");
        p=restart(store,directory,car,p);
        require(!recordOriginalBuntaResult(p,0,0),"unfinished ahead state counted as Bunta win");updateOriginalPostRaceRank(p);
        const auto lost=awardOriginalBuntaPoints(p,2,20.f);
        require(lost.deduction&&lost.total==1000&&p.u(72)==6200&&p.u(levelOffset)==1,"time-up deduction/level preservation");
        p=restart(store,directory,car,p);
        require(p.u(16)==car&&(p.u(1180)&1),"selected car/accepted-card flag lost");
        // Updating this selected car must not overwrite the preceding one.
        if(car)require(store.load(car-1).profile.u(72)==6200,"per-car Bunta points leaked");
    }
    // Advance Akina using real win mutations rather than setting its level.
    auto p=store.load(0).profile;p.setu(0,2);
    require(p.u(1092)==0,"independent Akina level was changed by Myogi result");
    for(unsigned level=0;level<16;++level){
        selectOriginalBuntaCourse(p,3);
        require(p.u(4)==(level>10?8u:3u),"Akina-to-Snow source boundary");
        require(p.u(32)==(level>10?1u:0u)&&p.u(8)==1,"Bunta original night/weather");
        require(p.u(24)==(level<=5?13u:level<=10?29u:30u),"Bunta opponent tier boundary");
        require(p.u(20)==(level>10?29u:0u),"Bunta visible model must not use solver31");
        const auto setup=makeOriginalBuntaRaceSetup(p,2);
        require(setup.condition1568==(level>10?16u:6u),"Bunta scene condition boundary");
        require(setup.progress0C901604[3]==level,"Bunta solver receives course progression");
        require(recordOriginalBuntaResult(p,1,0),"Bunta level victory rejected");
        require(p.u(1092)==level+1,"Bunta level progression");
        require(bool(p.u(1180)&0x00100000u)==(level<15),"level16 special progression flag");
        updateOriginalPostRaceRank(p);const auto points=awardOriginalBuntaPoints(p,0,125.f);
        const unsigned award=level+1>9?12000:level+1>4?6000:3000;
        require(points.win==award&&points.advantage==(level>10?0u:1250u),"post-result tier/Snow adjacent factor semantics");
        p=restart(store,directory,0,p);
    }
    selectOriginalBuntaCourse(p,3);require(recordOriginalBuntaResult(p,1,0),"level16 victory rejected");
    require(p.u(1092)==16&&!(p.u(1180)&0x00100000u),"original level16 saturation");
    const auto before=p.u(72);require(!recordOriginalBuntaResult(p,1,1),"finished loss advanced level");
    const auto loss=awardOriginalBuntaPoints(p,1,0.f);
    require(loss.total==4000&&loss.deduction&&p.u(72)==before-4000&&p.u(1092)==16,"high-tier loss deduction");
    restart(store,directory,0,p);
    // A source win cannot confer card ownership; accepted-card assignment is
    // an explicit successful-storage boundary owned by the native host.
    auto noCard=makeOriginalFreshBattleProfile();noCard.setu(72,4000);
    require(originalBuntaEligibility(noCard)==OriginalBuntaEligibility::CardRequired,"points bypassed card requirement");
    std::cout<<"PASS Bunta earned eligibility,35 independent cars, source Snow/opponent/points boundaries, level16 saturation, "
        <<saves<<" full-profile save/restarts, "<<checks<<" integration checks. Original arithmetic is separately opcode-tested.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
