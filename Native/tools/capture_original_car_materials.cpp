// Development-only source-byte capture. Shipping code reads ordinary material
// overrides; it does not execute this reference CPU or original addresses.
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3::reference;
static std::vector<unsigned char> bytes(const std::filesystem::path&p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("Missing material source");return {std::istreambuf_iterator<char>(f),{}};}
static unsigned word(const std::vector<unsigned char>&b,unsigned p){return b.at(p)|(unsigned(b.at(p+1))<<8)|(unsigned(b.at(p+2))<<16)|(unsigned(b.at(p+3))<<24);}
int main(int argc,char**argv)try{
    if(argc!=8&&argc!=9)throw std::runtime_error("image rawPolygon offsetTable car enemy outputRaw metadata [playerColor] required");
    RefMemory m(argv[1]);const auto polygon=bytes(argv[2]),table=bytes(argv[3]);
    const unsigned car=std::stoul(argv[4]);const int enemy=std::stoi(argv[5]);if(car>=35||enemy < -1||enemy>30)throw std::runtime_error("Appearance bounds");
    const unsigned color=argc==9?std::stoul(argv[8]):0;
    if(color>=8||(enemy>=0&&color))throw std::runtime_error("Player paint selection bounds");
    constexpr unsigned obj=0xd000000,scene=0xd040000,frame=0xd050000,stack=0xd0f0000,model=0xd100000,pointers=0xd060000,stop=0xf000000;
    m.zeroRegion(obj,0x100000);for(unsigned i=0;i<polygon.size();++i)m.write8(model+i,polygon[i]);
    //1D4FC0/1D5160 reject the authoredFFFFFFFF nongeometry marker;
    //the original materialization table publishes a null pointer for it.
    for(unsigned i=0;i<table.size()/4;++i){const unsigned offset=word(table,i*4);m.write32(pointers+i*4,word(polygon,offset)==0xffffffffu?0:model+offset);}
    m.write32(obj+4,pointers);m.write32(obj+0x34c,car);m.write32(obj+0x354,m.read32(0xc33b250+car*4));
    m.write32(frame+48,obj);m.write32(frame+52,car);
    RefCpu cpu(m);cpu.r[15]=stack;cpu.pr=stop;std::size_t instructions=0,clones=0;
    cpu.callHooks[0xc055d60]=[](auto&){}; // diagnostics only
    cpu.callHooks[0xc212960]=[](auto&c){c.r[0]=0;}; // completed device upload
    cpu.callHooks[0xc1f6f80]=[](auto&){}; // texture-bank binding
    cpu.callHooks[0xc1d8a60]=[](auto&){}; // upload/classification of copied shadow models
    cpu.callHooks[0xc16d680]=[](auto&c){c.r[0]=0;}; // course query; return discarded at029958
    unsigned allocation=0xe000000;
    cpu.callHooks[0xc1cf360]=[&](auto&c){const unsigned size=m.read32(c.r[4]+24);if(size<96||size>0x400000)throw std::runtime_error("Shadow copy bounds");
        const unsigned result=allocation;allocation+=(size+31)&~31u;if(allocation>0xef00000)throw std::runtime_error("Shadow capture allocation cap");
        for(unsigned i=0;i<size;++i)m.write8(result+i,m.read8(c.r[4]+i));c.r[0]=result;++clones;};
    cpu.callHooks[0xc2223b8]=[](auto&c){if(!c.r[5])throw std::runtime_error("Unsigned division zero");c.fpul=c.r[4]/c.r[5];};
    cpu.r[4]=obj+0x2d4;instructions+=cpu.run(0xc228dc0,stop,1000);
    cpu.r[14]=frame;instructions+=cpu.run(0xc026436,0xc0264bc,10000);
    cpu.r[4]=obj;cpu.r[15]=stack;cpu.pr=stop;try{instructions+=cpu.run(0xc0267c0,stop,20000000);}catch(...){
        std::cerr<<"material constructor pc="<<hex(cpu.pc)<<" r11="<<hex(cpu.r[11])<<" r12="<<hex(cpu.r[12])<<" r14="<<hex(cpu.r[14])<<'\n';
        for(unsigned i=0;i<24;++i)std::cerr<<i*4<<':'<<hex(m.read32(cpu.r[14]+i*4))<<' ';std::cerr<<'\n';throw;}
    if(enemy>=0){cpu.r[4]=obj;cpu.r[5]=unsigned(enemy);cpu.r[15]=stack;cpu.pr=stop;instructions+=cpu.run(0xc035f00,stop,10000);}
    else{cpu.r[15]=stack;cpu.pr=stop;instructions+=cpu.run(0xc134a60,stop,100000);m.write32(0xc31c99c+16,car);m.write32(0xc31c99c+64,color);m.write32(scene+1048,obj);
        cpu.r[10]=scene+1020;cpu.r[13]=scene;cpu.r[9]=scene;cpu.r[15]=stack;instructions+=cpu.run(0xc0630b4,0xc06316e,100000);}
    //029AD0 begins suspension/tuning-height lookup. All GMP alpha, RGB,
    //specular and gloss writes are complete at this exact boundary.
    cpu.r[4]=obj;cpu.r[15]=stack;cpu.pr=stop;try{instructions+=cpu.run(0xc029040,0xc029ad0,20000000);}catch(...){std::cerr<<"Appearance material PC="<<hex(cpu.pc)<<" return="<<hex(cpu.pr)<<'\n';throw;}
    std::ofstream output(argv[6],std::ios::binary);for(unsigned i=0;i<polygon.size();++i)output.put(char(m.read8(model+i)));
    std::ofstream metadata(argv[7]);metadata<<"{\"car\":"<<car<<",\"enemy\":"<<enemy<<",\"config\":"<<m.read32(obj+0x2d4)<<",\"rgb\":["<<m.read32(obj+0x878)<<','<<m.read32(obj+0x87c)<<','<<m.read32(obj+0x880)<<"],\"instructions\":"<<instructions<<",\"copied_shadow_models\":"<<clones<<",\"paint_masks\":[";
    for(unsigned i=0;i<8;++i){if(i)metadata<<',';metadata<<m.read32(obj+0x294+i*4);}metadata<<"]}\n";
    std::cout<<"Captured car"<<car<<" enemy"<<enemy<<" materials, "<<instructions<<" original instructions\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
