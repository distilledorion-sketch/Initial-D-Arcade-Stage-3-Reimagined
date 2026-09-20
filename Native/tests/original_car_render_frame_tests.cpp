#include "original_car_render_frame.h"
#include "original_car_material_rebuild.h"
#include "car_catalog.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3;
using namespace idas3::original;
using namespace idas3::reference;
int main(int argc,char** argv)try{
    if(argc!=3)throw std::invalid_argument("game-root canonical-image required");
    const std::filesystem::path root=argv[1];RefMemory m(argv[2]);
    const auto trig=OriginalFscaTable::load(root/"data/original_physics/fsca_table.bin");
    std::ifstream sourceTable(root/"data/original_physics/fsca_table.bin",std::ios::binary);sourceTable.seekg(16);
    std::vector<unsigned> wave(32768);if(!sourceTable.read(reinterpret_cast<char*>(wave.data()),131072))throw std::runtime_error("Missing source FSCA data");
    constexpr unsigned object=0x0d000000,partsAddress=0x0d010000,stack=0x0d020000,matrixStack=0x0ce00000;
    constexpr unsigned effectOwner=0x0d040000,modelTable=0x0d050000,plateBank=0x0d060000,blurBank=0x0d061000,blurGmp=0x0d062000;
    constexpr unsigned carHandleBase=0x0d800000,plateHandleBase=0x0d900000,blurHandle=0x0da00000,stop=0x00ff0000;
    using K=CarAssemblyCommandKind;using Bank=OriginalCarRenderBank;
    std::vector<OriginalCarRenderItem> reference;
    RefCpu cpu(m);cpu.fscaHalfWave=wave;
    const auto emit=[&](K operation,Bank bank=Bank::car,unsigned chunk=0xffffffffu,std::array<unsigned,4> parameters={},unsigned diffuse=0){
        OriginalMatrix matrix;for(unsigned i=0;i<16;++i)matrix.elements[i]=std::bit_cast<float>(cpu.xf[i]);
        reference.push_back({operation,bank,chunk,matrix,parameters,diffuse});
    };
    for(auto address:{0xc1d7900u,0xc1d7120u})cpu.callHooks[address]=[&,address](auto& c){
        const unsigned handle=c.r[4];
        if(handle>=carHandleBase&&handle<carHandleBase+0x100000&&(handle&255)==0)emit(address==0xc1d7900?K::DrawMaterial:K::DrawDirect,Bank::car,(handle-carHandleBase)/256);
        else if(handle>=plateHandleBase&&handle<plateHandleBase+11*256&&(handle&255)==0)emit(K::NumberPlate,Bank::numberPlate,(handle-plateHandleBase)/256);
        else if(handle==blurHandle)emit(K::WheelBlur,Bank::wheelBlur,0,{},m.read32(blurGmp));
        else throw std::runtime_error("Unexpected original model submission "+hex(handle));
    };
    cpu.callHooks[0xc05a8e0]=[&](auto& c){if(c.r[4]!=plateBank||c.r[5]>10)throw std::runtime_error("Original plate bank lookup");c.r[0]=plateHandleBase+c.r[5]*256;};
    cpu.callHooks[0xc1d5400]=[&](auto& c){if(c.r[4]!=blurBank)throw std::runtime_error("Original blur bank lookup");c.r[0]=blurHandle;};
    cpu.callHooks[0xc1d77e0]=[&](auto& c){emit(K::MainDrawState,Bank::car,0xffffffffu,{c.r[4],0,0,0});};
    cpu.callHooks[0xc1d78a0]=[&](auto& c){emit(K::LayerDrawState,Bank::car,0xffffffffu,{c.r[4],0,0,0});};
    cpu.callHooks[0xc1d8380]=[&](auto& c){emit(K::LayerParameters,Bank::car,0xffffffffu,{c.fr[4],c.fr[5],0,0});};
    cpu.callHooks[0xc1db280]=[&](auto& c){emit(c.r[4]==88?K::BodyEffect:K::WheelEffect,Bank::car,0xffffffffu,{c.fr[4],c.fr[5],c.fr[6],c.fr[7]});};
    cpu.callHooks[0xc2223b8]=[](auto& c){if(!c.r[5])throw std::runtime_error("Source divisor zero");c.fpul=c.r[4]/c.r[5];};
    unsigned random=0x026160;auto next=[&](){random^=random<<13;random^=random>>17;random^=random<<5;return random;};
    std::size_t frames=0,items=0,matrixWords=0,comparisons=0,instructions=0,copies=0,plates=0,blurDraws=0;
    unsigned car=0,sample=0;
    const auto equal=[&](unsigned a,unsigned b,const char* label){++comparisons;if(a!=b)throw std::runtime_error(std::string(label)+" car="+std::to_string(car)+" sample="+std::to_string(sample)+" actual="+hex(a)+" original="+hex(b));};
    for(car=0;car<35;++car){
        m.clear();m.zeroRegion(object,0x80000);m.zeroRegion(matrixStack,0x10000);
        const auto base=root/"data/original_models"/originalCarFolders[car];
        const auto parts=OriginalCarParts::load(base/"assembly_parts.bin");
        const auto materials=OriginalCarMaterialRebuild::load(base/"material_layout.bin",car);
        const auto rawModel=NativeModel::load(base/(std::string(originalCarFolders[car])+".idasmesh"));
        // The separately verified material constructor owns copied handles.
        // The actual025FE0/026040/026100 source resolver executes below.
        const auto& mapping=materials.semanticChunks();
        const unsigned map=m.read32(0xc33b250+car*4);m.write32(object+4,modelTable);m.write32(object+0x354,map);
        for(unsigned chunk=0;chunk<rawModel.chunks.size();++chunk)m.write32(modelTable+chunk*4,rawModel.chunks[chunk].sourceSize==8?0:carHandleBase+chunk*256);
        for(unsigned slot=140;slot<=186;++slot)m.write32(object+0x898+(slot-140)*4,mapping[slot]<0?0:carHandleBase+unsigned(mapping[slot])*256);
        unsigned pointer=partsAddress;for(const auto& t:parts.transforms)for(const auto* values:{&t.scale,&t.rotationDegrees,&t.translation})for(float value:*values){m.writeFloat(pointer,value);pointer+=4;}
        for(sample=0;sample<96;++sample){
            OriginalCarAssemblyInput in;in.appearance=OriginalCarAppearanceConfig(car);
            for(unsigned shift:{0u,3u,7u,10u,13u,16u,19u})in.appearance.word|=(sample<6?sample:next()%6)<<shift;
            in.appearance.word|=(next()%7)<<22;in.appearance.word|=(next()&7)<<25;in.appearance.word|=(next()&3)<<28;
            if(car==33&&sample<4)in.appearance.word=0x02000000;
            if(car==24&&sample<2)in.appearance.word=0x10000000|(sample?4u<<13:0);
            const int enemy=car==24&&sample<2?12:sample%3?-1:int(next()%31);
            in.visibility=originalCarVisibility(in.appearance,enemy);in.wheelOffsets=originalCarWheelOffsets(in.appearance);
            in.lights=bool(sample&1);in.braking=bool(sample&2);in.overlayLayers=(sample>>2)&3;in.effects=(sample>>4)&3;
            in.primaryUsesCurrent=bool(sample&16);in.secondaryUsesCurrent=bool(sample&32);in.headlights.counter=int(sample%42)-1;
            in.secondaryParameters={.125f,.875f};in.effectParameters={.1f,.2f,.3f};in.steering=float(int(next()%200)-100)/500.f;
            constexpr std::array<float,8> blurValues{-.25f,0.f,.001f,.25f,.5f,1.f,1.1f,128.f/255.f};
            for(unsigned i=0;i<4;++i){in.suspension[i]=float(int(next()%100)-50)/3000.f;in.spin[i]=float(int(next()%1000)-500)/100.f;in.wheelBlur[i]=blurValues[(sample+i)%blurValues.size()];}
            OriginalCarRenderContext context;context.semanticChunks=mapping;context.carChunkCount=materials.chunks().size();
            for(auto& digit:context.digits)digit=std::uint8_t(next()%10);context.wheelBlurDiffuse=0x3fabcdef;
            context.current=originalActorMatrix({float(sample)*.3f,-2.1f,4.2f},{.31f,float(sample)*.043f,-.18f},trig);
            context.primary=originalActorMatrix({1,2,3},{-.25f,.7f,.1f},trig);scaleOriginalMatrix(context.primary,{.7f,1.1f,-.8f});
            context.secondary=originalActorMatrix({-3,5,8},{.4f,-1.2f,-.3f},trig);
            for(unsigned i=0;i<16;++i){cpu.xf[i]=std::bit_cast<unsigned>(context.current.elements[i]);m.writeFloat(object+84+i*4,context.primary.elements[i]);m.writeFloat(object+148+i*4,context.secondary.elements[i]);}
            m.write32(0xc98ad0c,0x00200000);m.write32(0xc98ad10,matrixStack);m.write32(0xc98ad14,matrixStack);
            m.write32(object+0x34c,car);m.write32(object+0x2d4,in.appearance.word);m.write32(object+0x2f0,partsAddress);
            m.write8(object+80,in.braking);m.write8(object+81,in.lights);m.write32(object+0xe0,in.overlayLayers);m.write32(object+0xe4,in.effects);
            m.write8(object+0xd4,in.primaryUsesCurrent);m.write8(object+0xd5,in.secondaryUsesCurrent);
            for(unsigned i=0;i<2;++i)m.writeFloat(object+0xd8+i*4,in.secondaryParameters[i]);
            for(unsigned i=0;i<3;++i)m.writeFloat(object+0x320+i*4,in.effectParameters[i]);
            m.writeFloat(object+0x314,in.steering);m.writeFloat(object+0x318,in.wheelOffsets.front);m.writeFloat(object+0x31c,in.wheelOffsets.rear);
            for(unsigned i=0;i<4;++i){m.writeFloat(object+0x2f4+i*4,in.suspension[i]);m.writeFloat(object+0x304+i*4,in.spin[i]);m.writeFloat(object+0x33c+i*4,in.wheelBlur[i]);}
            m.write32(object+0x32c,effectOwner);m.write32(effectOwner+12,effectOwner+64);m.write32(effectOwner+64,77);m.write32(effectOwner+68,88);
            m.write32(object+0x330,plateBank);m.write32(object+0x334,blurBank);m.write32(object+0x338,blurGmp);m.write32(blurGmp,context.wheelBlurDiffuse);
            for(unsigned i=0;i<5;++i)m.write8(object+0x6b4+i,context.digits[i]);
            m.write32(object+0x6c4,in.visibility.popupMotorEnabled);m.write32(object+0x6c8,unsigned(in.headlights.counter));m.write32(object+0x6cc,40);
            for(unsigned i=0;i<212;++i)m.write32(object+0x358+i*4,in.visibility.slots[i]);
            const auto commands=originalCarAssemblyCommands(in,parts);
            const auto native=originalCarRenderFrame(commands,context,trig);
            reference.clear();cpu.r[4]=object;cpu.r[15]=stack+0xf000;cpu.pr=stop;instructions+=cpu.run(0xc026d80,stop,100000);
            equal(unsigned(native.items.size()),unsigned(reference.size()),"Complete rendered item count");
            for(unsigned i=0;i<native.items.size();++i){const auto& a=native.items[i];const auto& b=reference[i];++items;
                equal(unsigned(a.operation),unsigned(b.operation),"Operation");equal(unsigned(a.bank),unsigned(b.bank),"Model bank");equal(a.chunk,b.chunk,"Resolved original model");equal(a.materialDiffuse,b.materialDiffuse,"Per-draw blur alpha");
                for(unsigned w=0;w<4;++w)equal(a.parameters[w],b.parameters[w],"Submission parameter");
                for(unsigned w=0;w<16;++w){equal(std::bit_cast<unsigned>(a.matrix.elements[w]),std::bit_cast<unsigned>(b.matrix.elements[w]),"Actual original draw matrix");++matrixWords;}
                if(a.isGeometry()&&a.bank==Bank::car&&a.chunk>=rawModel.chunks.size())++copies;
                if(a.operation==K::NumberPlate)++plates;if(a.operation==K::WheelBlur)++blurDraws;
            }
            for(unsigned i=0;i<16;++i)equal(std::bit_cast<unsigned>(native.finalMatrix.elements[i]),cpu.xf[i],"Restored enclosing matrix");
            equal(native.wheelBlurDiffuse,m.read32(blurGmp),"Final shared blur material");equal(m.read16(0xc98ad0c),0,"Original stack balance");
            for(auto bank:{Bank::car,Bank::numberPlate,Bank::wheelBlur}){const auto assembly=native.geometry(bank);unsigned index=0;
                for(const auto& item:reference)if(item.isGeometry()&&item.bank==bank){const auto& instance=assembly.instances.at(index++);equal(instance.chunk,item.chunk,"Native instance model");
                    for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)equal(std::bit_cast<unsigned>(instance.transform[row*4+col]),std::bit_cast<unsigned>(item.matrix.elements[col*4+row]),"Native renderer matrix layout");}
                equal(unsigned(assembly.instances.size()),index,"Native geometry bank extent");
            }
            ++frames;
        }
        std::cout<<"car "<<car<<" complete draw transforms matched"<<std::endl;
    }
    if(!copies||!blurDraws||plates!=frames*12)throw std::runtime_error("Missing copied-model/plate/blur coverage");
    std::cout<<"PASS "<<frames<<" complete car frames, "<<items<<" ordered rendered items, "<<matrixWords<<" exact draw-matrix words, "<<comparisons<<" total comparisons, "<<instructions<<" original instructions; "<<copies<<" copied-model draws, "<<plates<<" plate draws, "<<blurDraws<<" wheel-blur draws. Matrix helpers, assembly, model resolver, number plates and wheel-blur arithmetic execute original bytes. Only final graphics/effect/state submission, plate/blur asset lookup and division are hooked; hardware rasterization remains separate.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
