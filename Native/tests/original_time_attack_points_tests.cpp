#include "original_time_attack_points.h"
#include "original_record_rules.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3::original;
using namespace idas3::reference;
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("canonical-image required");
    RefMemory m(argv[1]);std::size_t checks=0,instructions=0,cases=0;
    const auto equal=[&](unsigned a,unsigned b,const char* label){++checks;if(a!=b)throw std::runtime_error(std::string(label)+": "+hex(a)+" vs "+hex(b));};
    constexpr unsigned profile=0x0c31c99c,owner=0x0d000000,result=0x0d001000,race=0x0d002000,
        timer=0x0d003000,backup=0x0d010000,stack=0x0d100000;
    for(unsigned row=0;row<18;++row){equal(m.read32(0x0c2a1194+8*row),1000,"Participation table");equal(m.read32(0x0c2a1198+8*row),2000,"Finish table");}
    for(unsigned i=0;i<3;++i)equal(m.read32(0x0c2a1224+i*4),i?500:1000,"Record bonus table");
    for(unsigned condition=0;condition<18;++condition)for(bool wet:{false,true})
    for(int status:{-1,0,1,2,3})for(unsigned pattern=0;pattern<8;++pattern)
    for(unsigned elapsed:{0u,900000u,0xffffffffu}){
        m.clear();m.zeroRegion(owner,0x5000);m.zeroRegion(backup,0x10000);m.zeroRegion(stack,0x10000);
        OriginalBattleProfile p=makeOriginalFreshBattleProfile();p.setu(0,1);p.setu(4,condition/2);p.setu(12,condition&1);p.setu(32,wet);
        const auto car=(condition*13+pattern)%35;p.setu(16,car);p.setByte(153,std::uint8_t(pattern));
        p.setu(72,pattern==7?0xfffffff0u:999998500u);
        const auto previous=[&](unsigned bit){return pattern&bit?elapsed+1:elapsed;};
        OriginalTimeAttackPointsInput input{status,elapsed,previous(1),previous(2),pattern&4?0:elapsed};
        const auto partition=originalRecordPartition(condition,wet);
        p.setu(176+partition.personalIndex()*4,input.previousPersonal6000);
        for(unsigned i=0;i<p.words.size();++i)m.write32(profile+i*4,p.words[i]);
        for(unsigned i=0;i<10;++i)m.write32(backup+partition.courseOffset(i),input.previousCourse6000);
        m.write32(backup+partition.modelOffset(car),input.previousModel6000);
        m.write32(owner+388,race);m.write32(race+92,unsigned(status));m.write32(race+80,timer);m.write32(timer+40,elapsed);
        RefCpu c(m);c.r[8]=owner+372;c.r[9]=profile;c.r[10]=1;c.r[11]=result;c.r[12]=0;
        c.r[14]=stack+0xe000;c.r[15]=c.r[14];m.write32(c.r[14]+20,owner);
        // Only the global backup-object getter is a supplied boundary. All
        // actual record index/unsigned comparison/bonus routines execute.
        c.callHooks[0x0c031560]=[&](auto& q){q.r[0]=backup;};
        instructions+=c.run(0x0c07de00,0x0c07df34,10000);
        const auto out=calculateOriginalTimeAttackPoints(p,input);
        equal(out.participation,m.read32(result+36),"Participation");equal(out.finish,m.read32(result+40),"Finish");
        equal(out.recordBonus,m.read32(result+44),"Record bonus");equal(out.total,m.read32(result+48),"Total");
        equal(out.balanceBeforeCap,m.read32(result+52),"Balance before cap with source wrap");
        for(unsigned offset:{72u,152u,176u,1180u})equal(p.u(offset),m.read32(profile+offset),"No score-stage profile mutation");
        ++cases;
    }
    std::cout<<"PASS "<<cases<<" original Time Attack point cases, "<<checks<<" comparisons, "<<instructions<<" actual instructions; only backup-pointer getter boundary, no point/record comparison hooks.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
