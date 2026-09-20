#include "native_assets.h"
#include "sh4_scalar_reference.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>

using namespace idas3;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}

void checkOriginalColorBlocks(const std::filesystem::path& imagePath){
    // Actual instructions, with only the parser iterator replaced by explicit
    // GMP/ICH/VUR events. This closes color handling, not parser/animation parity.
    reference::RefMemory memory(imagePath);
    constexpr std::uint32_t object=0x0c600000,buffer=0x0c601000,saved=0x0c602000;
    constexpr std::uint32_t stack=0x0c700000,gmp=0x0c603000,ich=0x0c604000,vertices=0x0c605000;
    for(unsigned count:{0u,1u,4u,8u}){
        memory.clear();memory.zeroRegion(object,0x10000);memory.zeroRegion(stack-0x1000,0x2000);
        reference::RefCpu init(memory);init.r[14]=stack;init.r[0]=buffer;
        memory.write32(stack+48,object);memory.write32(stack+52,count);
        init.run(0x0c1b8010,0x0c1b8036);
        for(unsigned i=0;i<count;i++){
            require(memory.read32(buffer+i*8)==0xffffffff,"Original default base color is not white");
            require(memory.read32(buffer+i*8+4)==0,"Original second-volume color is not zero");
        }
        if(!count)continue;
        for(unsigned forceAlpha:{0u,1u}){
            memory.write32(object+4,count);memory.write32(object+8,saved);
            memory.write32(gmp+8,0x22);memory.write32(gmp+16,0x00ffffff);
            memory.write32(ich+8,0x9400246c);
            for(unsigned i=0;i<count;i++){
                memory.write32(vertices+i*32+24,0xff000000+i);
                memory.write32(vertices+i*32+28,0xff112233+i);
            }
            reference::RefCpu apply(memory);apply.r[4]=object;apply.r[5]=gmp;
            apply.r[6]=forceAlpha;apply.r[15]=stack;apply.pr=0x0c60ff00;
            apply.callHooks[0x0c1d4be0]=[](auto&){};
            unsigned event=0;
            apply.callHooks[0x0c1d4c40]=[&](auto& cpu){
                const unsigned current=event++;
                if(current==0){memory.write32(cpu.r[5],1);cpu.r[0]=gmp;}
                else if(current==1){memory.write32(cpu.r[5],2);memory.write32(cpu.r[6],0x4a);cpu.r[0]=ich;}
                else if(current<count+2){memory.write32(cpu.r[5],3);cpu.r[0]=vertices+(current-2)*32;}
                else{memory.write32(cpu.r[5],memory.read32(0x0c1b838c));cpu.r[0]=0;}
            };
            apply.run(0x0c1b8240,apply.pr);
            require(memory.read32(gmp+8)==0x600,"Original UI helper did not bypass both volumes' lighting");
            require(memory.read32(ich+8)==(0x9400246c|(forceAlpha?0x100000:0)),"Original UI alpha flag mismatch");
            for(unsigned i=0;i<count;i++){
                require(memory.read32(vertices+i*32+24)==0xffffffff&&memory.read32(vertices+i*32+28)==0,"Original UI color replacement mismatch");
                require(memory.read32(saved+i*8)==0xff000000+i&&memory.read32(saved+i*8+4)==0xff112233+i,"Original UI helper did not preserve prior colors");
            }
        }
    }
}

int main(int argc,char** argv){try{
    if(argc!=3)throw std::runtime_error("Pass game root and canonical original image");
    checkOriginalColorBlocks(argv[2]);
    const auto bank=NativeTextureBank::load(std::filesystem::path(argv[1])/"data/original_assets/menus/v3/v3sS00minicar/textures/textures.idastex");
    const auto& texture=bank.at(0);
    auto selected=std::find_if(texture.argb.begin(),texture.argb.end(),[](auto color){
        const auto rgb=color&0xffffff;return (color>>24)==255&&rgb!=0&&rgb!=0xffffff&&((rgb>>16)&255)!=(rgb&255);
    });
    require(selected!=texture.argb.end(),"Original minicar texture has no opaque colored sample");
    const auto sample=std::size_t(selected-texture.argb.begin());
    const float u=(float(sample%texture.width)+.5f)/texture.width;
    const float v=(float(sample/texture.width)+.5f)/texture.height;
    NativeModelChunk chunk;chunk.batches.resize(1);auto& batch=chunk.batches[0];
    batch.material[2]=0x22;batch.material[4]=0x00ffffff;batch.material[9]=0;
    batch.ich[0]=0x0c;batch.ich[2]=(1u<<29)|(1u<<20)|(1u<<16)|(1u<<15)|(3u<<6);batch.ich[6]=0x4a;
    for(auto position:{Vec3{0,0,0},Vec3{0,8,0},Vec3{8,0,0},Vec3{8,8,0}}){
        NativeModelVertex vertex;vertex.position=position;vertex.u=u;vertex.v=v;
        vertex.color0=0xff000000;vertex.color1=0xff00ff00;batch.vertices.push_back(vertex);
    }
    batch.indices={0,1,2,2,1,3};std::vector<std::uint32_t> pixels(64);
    SpritePlacement placement;placement.defaultOriginalUiColors=true;
    compositeOriginalMenuChunk(pixels,8,8,bank,chunk,placement);
    for(auto pixel:pixels)require(pixel==*selected,"UI source texture RGB was replaced by a silhouette");
    require(batch.material[2]==0x22&&batch.vertices[0].color0==0xff000000,"UI rendering mutated imported source");
    placement.defaultOriginalUiColors=false;batch.material[2]=0x600;
    for(auto& vertex:batch.vertices)vertex.color0=0xffffffff;
    compositeOriginalMenuChunk(pixels,8,8,bank,chunk,placement);
    for(auto pixel:pixels)require(pixel==*selected,"Second-volume diffuse leaked into first-volume offset");
    placement.defaultOriginalUiColors=true;batch.ich[6]=0x0a;batch.material[2]=2;
    compositeOriginalMenuChunk(pixels,8,8,bank,chunk,placement);
    for(auto pixel:pixels)require(pixel==0xffffffff,"VUR UI override altered another original vertex layout");
    std::cout<<"PASS original menu colors: actual constructor loop and 1B8240 instructions with explicit parser-event hooks; source texture RGB, volume separation, source immutability and VUR-only compositor scope\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
