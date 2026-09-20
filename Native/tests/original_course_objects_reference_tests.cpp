// Standalone development oracle: source 084780 builds the actual linked grid,
// source 084320 selects records, and source 085260 transforms/submits each.
// Allocator, exception bookkeeping, debug output and matrix library boundaries
// are explicit hooks. No original devices or game loop are executed.
#include "original_course_objects.h"
#include "sh4_scalar_reference.h"
#include <algorithm>
#include <iostream>
#include <memory>

using namespace idas3;
using namespace idas3::original;
using namespace idas3::reference;
template<class T>T read(std::istream& f){T v{};if(!f.read(reinterpret_cast<char*>(&v),sizeof(v)))throw std::runtime_error("test fixture truncated");return v;}
OriginalMatrix matrixMultiply(const OriginalMatrix& a,const OriginalMatrix& b){OriginalMatrix out;for(unsigned col=0;col<4;++col){auto v=transformOriginalVector(a,{b.elements[col*4],b.elements[col*4+1],b.elements[col*4+2],b.elements[col*4+3]});for(unsigned row=0;row<4;++row)out.elements[col*4+row]=v[row];}return out;}
struct SourceOwner {
    RefMemory memory;RefCpu cpu;const OriginalFscaTable& trig;
    OriginalMatrix matrix=originalIdentityMatrix();std::vector<OriginalMatrix> matrices;
    std::vector<unsigned> admitted;NativeModelInstance emitted;
    unsigned allocator=0xd300000;
    static constexpr unsigned object=0xd000000,records=0xd010000,settings=0xd100000,stack=0xd200000,stop=0xff0000,tls=0xd110000,frame=0xd111000,bank=0xd112000,bankVtable=0xd112100,admitHook=0xd112200,drawHook=0xd112204;
    OriginalMatrix loadMatrix(unsigned ptr){OriginalMatrix out;for(unsigned i=0;i<16;++i)out.elements[i]=memory.readFloat(ptr+i*4);return out;}
    void run(unsigned entry){cpu.r[15]=stack+0xf000;cpu.pr=stop;try{cpu.run(entry,stop,3000000);}catch(std::exception& e){throw std::runtime_error("oracle entry="+hex(entry)+" pc="+hex(cpu.pc)+" "+e.what());}}
    SourceOwner(const std::filesystem::path& image,const OriginalFscaTable& table,unsigned mode,const std::array<unsigned,24>& words,const std::vector<std::array<unsigned,10>>& raw,unsigned courseIndex):memory(image),cpu(memory),trig(table){
        memory.zeroRegion(object,0x1000000);memory.write32(tls+4,frame);memory.write32(frame+4,frame);
        memory.write32(object+4,unsigned(raw.size()));memory.write32(object+8,records);memory.write32(object+12,courseIndex);memory.write32(object+308,mode);
        for(unsigned i=0;i<24;++i)memory.write32(settings+i*4,words[i]);
        for(unsigned i=0;i<raw.size();++i)for(unsigned j=0;j<10;++j)memory.write32(records+i*40+j*4,raw[i][j]);
        memory.write32(object+160,bank);memory.write32(bank,bankVtable);memory.write32(bankVtable+12,drawHook);memory.write32(bankVtable+28,admitHook);
        cpu.callHooks[0xc221fc0]=[&](RefCpu& c){c.r[0]=tls;};
        cpu.callHooks[0xc055d60]=[](RefCpu&){};
        auto alloc=[&](unsigned size){const auto p=allocator;allocator+=(size+255)&~255u;if(allocator>=0xdf00000)throw std::runtime_error("oracle allocation bound");return p;};
        cpu.callHooks[0xc021960]=[&,alloc](RefCpu& c){c.r[0]=alloc(c.r[5]);};
        cpu.callHooks[0xc021f00]=[&,alloc](RefCpu& c){c.r[0]=alloc(c.r[4]);};
        cpu.callHooks[0xc1fcc60]=[&](RefCpu&){matrices.push_back(matrix);matrix=originalIdentityMatrix();};
        cpu.callHooks[0xc1f6610]=[&](RefCpu& c){matrices.push_back(matrix);if(c.r[4])matrix=loadMatrix(c.r[4]);};
        cpu.callHooks[0xc1f65c0]=[&](RefCpu& c){for(unsigned i=0;i<c.r[4];++i){if(matrices.empty())throw std::runtime_error("oracle matrix underflow");matrix=matrices.back();matrices.pop_back();}};
        cpu.callHooks[0xc1f6ac0]=[&](RefCpu& c){translateOriginalMatrix(matrix,{c.getFloat(4),c.getFloat(5),c.getFloat(6)});};
        cpu.callHooks[0xc1fd060]=[&](RefCpu& c){translateOriginalMatrix(matrix,{memory.readFloat(c.r[4]),memory.readFloat(c.r[4]+4),memory.readFloat(c.r[4]+8)});};
        cpu.callHooks[0xc1f69d0]=[&](RefCpu& c){scaleOriginalMatrix(matrix,{c.getFloat(4),c.getFloat(5),c.getFloat(6)});};
        cpu.callHooks[0xc1f68a0]=[&](RefCpu& c){rotateOriginalMatrixPhase(matrix,1,std::uint16_t(c.r[4]),trig);};
        cpu.callHooks[0xc1f67e0]=[&](RefCpu& c){rotateOriginalMatrixPhase(matrix,0,std::uint16_t(c.r[4]),trig);};
        cpu.callHooks[0xc1f64a0]=[&](RefCpu& c){matrix=matrixMultiply(matrix,loadMatrix(c.r[4]));};
        cpu.callHooks[0xc1fc5a0]=[&](RefCpu& c){matrix=loadMatrix(c.r[4]);};
        cpu.callHooks[0xc1fbd60]=[&](RefCpu& c){for(unsigned i=0;i<16;++i)memory.writeFloat(c.r[4]+i*4,matrix.elements[i]);};
        cpu.callHooks[0xc1f66a0]=[&](RefCpu&){matrix=originalIdentityMatrix();}; // render's view-direction calculation is unused by admission
        cpu.callHooks[0xc1fbf80]=[&](RefCpu&){matrix=originalIdentityMatrix();};
        for(auto [address,w]:std::array<std::pair<unsigned,float>,2>{{{0xc1f6260,1.f},{0xc1f6280,0.f}}})cpu.callHooks[address]=[&,w](RefCpu& c){auto v=transformOriginalVector(matrix,{memory.readFloat(c.r[4]),memory.readFloat(c.r[4]+4),memory.readFloat(c.r[4]+8),w});for(unsigned i=0;i<3;++i)memory.writeFloat(c.r[5]+i*4,v[i]);};
        cpu.callHooks[0xc1f98c0]=[&](RefCpu& c){c.setFloat(0,trig.sinCos(std::uint16_t(c.r[4]))[0]);};
        cpu.callHooks[0xc1f9fe0]=[&](RefCpu& c){c.setFloat(0,trig.sinCos(std::uint16_t(c.r[4]))[1]);};
        cpu.callHooks[0xc1f6cf0]=[&](RefCpu& c){const float x=memory.readFloat(c.r[4]),y=memory.readFloat(c.r[4]+4),z=memory.readFloat(c.r[4]+8);const float inv=1/std::sqrt(float(double(x)*x+double(y)*y+double(z)*z));memory.writeFloat(c.r[4],x*inv);memory.writeFloat(c.r[4]+4,y*inv);memory.writeFloat(c.r[4]+8,z*inv);};
        cpu.callHooks[0xc1f6ca0]=[&](RefCpu& c){const float x=memory.readFloat(c.r[4]),y=memory.readFloat(c.r[4]+4),z=memory.readFloat(c.r[4]+8);c.setFloat(0,float(double(x)*x+double(y)*y+double(z)*z));};
        cpu.callHooks[admitHook]=[](RefCpu& c){c.r[0]=1;};
        cpu.callHooks[drawHook]=[&](RefCpu& c){emitted.chunk=c.r[5];for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)emitted.transform[row*4+col]=matrix.elements[col*4+row];};
        cpu.r[4]=object;cpu.r[5]=settings;run(0xc084780);
        if(!matrices.empty())throw std::runtime_error("setup matrix leak");
    }
    std::vector<NativeModelInstance> draw(Vec3 position,unsigned chunkBase,const std::array<int,4>& ranges){
        for(unsigned i=0;i<4;++i)memory.write32(object+144+i*4,unsigned(ranges[i]));memory.write32(object+164,chunkBase);
        memory.writeFloat(object+292,position.x);memory.writeFloat(object+296,position.y);memory.writeFloat(object+300,position.z);
        admitted.clear();matrix=originalIdentityMatrix();matrices.clear();cpu.callHooks[0xc085260]=[&](RefCpu& c){admitted.push_back(c.r[5]);};cpu.r[4]=object;run(0xc084320);
        if(!matrices.empty())throw std::runtime_error("render matrix leak");
        cpu.callHooks.erase(0xc085260);std::vector<NativeModelInstance> out;
        for(auto record:admitted){matrix=originalIdentityMatrix();cpu.r[4]=object;cpu.r[5]=record;run(0xc085260);out.push_back(emitted);}
        return out;
    }
};
struct Draw{unsigned owner,base,before;std::array<int,4> ranges;};
struct Choice{unsigned staticCount;std::vector<Draw> draws;};
int main(int argc,char**argv){try{
    if(argc!=3)throw std::runtime_error("image native-root required");const std::filesystem::path image=argv[1],root=argv[2];
    const auto trig=OriginalFscaTable::load(root/"data/original_physics/fsca_table.bin");
    const std::array<std::string,9> ids={"k_ez","s_nm","h_hd","k_df","s_vh","s_uh","n_sy","k_tu","k_df3"};
    unsigned cases=0,variants=0;std::size_t transforms=0,maxDraws=0;
    for(unsigned ci=0;ci<9;++ci)for(bool night:{false,true})for(bool wet:{false,true})for(bool reverse:{false,true}){
        const auto name=std::string(night?"night":"day")+(reverse?"_reverse":"_forward")+(wet?"_wet":"");
        const auto path=root/"data/original_models/courses"/ids[ci]/("scene_"+name+".idasobjects");
        std::ifstream f(path,std::ios::binary);(void)read<std::array<char,8>>(f);(void)read<unsigned>(f);auto pathCount=read<unsigned>(f);auto ownerCount=read<unsigned>(f),choiceCount=read<unsigned>(f),changes=read<unsigned>(f);
        std::vector<std::unique_ptr<SourceOwner>> owners;
        for(unsigned i=0;i<ownerCount;++i){auto mode=read<unsigned>(f);auto settings=read<std::array<unsigned,24>>(f);auto count=read<unsigned>(f);std::vector<std::array<unsigned,10>> records;
            for(unsigned j=0;j<count;++j){records.push_back(read<std::array<unsigned,10>>(f));(void)read<unsigned>(f);}
            owners.push_back(std::make_unique<SourceOwner>(image,trig,mode,settings,records,mode?ci:unsigned(-1)));}
        std::vector<Choice> choices;
        for(unsigned i=0;i<choiceCount;++i){Choice choice;choice.staticCount=read<unsigned>(f);auto count=read<unsigned>(f);for(unsigned j=0;j<count;++j){Draw d;d.owner=read<unsigned>(f);d.base=read<unsigned>(f);d.before=read<unsigned>(f);d.ranges=read<std::array<int,4>>(f);choice.draws.push_back(d);}choices.push_back(std::move(choice));}
        std::vector<std::array<unsigned,2>> transitions;for(unsigned i=0;i<changes;++i)transitions.push_back(read<std::array<unsigned,2>>(f));
        std::ifstream route(root/"data/courses"/((ci==8?std::string("k_df"):ids[ci])+"_path.bin"),std::ios::binary);auto header=read<std::array<unsigned,2>>(route);if(header[0]!=pathCount||header[1]!=3)throw std::runtime_error("test route header");
        std::vector<Vec3> points;for(unsigned i=0;i<pathCount;++i){auto p=read<std::array<float,3>>(route);points.push_back({p[0],p[1],p[2]});}
        auto native=OriginalCourseObjects::load(root,ids[ci],night,reverse,wet,4096);
        std::vector<unsigned> samples{0,pathCount-1};for(unsigned i=31;i<pathCount;i+=173)samples.push_back(i);for(const auto& t:transitions){samples.push_back(t[0]);if(t[0])samples.push_back(t[0]-1);}std::sort(samples.begin(),samples.end());samples.erase(std::unique(samples.begin(),samples.end()),samples.end());
        for(auto index:samples){unsigned selected=0;for(auto t:transitions){if(t[0]>index)break;selected=t[1];}const auto& choice=choices[selected];const auto actual=native.insertionsForPathIndex(index,points[index],choice.staticCount);
            std::vector<NativeAssemblyInsertion> expected;for(const auto& draw:choice.draws){if(expected.empty()||expected.back().before!=draw.before)expected.push_back({draw.before,{}});auto instances=owners[draw.owner]->draw(points[index],draw.base,draw.ranges);expected.back().assembly.instances.insert(expected.back().assembly.instances.end(),instances.begin(),instances.end());}
            if(actual.size()!=expected.size())throw std::runtime_error("insertion count mismatch");std::size_t draws=0;
            for(unsigned i=0;i<actual.size();++i){const auto& a=actual[i];const auto& b=expected[i];if(a.before!=b.before||a.assembly.instances.size()!=b.assembly.instances.size())throw std::runtime_error(ids[ci]+" "+name+" path"+std::to_string(index)+" count native="+std::to_string(a.assembly.instances.size())+" source="+std::to_string(b.assembly.instances.size()));
                for(unsigned j=0;j<a.assembly.instances.size();++j){const auto& av=a.assembly.instances[j];const auto& bv=b.assembly.instances[j];if(av.chunk!=bv.chunk||av.transform!=bv.transform)throw std::runtime_error(ids[ci]+" "+name+" path"+std::to_string(index)+" exact ordered transform mismatch");++transforms;}draws+=a.assembly.instances.size();}
            maxDraws=std::max(maxDraws,draws);++cases;}
        ++variants;std::cout<<ids[ci]<<' '<<name<<" passed "<<samples.size()<<" paths\n";
    }
    std::cout<<"PASS variants="<<variants<<" path_cases="<<cases<<" exact_ordered_transforms="<<transforms<<" maximum_tree_draws="<<maxDraws<<'\n';
}catch(std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
