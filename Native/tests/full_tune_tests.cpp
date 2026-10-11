#include "original_result_tuning_visit.h"
#include "original_driver_entry_route.h"
#include "original_tuning_course_menu.h"
#include "original_tuning_preview_state.h"
#include <iostream>
#include <stdexcept>
using namespace idas3::original;
namespace {
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct PreviewChecks {unsigned starts{},completedBasic{},ordinaryVisits{},renewalCases{};};
void checkPreviews(const OriginalResultTuningVisitFrame& frame,const OriginalResultTuningVisit& visit,
        const OriginalBattleProfile& p,const OriginalTuningData& tables,PreviewChecks& checks){
    bool completedStart=false,completionFocus=false;
    for(const auto& event:frame.previewEvents){
        if(event.kind==OriginalTuningPreviewEvent::Kind::startPreview){
            const auto before=p.words;const auto focus=originalTuningPreviewFocus(p,tables,visit.tuning.kind);
            check(p.words==before,"Preview changed saved profile values");++checks.starts;
            if(visit.tuning.kind==OriginalTuningChildKind::basic&&(p.u(1180)&0x400)){
                check(focus&&*focus==0,"Completed basic preview did not use completion focus");
                completedStart=true;++checks.completedBasic;
            }
        }else if(event.kind==OriginalTuningPreviewEvent::Kind::focusZero){
            check(completedStart,"Completion focus did not follow the completed basic preview");completionFocus=true;
        }
    }
    check(!completedStart||completionFocus,"Completed basic preview lost its ordered completion event");
}
void checkUnrelatedProgress(const OriginalBattleProfile& p,const OriginalBattleProfile& before){
    for(unsigned offset=0;offset<p.words.size()*4;++offset){
        if((offset>=152&&offset<168)||(offset>=1176&&offset<1184))continue;
        check(p.byte(offset)==before.byte(offset),"Ordinary tuning changed unrelated saved progress or points");
    }
    check((p.u(1180)&~0xc00u)==(before.u(1180)&~0xc00u),"Ordinary tuning changed unrelated profile flags");
}
void finishOrdinaryTuning(OriginalBattleProfile& p,const OriginalTuningData& tables,PreviewChecks& checks){
    const auto before=p;std::uint32_t rng=77;unsigned visits=0;
    for(;visits<128;++visits){
        OriginalResultTuningVisit visit;beginOriginalResultTuningVisit(visit,p,tables,rng,{0,p.u(72),false});
        ++checks.ordinaryVisits;unsigned tick=0;
        for(;tick<10000;++tick){
            const auto frame=advanceOriginalResultTuningVisit(visit,p,tables,{tick==1||tick==2,1.f});
            checkPreviews(frame,visit,p,tables,checks);
            if(frame.result.finished)break;
        }
        check(tick<10000,"Ordinary result tuning child stuck");
        if((p.u(1180)&0xc00)==0xc00)break;
    }
    check(visits<128,"Ordinary tuning did not finish");
    check(p.byte(164)==tables.car(p.u(16)).performance.back().words[0],"Ordinary tuning did not retain maximum engine level");
    checkUnrelatedProgress(p,before);
}
void checkRenewalContinuation(OriginalBattleProfile p,unsigned package,const OriginalTuningData& tables,
        PreviewChecks& checks){
    // Use the source card-renewal/package-change operations. They preserve the
    // earned engine level while restarting basic tuning for a different route.
    p.setu(1140,0);acceptOriginalDriverCard(p,{1,2,3,4});
    check(p.byte(1192)==1,"Expired card did not request renewal");const auto before=p;
    OriginalTuningCourseMenu menu;initializeOriginalTuningCourseMenu(menu,p,tables);
    for(unsigned frame=0;!menu.parentEvent64&&frame<500;++frame)
        tickOriginalTuningCourseMenu(menu,p,{package,menu.phase464==1,4});
    check(menu.parentEvent64!=0,"Renewal package menu did not commit");
    check(p.byte(152)==package&&p.byte(153)==0&&!(p.u(1180)&0xc00),"Renewal did not restart basic tuning");
    check(p.byte(164)==before.byte(164),"Renewal discarded the earned engine level");
    checkUnrelatedProgress(p,before);
    const auto completedBefore=checks.completedBasic;finishOrdinaryTuning(p,tables,checks);
    check(checks.completedBasic==completedBefore+1,"Renewal did not exercise basic completion preview");
    ++checks.renewalCases;
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::runtime_error("Game asset root required");
    const auto tables=OriginalTuningData::load(argv[1]);
    unsigned cases=0,awards=0,purchases=0,declines=0,frames=0;
    PreviewChecks previews;
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
                checkPreviews(f,v,p,tables,previews);
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
    unsigned ordinaryCases=0;
    for(unsigned car=0;car<35;++car){
        const auto packageCount=unsigned(tables.car(car).packages.size());
        for(unsigned package=0;package<packageCount;++package){
            auto p=makeOriginalFreshBattleProfile();p.setu(16,car);p.setByte(152,package);p.setu(1180,3);p.setu(72,999999);
            p.setByte(116,0x21);p.setu(1080,7); // Nonzero earned race/Bunta progress must survive.
            finishOrdinaryTuning(p,tables,previews);++ordinaryCases;
            if(package==packageCount-1)
                for(unsigned replacement=0;replacement+1<packageCount;++replacement)
                    checkRenewalContinuation(p,replacement,tables,previews);
        }
    }
    check(ordinaryCases==146&&previews.renewalCases==111,"Missing real car/package coverage");
    std::cout<<"PASS "<<cases<<" full-tune sequences across all 35 cars and all tuning packages; "<<frames<<" child frames, "<<awards<<" mandatory awards, "<<purchases<<" purchases, "<<declines<<" declines; "
        <<ordinaryCases<<" ordinary progressions and "<<previews.renewalCases<<" high-engine renewal continuations, "<<previews.starts<<" preview starts, "<<previews.completedBasic<<" completed-basic previews; exact spending and unrelated profile fields preserved.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
