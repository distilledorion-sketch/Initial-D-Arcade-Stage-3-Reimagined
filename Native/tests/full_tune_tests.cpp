#include "original_result_tuning_visit.h"
#include <iostream>
#include <stdexcept>
using namespace idas3::original;
int main(int argc,char** argv)try{
    if(argc!=2)throw std::runtime_error("Game asset root required");
    const auto tables=OriginalTuningData::load(argv[1]);
    unsigned cases=0,awards=0,purchases=0,declines=0,frames=0;
    auto check=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
    for(unsigned car=0;car<35;++car)for(unsigned package=0;package<tables.car(car).packages.size();++package)
    for(unsigned scenario=0;scenario<3;++scenario){
        auto p=makeOriginalFreshBattleProfile();p.setu(16,car);p.setByte(152,package);p.setu(1180,3);p.setu(72,999999);
        const auto before=p;unsigned rng=77,spent=0,visits=0;std::vector<std::uint8_t> offers;
        for(;visits<256;++visits){
            OriginalResultTuningVisit v;beginOriginalResultTuningVisit(v,p,tables,rng,{0,p.u(72),false},&offers);
            if(v.tuning.kind==OriginalTuningChildKind::none)break;
            const bool optional=v.tuning.kind==OriginalTuningChildKind::optionalPart;
            if(optional)check((p.u(1180)&0xc00)==0xc00,"Optional appeared before forced upgrades finished");
            unsigned tick=0;const bool buy=scenario==0||(scenario==2&&visits%2==0);
            for(;tick<10000;++tick){
                OriginalResultTuningVisitInput input;
                if(optional){input.selectionAxis=buy?0.f:1.f;input.confirmEdge=tick==20;}
                auto f=advanceOriginalResultTuningVisit(v,p,tables,input);++frames;
                awards+=f.child.command==1||f.child.command==2;
                if(f.child.command==3){++purchases;spent+=f.mutation.pointsSpent;check(buy,"Decline bought a part");}
                if(f.child.command==4){++declines;check(!buy,"Accept declined a part");}
                if(f.result.finished)break;
            }
            check(tick<10000,"Tuning child stuck");
        }
        check(visits<256,"Full Tune repeated optional offers or did not finish");
        check((p.u(1180)&0xc00)==0xc00,"Mandatory upgrades incomplete");
        check(p.byte(164)==tables.car(car).performance.back().words[0],"Performance level not maxed");
        check(p.u(72)==999999-spent,"Points granted again or optional cost incorrect");
        for(unsigned offset=0;offset<p.words.size()*4;++offset){
            if((offset>=72&&offset<76)||(offset>=152&&offset<168)||(offset>=1176&&offset<1184))continue;
            check(p.byte(offset)==before.byte(offset),"Unrelated profile progress changed");
        }
        ++cases;
    }
    check(awards&&purchases&&declines,"Missing mandatory/accept/decline coverage");
    std::cout<<"PASS "<<cases<<" full-tune sequences across all 35 cars and all tuning packages; "<<frames<<" child frames, "<<awards<<" mandatory awards, "<<purchases<<" purchases, "<<declines<<" declines; exact spending and unrelated profile fields preserved.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
