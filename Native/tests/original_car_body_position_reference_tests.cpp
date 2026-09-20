#include "original_car_body_position.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3;
using namespace idas3::reference;
static std::vector<unsigned char> bytes(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("Source unavailable");return {std::istreambuf_iterator<char>(f),{}};}
static unsigned word(const std::vector<unsigned char>& b,unsigned n){return b.at(n)|(unsigned(b.at(n+1))<<8)|(unsigned(b.at(n+2))<<16)|(unsigned(b.at(n+3))<<24);}
int main(int argc,char**argv)try{
    if(argc!=4)throw std::runtime_error("project-root original-image HOSTFS required");
    const std::filesystem::path root=argv[1],host=argv[3];RefMemory m(argv[2]);
    const auto raw=bytes(host/"binary/k_df_colli_0.bin.nz"),path=bytes(host/"binary/PATH_dfi_0.bin");
    const auto collision=original::OriginalCollisionData::load(host/"binary/k_df_colli_0.bin.nz");
    constexpr unsigned data=0xcb00000,obj=0xcf00000,actor=0xc9008a4,stack=0xcfff000,frame=0xcf10000,thread=0xcf11000,stop=0xf000000;
    for(unsigned i=0;i<raw.size();++i)m.write8(data+i,raw[i]);
    for(auto offset:{12u,20u,28u,36u,44u})m.write32(data+offset,data+word(raw,offset));m.write32(0xc2eef6c,data);
    const auto fscaBytes=bytes(root/"data/original_physics/fsca_table.bin");std::vector<unsigned> fsca(32768);for(unsigned i=0;i<fsca.size();++i)fsca[i]=word(fscaBytes,16+i*4);
    std::size_t comparisons=0,instructions=0,hits=0,misses=0;
    const auto equal=[&](unsigned a,unsigned b,const char* what){++comparisons;if(a!=b)throw std::runtime_error(std::string(what)+" actual="+std::to_string(a)+" expected="+std::to_string(b));};
    for(unsigned car=0;car<35;++car){
        m.zeroRegion(obj,0x20000);m.zeroRegion(actor,168);m.zeroRegion(stack-0x2000,0x2000);m.zeroRegion(0xc98ad0c,12);m.zeroRegion(0xce00000,4096);
        m.write32(0xc98ad0c,0x200000);m.write32(0xc98ad10,0xce00000);m.write32(0xc98ad14,0xce00000);
        m.write32(obj+0x95c,actor);m.write32(obj+0x964+64,0xc380b8c);m.write32(obj+0x34c,car);m.write32(thread+4,thread+32);
        OriginalCarBodyPosition native;
        for(unsigned i=0;i<16;++i)m.write32(obj+0xaa0+i*4,native.query().words[i]);
        for(unsigned sample=0;sample<9;++sample){
            const auto index=(sample*97+car*3)%772;Vec3 p{std::bit_cast<float>(word(path,index*12)),std::bit_cast<float>(word(path,index*12+4))+2.f,std::bit_cast<float>(word(path,index*12+8))};
            if(sample==8){p.x+=100000.f;p.z+=100000.f;}
            m.writeFloat(actor,p.x);m.writeFloat(actor+4,p.y);m.writeFloat(actor+8,p.z);
            //034840 publishes raw actorXYZ at+9A8 after its matrix block.
            m.writeFloat(obj+0x9a8,p.x);m.writeFloat(obj+0x9ac,p.y);m.writeFloat(obj+0x9b0,p.z);
            RefCpu cpu(m);cpu.fscaHalfWave=fsca;cpu.r[13]=obj;cpu.r[14]=stack;cpu.r[15]=stack;
            instructions+=cpu.run(0xc03485c,0xc0348dc,2000);
            for(unsigned i=0;i<16;++i)cpu.xf[i]=std::bit_cast<unsigned>(i%5==0?1.f:0.f);
            cpu.r[12]=obj;cpu.r[13]=thread;cpu.r[14]=frame;cpu.r[15]=stack;
            cpu.callHooks[0xc055d60]=[](auto&){}; // diagnostic output on missed surface only
            instructions+=cpu.run(0xc034c4a,0xc034d0a,100000);
            cpu.r[4]=obj+0x964;cpu.pr=stop;instructions+=cpu.run(0xc1f64a0,stop,1000);
            const auto result=native.update(collision,car,p);if(native.surfaceFound())++hits;else ++misses;
            equal(std::bit_cast<unsigned>(result.x),cpu.xf[12],"body X");equal(std::bit_cast<unsigned>(result.y),cpu.xf[13],"body Y");equal(std::bit_cast<unsigned>(result.z),cpu.xf[14],"body Z");
            for(unsigned i=0;i<16;++i)equal(native.query().words[i],m.read32(obj+0xaa0+i*4),"persistent query");
        }
    }
    if(!hits||!misses)throw std::runtime_error("Missing success/failure coverage");
    std::cout<<"PASS original live body placement35 cars/315 queries, "<<comparisons<<" exact words, "<<instructions<<" original instructions; source matrix/query math, diagnostic-only hook; hits="<<hits<<", misses="<<misses<<'\n';
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
