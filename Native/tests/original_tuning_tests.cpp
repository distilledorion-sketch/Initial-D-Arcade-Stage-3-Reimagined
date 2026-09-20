#include "original_tuning.h"
#include "sh4_scalar_reference.h"
#include <iostream>

using namespace idas3::original;
using namespace idas3::reference;
namespace {constexpr unsigned profile=0x0c31c99c,owner=0x0d000000,result=0x0d001000,child=0x0d002000,vtable=0x0d003000,tls=0x0d004000,stack=0x0d100000,stop=0x00ff0000,initialize=0x00ff0020;}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::invalid_argument("canonical-image game-root required");
    RefMemory m(argv[1]);const auto data=OriginalTuningData::load(argv[2]);std::size_t checks=0,instructions=0,cases=0,hooks=0;unsigned carCase=0,stepCase=0;
    const auto equal=[&](unsigned a,unsigned b,const char* label){++checks;if(a!=b)throw std::runtime_error(std::string(label)+" car "+std::to_string(carCase)+" step "+std::to_string(stepCase)+": "+hex(a)+" vs "+hex(b));};
    const auto seed=[&](const OriginalBattleProfile& p){m.clear();m.zeroRegion(owner,0x10000);m.zeroRegion(stack,0x10000);for(unsigned i=0;i<p.words.size();++i)m.write32(profile+i*4,p.words[i]);m.write32(owner+252,result);m.write32(owner+268,owner+0x5000);m.write32(tls+4,tls+128);m.write32(vtable+20,initialize);};
    const auto compare=[&](const OriginalBattleProfile& p){for(unsigned i=0;i<p.words.size();++i){if(m.read32(profile+i*4)!=p.words[i])std::cerr<<"profile offset "<<i*4<<'\n';equal(m.read32(profile+i*4),p.words[i],"Profile mutation");}};
    for(carCase=0;carCase<35;++carCase){const auto& d=data.car(carCase);for(unsigned i=0;i<8;++i)equal(d.sourceRow[i],m.read32(0x0c30ecc0+carCase*32+i*4),"Car table");
        for(unsigned k=0;k<d.packages.size();++k){const auto& p=d.packages[k];equal(p.sourceAddress,m.read32(d.sourceRow[0]+k*8),"Package address");equal(unsigned(p.steps.size()),m.read32(d.sourceRow[0]+k*8+4),"Package count");for(unsigned j=0;j<p.steps.size();++j)for(unsigned w=0;w<5;++w)equal(p.steps[j].words[w],m.read32(p.sourceAddress+j*20+w*4),"Basic source record");}
        for(unsigned j=0;j<d.optional.size();++j)for(unsigned w=0;w<6;++w)equal(d.optional[j].words[w],m.read32(d.sourceRow[2]+j*24+w*4),"Optional source record");
        for(unsigned j=0;j<d.performance.size();++j)for(unsigned w=0;w<3;++w)equal(d.performance[j].words[w],m.read32(d.sourceRow[6]+j*12+w*4),"Performance source record");
        for(unsigned j=0;j<d.colors.size();++j)equal(d.colors[j],m.read32(d.sourceRow[4]+j*4),"Color source record");
    }
    for(carCase=0;carCase<35;++carCase)for(unsigned flags:{0u,1u,2u,0x401u,0xc02u})for(unsigned cooldown:{0u,1u,2u})
    for(unsigned points:{0u,4999u,5000u,120000u,135000u,990000u})for(unsigned level:{0u,8u,63u}){
        auto p=makeOriginalFreshBattleProfile();p.setu(16,carCase);p.setu(1180,flags);p.setu(72,points);p.setByte(155,std::uint8_t(cooldown));p.setByte(164,std::uint8_t(level));
        seed(p);unsigned rng=0x12345678u+carCase*71+level;m.write32(0x0c37c778,rng);
        RefCpu c(m);c.r[14]=stack+0xe000;c.r[15]=c.r[14];m.write32(c.r[14]+336,owner);m.write32(c.r[14]+488,tls);m.write32(c.r[14]+492,tls+4);
        unsigned constructor=0;
        c.callHooks[0x0c2223e0]=[&](auto& q){++hooks;q.fpul=q.r[4]/q.r[5];};
        c.callHooks[0x0c021960]=[&](auto& q){++hooks;q.r[0]=child;};
        for(unsigned address:{0x0c115a20u,0x0c116a20u,0x0c1175a0u})c.callHooks[address]=[&,address](auto& q){++hooks;constructor=address;m.write32(q.r[4],vtable);q.r[0]=q.r[4];
            // Explicit constructor's known profile effect; its two source
            // callees are executed separately below, not a fabricated award.
            if(address==0x0c1175a0)m.write32(profile+1176,879);
        };
        c.callHooks[initialize]=[&](auto&){++hooks;};
        instructions+=c.run(0x0c06fca4,0x0c07042a,100000);
        const auto out=prepareOriginalResultTuning(p,data,rng);
        const unsigned expectedCtor=out.kind==OriginalTuningChildKind::basic?0x0c115a20:out.kind==OriginalTuningChildKind::performance?0x0c116a20:out.kind==OriginalTuningChildKind::optionalPart?0x0c1175a0:0;
        equal(constructor,expectedCtor,"Child factory");equal(m.read32(owner+284),out.threshold,"Highlight threshold");equal(m.read32(result+68),out.notice,"Upgrade notice");
        if(constructor)equal(m.read32(owner+292),out.sourceOwnerKind,"Child kind tag");equal(m.read32(0x0c37c778),rng,"Selection RNG");compare(p);++cases;
    }
    // Execute the actual159720 prefix, including133240, with no hooks. A
    // loaded profile's engine level changes the solver's selected force row.
    for(carCase=0;carCase<35;++carCase)for(unsigned level:{0u,4u,5u,6u,63u,75u})
    for(unsigned package:{0u,2u,3u,4u})for(unsigned condition:{6u,17u}){
        auto p=makeOriginalFreshBattleProfile();p.setu(16,carCase);p.setu(24,carCase);p.setu(32,condition&1);p.setByte(164,std::uint8_t(level));p.setByte(152,std::uint8_t(package));seed(p);
        RefCpu c(m);c.r[4]=condition;c.r[15]=stack+0xf000;c.pr=stop;
        instructions+=c.run(0x0c159720,0x0c1597ce,1000);
        OriginalPhysicsSelection s;s.conditionCode=condition;s.progressEnabled0C9015E4=1;s.progressMode0C9015D4=7;s.progress0C901650=123.25f;
        applyOriginalProfilePhysicsSelection(s,p);
        equal(m.read32(0x0c901654),s.vehicleIndex,"Profile solver car");equal(m.read32(0x0c9015e0),s.vehicleMode0C9015E0,"Profile solver enemy");
        equal(m.read32(0x0c9015f0),s.upgradeIndex0C9015F0,"Profile solver upgrade");equal(m.read32(0x0c9015f4),s.overrideMode0C9015F4,"Profile solver special engine");
        equal(m.read32(0x0c9015fc),s.mode0C9015FC,"Profile weather");equal(m.read32(0x0c9015c0),s.mode0C9015C0,"Profile snow");
        equal(s.progressEnabled0C9015E4,1,"Preserve caller progress enable");equal(s.progressMode0C9015D4,7,"Preserve caller progress mode");equal(std::bit_cast<unsigned>(s.progress0C901650),std::bit_cast<unsigned>(123.25f),"Preserve caller start progress");++cases;
    }
    // Full071300 profile mutation, with ACar presentation calls observed.
    for(carCase=0;carCase<35;++carCase){const auto& d=data.car(carCase);
        for(unsigned package=0;package<d.packages.size();++package)for(stepCase=0;stepCase<d.packages[package].steps.size();++stepCase)
        for(unsigned oldLevel:{0u,1u,5u,63u}){
            auto p=makeOriginalFreshBattleProfile();p.setu(16,carCase);p.setu(1180,3);p.setByte(152,std::uint8_t(package));p.setByte(153,std::uint8_t(stepCase));p.setByte(164,std::uint8_t(oldLevel));
            seed(p);RefCpu c(m);c.r[4]=owner;c.r[15]=stack+0xf000;c.pr=stop;std::vector<OriginalTuningMutation::CarCall> calls;
            for(unsigned address:{0x0c028720u,0x0c0283c0u,0x0c028400u,0x0c028480u,0x0c0284c0u,0x0c028500u,0x0c028540u,0x0c028580u,0x0c0285c0u,0x0c0286c0u,0x0c029040u})c.callHooks[address]=[&,address](auto& q){++hooks;calls.push_back({address,q.r[5],q.r[6]});};
            instructions+=c.run(0x0c071300,stop,10000);const auto changed=applyOriginalTuningCommand(p,data,1);compare(p);
            equal(unsigned(calls.size()),unsigned(changed.carCalls.size()),"ACar call count");for(unsigned i=0;i<calls.size();++i){equal(calls[i].address,changed.carCalls[i].address,"ACar method");if(calls[i].address!=0x0c029040)equal(calls[i].argument5,changed.carCalls[i].argument5,"ACar argument");}++cases;
        }
        for(stepCase=0;stepCase<d.performance.size();++stepCase){auto p=makeOriginalFreshBattleProfile();p.setu(16,carCase);p.setu(1180,0x403);p.setByte(153,std::uint8_t(stepCase));seed(p);RefCpu c(m);c.r[15]=stack+0xf000;c.pr=stop;instructions+=c.run(0x0c0710e0,stop,1000);applyOriginalTuningCommand(p,data,2);compare(p);++cases;}
        for(stepCase=0;stepCase<d.optional.size();++stepCase)for(unsigned command:{3u,4u,5u,6u}){
            auto p=makeOriginalFreshBattleProfile();p.setu(16,carCase);p.setu(72,70000);p.setByte(154,std::uint8_t(stepCase));seed(p);RefCpu c(m);c.r[4]=owner;c.r[15]=stack+0xf000;c.pr=stop;
            std::vector<OriginalTuningMutation::CarCall> calls;for(unsigned address:{0x0c0287a0u,0x0c029040u})c.callHooks[address]=[&,address](auto& q){++hooks;calls.push_back({address,q.r[5],q.r[6]});};
            if(command==3){instructions+=c.run(0x0c071160,stop,1000);instructions+=c.run(0x0c0712c0,stop,1000);}
            else if(command==4){c.r[9]=owner;instructions+=c.run(0x0c070cc0,0x0c070e22,1000);}
            else instructions+=c.run(command==5?0x0c071200:0x0c071260,stop,1000);
            const auto changed=applyOriginalTuningCommand(p,data,command);compare(p);equal(unsigned(calls.size()),unsigned(changed.carCalls.size()),"Optional preview call count");
            for(unsigned i=0;i<calls.size();++i){equal(calls[i].address,changed.carCalls[i].address,"Optional ACar method");if(calls[i].address==0x0c0287a0){equal(calls[i].argument5,changed.carCalls[i].argument5,"Optional slot");equal(calls[i].argument6,changed.carCalls[i].argument6,"Optional value");}}++cases;
        }
    }
    {auto p=makeOriginalFreshBattleProfile();seed(p);RefCpu c(m);c.r[15]=stack+0xf000;c.pr=stop;instructions+=c.run(0x0c192760,stop,100);equal(c.r[0],879,"Optional child profile constructor value");c.r[4]=c.r[0];instructions+=c.run(0x0c1346a0,stop,100);equal(m.read32(profile+1176),879,"Optional child constructor profile store");}
    std::cout<<"PASS "<<cases<<" original tuning cases, "<<checks<<" exact comparisons, "<<instructions<<" original instructions; "<<hooks<<" explicit allocation/child initialization/ACar/PR1 division boundaries.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
