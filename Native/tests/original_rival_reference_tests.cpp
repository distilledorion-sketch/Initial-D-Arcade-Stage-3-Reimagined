#include "original_rival.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <random>

using namespace idas3::original;
using namespace idas3::reference;
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("Pass canonical image and project data directory");
    RefMemory memory(argv[1]);const auto root=std::filesystem::path(argv[2])/"original_rival";
    const auto data=OriginalRivalData::load(root);
    std::size_t checks=0,instructions=0,cases=0,fractionalCorrections=0;
    const auto equal=[&](std::uint32_t actual,std::uint32_t expected,const std::string& context){
        if(actual!=expected)throw std::runtime_error(context+" expected="+hex(expected)+" actual="+hex(actual));++checks;
    };
    for(std::uint32_t a=0x0c271618;a<0x0c283de4;a+=4)equal(data.word(a),memory.read32(a),"Exported original table "+hex(a));
    std::mt19937 random(0x15b0a0);
    constexpr std::uint32_t rivalBase=0x0c901c6c+716,publicBase=0x0c90172c+168,playerBase=0x0c900f00,actorBase=0x0c9008a4,pathBase=0x0cd00000;
    for(std::uint32_t condition=0;condition<18;++condition){
        const auto path=data.loadPath(root,condition);
        // Verify complete source capacity, including the lookahead after the
        // shorter player-valid prefix. Source stem comes from042700's table.
        const auto slot=0x0c2eff40+condition*1024+7*64;
        std::string stem;for(std::uint32_t j=0;j<64&&memory.read8(slot+j);++j)stem.push_back(char(memory.read8(slot+j)));
        stem=std::filesystem::path(stem).filename().string().substr(5)+(condition&1?"o":"i");
        const auto sourcePath=std::filesystem::path(argv[1]).parent_path()/"driveA/HOSTFS/binary"/("PATH_"+stem+"_0.bin");
        std::ifstream source(sourcePath,std::ios::binary);if(!source)throw std::runtime_error("Missing rival path source "+sourcePath.string());
        for(const auto& point:path.points)for(float value:point){std::uint32_t expected=0;source.read(reinterpret_cast<char*>(&expected),4);if(!source)throw std::runtime_error("Rival path source short");equal(std::bit_cast<std::uint32_t>(value),expected,"Rival path capacity");}
        for(std::uint32_t profile=0;profile<32;++profile)for(std::uint32_t variant=0;variant<16;++variant){
            memory.clear();memory.zeroRegion(0x0c8ff000,0x1c0000);memory.zeroRegion(0x0cff0000,0x10000);
            for(std::size_t i=0;i<path.points.size();++i)for(unsigned k=0;k<3;++k)memory.writeFloat(pathBase+std::uint32_t(i*12+k*4),path.points[i][k]);
            OriginalRivalState rival;OriginalActorState pub,actor;OriginalDriveState player;
            for(auto& w:rival.words)w=random();for(auto& w:pub.words)w=random();for(auto& w:actor.words)w=random();for(auto& w:player.words)w=random();
            const auto index=variant==0?0:variant==1?path.inclusiveLastIndex:variant==2?path.inclusiveLastIndex-5:(profile*19+variant*29)%path.inclusiveLastIndex;
            rival.setu(0,variant==11?0:1);rival.setu(12,index);rival.setu(16,variant==10?1:0);
            rival.setf(200,path.points[index][0]+float(variant%3)*.125f);rival.setf(204,path.points[index][1]);rival.setf(208,path.points[index][2]-float(variant%5)*.125f);
            rival.setf(68,variant==4?-2.f:variant==5?150.f:20.f+float(profile*3));
            if(variant>=12)rival.setf(68,80.f+float(profile*5)+float(variant-12)*.375f);
            player.setu(0x118,index+(variant%3==0?0:variant%3==1?100u:std::uint32_t(-100)));
            player.setu(0x434,variant&1);player.setu(0x438,(variant>>1)&1);player.setu(0x1a8,variant==7?1:0);
            player.setf(0x248,.75f);player.setf(0x250,200.f);actor.setu(0x50,variant==9?0:0x8000);
            OriginalRivalPaceInputs input;input.condition0C9015CC=condition;input.profile0CAA9868=profile;
            input.level0C9015D0=variant+profile;input.opponentProgress0C901644=profile+variant;
            for(auto& w:input.progress0C901604)w=random();
            std::uint32_t ticks=variant%4==0?778:variant%4==1?779:variant%4==2?0:0x7fffffff;
            for(std::size_t i=0;i<rival.words.size();++i)memory.write32(rivalBase+std::uint32_t(i*4),rival.words[i]);
            for(std::size_t i=0;i<42;++i)memory.write32(publicBase+std::uint32_t(i*4),pub.words[i]);
            for(std::size_t i=0;i<actor.words.size();++i)memory.write32(actorBase+std::uint32_t(i*4),actor.words[i]);
            for(std::size_t i=0;i<player.words.size();++i)memory.write32(playerBase+std::uint32_t(i*4),player.words[i]);
            memory.write32(0x0c901728,pathBase);memory.write32(0x0c9015cc,condition);memory.write32(0x0caa9868,profile);memory.write32(0x0caa986c,ticks);
            memory.write32(0x0c9015d0,input.level0C9015D0);memory.write32(0x0c901644,input.opponentProgress0C901644);memory.write32(0x0c900954,actorBase);
            for(unsigned i=0;i<8;++i)memory.write32(0x0c901604+i*4,input.progress0C901604[i]);
            const float initialSpeed=rival.f(68);
            const bool active=updateOriginalRivalPace(rival,pub,ticks,data,path,input,player,actor);
            const float correction=rival.f(68)-initialSpeed;
            if(active&&variant!=7&&correction!=0.f&&std::abs(correction)<1.f)++fractionalCorrections;
            RefCpu cpu(memory);cpu.r[4]=1;cpu.r[5]=1;cpu.r[15]=0x0cfff000;cpu.pr=0x0f000000;
            instructions+=cpu.run(0x0c15b0a0,active?0x0c15b7a8:0x0f000000,20000);
            const auto context="condition="+std::to_string(condition)+" profile="+std::to_string(profile)+" variant="+std::to_string(variant);
            for(std::size_t i=0;i<rival.words.size();++i)equal(rival.words[i],memory.read32(rivalBase+std::uint32_t(i*4)),context+" rival+"+hex(std::uint32_t(i*4)));
            for(std::size_t i=0;i<42;++i)equal(pub.words[i],memory.read32(publicBase+std::uint32_t(i*4)),context+" public+"+hex(std::uint32_t(i*4)));
            equal(ticks,memory.read32(0x0caa986c),context+" counter");
            for(std::size_t i=0;i<player.words.size();++i)equal(player.words[i],memory.read32(playerBase+std::uint32_t(i*4)),context+" player unchanged");
            ++cases;
        }
    }
    if(fractionalCorrections<10)throw std::runtime_error("Insufficient fractional pace correction coverage");
    std::cout<<"PASS original rival route/pace: "<<cases<<" cases ("<<fractionalCorrections<<" fractional corrections), "<<checks<<" exact comparisons, "<<instructions<<" original instructions, zero hooks\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
