#include "original_gasstand_attract.h"
#include "original_gasstand_data.h"
#include "sh4_scalar_reference.h"
#include <bit>
#include <chrono>
#include <fstream>
#include <iostream>
using namespace idas3;
using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned owner=0xcd00000,gas=0xcd01000,etc=0xcd02000,font=0xcd03000,presenter=0xcd04000,gaspos=0xcd05000,etcpos=0xcd06000,
    stack=0xcfff000,stop=0x00ff0000;
std::uint64_t checks=0,steps=0,frames=0,draws=0;
void eq(unsigned a,unsigned b,const std::string& what){++checks;if(a!=b)throw std::runtime_error(what+": "+hex(a)+" != "+hex(b));}
void ef(float a,float b,const std::string& what){eq(std::bit_cast<unsigned>(a),std::bit_cast<unsigned>(b),what);}
void run(RefCpu& c,unsigned pc,unsigned end=stop){try{steps+=c.run(pc,end,500000);}catch(const std::exception& e){throw std::runtime_error(hex(pc)+" at "+hex(c.pc)+": "+e.what());}}
void seed(RefMemory& m,const OriginalGasstandState& s){
    m.clear();m.zeroRegion(owner,0x10000);m.zeroRegion(stack-0x10000,0x11000);
    m.write32(owner+16,0xc387434);m.write32(owner+364,gas);m.write32(owner+368,etc);m.write32(owner+360,font);m.write32(owner+376,presenter);
    m.write32(owner+372,m.read32(0xc2fbf58+s.script*8));
    m.write32(owner+88,s.elapsed);m.write32(owner+92,s.phase);m.write32(owner+100,s.fadeEnabled);m.write32(owner+104,s.fade);
    m.write32(owner+108,s.script);m.write32(owner+112,s.message);m.write32(owner+116,s.speaker);
    m.write32(owner+380,s.brands);m.writeFloat(owner+384,s.brandX);m.write32(owner+388,s.bubble);m.writeFloat(owner+392,s.bubbleX);m.writeFloat(owner+396,s.bubbleY);m.write32(owner+404,s.ranking);
    for(unsigned bank:{gas,etc}){m.write32(bank,0xc38a8ec);m.write32(bank+56,bank==gas?gaspos:etcpos);m.writeFloat(bank+72,1);m.writeFloat(bank+76,1);
        for(unsigned i=0;i<64;++i){const auto p=(bank==gas?gaspos:etcpos)+12*i;m.write32(p,0xc04ccccc);m.write32(p+4,0x40199999);m.write32(p+8,0xbe19999a);}}
    m.writeFloat(font+24,1);m.writeFloat(font+28,1);m.writeFloat(font+32,1);m.writeFloat(font+36,1);m.writeFloat(font+40,5);
    m.write32(presenter,gas);m.write32(presenter+4,57);m.write32(presenter+8,3);m.write32(presenter+12,2);m.write32(presenter+16,s.backgroundPhase);m.write32(presenter+20,0x3c020821);
}
void animation(RefMemory& m){
    for(unsigned variant=0;variant<3;++variant){OriginalGasstandState s;resetOriginalGasstand(s,variant);seed(m,s);
        unsigned n=0;while(!s.completed&&n<15000){
            RefCpu c(m);c.r[4]=owner;c.r[15]=stack;c.pr=stop;
            c.callHooks[0xc1d0880]=[](auto&){};
            c.callHooks[0xc0d0f60]=[&](auto&){if(m.read32(owner+108)==0&&m.read32(owner+380))m.writeFloat(owner+384,m.readFloat(owner+384)-gasstand_data::lit_0C0D14F0);};
            run(c,0xc0d0d20);stepOriginalGasstand(s);
            for(const auto pair:std::array<std::pair<unsigned,unsigned>,11>{{{88,s.elapsed},{92,s.phase},{100,s.fadeEnabled},{104,unsigned(s.fade)},{108,s.script},{112,s.message},{116,s.speaker},{380,s.brands},{388,s.bubble},{404,s.ranking},{32,s.completed}}})eq(m.read32(owner+pair.first),pair.second,"state"+std::to_string(pair.first)+" frame"+std::to_string(n));
            ef(m.readFloat(owner+384),s.brandX,"brand position");ef(m.readFloat(owner+392),s.bubbleX,"bubble x");ef(m.readFloat(owner+396),s.bubbleY,"bubble y");++n;++frames;
        }
        if(!s.completed)throw std::runtime_error("Gasstand did not complete");std::cout<<"script"<<variant<<" completes after"<<n<<" source frames\n";
    }
}
void commandCase(RefMemory& m,OriginalGasstandState s){
    seed(m,s);std::vector<OriginalGasstandDraw> actual;using Bank=OriginalGasstandDraw::Bank;
    RefCpu c(m);c.r[4]=owner;c.r[15]=stack;c.pr=stop;
    auto append=[&](auto& x,bool dynamic){OriginalGasstandDraw d;d.bank=x.r[4]==gas?Bank::gasstand:Bank::etc;d.index=x.r[5];
        const auto p=m.read32(x.r[4]+56)+12*d.index;d.position={m.readFloat(p)-gasstand_data::word(0xc04ccccc),m.readFloat(p+4)-gasstand_data::word(0x40199999),m.readFloat(p+8)-gasstand_data::word(0xbe19999a)};
        if(dynamic){d.position={m.readFloat(x.r[6]),m.readFloat(x.r[6]+4),m.readFloat(x.r[6]+8)};d.scaleX=m.readFloat(x.r[4]+72);d.scaleY=m.readFloat(x.r[4]+76);}actual.push_back(d);};
    c.callHooks[0xc145920]=[&](auto& x){append(x,false);};c.callHooks[0xc145ec0]=[&](auto& x){append(x,true);};
    c.callHooks[0xc0c5200]=[&](auto& x){eq(x.r[5],originalGasstandFadeArgb(s),"source fade");};
    c.callHooks[0xc1e7e80]=[&](auto& x){OriginalGasstandDraw d;d.bank=Bank::alphabet;d.index=m.read32(x.r[4]);d.position={m.readFloat(x.r[4]+4),m.readFloat(x.r[4]+8),m.readFloat(x.r[4]+12)};d.scaleX=m.readFloat(x.r[4]+16);d.scaleY=m.readFloat(x.r[4]+20);d.color=m.read32(x.r[4]+56)|0xff000000;actual.push_back(d);};
    run(c,0xc0d0f60);s.displayedBackgroundPhase=s.backgroundPhase;
    if(s.script==0&&s.brands)s.brandX-=gasstand_data::lit_0C0D14F0;
    const auto expected=originalGasstandDraws(s);eq(unsigned(actual.size()),unsigned(expected.size()),"draw count");
    for(unsigned i=0;i<actual.size();++i){const auto& a=actual[i];const auto& b=expected[i];const auto id="draw"+std::to_string(i)+" chunk"+std::to_string(a.index)+" ";
        eq(unsigned(a.bank),unsigned(b.bank),id+"bank");eq(a.index,b.index,id+"index");
        ef(a.position.x,b.position.x,id+"x");ef(a.position.y,b.position.y,id+"y");
        ef(a.position.z,b.position.z,id+"z");ef(a.scaleX,b.scaleX,id+"sx");ef(a.scaleY,b.scaleY,id+"sy");eq(a.color,b.color,id+"color");++draws;
    }
}
void commands(RefMemory& m){
    for(unsigned variant=0;variant<3;++variant){OriginalGasstandState s;resetOriginalGasstand(s,variant);
        commandCase(m,s);unsigned last=999;
        while(!s.completed){stepOriginalGasstand(s);if(s.phase==2&&s.message!=last){last=s.message;commandCase(m,s);}}
    }
}
void raster(const std::filesystem::path& root){
    OriginalGasstandAttract scene;scene.load(root);std::vector<std::uint32_t> reference(640*480),optimized(640*480);unsigned count=0;
    for(unsigned script=0;script<3;++script){scene.reset(script);
        for(unsigned frame=0;frame<1500;++frame){scene.step();
            if((script==0&&frame<322)||(frame>=1000&&frame<1100&&frame%5==0)||frame==1499){
                std::fill(reference.begin(),reference.end(),0xff123456);optimized=reference;
                scene.paintReference(reference,640,480);scene.paint(optimized,640,480);
                if(reference!=optimized){const auto p=std::mismatch(reference.begin(),reference.end(),optimized.begin()).first-reference.begin();throw std::runtime_error("Raster mismatch script"+std::to_string(script)+" frame"+std::to_string(frame)+" pixel"+std::to_string(p));}++count;
            }
        }
    }
    std::cout<<"gasstand optimized pixels match general compositor for "<<count<<" dynamic frames\n";
}
void bmp(const std::filesystem::path& path,const std::vector<std::uint32_t>& pixels){std::ofstream f(path,std::ios::binary);const std::array<unsigned char,14> fh{'B','M',54,0xc0,0x12,0,0,0,0,0,54,0,0,0};f.write(reinterpret_cast<const char*>(fh.data()),14);const std::array<unsigned,10> info{40,640,unsigned(-480),0x00200001,0,640*480*4,0,0,0,0};f.write(reinterpret_cast<const char*>(info.data()),40);f.write(reinterpret_cast<const char*>(pixels.data()),pixels.size()*4);}
}
int main(int argc,char** argv){try{if(argc<3)throw std::runtime_error("canonical image and game root required");RefMemory m(argv[1]);animation(m);commands(m);raster(argv[2]);
    if(argc>3){std::filesystem::create_directories(argv[3]);OriginalGasstandAttract a;a.load(argv[2]);std::vector<std::uint32_t> p(640*480);
        for(unsigned variant=0;variant<3;++variant){a.reset(variant);for(unsigned n=0;n<2000;++n){a.step();if(n==40||n==1000||n==1999){std::fill(p.begin(),p.end(),0xff000000);a.paint(p,640,480);bmp(std::filesystem::path(argv[3])/("script"+std::to_string(variant)+"-"+std::to_string(n)+".bmp"),p);}}}
        a.reset();for(unsigned i=0;i<1100;++i)a.step();std::vector<double> times;
        for(unsigned i=0;i<90;++i){a.step();std::fill(p.begin(),p.end(),0xff000000);const auto begin=std::chrono::steady_clock::now();a.paint(p,640,480);times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());}
        double sum=0;for(auto t:times)sum+=t;std::sort(times.begin(),times.end());std::ofstream report(std::filesystem::path(argv[3])/"cpu-performance.json");report<<"{\"source_canvas\":\"640x480\",\"frames\":90,\"mean_ms\":"<<sum/times.size()<<",\"p95_ms\":"<<times[85]<<",\"max_ms\":"<<times.back()<<"}\n";
        std::cout<<"gasstand CPU mean "<<sum/times.size()<<" ms, p95 "<<times[85]<<" ms\n";}
    std::cout<<"Gasstand: "<<frames<<" frames, "<<draws<<" source draws, "<<checks<<" comparisons, "<<steps<<" instructions\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
