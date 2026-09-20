#include "original_result_tuning_visit.h"
#include "sh4_scalar_reference.h"
#include <iostream>

using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned profile=0x0c31c99c,owner=0x0d000000,result=0x0d001000,child=0x0d002000,
    vtable=0x0d003000,choice=0x0d004000,tls=0x0d005000,textWidget=0x0d006000,
    blinkWidget=0x0d007000,stack=0x0d100000,stop=0x00ff0000,
    initialize=0x00ff0020,textReset=0x00ff0040,exitCallback=0x00ff0060;
}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::invalid_argument("canonical-image game-root required");
    RefMemory m(argv[1]);const auto data=OriginalTuningData::load(argv[2]);
    std::size_t checks=0,instructions=0,cases=0,frames=0,hooks=0;
    unsigned car=0,scenario=0,tick=0,totalAwards=0,totalPurchases=0,totalDeclines=0,totalResets=0;
    const auto equal=[&](unsigned a,unsigned b,const char* label){++checks;if(a!=b)throw std::runtime_error(std::string(label)+" car "+std::to_string(car)+" scenario "+std::to_string(scenario)+" tick "+std::to_string(tick)+": "+hex(a)+" vs "+hex(b));};
    for(car=0;car<35;++car)for(scenario=0;scenario<9;++scenario){
        auto p=makeOriginalFreshBattleProfile();const auto& d=data.car(car);
        p.setu(0,scenario%3);p.setu(16,car);p.setu(1180,scenario==0?0x80:3);
        unsigned balance=120000;
        if(scenario==2)p.setByte(153,std::uint8_t(d.packages[0].steps.size()-1));
        if(scenario==3||scenario==4){p.setu(1180,0x403);p.setByte(153,scenario==3?0:57);balance=990000;}
        if(scenario>=5){p.setu(1180,0xc03);p.setByte(155,2);balance=990000;}
        if(p.u(0)==1&&(car&1))p.setu(1180,p.u(1180)|0x20000);
        p.setu(72,balance-1000);
        m.clear();m.zeroRegion(owner,0x10000);m.zeroRegion(stack,0x10000);m.write8(0x0c92ed00,0);
        for(unsigned i=0;i<p.words.size();++i)m.write32(profile+i*4,p.words[i]);
        m.write32(owner+252,result);m.write32(owner+268,owner+0x9000);
        m.write32(owner+304,vtable+128);m.write32(vtable+148,exitCallback);
        m.write32(tls+4,tls+128);m.write32(vtable+20,initialize);
        m.write32(result+48,1000);m.write32(result+52,balance);
        unsigned rng=0x12345678u+car*71+scenario;m.write32(0x0c37c778,rng);
        RefCpu c(m);c.r[14]=stack+0xe000;c.r[15]=c.r[14];
        m.write32(c.r[14]+336,owner);m.write32(c.r[14]+488,tls);m.write32(c.r[14]+492,tls+4);
        c.callHooks[0x0c2223e0]=[&](auto& q){++hooks;q.fpul=q.r[4]/q.r[5];};
        c.callHooks[0x0c2223b8]=[&](auto& q){++hooks;q.fpul=unsigned(std::int64_t(signed32(q.r[4]))/signed32(q.r[5]));};
        c.callHooks[0x0c021960]=[&](auto& q){++hooks;q.r[0]=child;};
        unsigned sourceCtor=0;
        for(unsigned address:{0x0c115a20u,0x0c116a20u,0x0c1175a0u})c.callHooks[address]=[&,address](auto& q){
            ++hooks;sourceCtor=address;m.write32(q.r[4],vtable);q.r[0]=q.r[4];
            if(address==0x0c1175a0)m.write32(profile+1176,879);
        };
        c.callHooks[initialize]=[&](auto&){++hooks;};
        instructions+=c.run(0x0c06fc48,0x0c07042a,100000);
        OriginalResultTuningVisit visit;OriginalBattleResultAnimationSetup setup;setup.earnedMagnitude=1000;setup.balanceBeforeCap=balance;
        const auto prepared=beginOriginalResultTuningVisit(visit,p,data,rng,setup);
        equal(prepared.balanceCommit.balance,m.read32(profile+72),"Commit exactly once");
        equal(visit.animation.startingBalance,m.read32(owner+276),"Starting display");
        equal(visit.animation.upgradeThreshold,m.read32(owner+284),"Threshold");
        equal(visit.animation.upgradeNotice,m.read32(result+68),"Notice");
        equal(visit.basicCompletionPresented,m.read8(owner+300),"Initial completion latch");
        equal(rng,m.read32(0x0c37c778),"Shared RNG after initialization");
        for(unsigned i=0;i<p.words.size();++i)equal(p.words[i],m.read32(profile+i*4),"Prepared profile");
        std::vector<unsigned> initializationAudio;
        m.write32(0x0c31de08,owner+0x8000);
        c.callHooks[0x0c1416a0]=[&](auto& q){++hooks;equal(q.r[4],1,"Result sound-set");initializationAudio.push_back(1);};
        c.callHooks[0x0c141ec0]=[&](auto& q){++hooks;equal(q.r[4],2,"Result music request");initializationAudio.push_back(2);};
        c.callHooks[0x0c142fc0]=[&](auto& q){++hooks;equal(q.r[4],owner+0x8000,"Manager identity");initializationAudio.push_back(3);};
        instructions+=c.run(0x0c07042a,0x0c070468,1000);
        equal(unsigned(initializationAudio.size()),prepared.requestResultSoundSet?3u:0u,"Initial audio gate");
        for(unsigned i=0;i<initializationAudio.size();++i)equal(initializationAudio[i],i+1,"Initial audio order");
        // Numeric constructors are covered independently by the child suite.
        // Set their established storage, then execute the actual owner AND
        // child Main AND profile mutation functions together without hooks.
        const auto& initial=visit.child;
        const unsigned childMain=sourceCtor==0x0c115a20?0x0c115f60:sourceCtor==0x0c116a20?0x0c116ea0:sourceCtor?0x0c117f40:0;
        m.write32(vtable+28,childMain);
        for(const auto [offset,value]:std::initializer_list<std::pair<unsigned,unsigned>>{{12,initial.frame},{16,initial.phase},{20,initial.car},{28,initial.package},{32,initial.selected},{36,initial.skip},{48,initial.balance},{56,initial.count},{88,initial.flags}})m.write32(child+offset,value);
        m.write32(child+84,textWidget);m.write32(textWidget,vtable+256);m.write32(vtable+300,textReset);
        if(visit.tuning.kind==OriginalTuningChildKind::optionalPart){
            unsigned allocation=choice+128;
            c.callHooks[0x0c021960]=[&](auto& q){++hooks;q.r[0]=allocation;allocation+=128;};
            c.r[14]=stack+0xe000;c.r[15]=c.r[14];c.r[0]=tls+4;
            m.write32(c.r[14]+48,choice);m.write32(c.r[14]+52,2);m.write32(c.r[14]+56,1);m.write32(c.r[14]+64,tls);
            instructions+=c.run(0x0c09c264,0x0c09c3c2,5000);
            m.write32(child+64,choice);m.write32(child+72,d.sourceRow[2]+initial.optionalIndex*24);
            m.write32(child+80,initial.choice);m.write32(child+84,initial.optionalIndex);m.write32(child+88,0);
        }
        std::vector<unsigned> sourceCues;
        std::vector<OriginalTuningPreviewEvent> sourceEvents;
        bool sourceExit=false,descriptionChanged=false;unsigned description=0;
        float descriptionX=0,descriptionY=0;
        OriginalResultTuningVisitInput input;
        c.callHooks[0x0c1d0880]=[&](auto&){++hooks;};
        c.callHooks[0x0c141f80]=[&](auto& q){++hooks;equal(q.r[5],1,"Cue argument");sourceCues.push_back(q.r[4]);};
        c.callHooks[exitCallback]=[&](auto&){++hooks;sourceExit=true;};
        c.callHooks[0x0c0717c0]=[&](auto&){++hooks;sourceEvents.push_back({OriginalTuningPreviewEvent::Kind::startPreview,{}});};
        c.callHooks[0x0c078a00]=[&](auto& q){++hooks;equal(q.r[5],0,"Completion preview focus");sourceEvents.push_back({OriginalTuningPreviewEvent::Kind::focusZero,{}});};
        c.callHooks[0x0c078a20]=[&](auto&){++hooks;sourceEvents.push_back({OriginalTuningPreviewEvent::Kind::resetFocus,{}});};
        c.callHooks[0x0c0c6c20]=[&](auto& q){++hooks;descriptionX=q.getFloat(4);descriptionY=q.getFloat(5);};
        c.callHooks[0x0c0c6a00]=[&](auto& q){++hooks;descriptionChanged=true;description=q.r[5];};
        for(unsigned address:{textReset,0x0c0c6ca0u})c.callHooks[address]=[&](auto&){++hooks;};
        for(unsigned address:{0x0c028720u,0x0c0283c0u,0x0c028400u,0x0c028480u,0x0c0284c0u,0x0c028500u,0x0c028540u,0x0c028580u,0x0c0285c0u,0x0c0286c0u,0x0c029040u,0x0c0287a0u})c.callHooks[address]=[&,address](auto& q){
            ++hooks;sourceEvents.push_back({OriginalTuningPreviewEvent::Kind::carCall,{address,address==0x0c029040?0:q.r[5],address==0x0c0287a0?q.r[6]:0}});
        };
        c.callHooks[0x0c0d43a0]=[&](auto& q){++hooks;q.setFloat(0,input.selectionAxis);};
        c.callHooks[0x0c0d4300]=[&](auto& q){++hooks;q.r[0]=input.confirmEdge;};
        c.callHooks[0x0c0d42c0]=[&](auto&){++hooks;};
        unsigned childTicks=0;
        for(tick=0;tick<6500;++tick){
            input={};
            if(scenario%2==0&&tick<2)input.confirmEdge=true;
            if(visit.animation.phase==3){
                if(visit.child.kind==OriginalTuningChildKind::optionalPart){
                    if(scenario==5){input.selectionAxis=0;input.confirmEdge=childTicks==20;}
                    if(scenario==6)input.confirmEdge=childTicks==20;
                    if(scenario==8){input.selectionAxis=float(childTicks%101)/100;input.confirmEdge=childTicks==240;}
                }
                ++childTicks;
            }
            sourceCues.clear();sourceEvents.clear();descriptionChanged=false;sourceExit=false;
            c.r[4]=owner;c.r[15]=stack+0xf000;c.pr=stop;
            instructions+=c.run(0x0c0709e0,0x0c070e42,25000);
            const auto out=advanceOriginalResultTuningVisit(visit,p,data,input);
            equal(out.result.displayedBalance,m.read32(result+52),"Displayed balance");
            equal(out.result.fadeAlpha,c.r[11],"Fade");equal(out.result.sourcePhase,m.read32(owner+4),"Visible phase");
            equal(out.result.showUpgradeChild,m.read32(owner+280),"Child visible");equal(out.result.finished,sourceExit,"Parent release");
            equal(visit.previewStarted,m.read8(owner+288),"Preview latch");equal(visit.basicCompletionPresented,m.read8(owner+300),"Completion latch");
            equal(visit.previewRemovalFrames,m.read32(owner+296),"Removal timer");
            equal(unsigned(out.cueIds.size()),unsigned(sourceCues.size()),"Ordered cue count");
            for(unsigned i=0;i<sourceCues.size();++i)equal(out.cueIds[i],sourceCues[i],"Ordered cue");
            equal(unsigned(out.previewEvents.size()),unsigned(sourceEvents.size()),"Ordered preview event count");
            for(unsigned i=0;i<sourceEvents.size();++i){const auto& a=out.previewEvents[i];const auto& b=sourceEvents[i];
                equal(unsigned(a.kind),unsigned(b.kind),"Preview event kind");equal(a.call.address,b.call.address,"Car method");
                equal(a.call.argument5,b.call.argument5,"Car argument5");equal(a.call.argument6,b.call.argument6,"Car argument6");
                totalResets+=a.kind==OriginalTuningPreviewEvent::Kind::resetFocus;
            }
            equal(out.child.descriptionChanged,descriptionChanged,"Description changed");
            if(descriptionChanged){equal(out.child.descriptionAddress,description,"Description");equal(std::bit_cast<unsigned>(out.child.descriptionX),std::bit_cast<unsigned>(descriptionX),"Description X");equal(std::bit_cast<unsigned>(out.child.descriptionY),std::bit_cast<unsigned>(descriptionY),"Description Y");}
            if(out.childAdvanced){equal(visit.child.frame,m.read32(child+12),"Child frame");equal(visit.child.phase,m.read32(child+16),"Child phase");equal(visit.child.balance,m.read32(child+48),"Child balance");}
            for(unsigned offset:{72u,152u,156u,160u,164u,1176u,1180u})equal(p.u(offset),m.read32(profile+offset),"Combined profile mutation");
            bool visible=true;
            if(!out.result.showUpgradeChild&&m.read32(result+64)){RefCpu draw(m);draw.r[11]=blinkWidget;draw.callHooks[0x0c2223b8]=c.callHooks.at(0x0c2223b8);instructions+=draw.run(0x0c0ee278,0x0c0ee298,1000);visible=!draw.t;}
            equal(out.result.balanceVisible,visible,"Balance blink");
            c.r[8]=owner+252;c.r[10]=252;m.write8(0x0c92ed00,input.confirmEdge?128:0);
            instructions+=c.run(0x0c070ecc,stop,1000);
            equal(visit.animation.frame,m.read32(owner),"Owner next frame");equal(visit.animation.phase,m.read32(owner+4),"Owner next phase");
            equal(visit.animation.displayedBalance,m.read32(result+52),"Next displayed balance");
            totalAwards+=out.child.command==1||out.child.command==2;totalPurchases+=out.child.command==3;totalDeclines+=out.child.command==4;
            ++frames;if(out.result.finished)break;
        }
        if(tick==6500)throw std::runtime_error("Combined result visit failed to finish");
        for(unsigned i=0;i<p.words.size();++i)equal(p.words[i],m.read32(profile+i*4),"Final full profile");
        equal(rng,m.read32(0x0c37c778),"No per-frame RNG consumption");
        const auto finalProfile=p.words;const auto frame=visit.animation.frame;
        for(unsigned i=0;i<5;++i){const auto again=advanceOriginalResultTuningVisit(visit,p,data,{true,0});equal(again.result.finished,true,"Completed status");equal(again.profileChanged,false,"No repeated mutation");equal(unsigned(again.cueIds.size()),0,"No repeated cue");equal(unsigned(again.previewEvents.size()),0,"No repeated preview");equal(visit.animation.frame,frame,"Completed clock frozen");}
        if(p.words!=finalProfile)throw std::runtime_error("Completed profile changed");++cases;
    }
    if(!totalAwards||!totalPurchases||!totalDeclines||!totalResets)throw std::runtime_error("Required tuning branch not exercised");
    std::cout<<"PASS "<<cases<<" combined original result/tuning visits, "<<frames<<" frames, "<<checks<<" comparisons, "<<instructions<<" instructions; awards="<<totalAwards<<" purchases="<<totalPurchases<<" declines="<<totalDeclines<<" resets="<<totalResets<<". "<<hooks<<" constructor/allocation/input/text/ACar/preview/light/cue/PR1 boundaries; no owner, child-Main, mutation or RNG hooks.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
