#include "original_record_rules.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3::original;
using namespace idas3::reference;
void require(bool b,const char* message){if(!b)throw std::runtime_error(message);}
int main(int argc,char**argv){try{
    if(argc!=2)throw std::runtime_error("canonical-image required");RefMemory m(argv[1]);
    constexpr unsigned backup=0x0d000000,record=0x0d010000,stack=0x0d020000,stop=0x00ff0000;
    std::size_t instructions=0,checks=0;
    // Full source reset, with only display-name/car-metadata/random/backup
    // output boundaries. All nested course/route/weather/bank/row loops and
    // default-time loads/multiplication/writes execute original instructions.
    m.clear();m.zeroRegion(backup,0x10000);m.zeroRegion(stack,0x10000);
    RefCpu reset(m);reset.r[4]=backup;reset.r[15]=stack+0xf000;reset.pr=stop;
    for(unsigned a:{0x0c1920c0u,0x0c192160u,0x0c192200u,0x0c192220u,0x0c1f9e60u})reset.callHooks[a]=[](auto& c){c.r[0]=0;};
    reset.callHooks[0x0c0314c0]=[&](auto& c){for(unsigned i=0;i<6;i++)m.write32(c.r[4]+i*4,0);};
    reset.callHooks[0x0c032da0]=[](auto&){};reset.callHooks[0x0c031760]=[](auto&){};
    instructions+=reset.run(0x0c031860,stop,2000000);
    for(unsigned condition=0;condition<18;condition++)for(bool wet:{false,true}){
        const auto p=originalRecordPartition(condition,wet);const auto time=originalDefaultTimeAttackRecord6000(condition);
        require(time==m.read32(0x0c2ef2b8+16*p.scene)*6000,"default record source table");++checks;
        for(unsigned row=0;row<10;row++){require(m.read32(backup+p.courseOffset(row))==time,"source course reset clock");++checks;}
        for(unsigned car=0;car<35;car++){require(m.read32(backup+p.modelOffset(car))==time,"source model reset clock");++checks;}
    }
    for(unsigned condition=0;condition<18;condition++)for(bool wet:{false,true}){
        const auto p=originalRecordPartition(condition,wet);
        for(unsigned row=0;row<35;row++){
            for(bool model:{false,true}){
                if(!model&&row>=10)continue;RefCpu c(m);c.r[4]=backup;c.r[5]=p.scene;c.r[6]=p.route;c.r[7]=p.weather;c.r[15]=stack+0xf000;c.pr=stop;m.write32(c.r[15]+4,row);
                instructions+=c.run(model?0x0c0326c0:0x0c032680,stop,1000);require(c.r[0]==backup+(model?p.modelOffset(row):p.courseOffset(row)),"original record offset");++checks;
            }
        }
        RefCpu c(m);c.r[4]=p.scene;c.r[5]=p.route;c.r[6]=p.weather;c.r[15]=stack+0xf000;c.pr=stop;
        instructions+=c.run(0x0c134820,stop,1000);require(c.r[0]==p.personalIndex(),"personal record index");++checks;
    }
    const auto p=originalRecordPartition(6,false);
    const std::array<std::uint32_t,10> times{0,1,6000,6000,12000,900000,1000000,2000000,0xfffffffe,0xffffffff};
    for(unsigned i=0;i<10;i++)m.write32(backup+p.courseOffset(i),times[i]);
    for(unsigned candidate:{0u,1u,2u,5999u,6000u,6001u,1000000u,0xfffffffeu,0xffffffffu}){
        RefCpu c(m);c.r[4]=backup;c.r[5]=p.scene;c.r[6]=p.route;c.r[7]=p.weather;c.r[15]=stack+0xf000;c.pr=stop;m.write32(c.r[15]+4,record);m.write32(record,candidate);
        instructions+=c.run(0x0c032000,stop,1000);require(c.r[0]==originalTimeAttackRank(times,candidate),"original rank tie/order");++checks;
        for(unsigned car=0;car<35;car++)for(unsigned old:{0u,6000u,0xffffffffu}){
            m.write32(backup+p.modelOffset(car),old);m.write32(record+8,car<<26);RefCpu model(m);model.r[4]=backup;model.r[5]=p.scene;model.r[6]=p.route;model.r[7]=p.weather;model.r[15]=stack+0xf000;model.pr=stop;m.write32(model.r[15]+4,record);
            instructions+=model.run(0x0c032060,stop,1000);require(model.r[0]==unsigned(originalModelRecordImproved(old,candidate)),"model record unsigned/tie comparison");++checks;
        }
    }
    // ARegistTA's complete personal-card branch. Stop at its next global
    // leaderboard branch; only logging and the external section getter hook.
    constexpr unsigned profile=0x0c31c99c,locals=0x0d030000,owner=0x0d040000,
        parent=0x0d050000,sections=0x0d060000,finish=0x0d070000;
    for(unsigned condition=0;condition<18;++condition)for(bool wet:{false,true})
    for(unsigned flags:{0u,1u,2u,3u,128u,129u})for(unsigned old:{0u,100000u,200000u})
    for(unsigned candidate:{100000u,150000u})for(unsigned count:{0u,2u,3u}){
        const auto partition=originalRecordPartition(condition,wet);auto card=makeOriginalFreshBattleProfile();
        // Distinct values in every word expose accidental writes to another
        // course, another record field, or unrelated driver progress.
        for(unsigned i=0;i<card.words.size();++i)card.words[i]=0x70000000+i;
        card.setu(4,partition.scene);card.setu(12,partition.route);card.setu(32,partition.weather);
        card.setu(8,condition&1);card.setu(1180,flags);card.setu(176+4*partition.personalIndex(),old);
        for(unsigned i=0;i<card.words.size();++i)m.write32(profile+4*i,card.words[i]);
        m.zeroRegion(locals,0x100);m.zeroRegion(owner,0x100);m.zeroRegion(parent,0x100);
        m.zeroRegion(sections,0x100);m.zeroRegion(finish,0x100);m.zeroRegion(stack,0x10000);
        m.write32(locals+192,owner);m.write32(owner+80,parent);m.write32(parent+80,sections);
        m.write32(sections+36,count);m.write32(locals+196,finish);m.write32(finish,candidate);
        const std::array<std::uint32_t,3> intermediate{22000,54000,83000};
        RefCpu c(m);c.r[14]=locals;c.r[15]=stack+0xf000;c.pr=stop;
        c.callHooks[0x0c055d60]=[](auto& cpu){cpu.r[0]=0;};
        c.callHooks[0x0c09b660]=[&](auto& cpu){if(cpu.r[4]!=sections||cpu.r[5]>=count)throw std::runtime_error("Original section query escaped fixture");cpu.r[0]=intermediate.at(cpu.r[5]);};
        // Count0 jumps directly toE962; E960 only loads the next branch's
        // local offset. Both paths must stop before global-board handling.
        instructions+=c.run(0x0c07e83a,0x0c07e962,3000);
        const auto before=card.words;
        const bool changed=registerOriginalPersonalTimeAttackRecord(card,partition,candidate,condition&1,{intermediate.data(),count});
        require(changed==(card.words!=before),"personal result changed indication");++checks;
        for(unsigned i=0;i<card.words.size();++i){require(m.read32(profile+4*i)==card.words[i],"original personal record branch word parity");++checks;}
        const auto saved=originalPersonalTimeAttackRecord(card,partition);
        require(saved.ticks6000==m.read32(0x0c31ca4c+4*partition.personalIndex())&&saved.night==m.read32(0x0c31cadc+4*partition.personalIndex()),"personal record source reader");++checks;
    }
    auto driverA=makeOriginalFreshBattleProfile(),driverB=driverA;driverA.setu(1180,129);driverB.setu(1180,129);
    require(registerOriginalPersonalTimeAttackRecord(driverA,p,900000,0,{}),"first driver personal record");
    require(originalPersonalTimeAttackRecord(driverB,p).ticks6000==0,"different save slot has independent personal record");checks+=2;
    std::cout<<"PASS original record defaults/partitions/ranking/personal card: "<<checks<<" comparisons, "<<instructions<<" actual instructions. Reset metadata hooks declared; personal branch hooks only logging/section getter.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
