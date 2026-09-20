#include "original_tuning_child.h"
#include "sh4_scalar_reference.h"
#include <iostream>

using namespace idas3::original;
using namespace idas3::reference;
namespace {constexpr unsigned profile=0x0c31c99c,child=0x0d000000,owner=0x0d001000,textWidget=0x0d002000,vtable=0x0d003000,choice=0x0d004000,tls=0x0d005000,stack=0x0d100000,stop=0x00ff0000,textReset=0x00ff0020;}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::invalid_argument("canonical-image game-root required");
    RefMemory m(argv[1]);const auto data=OriginalTuningData::load(argv[2]);std::size_t checks=0,instructions=0,cases=0,frames=0,hooks=0;
    unsigned car=0,caseNumber=0,tick=0;
    const auto equal=[&](unsigned a,unsigned b,const char* label){++checks;if(a!=b)throw std::runtime_error(std::string(label)+" car "+std::to_string(car)+" case "+std::to_string(caseNumber)+" tick "+std::to_string(tick)+": "+hex(a)+" vs "+hex(b));};
    const auto runCase=[&](OriginalBattleProfile p,OriginalTuningChildKind kind,unsigned inputCase){
        m.clear();m.zeroRegion(child,0x10000);m.zeroRegion(stack,0x10000);
        m.write8(0x0c92ed00,0);
        for(unsigned i=0;i<p.words.size();++i)m.write32(profile+i*4,p.words[i]);
        auto s=beginOriginalTuningChild(p,data,kind);const auto& d=data.car(car);
        for(const auto [offset,value]:std::initializer_list<std::pair<unsigned,unsigned>>{{12,s.frame},{16,s.phase},{20,s.car},{28,s.package},{32,s.selected},{36,s.skip},{48,s.balance},{56,s.count},{88,s.flags}})m.write32(child+offset,value);
        m.write32(child+84,textWidget);m.write32(textWidget,vtable);m.write16(vtable+40,0);m.write32(vtable+44,textReset);m.write32(owner+268,owner+512);
        RefCpu c(m);c.r[15]=stack+0xf000;c.pr=stop;
        // Source09C200 selector initialization, including both allocated
        // hysteresis arrays; only allocator/TLS setup is outside this slice.
        if(kind==OriginalTuningChildKind::optionalPart){
            unsigned allocation=choice+128;c.callHooks[0x0c021960]=[&](auto& cpu){++hooks;cpu.r[0]=allocation;allocation+=128;};
            c.r[14]=stack+0xe000;c.r[15]=c.r[14];c.r[0]=tls+4;m.write32(c.r[14]+48,choice);m.write32(c.r[14]+52,2);m.write32(c.r[14]+56,1);m.write32(c.r[14]+64,tls);
            instructions+=c.run(0x0c09c264,0x0c09c3c2,5000);c.callHooks.erase(0x0c021960);
            m.write32(child+64,choice);m.write32(child+72,d.sourceRow[2]+s.optionalIndex*24);m.write32(child+80,s.choice);m.write32(child+84,s.optionalIndex);m.write32(child+88,0);
        }
        bool descriptionChanged=false;unsigned description=0;float descriptionX=0,descriptionY=0;std::vector<unsigned> cues;
        c.callHooks[0x0c0c6c20]=[&](auto& cpu){++hooks;descriptionX=cpu.getFloat(4);descriptionY=cpu.getFloat(5);};
        c.callHooks[0x0c0c6a00]=[&](auto& cpu){++hooks;descriptionChanged=true;description=cpu.r[5];};
        for(unsigned address:{textReset,0x0c0c6ca0u})c.callHooks[address]=[&](auto&){++hooks;};
        c.callHooks[0x0c141f80]=[&](auto& cpu){++hooks;cues.push_back(cpu.r[4]);};
        for(unsigned address:{0x0c028720u,0x0c0283c0u,0x0c028400u,0x0c028480u,0x0c0284c0u,0x0c028500u,0x0c028540u,0x0c028580u,0x0c0285c0u,0x0c0286c0u,0x0c029040u,0x0c0287a0u})c.callHooks[address]=[&](auto&){++hooks;};
        c.callHooks[0x0c2223e0]=[&](auto& cpu){++hooks;cpu.fpul=cpu.r[4]/cpu.r[5];};
        OriginalTuningChildInput input;
        c.callHooks[0x0c0d43a0]=[&](auto& cpu){++hooks;cpu.setFloat(0,input.selectionAxis);};
        c.callHooks[0x0c0d4300]=[&](auto& cpu){++hooks;cpu.r[0]=input.confirm;};
        c.callHooks[0x0c0d42c0]=[&](auto&){++hooks;};
        unsigned applied=0;
        for(tick=0;tick<6000;++tick){
            input={};if(kind==OriginalTuningChildKind::optionalPart){
                if(inputCase==0){input.selectionAxis=0;input.confirm=tick==20;}
                else if(inputCase==1)input.confirm=tick==20;
                else if(inputCase==3){input.selectionAxis=float(tick%101)/100.0f;input.confirm=tick==240;}
            }
            descriptionChanged=false;description=0;cues.clear();c.r[4]=child;c.r[15]=stack+0xf000;c.pr=stop;
            instructions+=c.run(kind==OriginalTuningChildKind::basic?0x0c115f60:kind==OriginalTuningChildKind::performance?0x0c116ea0:0x0c117f40,stop,10000);
            const unsigned command=c.r[0];const auto out=advanceOriginalTuningChild(s,p,data,input);
            equal(command,out.command,"Child command");equal(m.read32(child+12),s.frame,"Child frame");equal(m.read32(child+16),s.phase,"Child phase");equal(m.read32(child+48),s.balance,"Display balance");
            if(kind!=OriginalTuningChildKind::optionalPart){
                equal(m.read32(child+32),s.selected,"Selected step");equal(m.read32(child+36),s.skip,"Skipped attained steps");equal(m.read32(child+68),s.nextThreshold,"Next threshold");equal(m.read32(child+88),s.flags,"Drawing flags");
                unsigned sourceBase=kind==OriginalTuningChildKind::basic?d.packages[s.package].sourceAddress:d.sourceRow[6];unsigned stride=kind==OriginalTuningChildKind::basic?20:12;
                equal(m.read32(child+(kind==OriginalTuningChildKind::basic?76:104)),s.current<0?0:sourceBase+unsigned(s.current)*stride,"Current record");
                equal(m.read32(child+(kind==OriginalTuningChildKind::basic?80:108)),s.next<0?0:sourceBase+unsigned(s.next)*stride,"Next record");
                if(kind==OriginalTuningChildKind::basic){equal(m.read32(child+60),s.picture,"Part image");equal(m.read32(child+64),s.extraIndex,"Part extra image");equal(m.read32(child+92),std::bit_cast<unsigned>(s.completionX),"Completion X");equal(m.read32(child+96),std::bit_cast<unsigned>(s.completionY),"Completion Y");equal(m.read32(child+100),std::bit_cast<unsigned>(s.completionZ),"Completion Z");}
            }else{equal(m.read32(child+80),s.choice,"Choice");equal(m.read32(choice+4),s.choiceCursor,"Hysteresis cursor");equal(m.read32(child+88),s.balanceBeforeSpend,"Prior balance");equal(m.read32(profile+1176),p.u(1176),"Choice timeout");}
            equal(descriptionChanged,out.descriptionChanged,"Description event");if(descriptionChanged){equal(description,out.descriptionAddress,"Description text");equal(std::bit_cast<unsigned>(descriptionX),std::bit_cast<unsigned>(out.descriptionX),"Description X");equal(std::bit_cast<unsigned>(descriptionY),std::bit_cast<unsigned>(out.descriptionY),"Description Y");}
            equal(unsigned(cues.size()),unsigned(out.directCueIds.size()),"Direct cue count");for(unsigned i=0;i<cues.size();++i)equal(cues[i],out.directCueIds[i],"Direct cue");
            if(command>=1&&command<=6){
                c.r[4]=owner;c.r[15]=stack+0xf000;c.pr=stop;
                const unsigned entry=command==1?0x0c071300:command==2?0x0c0710e0:command==3?0x0c071160:command==4?0x0c070cc0:command==5?0x0c071200:0x0c071260;
                c.r[9]=owner;instructions+=c.run(entry,command==4?0x0c070e22:stop,10000);
                if(command==3)instructions+=c.run(0x0c0712c0,stop,1000);
                applyOriginalTuningCommand(p,data,command);if(command<=3)++applied;
            }
            for(unsigned offset:{72u,152u,156u,160u,164u,1176u,1180u})equal(m.read32(profile+offset),p.u(offset),"Profile after dispatch");
            ++frames;if(out.finished){if(kind==OriginalTuningChildKind::performance)equal(applied,1,"One performance upgrade per visit");break;}
        }
        if(tick==6000)throw std::runtime_error("Tuning child did not finish");
        for(unsigned i=0;i<p.words.size();++i)equal(m.read32(profile+i*4),p.words[i],"Final complete profile");++cases;
    };
    for(car=0;car<35;++car){const auto& d=data.car(car);
        for(unsigned package=0;package<d.packages.size();++package)for(unsigned step=0;step<d.packages[package].steps.size();++step){
            ++caseNumber;auto p=makeOriginalFreshBattleProfile();p.setu(16,car);p.setu(72,120000);p.setu(1180,3);p.setByte(152,std::uint8_t(package));p.setByte(153,std::uint8_t(step));runCase(p,OriginalTuningChildKind::basic,0);
        }
        for(unsigned step:{0u,57u}){++caseNumber;auto p=makeOriginalFreshBattleProfile();p.setu(16,car);p.setu(72,990000);p.setu(1180,0x403);p.setByte(153,std::uint8_t(step));runCase(p,OriginalTuningChildKind::performance,0);}
        if(!d.optional.empty())for(unsigned inputCase=0;inputCase<4;++inputCase){++caseNumber;auto p=makeOriginalFreshBattleProfile();p.setu(16,car);p.setu(72,990000);p.setu(1180,0xc03);p.setu(1176,879);runCase(p,OriginalTuningChildKind::optionalPart,inputCase);}
    }
    std::cout<<"PASS "<<cases<<" full original tuning visits, "<<frames<<" frames, "<<checks<<" exact comparisons, "<<instructions<<" original instructions; "<<hooks<<" allocator/text/input/audio/ACar/PR1 division boundary calls.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
