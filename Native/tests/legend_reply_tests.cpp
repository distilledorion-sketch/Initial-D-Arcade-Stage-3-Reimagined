#include "original_legend_visit.h"
#include "original_rival_dialog.h"
#include "original_rival_setup.h"
#include "original_race_rules.h"
#include <iostream>
#include <stdexcept>
using namespace idas3::original;
void require(bool b,const char* m){if(!b)throw std::runtime_error(m);}
int main(int argc,char**argv)try{
    if(argc!=2)throw std::runtime_error("Native assets root required");
    const auto data=OriginalRivalDialogData::load(argv[1]);
    unsigned pages=0;
    for(unsigned enemy=0;enemy<31;++enemy)for(unsigned kind:{0u,4u,7u,11u,14u,18u}){
        auto p=makeOriginalFreshBattleProfile();p.setu(16,0);selectOriginalRival(p,enemy);
        OriginalRivalDialogState d;OriginalRivalDialogSetup setup;setup.enemy=enemy;setup.kind=kind;
        resetOriginalRivalDialog(d,data,p,setup);
        for(unsigned i=0;i<10000&&!originalRivalDialogReady(d);++i)stepOriginalRivalDialog(d,data,p);
        require(originalRivalDialogReady(d),"Initial page did not finish");
        advanceOriginalRivalDialogPage(d,data,p,2);
        require(d.phase==1&&!originalRivalDialogReady(d)&&!d.skip,"Refusal inherited completed phase");
        unsigned frames=0;for(;frames<10000&&!originalRivalDialogReady(d);++frames)stepOriginalRivalDialog(d,data,p);
        require(originalRivalDialogReady(d)&&frames>1,"Refusal did not play its script");++pages;
    }
    OriginalLegendVisit visit;visit.load(argv[1]);
    for(unsigned status:{0u,1u,2u}){
        auto p=makeOriginalFreshBattleProfile();p.setu(16,0);selectOriginalRival(p,0);p.setByte(116,status?1:16);
        OriginalLegendVisit::Setup setup;setup.resultStatus=status;visit.begin(p,setup);
        int answered=-1;unsigned refusalFrames=0;bool refusal=false;
        for(unsigned i=0;i<24000&&!visit.finished();++i){
            OriginalLegendVisit::Input in;
            if(visit.choiceVisible()){
                const int kind=int(visit.choiceKind());
                if(kind!=answered){
                    in.confirm=true;answered=kind;
                    const bool next=visit.choiceKind()==OriginalLegendChoiceKind::Challenge||visit.choiceKind()==OriginalLegendChoiceKind::Rematch;
                    in.next=next;in.previous=!next;
                }
            }
            const auto before=visit.dialogueState().kind;
            visit.advance(p,in);
            if(visit.dialogueState().kind==before+2){refusal=true;require(visit.dialogueState().phase==1,"Owner skipped refusal page");}
            if(refusal&&visit.dialogueState().phase==1)++refusalFrames;
        }
        require(visit.finished()&&refusal&&refusalFrames>1,"Owner failed to present refusal then finish");
        require(visit.destination()==OriginalLegendReturnDestination::Select,"Refusal did not return to course selection");
    }
    OriginalRaceRules race;require(!race.retire(),"Uninitialized retirement accepted");
    race.resetLegend(argv[1],{6,0,2,0},{0,0},{0,0,0});race.start();
    require(race.retire()&&race.state().phase==OriginalRacePhase::TimeUp&&race.state().timeUpFlag&&race.state().automaticBrake,"Retire did not enter timeout");
    require(race.state().times.finishTime==0xffffffffu&&!race.retire(),"Retire fabricated finish or applied twice");
    std::cout<<"PASS "<<pages<<" authored refusal pages; win/loss/timeout refusal flows; retirement timeout/idempotence.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
