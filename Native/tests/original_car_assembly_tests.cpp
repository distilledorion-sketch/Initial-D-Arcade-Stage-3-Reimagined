#include "original_car_assembly.h"
#include "original_tuning.h"
#include "car_catalog.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3;
using namespace idas3::original;
using namespace idas3::reference;
int main(int argc,char**argv)try{
    if(argc!=3)throw std::runtime_error("project-root canonical-image required");
    const std::filesystem::path root=argv[1];RefMemory m(argv[2]);const auto tuning=OriginalTuningData::load(root);
    constexpr unsigned object=0xd000000,partsAddress=0xd010000,stack=0xd020000,effectOwner=0xd050000,stop=0x00ff0000;
    m.zeroRegion(object,0x2000);m.zeroRegion(partsAddress,0x1000);m.zeroRegion(stack,0x10000);m.zeroRegion(effectOwner,0x1000);
    std::vector<CarAssemblyCommand> reference;using K=CarAssemblyCommandKind;
    auto emit=[&](K kind,unsigned a=0,unsigned b=0,unsigned c=0,unsigned d=0){reference.push_back({kind,{a,b,c,d}});};
    RefCpu cpu(m);
    for(auto address:{0xc025fe0u,0xc026040u})cpu.callHooks[address]=[&,address](auto& c){const unsigned slot=c.r[5];if(slot>=212)throw std::runtime_error("Source semantic slot outside car bank");emit(address==0xc025fe0?K::DrawMaterial:K::DrawDirect,slot,m.read32(object+0x358+slot*4));};
    cpu.callHooks[0xc1f6610]=[&](auto& c){if(!c.r[4])emit(K::Push);else if(c.r[4]==object+84)emit(K::LoadPrimary);else if(c.r[4]==object+148)emit(K::LoadSecondary);else throw std::runtime_error("Unexpected original matrix source");};
    cpu.callHooks[0xc1f65c0]=[&](auto& c){emit(K::Pop,c.r[4]);};
    cpu.callHooks[0xc1fd060]=[&](auto& c){emit(K::Translate,m.read32(c.r[4]),m.read32(c.r[4]+4),m.read32(c.r[4]+8));};
    cpu.callHooks[0xc1f6ac0]=[&](auto& c){emit(K::Translate,c.fr[4],c.fr[5],c.fr[6]);};
    cpu.callHooks[0xc1f69d0]=[&](auto& c){emit(K::Scale,c.fr[4],c.fr[5],c.fr[6]);};
    for(auto [address,kind]:{std::pair{0xc1f67e0u,K::RotateXPhase},{0xc1f68a0u,K::RotateYPhase},{0xc1f6950u,K::RotateZPhase}})cpu.callHooks[address]=[&,kind](auto& c){emit(kind,c.r[4]);};
    for(auto [address,kind]:{std::pair{0xc1f6770u,K::RotateXRadians},{0xc1f6780u,K::RotateYRadians}})cpu.callHooks[address]=[&,kind](auto& c){emit(kind,c.fr[4]);};
    cpu.callHooks[0xc1d77e0]=[&](auto& c){emit(K::MainDrawState,c.r[4]);};
    cpu.callHooks[0xc1d78a0]=[&](auto& c){emit(K::LayerDrawState,c.r[4]);};
    cpu.callHooks[0xc1d8380]=[&](auto& c){emit(K::LayerParameters,c.fr[4],c.fr[5]);};
    cpu.callHooks[0xc1db280]=[&](auto& c){emit(c.r[4]==88?K::BodyEffect:K::WheelEffect,c.fr[4],c.fr[5],c.fr[6],c.fr[7]);};
    cpu.callHooks[0xc026160]=[&](auto&){emit(K::NumberPlate);};
    cpu.callHooks[0xc029ee0]=[&](auto& c){emit(K::WheelBlur,c.fr[4]);};
    cpu.callHooks[0xc2223b8]=[](auto& c){if(!c.r[5])throw std::runtime_error("Source popup divisor");c.fpul=c.r[4]/c.r[5];};
    std::size_t cases=0,commands=0,instructions=0,checks=0,authoredConfigurations=0;unsigned random=0x026d80;
    auto next=[&](){random^=random<<13;random^=random>>17;random^=random<<5;return random;};
    for(unsigned car=0;car<35;++car){
        const auto parts=OriginalCarParts::load(root/"data/original_models"/originalCarFolders[car]/"assembly_parts.bin");
        unsigned pointer=partsAddress;for(const auto& t:parts.transforms)for(const auto* values:{&t.scale,&t.rotationDegrees,&t.translation})for(float value:*values){m.writeFloat(pointer,value);pointer+=4;}
        OriginalCarAssemblyInput in;in.appearance=OriginalCarAppearanceConfig(car);
        // Exercise every authored package step and optional purchase through
        // the saved-profile bridge, as well as independent packed selections.
        std::vector<unsigned> authoredWords;
        auto fresh=makeOriginalFreshBattleProfile();fresh.setu(16,car);
        for(unsigned color=0;color<tuning.car(car).colors.size();++color){auto p=fresh;p.setu(64,color);authoredWords.push_back(originalPlayerAppearanceConfig(p).word);}
        for(unsigned package=0;package<tuning.car(car).packages.size();++package){auto p=fresh;p.setByte(152,std::uint8_t(package));p.setByte(153,0);p.setu(1180,0);
            for(unsigned visit=0;!(p.u(1180)&0x400);++visit){
                if(visit>=tuning.car(car).packages[package].steps.size())throw std::runtime_error("Authored package did not finish");
                applyOriginalTuningCommand(p,tuning,1);authoredWords.push_back(originalPlayerAppearanceConfig(p).word);
            }
        }
        for(unsigned index=0;index<tuning.car(car).optional.size();++index){auto p=fresh;p.setByte(154,std::uint8_t(index));p.setu(72,1000000);applyOriginalTuningCommand(p,tuning,3);authoredWords.push_back(originalPlayerAppearanceConfig(p).word);}
        authoredConfigurations+=authoredWords.size();
        // Explicit offsets and the car33 palette/body exception cannot depend
        // on the pseudorandom sequence accidentally selecting those branches.
        if(car==6){authoredWords.push_back(1u<<19);authoredWords.push_back(2u<<19);}
        if(car==25||car==30)authoredWords.push_back(2u<<13);
        if(car==33){authoredWords.push_back(0x02000000);authoredWords.push_back(0x02010000);}
        const unsigned authoredEnd=96+unsigned(authoredWords.size())*4;
        for(unsigned sample=0;sample<authoredEnd+4;++sample){
            unsigned word=0;for(unsigned shift:{0u,3u,7u,10u,13u,16u,19u})word|=(sample<6?sample:next()%6)<<shift;
            word|=(next()%7)<<22;word|=(next()&7)<<25;word|=(next()&7)<<28;
            if(sample>=96)word=sample<authoredEnd?authoredWords[(sample-96)/4]:0;
            in.appearance.word=word;
            in.visibility=originalCarVisibility(in.appearance,sample%3?-1:int(next()%31));in.wheelOffsets=originalCarWheelOffsets(in.appearance);
            in.lights=bool(sample&1);in.braking=bool(sample&2);in.overlayLayers=(sample>>2)&3;in.effects=(sample>>4)&3;
            in.hasParts=sample<authoredEnd;if(!in.hasParts)in.overlayLayers=0;
            in.primaryUsesCurrent=bool(sample&16);in.secondaryUsesCurrent=bool(sample&32);
            in.secondaryParameters={0.125f,0.875f};in.effectParameters={0.1f,0.2f,0.3f};in.steering=(float(int(next()%200)-100)/500.f);
            for(unsigned n=0;n<4;++n){in.suspension[n]=float(int(next()%100)-50)/3000.f;in.spin[n]=float(int(next()%1000)-500)/100.f;in.wheelBlur[n]=float(next()%100)/100.f;}
            in.headlights.maximumPhase=in.visibility.maximumPopupPhase;
            if(sample%9==0)in.headlights.reset();
            if(sample>=96)in.headlights.counter=int(sample%42)-1;
            m.write32(object+0x34c,car);m.write32(object+0x2d4,word);m.write32(object+0x2f0,in.hasParts?partsAddress:0);
            m.write8(object+80,in.braking);m.write8(object+81,in.lights);m.write32(object+0xe0,in.overlayLayers);m.write32(object+0xe4,in.effects);
            m.write8(object+0xd4,in.primaryUsesCurrent);m.write8(object+0xd5,in.secondaryUsesCurrent);
            for(unsigned i=0;i<2;++i)m.writeFloat(object+0xd8+i*4,in.secondaryParameters[i]);
            for(unsigned i=0;i<3;++i)m.writeFloat(object+0x320+i*4,in.effectParameters[i]);
            m.writeFloat(object+0x314,in.steering);m.writeFloat(object+0x318,in.wheelOffsets.front);m.writeFloat(object+0x31c,in.wheelOffsets.rear);
            for(unsigned i=0;i<4;++i){m.writeFloat(object+0x2f4+i*4,in.suspension[i]);m.writeFloat(object+0x304+i*4,in.spin[i]);m.writeFloat(object+0x33c+i*4,in.wheelBlur[i]);}
            m.write32(object+0x32c,effectOwner);m.write32(effectOwner+12,effectOwner+64);m.write32(effectOwner+64,77);m.write32(effectOwner+68,88);
            m.write32(object+0x6c4,in.visibility.popupMotorEnabled);m.write32(object+0x6c8,unsigned(in.headlights.counter));m.write32(object+0x6cc,40);
            for(unsigned i=0;i<212;++i)m.write32(object+0x358+i*4,in.visibility.slots[i]);
            reference.clear();cpu.r[4]=object;cpu.r[15]=stack+0xf000;cpu.pr=stop;instructions+=cpu.run(0xc026d80,stop,100000);
            const auto native=originalCarAssemblyCommands(in,parts);
            if(native.size()!=reference.size()){std::cerr<<"car"<<car<<" sample"<<sample<<" config"<<hex(word)<<"\n";throw std::runtime_error("Assembly command count source="+std::to_string(reference.size())+" native="+std::to_string(native.size()));}
            for(unsigned i=0;i<native.size();++i){++commands;if(native[i]!=reference[i]){std::cerr<<"car"<<car<<" sample"<<sample<<" config"<<hex(word)<<" command"<<i<<" source kind"<<unsigned(reference[i].kind)<<" native kind"<<unsigned(native[i].kind)<<'\n';
                for(unsigned w=0;w<4;++w)std::cerr<<hex(reference[i].words[w])<<'/'<<hex(native[i].words[w])<<' ';std::cerr<<'\n';throw std::runtime_error("Original assembly event mismatch");}checks+=5;}
            if(unsigned(in.headlights.counter)!=m.read32(object+0x6c8)||in.headlights.phase!=m.read32(object+0x6d0)||unsigned(in.headlights.visible)!=m.read32(object+0x6d4)||std::bit_cast<unsigned>(in.headlights.fraction)!=m.read32(object+0x6d8))throw std::runtime_error("Original assembly popup state");checks+=4;++cases;
        }
        std::cout<<"car"<<car<<" native assembly commands matched"<<std::endl;
    }
    std::cout<<"PASS "<<cases<<" assembly frames, "<<commands<<" complete ordered commands, "<<checks<<" exact words, "<<instructions<<" original instructions; "<<authoredConfigurations<<" authored profile configurations in all four lighting states. Draw/matrix/device/plate/blur and division helper boundaries are explicit; source assembly selection and transform-argument arithmetic execute original bytes. Matrix rasterization is separate.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
