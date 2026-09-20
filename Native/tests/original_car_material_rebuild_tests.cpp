#include "original_car_material_rebuild.h"
#include "original_car_color_catalog.h"
#include "car_catalog.h"
#include "sh4_scalar_reference.h"
#include <iostream>

using namespace idas3;
using namespace idas3::original;
using namespace idas3::reference;
template<class T>T read(std::istream& f){T out{};if(!f.read(reinterpret_cast<char*>(&out),sizeof out))throw std::runtime_error("Test layout truncated");return out;}
int main(int argc,char**argv)try{
    if(argc!=3)throw std::runtime_error("project-root canonical-image required");
    const std::filesystem::path root=argv[1];RefMemory m(argv[2]);
    constexpr unsigned object=0xd000000,frame=0xd050000,stack=0xd0f0000,modelBase=0xd100000,pointers=0xd060000,stop=0x00ff0000;
    std::size_t cases=0,checks=0,instructions=0,cloneCount=0,vertices=0;unsigned activeCar=0,variant=0,condition=0,color=0;
    auto equal=[&](unsigned a,unsigned b,const std::string& label){++checks;if(a!=b)throw std::runtime_error(label+" car="+std::to_string(activeCar)+" variant="+hex(variant)+" condition="+hex(condition)+" color="+std::to_string(color)+" original="+hex(a)+" native="+hex(b));};
    for(activeCar=0;activeCar<35;++activeCar){
        m.clear();
        const auto folder=root/"data/original_models"/originalCarFolders[activeCar];
        const auto model=NativeModel::load(folder/(std::string(originalCarFolders[activeCar])+".idasmesh"));
        auto native=OriginalCarMaterialRebuild::load(folder/"material_layout.bin",activeCar);
        m.zeroRegion(object,0x100000);std::ifstream layout(folder/"material_layout.bin",std::ios::binary);layout.seekg(20);
        const auto rawMap=read<std::array<int,212>>(layout);
        std::vector<unsigned> addresses;
        // The source parser reads authored GMP/ICH records and skips vertex
        // payloads. NativeModel independently checks each decoded draw batch.
        for(unsigned i=0;i<model.chunks.size();++i){const unsigned offset=read<unsigned>(layout),size=read<unsigned>(layout),materials=read<unsigned>(layout),batches=read<unsigned>(layout);
            const unsigned address=modelBase+offset;addresses.push_back(address);m.zeroRegion(address,size);
            const auto& c=model.chunks[i];equal(offset,c.sourceOffset,"Source chunk offset");equal(size,c.sourceSize,"Source chunk size");
            const bool omitted=c.header[0]==0xffffffffu;
            for(unsigned w=0;w<(omitted?2u:24u);++w)m.write32(address+w*4,c.header[w]);
            m.write32(pointers+i*4,omitted?0:address);
            for(unsigned j=0;j<materials;++j){const auto off=read<unsigned>(layout);const auto words=read<std::array<unsigned,16>>(layout);for(unsigned w=0;w<16;++w)m.write32(modelBase+off+w*4,words[w]);}
            for(unsigned j=0;j<batches;++j){const auto off=read<unsigned>(layout);read<unsigned>(layout);const auto words=read<std::array<unsigned,8>>(layout);for(unsigned w=0;w<8;++w)m.write32(modelBase+off+w*4,words[w]);}
        }
        m.write32(object+4,pointers);m.write32(frame+48,object);m.write32(frame+52,activeCar);
        RefCpu cpu(m);cpu.r[14]=frame;cpu.r[15]=stack;cpu.pr=stop;instructions+=cpu.run(0xc026436,0xc0264bc,10000);
        cpu.callHooks[0xc055d60]=[](auto&){}; // diagnostics
        cpu.callHooks[0xc212960]=[](auto& c){c.r[0]=0;}; // completed upload
        cpu.callHooks[0xc1f6f80]=[](auto&){}; // texture-bank device binding
        cpu.callHooks[0xc1d8a60]=[](auto&){}; // device classification of copies
        unsigned allocation=0xe000000;
        cpu.callHooks[0xc1cf360]=[&](auto& c){const unsigned size=m.read32(c.r[4]+24),result=allocation;allocation+=(size+31)&~31u;
            if(size<96||allocation>=0xef00000)throw std::runtime_error("Material reference allocation bound");
            for(unsigned i=0;i<size;++i)m.write8(result+i,m.read8(c.r[4]+i));c.r[0]=result;addresses.push_back(result);++cloneCount;
        };
        cpu.r[4]=object;cpu.r[15]=stack;cpu.pr=stop;instructions+=cpu.run(0xc0267c0,stop,20000000);
        for(unsigned slot=0;slot<212;++slot){
            const int index=native.semanticChunks()[slot];const unsigned expected=index<0?0:addresses.at(index);
            cpu.r[4]=object;cpu.r[5]=slot;cpu.r[15]=stack;cpu.pr=stop;instructions+=cpu.run(0xc026100,stop,1000);
            equal(cpu.r[0],expected,"Resolved semantic "+std::to_string(slot));
            if(slot<140||slot>186)equal(unsigned(rawMap[slot]),m.read32(m.read32(object+0x354)+slot*4),"Canonical static map");
        }
        auto compare=[&](bool rebuilt){
            equal(unsigned(addresses.size()),unsigned(native.chunks().size()),"Copied chunk count");
            for(unsigned c=0;c<native.chunks().size();++c){const auto& chunk=native.chunks()[c];const unsigned address=addresses[c];
                for(const auto& mat:chunk.materials)for(unsigned w=0;w<16;++w){const auto value=m.read32(address+mat.sourceOffset-chunk.sourceOffset+w*4);
                    if(value!=mat.words[w])equal(value,mat.words[w],"GMP chunk"+std::to_string(c)+" offset"+hex(mat.sourceOffset)+" word"+std::to_string(w));else ++checks;}
                for(const auto& batch:chunk.batches)for(unsigned w=0;w<8;++w){const auto value=m.read32(address+batch.sourceOffset-chunk.sourceOffset+w*4);
                    if(value!=batch.words[w])equal(value,batch.words[w],"ICH chunk"+std::to_string(c)+" word"+std::to_string(w));else ++checks;}
            }
            const auto& s=native.state();for(unsigned i=0;i<8;++i)equal(m.read32(object+0x294+i*4),s.paintMask[i],"Paint mask");
            equal(m.read32(object+0x874),s.gloss,"Gloss state");equal(m.read32(object+0x884),s.specular,"Specular state");
            if(rebuilt){for(unsigned i=0;i<3;++i)equal(m.read32(object+0x878+i*4),s.rgb[i],"Paint RGB");
                equal(m.read32(object+0x888),std::bit_cast<unsigned>(s.bodyAlpha),"Body alpha state");
                equal(m.read32(object+0x88c),std::bit_cast<unsigned>(s.glassAlpha),"Glass alpha state");
                equal(m.read32(object+0x890),std::bit_cast<unsigned>(s.bodyShadowAlpha),"Body shadow state");
                equal(m.read32(object+0x894),std::bit_cast<unsigned>(s.glassShadowAlpha),"Glass shadow state");}
            ++cases;
        };
        compare(false);
        // This discarded course query is the only material-rebuild hook.
        // Both original parsers, palette, alpha, power and conversion run.
        cpu.callHooks[0xc16d680]=[](auto& c){c.r[0]=0;};
        OriginalCarAppearanceConfig config(activeCar);
        auto rebuild=[&](bool dirty){config.word=(color<<25)|0x01ffffff;config.materialVariant=variant;config.paintDirty=dirty;
            m.write32(object+0x2d4,config.word);m.write32(object+0x6a8,variant);m.write8(object+0x6b9,dirty?1:0);m.write32(0xc31c99c+32,condition);
            cpu.r[13]=object;cpu.r[14]=frame;cpu.r[15]=stack;cpu.pr=stop;instructions+=cpu.run(0xc02988e,0xc029ad0,20000000);
            native.rebuild(config,condition);equal(m.read8(object+0x6b9),config.paintDirty,"Paint dirty consumed");compare(true);
        };
        // Every factory palette, and every day/night/condition branch on each
        // authored model. The signed-remainder boundary is model-independent;
        // its additional extreme inputs are checked on the first car.
        variant=condition=0;for(color=0;color<originalCarColorCounts[activeCar];++color)rebuild(true);
        color=0;for(condition=0;condition<2;++condition)for(unsigned v:{0u,4u}){variant=v;if(condition||variant)rebuild(true);}
        if(activeCar==0)for(unsigned v:{1u,3u,7u,8u,12u,0x7fffffffu,0x80000000u,0xfffffffcu,0xffffffffu})for(unsigned c:{0u,1u,2u,0xffffffffu}){
            variant=v;condition=c;rebuild(true);
        }
        // Rebuilding an unchanged part selection must retain previous paint.
        if(originalCarColorCounts[activeCar]>1){color=1;rebuild(false);}
        const auto rendered=native.apply(model);
        for(unsigned c=0;c<model.chunks.size();++c)for(unsigned b=0;b<model.chunks[c].batches.size();++b){const auto& a=model.chunks[c].batches[b];const auto& r=rendered.chunks[c].batches[b];
            equal(unsigned(a.vertices.size()),unsigned(r.vertices.size()),"Preserved vertex count");if(a.indices!=r.indices)throw std::runtime_error("Material rebuild changed index geometry");
            for(unsigned n=0;n<a.vertices.size();++n){const auto& x=a.vertices[n];const auto& y=r.vertices[n];
                if(x.header!=y.header||x.position.x!=y.position.x||x.position.y!=y.position.y||x.position.z!=y.position.z||x.normal.x!=y.normal.x||x.normal.y!=y.normal.y||x.normal.z!=y.normal.z||x.u!=y.u||x.v!=y.v||x.color0!=y.color0||x.color1!=y.color1)throw std::runtime_error("Material rebuild changed authored vertex");++vertices;}
        }
        std::cout<<"car"<<activeCar<<" constructor/repaint/material copies matched"<<std::endl;
    }
    std::cout<<"PASS "<<cases<<" native material constructor/rebuild states, "<<checks<<" exact comparisons, "<<instructions<<" original instructions, "<<cloneCount<<" separate copied models, "<<vertices<<" preserved authored vertices. Hardware upload/classification and discarded query boundaries explicitly hooked; GMP/ICH parsers and material arithmetic unhooked.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
