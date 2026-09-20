#include "original_car_appearance_config.h"
#include "original_tuning.h"
#include "sh4_scalar_reference.h"
#include <iostream>

using namespace idas3::original;
using namespace idas3::reference;
namespace {constexpr unsigned object=0x0d000000,stack=0x0d100000,scene=0x0d002000,frame=0x0d008000,profileAddress=0x0c31c99c,stop=0x00ff0000;}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::invalid_argument("canonical-image game-root required");
    RefMemory m(argv[1]);const auto data=OriginalTuningData::load(argv[2]);
    std::size_t cases=0,checks=0,instructions=0;unsigned car=0,method=0,value=0;
    const auto equal=[&](unsigned a,unsigned b,const char* label){++checks;if(a!=b)throw std::runtime_error(std::string(label)+" car="+std::to_string(car)+" method="+hex(method)+" value="+std::to_string(value)+" source="+hex(a)+" native="+hex(b));};
    const auto init=[&](){
        m.clear();m.zeroRegion(object,0x10000);m.zeroRegion(stack,0x10000);
        RefCpu c(m);c.r[15]=stack+0xf000;c.pr=stop;c.r[4]=object+0x2d4;
        instructions+=c.run(0x0c228dc0,stop);
        c.r[14]=frame;m.write32(frame+48,object);m.write32(frame+52,car);
        instructions+=c.run(0x0c026436,0x0c0264bc);
        const OriginalCarAppearanceConfig native(car);
        equal(m.read32(object+0x2d4),native.word,"Constructor word");equal(m.read32(object+0x34c),native.car,"Constructor car");
        for(unsigned i=0;i<6;++i)equal(m.read32(object+0x2d8+i*4),native.variants[i],"Constructor variant map");
        return c;
    };
    const auto compare=[&](const OriginalCarAppearanceConfig& native){
        equal(m.read32(object+0x2d4),native.word,"Appearance word");
        equal(m.read32(object+0x6a8),native.materialVariant,"Material variant");
        equal(m.read8(object+0x6b9),native.paintDirty?1:0,"Paint dirty flag");
    };
    constexpr unsigned methods[]{0x0c0283c0,0x0c028400,0x0c028440,0x0c028480,0x0c0284c0,0x0c028500,0x0c028540,0x0c028580,0x0c0285c0,0x0c028660,0x0c0286a0,0x0c0286c0,0x0c028720,0x0c028760};
    // Every possible saved byte with nonzero pre-existing fields. The two
    // original seven-entry wheel maps are bounded to their valid indices.
    for(car=0;car<35;++car){auto c=init();for(auto entry:methods){method=entry;
        for(value=0;value<256;++value){
            if(method==0x0c0285c0&&(car==19||car==27)&&value>=7)continue;
            for(unsigned word:{0u,0xffffffffu,0x5a695a69u}){
                OriginalCarAppearanceConfig native(car);native.word=word;
                m.write32(object+0x2d4,word);m.write32(object+0x6a8,0);m.write8(object+0x6b9,0);
                c.r[4]=object;c.r[5]=value;c.r[15]=stack+0xf000;c.pr=stop;
                instructions+=c.run(method,stop);applyOriginalCarAppearanceCall(native,method,value);compare(native);++cases;
            }
        }
    }}
    // Repeated result-owner dispatch checks accumulation and slot8's no-op.
    for(car=0;car<35;++car){auto c=init();OriginalCarAppearanceConfig native(car);
        for(unsigned cycle=0;cycle<6;++cycle)for(unsigned slot=0;slot<13;++slot){
            method=0x0c0287a0;value=(cycle+slot)%6;
            c.r[4]=object;c.r[5]=slot;c.r[6]=value;c.r[15]=stack+0xf000;c.pr=stop;
            instructions+=c.run(method,stop);applyOriginalCarAppearanceCall(native,method,slot,value);compare(native);++cases;
        }
    }
    // Full source player-owner block, not a list of mocked setters. Covers
    // all authored basic packages and each optional candidate on every car.
    for(car=0;car<35;++car){
        std::vector<OriginalBattleProfile> profiles;
        auto fresh=makeOriginalFreshBattleProfile();fresh.setu(16,car);profiles.push_back(fresh);
        for(unsigned package=0;package<data.car(car).packages.size();++package){auto p=fresh;
            p.setByte(152,std::uint8_t(package));p.setByte(153,0);p.setu(1180,0);
            for(unsigned visit=0;!(p.u(1180)&0x400);++visit){
                if(visit>=data.car(car).packages[package].steps.size())throw std::runtime_error("Basic package failed to finish");
                applyOriginalTuningCommand(p,data,1);profiles.push_back(p);
            }
        }
        for(unsigned index=0;index<data.car(car).optional.size();++index){auto p=fresh;p.setByte(154,std::uint8_t(index));p.setu(72,1000000);applyOriginalTuningCommand(p,data,3);profiles.push_back(p);}
        for(unsigned i=0;i<64;++i){auto p=fresh;for(unsigned slot=0;slot<12;++slot)p.setByte(156+slot,std::uint8_t((i+slot)%6));p.setByte(163,std::uint8_t(i%7));p.setByte(165,std::uint8_t(i%4));p.setByte(166,std::uint8_t(i%4));profiles.push_back(p);}
        for(auto p:profiles)for(unsigned color=0;color<data.car(car).colors.size();++color){
            auto c=init();p.setu(64,color);for(unsigned i=0;i<p.words.size();++i)m.write32(profileAddress+i*4,p.words[i]);
            m.write32(scene+1048,object);m.write32(scene+1652,3);m.write32(scene+1664,2);
            c.r[10]=scene+1020;c.r[13]=scene;c.r[9]=scene;c.r[15]=stack+0xf000;c.pr=stop;
            method=0x0c0630b4;value=color;instructions+=c.run(method,0x0c06316e,10000);
            compare(originalPlayerAppearanceConfig(p,8));++cases;
        }
    }
    std::cout<<"PASS "<<cases<<" original appearance config cases, "<<checks<<" exact comparisons, "<<instructions<<" original instructions; no call hooks. Geometry/material rebuild remains separate.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
