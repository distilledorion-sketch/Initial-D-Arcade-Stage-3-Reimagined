#include "original_tuning_candidate.h"
#include "sh4_scalar_reference.h"
#include <iostream>

using namespace idas3::original;
using namespace idas3::reference;
namespace {constexpr unsigned owner=0x0d000000,carObject=0x0d010000,cars=0x0d020000,
    stack=0x0d100000,profile=0x0c31c99c,stop=0x00ff0000;}
int main(int argc,char**argv)try{
    if(argc!=3)throw std::invalid_argument("canonical-image game-root required");
    RefMemory m(argv[1]);const auto data=OriginalTuningData::load(argv[2]);
    std::size_t checks=0,instructions=0,cases=0,rebuilds=0,observedSetters=0;unsigned currentCar=0,currentPackage=0,currentCount=0;
    const auto eq=[&](unsigned a,unsigned b,const char* why){++checks;if(a!=b)throw std::runtime_error(std::string(why)+" car="+std::to_string(currentCar)+" package="+std::to_string(currentPackage)+" count="+std::to_string(currentCount)+" source="+hex(a)+" native="+hex(b));};
    const auto run=[&](unsigned entry,unsigned argument5,unsigned oldWord){
        m.clear();m.zeroRegion(owner,0x30000);m.zeroRegion(stack,0x10000);m.zeroRegion(profile,0x1000);
        const unsigned slot=currentCar%4;m.write32(0x0c31ce2c,slot);m.write32(owner+448,cars);m.write32(cars+slot*4,carObject);
        m.write32(owner+484,currentCar);m.write32(owner+496,currentPackage);m.write32(owner+504,unsigned(data.car(currentCar).packages.size()));m.write32(owner+512,currentCount);
        OriginalCarAppearanceConfig config(currentCar);config.word=oldWord;config.materialVariant=7;
        m.write32(carObject+0x2d4,oldWord);m.write32(carObject+0x34c,currentCar);m.write32(carObject+0x6a8,7);
        for(unsigned i=0;i<6;++i)m.write32(carObject+0x2d8+4*i,config.variants[i]);
        RefCpu c(m);c.r[4]=owner;c.r[5]=argument5;c.r[15]=stack+0xf000;c.pr=stop;
        std::vector<OriginalTuningMutation::CarCall> calls;
        for(unsigned method:{0x0c0283c0u,0x0c028480u,0x0c028400u,0x0c0284c0u,0x0c028500u,0x0c028540u,0x0c028580u,0x0c0285c0u,0x0c0286c0u,0x0c028720u,0x0c028760u,0x0c0286a0u})
            c.callHooks[method]=[&,method](auto&q){
                ++observedSetters;
                eq(q.r[4],carObject,"Actual selected preview object");calls.push_back({method,q.r[5],q.r[6]});
                // Observe the call then execute the unchanged actual setter.
                // Only029040's resource/material rebuild remains substituted.
                auto hooks=std::move(q.callHooks);q.callHooks.clear();instructions+=q.run(method,q.pr,1000);q.callHooks=std::move(hooks);
            };
        c.callHooks[0x0c029040]=[&](auto&q){eq(q.r[4],carObject,"Rebuild target");calls.push_back({0x0c029040,0,0});++rebuilds;};
        instructions+=c.run(entry,stop,20000);
        const auto expected=entry==0x0c12b440?originalTuningStockAppearanceCalls(argument5==1):originalTuningCandidateAppearance(data,currentCar,currentPackage,currentCount).carCalls;
        eq(unsigned(calls.size()),unsigned(expected.size()),"Ordered call count");
        for(unsigned i=0;i<calls.size();++i){eq(calls[i].address,expected[i].address,"Ordered method");if(calls[i].address!=0x0c029040){eq(calls[i].argument5,expected[i].argument5,"Setter value");applyOriginalCarAppearanceCall(config,expected[i].address,expected[i].argument5,expected[i].argument6);}}
        eq(m.read32(carObject+0x2d4),config.word,"Actual final appearance word");eq(m.read32(carObject+0x6a8),config.materialVariant,"Material variant");
        eq(m.read8(carObject+0x6b9),0,"Preserved clean factory paint");eq(config.word&0x0e000040u,oldWord&0x0e000040u,"Preserved factory paint and bit6");
        if(entry==0x0c12b520)eq(m.read32(carObject+224),2,"Source presentation state224");
        eq(m.read32(owner+484),currentCar,"Owner car unchanged");eq(m.read32(owner+496),currentPackage,"Owner package unchanged");
        for(unsigned off=0;off<OriginalBattleProfile{}.words.size()*4;off+=4)eq(m.read32(profile+off),off==1168?slot:0,"No saved profile mutation");
        ++cases;
    };
    for(currentCar=0;currentCar<35;++currentCar){
        currentPackage=currentCount=0;
        for(unsigned flag:{0u,1u,2u})for(unsigned word:{0u,0xffffffffu,0x5a695a69u})run(0x0c12b440,flag,word);
        for(currentPackage=0;currentPackage<data.car(currentCar).packages.size();++currentPackage){
            const auto count=unsigned(data.car(currentCar).packages[currentPackage].steps.size());
            for(currentCount=0;currentCount<=count;++currentCount)for(unsigned word:{0u,0xffffffffu,0x5a695a69u})run(0x0c12b520,0,word);
            const auto full=originalTuningCandidateAppearance(data,currentCar,currentPackage);eq(unsigned(full.carCalls.size()),unsigned(originalTuningCandidateAppearance(data,currentCar,currentPackage,count).carCalls.size()),"Default all-step preview");
            bool rejected=false;try{originalTuningCandidateAppearance(data,currentCar,currentPackage,count+1);}catch(const std::out_of_range&){rejected=true;}eq(rejected,1,"Reject out-of-bank count");
        }
    }
    std::cout<<"PASS "<<cases<<" original candidate/reset cases, "<<checks<<" comparisons, "<<instructions-observedSetters-rebuilds<<" actual bounded SH4 instructions, "<<observedSetters<<" observed actual setters, "<<rebuilds<<" explicit resource rebuild boundaries;35cars/all146packages/everyprefix; no profile writes.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
