#include "original_legend_menu.h"
#include "original_battle_profile.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <map>

using namespace idas3::original;
using namespace idas3::reference;
struct Matrix {float x=0,y=0,z=0,scale=1;};
int main(int argc,char** argv)try{
    if(argc!=2)throw std::runtime_error("canonical-image required");RefMemory m(argv[1]);std::size_t cases=0,checks=0,instructions=0,submissions=0;
    const auto equal=[&](unsigned actual,unsigned expected,const std::string& label){++checks;if(actual!=expected)throw std::runtime_error(label+" expected="+hex(expected)+" actual="+hex(actual));};
    constexpr unsigned object=0x0D000000,stack=0x0D010000,stop=0x00FF0000,selectedBuffer=0x0D020000,inactiveBuffer=0x0D020100;
    for(unsigned enemy=0;enemy<31;++enemy){
        const auto metadata=originalLegendStartMetadata(enemy);const auto course=originalRival(enemy).course;
        unsigned choice=0;while(choice<6&&originalLegendRivalId(course,choice)!=enemy)++choice;
        auto p=makeOriginalFreshBattleProfile();p.setu(1180,0x0800c000);selectOriginalRival(p,enemy);
        const auto draws=originalLegendMenuDraws(p,course,choice,0,0);
        const auto has=[&](unsigned value){for(const auto& d:draws)if(d.selector==value)return true;return false;};
        constexpr unsigned directions[]{2,6,14,1,12,8};
        if(metadata.direction>=6||!has(0x50000+directions[metadata.direction])||!has(metadata.extra?0x50016:metadata.race?0x5000f+metadata.race:0x50015))throw std::runtime_error("Start metadata differs from original opponent screen");
        // Independently cross-check start2d's encounter table, instead of
        // allowing the menu and banner to share an inverted semantic label.
        constexpr unsigned sourceDirections[]{5,4,1,0,2,3,6};
        const auto sourceDirection=m.read32(0x0c2fc77c+16*enemy+8);
        equal(metadata.direction,sourceDirections[sourceDirection],"Start encounter direction");
    }
    if(originalLegendStartMetadata(1).race!=2||originalLegendStartMetadata(2).race!=3||!originalLegendStartMetadata(30).extra||originalLegendStartMetadata(29).race!=0)throw std::runtime_error("Kenji/Shingo/Extra/Final metadata");
    for(unsigned progressCase=0;progressCase<9;++progressCase)for(unsigned course=0;course<9;++course){
        auto saved=makeOriginalFreshBattleProfile();
        if(progressCase){
            const unsigned bits=progressCase-1;
            saved.setu(1180,((bits&1)?0x8000u:0u)|((bits&2)?0x4000u:0u)|((bits&4)?0x08000000u:0u));
            constexpr std::array<unsigned,8> clearBytes={0u,1u,15u,16u,17u,32u,47u,255u};
            for(unsigned i=0;i<31;++i)saved.setByte(116+i,std::uint8_t(clearBytes[(i+progressCase)%clearBytes.size()]));
        }
        const auto count=originalLegendChoices(saved,course).count;
        for(unsigned choice=0;choice<count;++choice)for(float phase:{0.f,.009999999f,.01f,.010000001f,.5f,1.f})for(unsigned frame=0;frame<8;++frame){
            auto profile=saved;profile.setu(4,course);selectOriginalRival(profile,originalLegendRivalId(course,choice));
            m.clear();m.zeroRegion(object,2048);m.zeroRegion(stack,0x10000);for(unsigned i=0;i<307;++i)m.write32(0x0C31C99C+i*4,profile.words[i]);
            m.write32(object+20,selectedBuffer);m.write32(object+24,inactiveBuffer);m.write32(object+28,choice);m.writeFloat(object+32,phase);m.write32(object+36,frame);
            RefCpu cpu(m);cpu.r[4]=object;cpu.r[15]=stack+0xF000;cpu.pr=stop;Matrix matrix;std::vector<Matrix> matrices;std::map<unsigned,unsigned> colors;
            const auto expected=originalLegendMenuDraws(profile,course,choice,phase,frame);std::size_t next=0;
            if(progressCase==0){
                const auto fresh=originalLegendMenuDraws(course,choice,phase,frame);
                equal(unsigned(fresh.size()),unsigned(expected.size()),"Fresh compatibility count");
                for(unsigned i=0;i<fresh.size();++i)equal(fresh[i].selector,expected[i].selector,"Fresh compatibility selector");
            }
            for(unsigned entry:{0x0C1B8FE0u,0x0C1B90E0u,0x0C1BAF40u,0x0C1BB6E0u})cpu.callHooks[entry]=[](auto& c){c.r[0]=1;};
            cpu.callHooks[0x0C05A8E0]=[](auto& c){c.r[0]=c.r[5];};
            cpu.callHooks[0x0C1F6610]=[&](auto&){matrices.push_back(matrix);};
            cpu.callHooks[0x0C1F65C0]=[&](auto&){if(matrices.empty())throw std::runtime_error("Matrix underflow");matrix=matrices.back();matrices.pop_back();};
            cpu.callHooks[0x0C1F6AC0]=[&](auto& c){matrix.x+=matrix.scale*c.getFloat(4);matrix.y+=matrix.scale*c.getFloat(5);matrix.z+=c.getFloat(6);};
            cpu.callHooks[0x0C1F69D0]=[&](auto& c){equal(std::bit_cast<unsigned>(c.getFloat(4)),std::bit_cast<unsigned>(c.getFloat(5)),"Uniform scale");matrix.scale*=c.getFloat(4);};
            cpu.callHooks[0x0C1B8240]=[&](auto& c){if(c.r[4]!=selectedBuffer&&c.r[4]!=inactiveBuffer)throw std::runtime_error("Unknown source color buffer");colors[c.r[5]]=c.r[4]==selectedBuffer?1:2;};
            cpu.callHooks[0x0C1D7120]=[&](auto& c){
                if(next>=expected.size())throw std::runtime_error("Extra original Legend draw");const auto& e=expected[next++];
                const auto context="saved="+std::to_string(progressCase)+" course="+std::to_string(course)+" choice="+std::to_string(choice)+" phase="+std::to_string(phase)+" frame="+std::to_string(frame)+" draw="+std::to_string(next);
                equal(e.selector,c.r[4],context+" selector");equal(std::bit_cast<unsigned>(e.x),std::bit_cast<unsigned>(matrix.x),context+" X");equal(std::bit_cast<unsigned>(e.y),std::bit_cast<unsigned>(matrix.y),context+" Y");equal(std::bit_cast<unsigned>(e.z),std::bit_cast<unsigned>(matrix.z),context+" Z");equal(std::bit_cast<unsigned>(e.scale),std::bit_cast<unsigned>(matrix.scale),context+" scale");
                const unsigned color=e.colors==OriginalChoiceColors::Source?0:e.colors==OriginalChoiceColors::Selected?1:e.colors==OriginalChoiceColors::InactiveTransmission?2:99;equal(color,colors[c.r[4]],context+" color");++submissions;
            };
            instructions+=cpu.run(0x0C196C00,stop,100000);cpu.r[4]=object;instructions+=cpu.run(0x0C196CE0,stop,100000);
            equal(unsigned(next),unsigned(expected.size()),"All native draws submitted");equal(m.read32(object+36),frame+1,"Original caller-owned frame increment");if(!matrices.empty())throw std::runtime_error("Legend matrix imbalance");++cases;
        }
    }
    // Constructor196710..196746: exact inactive four-vertex color writes.
    m.clear();m.zeroRegion(object,512);m.write32(object+240,object+256);m.write32(object+256+24,inactiveBuffer);
    RefCpu constructor(m);constructor.r[14]=object;std::array<unsigned,4> values{};unsigned writes=0;
    constructor.callHooks[0x0C1B81C0]=[&](auto& c){equal(c.r[4],inactiveBuffer,"Constructor inactive buffer");if(c.r[5]>=4)throw std::runtime_error("Color index");values[c.r[5]]=c.r[6];++writes;};
    instructions+=constructor.run(0x0C196710,0x0C196748,1000);equal(writes,4,"Four color writes");
    for(unsigned i=0;i<4;++i)equal(values[i],i&1?0xCCCCCCCCu:0u,"Alternating inactive color");
    std::cout<<"PASS "<<cases<<" complete196C00/196CE0 cases, "<<checks<<" bit comparisons, "<<instructions<<" original instructions, "<<submissions<<" draws. All31 choices, every unlock-bit combination, saved clear/rematch states, fractional confirmation and8 blink frames; source selectors/FMAC layout/profile decisions execute directly, graphics/bank/matrix boundaries explicit.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
