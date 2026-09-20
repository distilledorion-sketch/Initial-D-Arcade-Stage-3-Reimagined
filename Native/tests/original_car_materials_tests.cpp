#include "car_presentation.h"
#include "car_catalog.h"
#include "original_rival_appearance_catalog.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3;
using namespace idas3::reference;
int main(int argc,char**argv)try{
    if(argc!=3)throw std::runtime_error("project-root canonical-image required");
    const std::filesystem::path root=argv[1];RefMemory memory(argv[2]);memory.zeroRegion(0xd000000,4096);
    std::size_t patches=0,painted=0,alpha=0,gloss=0,sourceWords=0,instructions=0,rejected=0;
    for(unsigned appearance=0;appearance<66;++appearance){
        const bool rival=appearance>=35;const unsigned enemy=rival?appearance-35:0,car=rival?originalRivalAppearances[enemy].car:appearance;
        const std::string folder(originalCarFolders[car]);auto model=NativeModel::load(root/"data/original_models"/folder/(folder+".idasmesh"));
        const auto original=model;auto presentation=rival?CarPresentation::loadRival(root,car,enemy,model.chunks.size()):CarPresentation::load(root,car,model.chunks.size());
        const auto base=rival?root/"data/original_models/rivals"/("enemy_"+std::string(enemy<10?"0":"")+std::to_string(enemy)):root/"data/original_models"/folder/"fresh_player";
        std::ifstream configFile(base/"plate.bin",std::ios::binary);configFile.seekg(16);unsigned config=0;if(!configFile.read(reinterpret_cast<char*>(&config),4))throw std::runtime_error("Appearance configuration missing");
        RefCpu cpu(memory);cpu.r[4]=car;cpu.r[5]=(config>>25)&7;cpu.r[15]=0xd000f00;cpu.pr=0xf000000;
        instructions+=cpu.run(0xc1911a0,0xf000000,1000);const unsigned rgb=cpu.r[0];
        presentation.applyMaterials(model);patches+=presentation.materialPatchCount();bool havePaint=false;
        for(unsigned chunk=0;chunk<model.chunks.size();++chunk)for(unsigned batch=0;batch<model.chunks[chunk].batches.size();++batch){
            const auto& a=model.chunks[chunk].batches[batch];const auto& b=original.chunks[chunk].batches[batch];
            if(a.indices!=b.indices||a.vertices.size()!=b.vertices.size())throw std::runtime_error("Material override changed geometry");
            for(unsigned n=0;n<a.vertices.size();++n){const auto& av=a.vertices[n];const auto& bv=b.vertices[n];
                if(av.position.x!=bv.position.x||av.position.y!=bv.position.y||av.position.z!=bv.position.z||av.u!=bv.u||av.v!=bv.v||av.color0!=bv.color0||av.color1!=bv.color1)throw std::runtime_error("Material override changed authored vertices");}
            if((a.material[3]&0xffffff)!=(b.material[3]&0xffffff)){
                if((b.material[3]&0xffffff)!=0xffffff||b.material[9]!=0xffffffff||(a.material[3]&0xffffff)!=rgb)throw std::runtime_error("Paint differs from actual1911A0 palette/mask");
                ++painted;havePaint=true;
            }
            alpha+=(a.material[3]>>24)!=(b.material[3]>>24);gloss+=a.material[1]!=b.material[1];sourceWords+=24;
        }
        if(!havePaint||presentation.materialPatchCount()==0)throw std::runtime_error("Appearance has no original paint coverage");
        // Applying an already selected appearance is harmless, but an unrelated
        // imported material mismatch must reject before changing the model.
        presentation.applyMaterials(model);
        auto bad=original;bool changed=false;
        for(unsigned c=0;c<model.chunks.size()&&!changed;++c)for(unsigned b=0;b<model.chunks[c].batches.size()&&!changed;++b){
            const auto& selected=model.chunks[c].batches[b];auto& damaged=bad.chunks[c].batches[b];
            if(selected.material!=damaged.material||selected.ich!=damaged.ich){damaged.material[3]^=0x12345u;changed=true;}}
        try{presentation.applyMaterials(bad);}catch(const std::runtime_error&){++rejected;}
    }
    if(rejected!=66||!alpha||!gloss)throw std::runtime_error("Material validation coverage missing");
    std::cout<<"PASS66 source car appearances: "<<patches<<" captured batch patches, "<<painted<<" original palette RGB writes, "<<alpha<<" alpha and "<<gloss<<" gloss updates; "<<sourceWords<<" material words checked, "<<rejected<<" wrong-source rejections; "<<instructions<<" actual palette instructions.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
