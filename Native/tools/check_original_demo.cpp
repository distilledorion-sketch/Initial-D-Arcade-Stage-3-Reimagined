#include "original_demo_data.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned owner=0xd000000,table=0xd001000,target=0xd002000,poseOwner=0xd003000,stack=0xd100000,stop=0x00ff0000;
std::uint64_t checks=0,instructions=0;
void eq(unsigned a,unsigned b,const char* label){++checks;if(a!=b)throw std::runtime_error(std::string(label)+": "+hex(a)+" != "+hex(b));}
void run(RefCpu& c,unsigned start,unsigned end=stop){try{instructions+=c.run(start,end,500000);}catch(const std::exception& e){throw std::runtime_error(hex(start)+" at "+hex(c.pc)+": "+e.what());}}
}
int main(int argc,char**argv){try{
    if(argc!=3)throw std::runtime_error("usage: check_original_demo image project");
    RefMemory m(argv[1]);const auto data=OriginalDemoData::load(argv[2]);
    m.zeroRegion(owner,0x10000);m.zeroRegion(stack-0x10000,0x11000);
    auto seed=[&](){RefCpu c(m);c.r[4]=owner;c.r[15]=stack;c.pr=stop;return c;};
    { // Exact Init table relocation, original instructions only.
        const auto& shots=data.shots();unsigned cumulative=0;
        for(unsigned i=0;i<shots.size();++i){for(unsigned k=0;k<6;++k)m.write32(table+i*24+k*4,shots[i].frames[k]-(k<4?cumulative:0));cumulative=shots[i].frames[1]+1;}
        m.write32(owner+1152,table);m.write32(owner+1144,unsigned(shots.size()));
        auto c=seed();c.r[4]=0;c.r[14]=stack-0x1000;c.r[7]=owner+1148;
        m.write32(c.r[14]+320,owner+1084);
        run(c,0xc0e64cc,0xc0e653c);
        for(unsigned i=0;i<shots.size();++i)for(unsigned k=0;k<6;++k)eq(m.read32(table+i*24+k*4),shots[i].frames[k],"source relocated shot");
    }
    const auto poseBase=RefMemory::imageBase+unsigned(m.image.size());
    {std::ifstream f(std::filesystem::path(argv[2])/"data/original_assets/attract/demo/actors.bin",std::ios::binary);m.image.insert(m.image.end(),std::istreambuf_iterator<char>(f),{});}
    m.write32(poseOwner+4,table);m.write32(poseOwner+8,poseBase);m.write32(poseOwner+16,unsigned(data.shots().size()));m.write32(poseOwner+24,data.frameCount());
    OriginalDemoCursor cursor;
    for(unsigned frame=0;frame<data.frameCount();++frame){
        eq(cursor.frame,frame,"full inclusive timeline");
        for(unsigned actor=0;actor<2;++actor){auto c=seed();c.r[4]=poseOwner;c.r[5]=target;c.r[6]=actor;m.write32(poseOwner+20,frame);m.write32(target+168,0x5a5a5a5a);run(c,0xc157580);
            const auto& expected=data.actor(frame,actor);for(unsigned k=0;k<42;++k)eq(m.read32(target+k*4),expected.words[k],"original actor record");eq(m.read32(target+168),0x5a5a5a5a,"copy boundary");}
        m.write32(poseOwner+12,cursor.shot);m.write32(poseOwner+20,cursor.frame);auto c=seed();c.r[4]=poseOwner;run(c,0xc157640);const auto event=data.step(cursor);
        eq(m.read32(poseOwner+12),cursor.shot,"shot advancement");eq(m.read32(poseOwner+20),cursor.frame,"frame advancement");eq(c.r[0],event,"shot event");
    }
    eq(cursor.shot,0,"wrap shot");eq(cursor.frame,0,"wrap frame");
    for(unsigned variant=0;variant<3;++variant)for(unsigned shot=0;shot<data.shots().size();++shot)for(unsigned actor=0;actor<2;++actor){
        m.write32(owner+1216,variant);m.write32(owner+1184,shot);auto c=seed();run(c,actor?0xc0e8000:0xc0e7fc0);
        eq(m.read32(0xc2624d4+c.r[0]*4),data.shots()[shot].cars[actor],"original car binding");
        eq(m.read32(0xc2624b8+c.r[0]*4),data.shots()[shot].enemies[actor],"original enemy binding");
    }
    std::cout<<"PASS original Demo7 data: "<<data.shots().size()<<" shots, "<<data.frameCount()<<" frames, 11674 complete original168-byte actor records; "<<checks<<" comparisons, "<<instructions<<" original instructions. No hooks; source table relocation, copy, advance and3 variant car bindings. Camera evaluation and enclosing demo rendering are separate.\n";
    return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
