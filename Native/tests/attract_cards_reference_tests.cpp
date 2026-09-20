#include "original_attract_cards.h"
#include "sh4_scalar_reference.h"
#include <array>
#include <bit>
#include <fstream>
#include <iostream>
using namespace idas3;
using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned owner=0x0cd00000,bank=0x0cd01000,fadeOwner=0x0cd02000,parent=0x0cd03000,
    packet=0x0cd04000,vt=0x0cd05000,stack=0x0cfff000,stop=0x00ff0000,draw=0x00ee0000,finish=0x00ee0020;
std::uint64_t comparisons{},steps{},frames{};
void eq(unsigned a,unsigned b,const char* label){++comparisons;if(a!=b)throw std::runtime_error(std::string(label)+": "+hex(a)+" != "+hex(b));}
void seed(RefMemory& m,unsigned child){m.clear();m.zeroRegion(owner,0x10000);m.zeroRegion(stack-0x4000,0x5000);
    m.write32(owner+12,vt);m.write16(vt+40,0);m.write32(vt+44,finish);
    m.write32(bank,vt+128);m.write16(vt+128+40,0);m.write32(vt+128+44,draw);m.write16(vt+128+56,0);m.write32(vt+128+60,draw);
    m.write32(owner+76,parent);m.write32(packet,packet+128);m.write32(packet+4,0);
    if(child==3){m.write32(owner+328,bank);m.write32(owner+332,fadeOwner);}
    if(child==4){m.write32(owner+88,bank+32);m.write32(owner+92,bank);m.write32(owner+96,fadeOwner);}
    if(child==5){m.write32(owner+332,bank);m.write32(owner+336,fadeOwner);}
}
void run(RefCpu& c,unsigned start,unsigned end=stop){try{steps+=c.run(start,end,10000);}catch(const std::exception& e){throw std::runtime_error(hex(start)+" at "+hex(c.pc)+": "+e.what());}}
void sequence(RefMemory& m,unsigned child,bool delayed){
    seed(m,child);OriginalAttractCardState state;resetOriginalAttractCard(state,child);
    unsigned calls=0;std::uint32_t background=child==5?0:0xffffff;
    while(!state.completed&&calls<1500){
        OriginalAttractCardReadiness ready;ready.resourcesReady=!delayed||calls>50;ready.networkWaiting=delayed&&calls<70;ready.cardEnabled=delayed;ready.cardReady=calls>400;
        RefCpu c(m);c.r[4]=owner;c.r[15]=stack;c.pr=stop;unsigned fade=0,mesh=0,draws=0;bool panel=false,finished=false;
        c.callHooks[0x0c1d0880]=[](auto&){};c.callHooks[0x0c055d60]=[](auto&){};
        c.callHooks[0x0c1f7e40]=[&](auto& x){background=x.r[4]&0xffffff;};
        c.callHooks[0x0c0c5200]=[&](auto& x){fade=x.r[5];};
        c.callHooks[draw]=[&](auto& x){eq(x.r[5],0,"source static card chunk");panel=true;++draws;};
        c.callHooks[finish]=[&](auto&){finished=true;m.write8(owner+28,1);};
        c.callHooks[0x0c05a300]=[](auto& x){x.r[0]=packet;};
        c.callHooks[0x0c161b00]=[](auto& x){x.r[0]=packet;};
        c.callHooks[0x0c02e620]=[&](auto& x){x.r[0]=ready.resourcesReady;};
        c.callHooks[0x0c073f60]=[&](auto& x){x.r[0]=ready.networkWaiting;};
        c.callHooks[0x0c0b0b20]=[](auto&){};
        c.callHooks[0x0c0b0ae0]=[&](auto& x){x.r[0]=ready.cardEnabled;};
        c.callHooks[0x0c0b3140]=[&](auto& x){x.r[0]=ready.cardReady;};
        for(unsigned fn:{0x0c1f6610u,0x0c1f65c0u,0x0c1d7120u,0x0c141cc0u,0x0c141d80u})c.callHooks[fn]=[](auto&){};
        c.callHooks[0x0c1f6ac0]=[&](auto& x){eq(x.fr[4],0,"Rosso translationX");eq(x.fr[5],0,"Rosso translationY");eq(x.fr[6],0xc0c00000,"Rosso source depth");};
        c.callHooks[0x0c05a8e0]=[&](auto& x){mesh=x.r[5];x.r[0]=bank+256;};
        c.callHooks[0x0c202140]=[](auto& x){x.r[0]=packet;};
        // Source signed division service used only for the background RGB.
        c.callHooks[0x0c2223b8]=[](auto& x){x.fpul=unsigned(std::int32_t(x.r[4])/std::int32_t(x.r[5]));};
        run(c,child==3?0x0c0746e0:child==4?0x0c073960:0x0c039bc0);
        stepOriginalAttractCard(state,ready);
        eq(m.read32(owner+84),state.frame,"source frame");eq(finished,state.completed,"source finish");
        eq(background,state.background,"source background");
        if(child!=4){eq(draws,1,"static source card draw");eq(m.read32(owner+(child==3?336:88)),state.phase,"source phase");eq(fade,originalAttractCardFadeArgb(state),"source fade");}
        else {eq(mesh,state.meshFrame,"Rosso authored mesh frame");eq(panel,state.logoPanel,"Rosso2D panel visibility");eq(fade,originalAttractCardFadeArgb(state),"Rosso source flash");}
        ++calls;++frames;
    }
    if(!state.completed)throw std::runtime_error("Source card did not complete");
    std::cout<<"child"<<child<<(delayed?" delayed":" ready")<<" completes in"<<calls<<" source updates\n";
}
void projection(RefMemory& m){
    m.clear();m.zeroRegion(stack-0x1000,0x2000);RefCpu c(m);c.r[4]=0x1000;c.r[15]=stack;c.pr=stop;run(c,0x0c1fa280);
    const float cotangent=1.f/std::bit_cast<float>(c.fr[0]);
    const float scale=((cotangent/std::bit_cast<float>(m.read32(0x0c1d0b34)))*320.f)/6.f;
    eq(std::bit_cast<unsigned>(scale),std::bit_cast<unsigned>(originalAttractCardProjectionScale()),"source tangent projection scale");
}
void bmp(const std::filesystem::path& path,const std::vector<std::uint32_t>& pixels){std::ofstream out(path,std::ios::binary);
    const std::array<unsigned char,14> header{'B','M',54,0xc0,0x12,0,0,0,0,0,54,0,0,0};out.write(reinterpret_cast<const char*>(header.data()),14);
    const std::array<unsigned,10> info{40,640,unsigned(-480),0x00200001,0,640*480*4,0,0,0,0};out.write(reinterpret_cast<const char*>(info.data()),40);out.write(reinterpret_cast<const char*>(pixels.data()),pixels.size()*4);
}
void previews(const std::filesystem::path& root,const std::filesystem::path& output){std::filesystem::create_directories(output);OriginalAttractCards cards;cards.load(root);
    for(unsigned child:{3u,4u,5u}){cards.reset(child);unsigned count=0;while(!cards.completed()){
        cards.step();if(count==15||count==40||count==100||count==160||count==200||count==360){std::vector<std::uint32_t> pixels(640*480);cards.paint(pixels,640,480);
            const auto fade=cards.fadeArgb();const unsigned alpha=fade>>24;for(auto& p:pixels){unsigned color=0xff000000;for(unsigned shift:{0u,8u,16u})color|=((((p>>shift)&255)*(255-alpha)+((fade>>shift)&255)*alpha)/255)<<shift;p=color;}
            bmp(output/("child"+std::to_string(child)+"-"+std::to_string(count)+".bmp"),pixels);}
        ++count;
    }}
}
}
int main(int argc,char** argv){try{if(argc!=4)throw std::runtime_error("image root output required");RefMemory m(argv[1]);projection(m);for(unsigned child:{3u,4u,5u})sequence(m,child,false);sequence(m,3,true);previews(argv[2],argv[3]);
    std::cout<<"Original cards: "<<frames<<" full source frames, "<<comparisons<<" comparisons, "<<steps<<" original instructions; original geometry previews saved\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
