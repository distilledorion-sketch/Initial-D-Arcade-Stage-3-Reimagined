#include "original_aura.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <limits>
using namespace idas3;
using namespace idas3::original;
using namespace idas3::reference;
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("canonical image and Native root required");
    RefMemory memory(argv[1]);const std::filesystem::path root=argv[2];
    constexpr unsigned obj=0xd000000,stack=obj+0x10000,stop=0xf000000;
    memory.zeroRegion(obj,0x200000);
    unsigned checks=0;std::size_t instructions=0;
    auto check=[&](bool b,const char* why){++checks;if(!b)throw std::runtime_error(why);};
    //Execute the original initializer's entire level/scale branch, stopping
    //before any file/GPU owner. No predicate or floating operation is hooked.
    for(bool opponent:{false,true})for(int level=-1;level<=101;++level)for(unsigned streak:{0u,9u,10u,99u,0xffffffffu}){
        memory.write32(stack+80,obj);memory.write32(stack+128,obj+124);
        memory.write32(obj+208,unsigned(level));memory.write32(obj+212,unsigned(level));
        memory.write32(obj+216,streak);memory.write32(obj+220,streak);
        memory.write32(obj+184,0);memory.write32(obj+188,0);
        RefCpu c(memory);c.r[14]=stack;c.r[15]=stack;c.r[4]=0;
        c.r[2]=opponent?obj+188:0xc28d898;c.r[3]=opponent?188:obj+188;
        instructions+=c.run(opponent?0xc17b2ac:0xc17b16a,opponent?0xc17b3c4:0xc17b266,1000);
        const auto expected=originalAuraStyle(unsigned(level),streak,opponent);
        check(expected.visible==(level>10),"Original battle-level visibility threshold");
        check(memory.read32(obj+(opponent?180:176))==expected.palette,"Original aura palette selection");
        if(memory.read32(obj+(opponent?188:184))!=std::bit_cast<unsigned>(expected.growth))std::cerr<<"growth opponent="<<opponent<<" level="<<level<<" expected="<<hex(memory.read32(obj+(opponent?188:184)))<<" actual="<<hex(std::bit_cast<unsigned>(expected.growth))<<'\n';
        check(memory.read32(obj+(opponent?188:184))==std::bit_cast<unsigned>(expected.growth),"Original local/opponent growth bits");
    }
    //The owner builds a fresh VUR draw block rather than submitting the
    //editing model's original material. Execute both actual constructors.
    RefCpu c(memory);c.r[15]=stack;c.pr=stop;c.r[4]=obj;c.r[5]=0x0100004a;c.r[6]=3474;
    instructions+=c.run(0xc1d0260,stop,2000);
    c.r[15]=stack;c.pr=stop;c.r[4]=obj;c.r[5]=0xffffffffu;
    c.callHooks[0xc1d8a60]=[](auto&){}; //cached geometry invalidation only
    instructions+=c.run(0xc1d0720,stop,1000);
    const auto ich=memory.read32(obj+16),material=memory.read32(obj+12);
    memory.write32(ich+4,memory.read32(0xc17b4d8));memory.write32(ich+8,memory.read32(0xc17b4dc));
    memory.write32(material+8,memory.read16(0xc17b4a2));
    OriginalAura aura;aura.load(root);const auto& batch=aura.model().chunks[0].batches[0];
    for(unsigned i=0;i<8;++i){if(memory.read32(ich+i*4)!=batch.ich[i])std::cerr<<"ICH "<<i<<" expected "<<hex(memory.read32(ich+i*4))<<" actual "<<hex(batch.ich[i])<<'\n';check(memory.read32(ich+i*4)==batch.ich[i],"Original runtime ICH material words");}
    for(unsigned i=0;i<16;++i)check(memory.read32(material+i*4)==batch.material[i],"Original runtime GMP material words");
    check(batch.vertices.size()==3474&&batch.indices.size()==10134,"Authored strip geometry retained");
    const std::array<const char*,10> names{"blue","green","crimson","orange","red","yellow","purple","cyan","white","spa"};
    const std::array<unsigned,10> levels{11,21,23,25,27,28,29,30,31,31};
    const auto original=NativeModel::load(root/"data/original_assets/aura/aura.idasmesh");
    //Execute the actual color-upload helper across both side branches and
    //wrap/half-frame boundaries. Only compiler integer division is hooked.
    constexpr unsigned colorData=obj+0x3000,colorPointers=obj+0x2000,draw=obj+0x50000,drawIch=draw+0x100;
    std::ifstream blueFile(root/"data/original_assets/aura/colors/blue_vtx.bin",std::ios::binary);
    const std::vector<unsigned char> blueBytes{std::istreambuf_iterator<char>(blueFile),{}};
    for(unsigned i=0;i<blueBytes.size();++i)memory.write8(colorData+i,blueBytes[i]);
    for(unsigned frame=0;frame<30;++frame)memory.write32(colorPointers+frame*4,colorData+frame*1788*4);
    memory.write32(obj+160,3474);memory.write32(obj+168,colorPointers);memory.write32(obj+172,colorPointers);
    memory.write32(draw+16,drawIch);
    aura.configure(11,0);
    for(unsigned side:{0u,1u})for(unsigned frame:{0u,1u,2u,3u,58u,59u,60u,61u,119u}){
        memory.write32(obj+20,frame);RefCpu upload(memory);
        upload.r[15]=stack;upload.pr=stop;upload.r[4]=obj;upload.r[5]=draw;upload.r[7]=side;
        upload.callHooks[0xc2223b8]=[](auto& cpu){cpu.fpul=cpu.r[4]/cpu.r[5];};
        instructions+=upload.run(0xc17b7e0,stop,200000);aura.update(frame,0,{},{0,0,-10});
        const auto& v=aura.model().chunks[0].batches[0].vertices;
        for(unsigned i=0;i<3474;++i){check(memory.read32(drawIch+32+i*32+24)==v[i].color0,"Actual SH4 local/remote color-upload words");
            check(memory.read32(drawIch+32+i*32+28)==v[i].color1,"Actual SH4 second-volume color words");}
    }
    for(unsigned palette=0;palette<10;++palette){
        std::ifstream f(root/"data/original_assets/aura/colors"/(std::string(names[palette])+"_vtx.bin"),std::ios::binary);
        const std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(f),{}};
        check(bytes.size()==214560,"Original color file size");
        aura.configure(levels[palette],palette==9?10:0);
        for(unsigned frame=0;frame<120;++frame){
            aura.update(frame,0,{2,3,5},{2,3,-10});
            check(aura.visible()&&aura.colorFrame()==(frame%60)/2,"Original30Hz sixty-tick color cycle");
            const auto& vertices=aura.model().chunks[0].batches[0].vertices;
            for(unsigned i=0;i<vertices.size();++i){
                const auto index=memory.read32(0xc320040+i*4);const auto offset=((frame%60)/2*1788+index)*4;
                const unsigned argb=(unsigned(bytes[offset])<<24)|(unsigned(bytes[offset+1])<<16)|(unsigned(bytes[offset+2])<<8)|bytes[offset+3];
                check(vertices[i].color0==argb&&vertices[i].color1==argb,"Authored ARGB colors and original duplicate-vertex mapping");
            }
            const auto before=vertices[917].color0;aura.update(frame,0,{2,3,5},{2,3,-10});
            check(aura.colorFrame()==(frame%60)/2&&aura.model().chunks[0].batches[0].vertices[917].color0==before,"Repeated draw does not advance animation");
        }
    }
    for(bool opponent:{false,true})for(unsigned level:{11u,17u,20u,21u,31u}){
        aura.configure(level,10,opponent);aura.update(15,0,{0,0,0},{0,0,-10});
        const auto style=originalAuraStyle(level,10,opponent);
        for(unsigned i=0;i<3474;++i){const auto p=original.chunks[0].batches[0].vertices[i].position,q=aura.model().chunks[0].batches[0].vertices[i].position;
            check(q.x==p.x*style.growth&&q.y==p.y*(style.growth+.1f)&&q.z==p.z*style.growth,"Source anisotropic aura scale");}
    }
    aura.configure(20,0);
    for(unsigned car=0;car<35;++car){
        aura.update(2,car,{7,11,13},{7,15,10});
        const auto& transform=aura.assembly().instances[0].transform;
        const float expected=memory.readFloat(0xc28d890+car*44+8)*.9f*3/5;
        check(std::abs(transform[11]-(13+expected))<0.00001f&&transform[3]==7&&transform[7]==11,"Source per-car depth displacement with 3D normalization and zero vertical shift");
        check(aura.assembly().instances[0].billboard,"Original view-facing aura orientation");
    }
    aura.update(2,35,{},{0,0,-10});check(!aura.visible(),"Invalid car ID rejected");
    aura.update(2,0,{std::numeric_limits<float>::quiet_NaN(),0,0},{});check(!aura.visible(),"Nonfinite pose rejected");
    aura.configure(10,100);aura.update(2,0,{},{0,0,-10});check(!aura.visible(),"Win streak alone cannot unlock aura");
    aura.configure(31,9);check(aura.style().palette==8,"Level31 streak9 remains white");
    aura.configure(31,10);check(aura.style().palette==9,"Level31 streak10 uses authored special palette");
    std::cout<<"PASS "<<checks<<" checks; "<<instructions<<" original scalar instructions;10 palettes x30 frames x3474 vertices, two loops and repeat-render stability\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
