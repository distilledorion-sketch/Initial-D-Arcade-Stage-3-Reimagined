#include "original_rival_motion.h"
#include "original_math.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <random>
#include <regex>

using namespace idas3::original;
using namespace idas3::reference;
int main(int argc,char** argv)try{
    if(argc!=4)throw std::runtime_error("Pass canonical image, project data directory and primary FSCA header");
    RefMemory memory(argv[1]);const auto root=std::filesystem::path(argv[2])/"original_rival";
    const auto data=OriginalRivalData::load(root);
    const auto fsca=OriginalFscaTable::load(std::filesystem::path(argv[2])/"original_physics/fsca_table.bin");
    std::ifstream primary(argv[3]);if(!primary)throw std::runtime_error("Primary FSCA source unavailable");
    const std::string sourceText{std::istreambuf_iterator<char>(primary),{}};const std::regex pattern("0x([0-9A-Fa-f]{8})");
    std::vector<std::uint32_t> halfWave;for(std::sregex_iterator i(sourceText.begin(),sourceText.end(),pattern),end;i!=end;++i)halfWave.push_back(std::stoul((*i)[1].str(),nullptr,16));
    if(halfWave.size()!=32768)throw std::runtime_error("Primary FSCA source word count");
    memory.image.resize(0xc00000); // Data-only oracle fixture beyond the executable image.
    const auto hostfs=std::filesystem::path(argv[1]).parent_path()/"driveA/HOSTFS";
    std::size_t checks=0,instructions=0,cases=0,fractionalCorrections=0,trajectoryFrames=0;
    const auto equal=[&](std::uint32_t actual,std::uint32_t expected,const std::string& context){
        if(actual!=expected)throw std::runtime_error(context+" expected="+hex(expected)+" actual="+hex(actual));++checks;
    };
    for(std::uint32_t a=0x0c271618;a<0x0c283de4;a+=4)equal(data.word(a),memory.read32(a),"Exported original table "+hex(a));
    std::mt19937 random(0x15b0a0);
    constexpr std::uint32_t rivalBase=0x0c901c6c+716,publicBase=0x0c90172c+168,playerBase=0x0c900f00,actorBase=0x0c9008a4,pathBase=0x0cd00000;
    for(std::uint32_t condition=0;condition<18;++condition){
        const auto path=data.loadPath(root,condition);
        std::string collisionName;const auto collisionSlot=0x0c2eff40+condition*1024+(5+(condition&1))*64;
        for(std::uint32_t i=0;i<64&&memory.read8(collisionSlot+i);++i)collisionName.push_back(char(memory.read8(collisionSlot+i)));
        const auto collisionFile=hostfs/"binary"/(std::filesystem::path(collisionName).filename().string()+".bin.nz");const auto collision=OriginalCollisionData::load(collisionFile);
        std::ifstream collisionInput(collisionFile,std::ios::binary);std::vector<std::uint8_t> rawCollision{std::istreambuf_iterator<char>(collisionInput),{}};
        constexpr std::uint32_t collisionBase=0x0cb00000;
        std::copy(rawCollision.begin(),rawCollision.end(),memory.image.begin()+(collisionBase-RefMemory::imageBase));
        for(auto offset:{12u,20u,28u,36u,44u}){std::uint32_t value;std::memcpy(&value,rawCollision.data()+offset,4);value+=collisionBase;std::memcpy(memory.image.data()+collisionBase-RefMemory::imageBase+offset,&value,4);}
        // Verify complete source capacity, including the lookahead after the
        // shorter player-valid prefix. Source stem comes from042700's table.
        const auto slot=0x0c2eff40+condition*1024+7*64;
        std::string stem;for(std::uint32_t j=0;j<64&&memory.read8(slot+j);++j)stem.push_back(char(memory.read8(slot+j)));
        stem=std::filesystem::path(stem).filename().string().substr(5)+(condition&1?"o":"i");
        const auto sourcePath=std::filesystem::path(argv[1]).parent_path()/"driveA/HOSTFS/binary"/("PATH_"+stem+"_0.bin");
        std::ifstream source(sourcePath,std::ios::binary);if(!source)throw std::runtime_error("Missing rival path source "+sourcePath.string());
        for(const auto& point:path.points)for(float value:point){std::uint32_t expected=0;source.read(reinterpret_cast<char*>(&expected),4);if(!source)throw std::runtime_error("Rival path source short");equal(std::bit_cast<std::uint32_t>(value),expected,"Rival path capacity");}
        for(std::uint32_t profile=0;profile<32;++profile)for(std::uint32_t variant=0;variant<4;++variant){
            memory.clear();memory.zeroRegion(0x0c8ff000,0x1c0000);memory.zeroRegion(0x0cff0000,0x10000);
            for(std::size_t i=0;i<path.points.size();++i)for(unsigned k=0;k<3;++k)memory.writeFloat(pathBase+std::uint32_t(i*12+k*4),path.points[i][k]);
            OriginalRivalState rival;OriginalActorState pub,actor;OriginalDriveState player;
            for(auto& w:rival.words)w=std::bit_cast<std::uint32_t>((float(random()%10000)-5000.f)*.00001f);for(auto& w:pub.words)w=std::bit_cast<std::uint32_t>((float(random()%10000)-5000.f)*.00001f);for(auto& w:actor.words)w=random();for(auto& w:player.words)w=random();
            auto index=variant==0?0:variant==1?path.inclusiveLastIndex:variant==2?path.inclusiveLastIndex-5:(profile*19+variant*29)%path.inclusiveLastIndex;
            if(profile==13){constexpr std::uint32_t special[]{29,30,40,41,110,120,260,285,330,340,495,535,720,735,736,10};index=special[variant]%path.inclusiveLastIndex;}
            if(profile==16){constexpr std::uint32_t special[]{314,315,320,321,341,342,360,361};index=special[variant%8]%path.inclusiveLastIndex;}
            rival.setf(0xac,float(profile)*.2f-3.f);rival.setf(0xe4,float(profile)*.2f);
            rival.setu(0,variant==11?0:1);rival.setu(12,index);rival.setu(16,variant==10?1:0);
            rival.setf(200,path.points[index][0]+float(variant%3)*.125f);rival.setf(204,path.points[index][1]);rival.setf(208,path.points[index][2]-float(variant%5)*.125f);
            for(unsigned k=0;k<3;++k)pub.setf(k*4,rival.f(200+k*4));
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
            memory.write32(0x0caa9864,variant&1);memory.write32(0x0c2eef6c,collisionBase);
            const auto car=(profile*13+variant)%35;memory.write32(0x0c9015f8,car);
            OriginalRivalRoadState road;road.surfaceValid0CAA9864=variant&1;
            for(unsigned i=0;i<4;++i){clearOriginalCollisionQuery(road.surfaces0CAA9764[i]);for(unsigned k=0;k<16;++k)memory.write32(0x0caa9764+i*64+k*4,road.surfaces0CAA9764[i].words[k]);}
            memory.write32(0x0c98ad0c,0x00200000);memory.write32(0x0c98ad10,0x0ce00000);memory.write32(0x0c98ad14,0x0ce00000);memory.zeroRegion(0x0ce00000,32*64);
            const float initialSpeed=rival.f(68);
            const bool active=updateOriginalRivalPace(rival,pub,ticks,data,path,input,player,actor);
            const float correction=rival.f(68)-initialSpeed;
            if(active&&variant!=7&&correction!=0.f&&std::abs(correction)<1.f)++fractionalCorrections;
            RefCpu cpu(memory);cpu.r[4]=1;cpu.r[5]=1;cpu.r[15]=0x0cfff000;cpu.pr=0x0f000000;cpu.fscaHalfWave=halfWave;
            instructions+=cpu.run(0x0c15b0a0,active?0x0c15b7a8:0x0f000000,20000);
            if(active){advanceOriginalRivalMotion(rival,pub,path,input,variant&1);instructions+=cpu.run(0x0c15b7a8,0x0c15be50,20000);finishOriginalRivalRoadContact(rival,pub,car,data,road,collision,fsca);instructions+=cpu.run(0x0c15be50,0x0f000000,2000000);}
            const auto context="condition="+std::to_string(condition)+" profile="+std::to_string(profile)+" variant="+std::to_string(variant);
            for(std::size_t i=0;i<rival.words.size();++i)equal(rival.words[i],memory.read32(rivalBase+std::uint32_t(i*4)),context+" rival+"+hex(std::uint32_t(i*4)));
            for(std::size_t i=0;i<42;++i)equal(pub.words[i],memory.read32(publicBase+std::uint32_t(i*4)),context+" public+"+hex(std::uint32_t(i*4)));
            equal(ticks,memory.read32(0x0caa986c),context+" counter");
            for(std::size_t i=0;i<player.words.size();++i)equal(player.words[i],memory.read32(playerBase+std::uint32_t(i*4)),context+" player unchanged");
            for(unsigned i=0;i<4;++i)for(unsigned k=0;k<16;++k)equal(road.surfaces0CAA9764[i].words[k],memory.read32(0x0caa9764+i*64+k*4),context+" query "+std::to_string(i)+" word "+std::to_string(k));
            equal(road.surfaceValid0CAA9864,memory.read32(0x0caa9864),context+" surface valid");
            for(unsigned i=0;i<21;++i)equal(road.surface.words[i],memory.read32(0x0c99aa98+i*4),context+" surface scratch");
            for(unsigned i=0;i<100;++i)equal(std::uint32_t(road.trace.indices0C99A904[i]),memory.read32(0x0c99a904+i*4),context+" trace");
            equal(road.trace.count0C99AA94,memory.read32(0x0c99aa94),context+" trace count");
            ++cases;
        }
        if(condition==6||condition==7)for(const auto profile:{0u,13u,16u,31u}){
            memory.clear();memory.zeroRegion(0x0c8ff000,0x1c0000);memory.zeroRegion(0x0cff0000,0x10000);memory.zeroRegion(0x0ce00000,32*64);
            for(std::size_t i=0;i<path.points.size();++i)for(unsigned k=0;k<3;++k)memory.writeFloat(pathBase+std::uint32_t(i*12+k*4),path.points[i][k]);
            const float dx=path.points[1][0]-path.points[0][0],dz=path.points[1][2]-path.points[0][2];
            float direction=float(std::bit_cast<std::int32_t>(originalAtan2Angle(dx,dz)));direction*=std::bit_cast<float>(0x40490fdbu);direction*=std::bit_cast<float>(0x38000000u);
            for(unsigned k=0;k<3;++k)memory.writeFloat(0x0cdf0000+k*4,k==1?direction-std::bit_cast<float>(0x40490fdbu):0.f);
            memory.write32(0x0c9015cc,condition);memory.write32(0x0c9015e0,profile);memory.write32(0x0c9015d0,3);memory.write32(0x0c901648,0);
            memory.write32(0x0c900954,actorBase);memory.write32(actorBase+80,0x8000);memory.write32(0x0c9015f8,profile%35);memory.write32(0x0c2eef6c,collisionBase);
            memory.write32(0x0c98ad0c,0x00200000);memory.write32(0x0c98ad10,0x0ce00000);memory.write32(0x0c98ad14,0x0ce00000);
            memory.write32(0x0cfff000,1);memory.write32(0x0cfff004,1);memory.write32(0x0cfff008,profile%35);
            RefCpu initialize(memory);initialize.r[4]=pathBase;initialize.r[5]=0x0cdf0000;initialize.r[6]=pathBase;initialize.r[7]=pathBase;
            initialize.r[15]=0x0cfff000;initialize.pr=0x0f000000;
            // Source initialization supplies the trajectory seed. These
            // frames validate update recurrence, not a native initializer.
            initialize.run(0x0c15ae00,0x0f000000,2000);
            OriginalRivalState rival;OriginalActorState pub,actor;OriginalDriveState player;OriginalRivalRoadState road;
            for(std::size_t i=0;i<rival.words.size();++i)rival.words[i]=memory.read32(rivalBase+std::uint32_t(i*4));
            for(unsigned i=0;i<42;++i)pub.words[i]=memory.read32(publicBase+i*4);
            for(unsigned i=0;i<4;++i)for(unsigned k=0;k<16;++k)road.surfaces0CAA9764[i].words[k]=memory.read32(0x0caa9764+i*64+k*4);
            actor.setu(80,0x8000);OriginalRivalPaceInputs inputs;inputs.condition0C9015CC=condition;inputs.profile0CAA9868=profile;inputs.level0C9015D0=3;
            std::uint32_t counter=0;const auto start=path.points[0];
            for(unsigned frame=0;frame<600;++frame){
                player.setu(0x118,rival.u(12));memory.write32(playerBase+0x118,player.u(0x118));
                updateOriginalRival(rival,pub,counter,data,path,inputs,player,actor,profile%35,road,collision,fsca);
                RefCpu cpu(memory);cpu.r[4]=1;cpu.r[5]=1;cpu.r[15]=0x0cfff000;cpu.pr=0x0f000000;cpu.fscaHalfWave=halfWave;
                instructions+=cpu.run(0x0c15b0a0,0x0f000000,2000000);
                const auto label="trajectory condition="+std::to_string(condition)+" profile="+std::to_string(profile)+" frame="+std::to_string(frame);
                for(std::size_t i=0;i<rival.words.size();++i)equal(rival.words[i],memory.read32(rivalBase+std::uint32_t(i*4)),label+" rival+"+hex(std::uint32_t(i*4)));
                for(unsigned i=0;i<42;++i)equal(pub.words[i],memory.read32(publicBase+i*4),label+" public+"+hex(i*4));
                for(unsigned i=0;i<4;++i)for(unsigned k=0;k<16;++k)equal(road.surfaces0CAA9764[i].words[k],memory.read32(0x0caa9764+i*64+k*4),label+" query");
                equal(counter,memory.read32(0x0caa986c),label+" counter");equal(road.surfaceValid0CAA9864,memory.read32(0x0caa9864),label+" road status");
                ++trajectoryFrames;
            }
            const float distance=std::hypot(rival.f(200)-start[0],rival.f(208)-start[2]);
            if(distance<20.f)throw std::runtime_error("Rival trajectory did not advance");
            std::cout<<"trajectory condition="<<condition<<" profile="<<profile<<" displacement="<<distance<<" pathIndex="<<rival.u(12)<<"\n";
        }
    }
    
    std::cout<<"PASS original rival complete update: "<<cases<<" cases ("<<fractionalCorrections<<" fractional corrections), "<<trajectoryFrames<<" continuous trajectory frames, "<<checks<<" exact comparisons, "<<instructions<<" original instructions, zero hooks\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
