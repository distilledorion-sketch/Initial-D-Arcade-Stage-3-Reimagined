#include "original_bunta_results.h"
#include "sh4_scalar_reference.h"
#include <array>
#include <iostream>

using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned profile=0x0c31c99c,owner=0x0d000000,result=0x0d001000,
    race=0x0d002000,times=0x0d003000,output=0x0d004000,stack=0x0d100000,stop=0xff0000;
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("canonical-image required");
    RefMemory m(argv[1]);std::size_t checks=0,instructions=0,cases=0,hooks=0;
    const auto equal=[&](unsigned a,unsigned b,const char* label){++checks;if(a!=b)throw std::runtime_error(std::string(label)+" "+hex(a)+" vs "+hex(b));};
    const auto seed=[&](const OriginalBattleProfile& p){m.clear();m.zeroRegion(stack,0x10000);m.zeroRegion(owner,0x5000);for(unsigned i=0;i<p.words.size();++i)m.write32(profile+i*4,p.words[i]);};
    const auto compare=[&](const OriginalBattleProfile& p){for(unsigned i=0;i<p.words.size();++i)equal(m.read32(profile+i*4),p.words[i],"Full profile");};
    for(unsigned course=0;course<9;++course)for(unsigned level:{0u,1u,4u,5u,9u,10u,14u,15u,16u,17u,0xffffffffu,0x7fffffffu,0x80000000u})
    for(unsigned finish:{0u,1u,128u,255u})for(unsigned outcome:{0u,1u,0xffffffffu}){
        auto p=makeOriginalFreshBattleProfile();p.setu(0,2);p.setu(4,course);p.setu(1180,0xa55a5aa5u);
        for(unsigned i=0;i<8;++i)p.setu(1080+i*4,i==((course==8)?3:course)?level:7);
        seed(p);m.write32(owner+80,race);m.write8(race+1572,std::uint8_t(finish));m.write32(race+1644,outcome);
        RefCpu c(m);c.r[14]=stack+0xf000;c.r[15]=c.r[14];c.r[9]=0x0c31ce18;c.r[12]=0x0c31ce45;c.r[11]=owner+64;c.r[10]=1;c.r[13]=2;c.r[2]=0xffdfffffu;
        for(unsigned entry:{0x0c141e40u,0x0c06a520u,0x0c061ae0u,0x0c141d40u})c.callHooks[entry]=[&](auto&){++hooks;};
        instructions+=c.run(0x0c05cebc,0x0c05cfac,10000);
        equal(recordOriginalBuntaResult(p,std::uint8_t(finish),outcome),finish&&outcome==0,"Bunta win classification");compare(p);++cases;
    }
    const std::array<float,15> advantages={0.f,-0.f,-500.f,.001f,.125f,1.f,12.345f,149.999f,150.f,1000.f,1.e20f,
        std::bit_cast<float>(0x7f800000u),std::bit_cast<float>(0xff800000u),std::bit_cast<float>(0x7fc01234u),std::bit_cast<float>(0x7f7fffffu)};
    const std::array<unsigned,9> balances={0,999,1000,3999,4000,999999998,999999999,0xfffffff0,0x80000000};
    for(unsigned course=0;course<9;++course)for(unsigned level:{0u,4u,5u,9u,10u,15u,16u,0xffffffffu})
    for(unsigned status:{0u,1u,2u,3u})for(unsigned balance:balances)for(float advantage:advantages){
        auto p=makeOriginalFreshBattleProfile();p.setu(0,2);p.setu(4,course);p.setu(72,balance);
        for(unsigned i=0;i<8;++i)p.setu(1080+i*4,level);
        seed(p);m.write32(owner+388,result);m.write32(result+80,status);m.writeFloat(result+84,advantage);m.write32(result+88,times);
        m.write32(times+16,4);m.write32(times+40,480000);
        for(unsigned i=0;i<4;++i)m.write32(times+i*4,100000+i*100000);
        RefCpu c(m);c.r[4]=owner;c.r[5]=output;c.r[15]=stack+0xf000;c.pr=stop;
        instructions+=c.run(0x0c187fa0,stop,10000);
        const auto points=calculateOriginalBuntaPoints(p,status,advantage);
        const std::array<unsigned,5> fields={points.participation,points.win,points.advantage,points.total,points.balanceBeforeCap};
        for(unsigned i=0;i<fields.size();++i)equal(m.read32(output+36+i*4),fields[i],"Bunta point field");
        equal(m.read8(output+56),points.deduction,"Bunta point deduction flag");
        c.r[14]=stack+0xf000;c.r[15]=c.r[14];m.write32(c.r[14]+336,owner);m.write32(owner+252,output);
        instructions+=c.run(0x0c06fc48,0x0c06fca4,1000);awardOriginalBuntaPoints(p,status,advantage);compare(p);++cases;
    }
    std::cout<<"PASS "<<cases<<" Bunta result/progression/scoring cases, "<<checks<<" exact comparisons, "<<instructions
        <<" original instructions, "<<hooks<<" explicit result-animation/state-notification hooks; full187FA0 scoring/getters/common balance commit have zero hooks.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
