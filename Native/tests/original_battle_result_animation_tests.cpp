#include "original_battle_result_animation.h"
#include "sh4_scalar_reference.h"
#include <array>
#include <iostream>
#include <vector>

using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned owner=0x0d000000,result=0x0d001000,child=0x0d002000,
    vtable=0x0d003000,widget=0x0d004000,stack=0x0d100000,stop=0x00ff0000,
    exitCallback=0x00ff0010,childCallback=0x00ff0020,profileBalance=0x0c31c9e4;
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("canonical-image required");
    RefMemory m(argv[1]);std::size_t checks=0,instructions=0,cases=0,frames=0,hooks=0;
    unsigned caseIndex=0,frameIndex=0;
    const auto equal=[&](unsigned a,unsigned b,const char* label){++checks;if(a!=b)throw std::runtime_error(std::string(label)+" case "+std::to_string(caseIndex)+" frame "+std::to_string(frameIndex)+": "+hex(a)+" vs "+hex(b));};
    const auto seed=[&](){m.clear();m.zeroRegion(owner,0x5000);m.zeroRegion(stack,0x10000);m.zeroRegion(0x0c92ed00,4);
        m.write32(owner+252,result);m.write32(owner+304,vtable);m.write32(vtable+20,exitCallback);
        m.write32(child,vtable);m.write32(vtable+28,childCallback);};
    const auto divide=[&](RefCpu& c){++hooks;c.fpul=unsigned(std::int64_t(signed32(c.r[4]))/signed32(c.r[5]));};
    equal(m.read32(0x0c2f4e28),30,"Reveal duration");equal(m.read32(0x0c2f4e2c),60,"Count duration");equal(m.read32(0x0c2f4e30),300,"Hold duration");
    const std::array<unsigned,10> earnedValues={0,1,599,1000,42000,100000,999999999,0x7fffffff,0x80000000,0xffffffff};
    const std::array<unsigned,9> finalBalances={0,1,3999,4000,999999998,999999999,1000000000,0x80000000,0xffffffff};
    for(unsigned earned:earnedValues)for(unsigned balance:finalBalances)for(bool deduction:{false,true}){
        seed();OriginalBattleResultAnimationState state;OriginalBattleResultAnimationSetup setup;
        setup.earnedMagnitude=earned;setup.balanceBeforeCap=balance;setup.deduction=deduction;
        m.write32(result+48,earned);m.write32(result+52,balance);m.write8(result+56,deduction);m.write32(result+64,0xabcdef);m.write32(result+68,0xabcdef);
        RefCpu c(m);c.r[14]=stack+0xf000;c.r[15]=c.r[14];m.write32(c.r[14]+336,owner);
        instructions+=c.run(0x0c06fc48,0x0c06fca4,1000);
        const auto commit=initializeOriginalBattleResultAnimation(state,setup);
        equal(state.delta,m.read32(owner+8),"Signed delta");equal(state.startingBalance,m.read32(owner+276),"Old balance");
        equal(state.displayedBalance,m.read32(result+52),"Initial display");equal(commit.balance,m.read32(profileBalance),"Commit cap");
        equal(unsigned(commit.signedDelta),m.read32(owner+8),"Commit delta");equal(state.highlightBalance,m.read32(result+64),"Initial highlight");equal(state.upgradeNotice,m.read32(result+68),"Initial notice");
        ++cases;++caseIndex;
    }
    // Finite ordinary awards, deductions, zero awards and deliberate32-bit
    // product-wrap cases. The original uses a low32-bit multiplication.
    const std::array<OriginalBattleResultAnimationSetup,8> setups={{{42000,142000,false},{42000,58000,true},{0,0,false},{1,999999999,false},
        {42000,1000040000,false},{999999999,999999999,false},{0x80000000,0x80000000,true},{0xffffffff,0,false}}};
    for(auto setup:setups)for(unsigned skip:{0u,1u,2u,3u,4u})for(bool childPresent:{false,true})for(bool upgrade:{false,true}){
        seed();setup.upgradeChildPresent=childPresent;setup.upgradeNotice=upgrade;setup.upgradeThreshold=upgrade?0u:0x7fffffffu;
        OriginalBattleResultAnimationState state;const auto commit=initializeOriginalBattleResultAnimation(state,setup);
        m.write32(owner+8,state.delta);m.write32(owner+276,state.startingBalance);m.write32(owner+272,childPresent?child:0);
        m.write32(owner+284,setup.upgradeThreshold);m.write32(result+52,state.startingBalance);m.write32(result+68,upgrade);m.write32(profileBalance,commit.balance);
        unsigned cueFive=0,childFrames=0;bool finished=false;
        for(frameIndex=0;frameIndex<520;++frameIndex){
            OriginalBattleResultAnimationInput input;
            input.confirmEdge=(skip==1&&frameIndex==0)||(skip==2&&(frameIndex==0||frameIndex==1))||
                (skip==3&&state.phase==1&&state.frame==30)||(skip==4&&state.phase==2&&state.frame==120);
            if(state.phase==3)input.upgradeChildFinished=++childFrames==7;
            const auto native=advanceOriginalBattleResultAnimation(state,input);
            std::vector<unsigned> sourceCues;bool exitCalled=false;
            RefCpu c(m);c.r[4]=owner;c.r[15]=stack+0xf000;c.pr=stop;
            c.callHooks[0x0c1d0880]=[&](auto&){++hooks;}; // rendering light-state boundary
            c.callHooks[0x0c2223b8]=divide; // positive-denominator signed PR1 division contract
            c.callHooks[0x0c141f80]=[&](auto& q){++hooks;equal(q.r[5],1,"Cue argument");sourceCues.push_back(q.r[4]);};
            c.callHooks[exitCallback]=[&](auto&){++hooks;exitCalled=true;}; // result-owner transition notification
            c.callHooks[childCallback]=[&](auto& q){++hooks;q.r[0]=input.upgradeChildFinished?14:0;}; // explicit tuning-child protocol boundary
            instructions+=c.run(0x0c0709e0,0x0c070e42,5000);
            equal(native.displayedBalance,m.read32(result+52),"Frame balance");equal(native.fadeAlpha,c.r[11],"Fade alpha");
            equal(native.sourcePhase,m.read32(owner+4),"Visible phase");equal(native.highlightBalance,m.read32(result+64),"Visible highlight");
            equal(native.upgradeNotice,m.read32(result+68),"Visible notice");equal(native.showUpgradeChild,m.read32(owner+280),"Child display");
            equal(native.finished,exitCalled,"Exit event");equal(native.cueCount,unsigned(sourceCues.size()),"Cue count");
            for(unsigned i=0;i<sourceCues.size();++i){equal(native.cueIds[i],sourceCues[i],"Cue ID");cueFive+=sourceCues[i]==5;}
            bool visible=true;
            if(!native.showUpgradeChild&&m.read32(result+64)){
                RefCpu draw(m);draw.r[11]=widget;draw.callHooks[0x0c2223b8]=divide;
                instructions+=draw.run(0x0c0ee278,0x0c0ee298,1000);visible=!draw.t;
            }
            equal(native.balanceVisible,visible,"Balance blink");equal(state.balanceBlinkFrame,m.read32(widget+248),"Blink counter");
            // Skip only the rendering body. Supply exactly the owner-relative
            // registers set by070EB6..070ECA, then execute the actual input tail.
            c.r[8]=owner+252;c.r[10]=252;m.write8(0x0c92ed00,input.confirmEdge?128:0);
            instructions+=c.run(0x0c070ecc,stop,1000);
            equal(state.frame,m.read32(owner),"Next frame");equal(state.phase,m.read32(owner+4),"Next phase");
            equal(state.displayedBalance,m.read32(result+52),"Next balance");equal(state.highlightBalance,m.read32(result+64),"Next highlight");
            equal(m.read32(profileBalance),commit.balance,"No repeated profile award");++frames;
            if(native.finished){finished=true;break;}
        }
        equal(finished,true,"Finite lifecycle completion");
        if(skip==0&&!childPresent){equal(frameIndex+1,427,"Full no-child duration");equal(cueFive,21,"Counting cue cadence");}
        if(skip==1&&!childPresent)equal(frameIndex+1,95,"Single-confirm duration");
        if(skip==2&&!childPresent)equal(frameIndex+1,37,"Double-confirm duration");
        const auto frozen=state;const auto repeated=advanceOriginalBattleResultAnimation(state,{true,true});
        equal(repeated.finished,true,"Consumed owner remains complete");equal(repeated.cueCount,0,"No events after completion");
        equal(state.frame,frozen.frame,"Completed frame frozen");equal(state.displayedBalance,frozen.displayedBalance,"Completed balance frozen");
        ++cases;++caseIndex;
    }
    std::cout<<"PASS "<<cases<<" original result animation/init cases, "<<frames<<" frames, "<<checks<<" exact comparisons, "<<instructions
        <<" original instructions; "<<hooks<<" explicit PR1 division/light/cue/owner/child boundaries. Rendering body excluded; balance blink executes actual0EE278.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
