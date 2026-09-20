#include "original_frame_state.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <limits>
using namespace idas3::original;
using namespace idas3::reference;
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Pass canonical original program image");
    RefMemory memory{std::filesystem::path(argv[1])};std::size_t checks=0,instructions=0,saves=0,restores=0,cues=0,diagnostics=0;
    std::uint32_t rng=0x159920;auto random=[&](){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;};
    const auto upload=[&](std::uint32_t base,const auto& words){for(std::size_t i=0;i<words.size();++i)memory.write32(base+std::uint32_t(i*4),words[i]);};
    const auto check=[&](std::uint32_t expected,std::uint32_t actual,const char* label){++checks;if(expected!=actual)throw std::runtime_error(std::string(label)+" expected="+hex(expected)+" actual="+hex(actual));};
    const auto compare=[&](std::uint32_t base,const auto& words){for(std::size_t i=0;i<words.size();++i)check(memory.read32(base+std::uint32_t(i*4)),words[i],"Memory");};
    for(unsigned sample=0;sample<1024;++sample){
        memory.clear();memory.zeroRegion(0x0CFFF000,0x1000);
        OriginalDriveState drive;OriginalActorState player,secondary;OriginalPublishedActors published;OriginalRecoveryState backup;
        for(auto& word:drive.words)word=random();for(auto& word:backup.drive0C9009F0.words)word=random();
        for(auto& word:player.words)word=random();for(auto& word:secondary.words)word=random();
        for(auto& word:published.player0C8FF388)word=random();for(auto& word:published.secondary0C8FF430)word=random();
        for(auto& word:backup.actor0C8FF580)word=random();
        for(unsigned corner=0;corner<4;++corner){
            drive.setu(0x164+corner*4,((sample>>corner)&1u)?0x20u:3u);
            backup.drive0C9009F0.setu(0x164+corner*4,((sample>>(corner+4))&1u)?0x10u:4u);
        }
        // Only the physical168-byte actor prefix is mapped at these original
        // addresses. The host actor has spare capacity which must stay intact.
        for(std::size_t i=0;i<42;++i){memory.write32(0x0C9008A4+std::uint32_t(i*4),player.words[i]);memory.write32(0x0C9017D4+std::uint32_t(i*4),secondary.words[i]);}
        upload(0x0C8FF388,published.player0C8FF388);upload(0x0C8FF430,published.secondary0C8FF430);
        memory.write32(0x0C9015E4,sample%3);
        RefCpu publication(memory);publication.r[15]=0x0CFFF800;publication.pr=0x0F000000;
        instructions+=publication.run(0x0C157A80,0x0F000000,3000);
        publishOriginalActors(player,secondary,sample%3,published);
        compare(0x0C8FF388,published.player0C8FF388);compare(0x0C8FF430,published.secondary0C8FF430);
        upload(0x0C900F00,drive.words);upload(0x0C9009F0,backup.drive0C9009F0.words);upload(0x0C8FF580,backup.actor0C8FF580);
        memory.write32(0x0C900954,0x0C9008A4);
        const auto before=backup.drive0C9009F0.words;
        RefCpu recovery(memory);recovery.r[14]=recovery.r[15]=0x0CFFF800;
        instructions+=recovery.run(0x0C159952,0x0C1599CA,6000);
        if(applyOriginalRecovery(drive,player,published,backup))++restores;
        if(before!=backup.drive0C9009F0.words)++saves;
        compare(0x0C900F00,drive.words);
        // Backup's true1088-byte record ends before neighboring globalE30.
        for(std::size_t i=0;i<272;++i)check(memory.read32(0x0C9009F0+std::uint32_t(i*4)),backup.drive0C9009F0.words[i],"Backup");
        for(std::size_t i=272;i<backup.drive0C9009F0.words.size();++i)check(before[i],backup.drive0C9009F0.words[i],"Untouched native backup suffix");
        compare(0x0C8FF580,backup.actor0C8FF580);compare(0x0C8FF388,published.player0C8FF388);compare(0x0C8FF430,published.secondary0C8FF430);
        for(std::size_t i=0;i<42;++i)check(memory.read32(0x0C9008A4+std::uint32_t(i*4)),player.words[i],"Actor");
    }
    for(unsigned sample=0;sample<1600;++sample){
        memory.clear();memory.zeroRegion(0x0CFFF000,0x1000);
        OriginalDriveState drive;for(auto& word:drive.words)word=random();
        drive.setf(0x260,float(int(sample%101)-50)*.2f);drive.setf(0x264,float(int(sample%87)-43)*.3f);
        OriginalBodyCollisionResult collision;collision.active=sample%3;collision.x=float(int(sample%51)-25);collision.z=-collision.x;
        if(sample%31==0)collision.x=std::numeric_limits<float>::infinity();
        if(sample%37==0)collision.z=std::numeric_limits<float>::quiet_NaN();
        std::uint32_t latch=sample%3,seed=random();
        upload(0x0C900F00,drive.words);memory.write32(0x0C401B04+40,collision.active);
        memory.writeFloat(0x0C401B04+76,collision.x);memory.writeFloat(0x0C401B04+84,collision.z);
        memory.write32(0x0C31FD44,latch);memory.write32(0x0C37C778,seed);
        RefCpu cpu(memory);cpu.r[10]=0x0C900F00;cpu.r[11]=0x0C401B04;cpu.r[14]=cpu.r[15]=0x0CFFF800;
        OriginalBodyCollisionEffects expected;
        cpu.callHooks[0x0C142460]=[&](RefCpu& c){expected.cue=c.r[4];++cues;};
        cpu.callHooks[0x0C055D60]=[&](RefCpu&){++expected.invalidScalarDiagnostics;++diagnostics;};
        instructions+=cpu.run(0x0C1578C4,0x0C157A06,3000);
        const auto actual=applyOriginalBodyCollisionResponse(drive,collision,latch,seed);
        compare(0x0C900F00,drive.words);check(memory.read32(0x0C31FD44),latch,"Collision latch");check(memory.read32(0x0C37C778),seed,"RNG");
        check(expected.cue,actual.cue,"Cue");check(expected.invalidScalarDiagnostics,actual.invalidScalarDiagnostics,"Diagnostics");
    }
    if(!saves||!restores||!cues||!diagnostics)throw std::runtime_error("Missing required recovery or response coverage");
    std::cout<<"PASS1024 actor-publication/recovery and1600 body-response cases, "<<checks<<" exact comparisons, "<<instructions<<" original instructions; saves="<<saves<<", restores="<<restores<<". Only platform cue/diagnostic hooks; pair detection is a separate supplied boundary.\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
