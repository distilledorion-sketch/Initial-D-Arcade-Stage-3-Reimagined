#include "original_battle_hud.h"
#include "sh4_scalar_reference.h"
#include <cmath>
#include <iostream>

using namespace idas3;
using namespace idas3::reference;
namespace {
constexpr unsigned hud=0x0D000000,state=0x0D001000,bank=0x0D002000,portrait=0x0D003000;
constexpr unsigned playerName=0x0D004000,opponentName=0x0D005000,stack=0x0D010000,stop=0x00FF0000;
void bmp(const std::filesystem::path& file,const std::vector<unsigned>& pixels,unsigned width,unsigned height){
    std::ofstream out(file,std::ios::binary);const auto u16=[&](std::uint16_t v){out.write(reinterpret_cast<const char*>(&v),2);};const auto u32=[&](unsigned v){out.write(reinterpret_cast<const char*>(&v),4);};
    out.write("BM",2);u32(54+width*height*4);u32(0);u32(54);u32(40);u32(width);u32(0u-height);u16(1);u16(32);u32(0);u32(width*height*4);u32(0);u32(0);u32(0);u32(0);out.write(reinterpret_cast<const char*>(pixels.data()),pixels.size()*4);
}
}
int main(int argc,char** argv)try{
    if(argc!=4)throw std::runtime_error("canonical-image native-root output-directory required");
    RefMemory memory(argv[1]);std::size_t cases=0,instructions=0,checks=0,draws=0,divisions=0;
    const auto equal=[&](unsigned actual,unsigned expected,const std::string& label){++checks;if(actual!=expected)throw std::runtime_error(label+" expected="+hex(expected)+" actual="+hex(actual));};
    const std::vector<float> advantages{-20000.f,-9999.901f,-9999.9f,-999.99f,-100.f,-10.f,-.10000001f,-.1f,-.09999999f,-0.f,0.f,.09999999f,.1f,.10000001f,1.f,9.9f,10.f,99.9f,100.f,999.9f,1000.f,9999.9f,20000.f};
    for(unsigned mode=0;mode<4;++mode)for(unsigned variant=0;variant<12;++variant)for(float advantage:advantages){
        memory.clear();memory.zeroRegion(hud,0x7000);memory.zeroRegion(stack,0x10000);memory.zeroRegion(0x0C98AD0C,12);memory.zeroRegion(0x0CE00000,32*64);
        memory.write16(0x0C98AD0E,32);memory.write32(0x0C98AD10,0x0CE00000);memory.write32(0x0C98AD14,0x0CE00000);
        OriginalBattleHudState input;input.profileMode0C31C99C=mode;input.signedAdvantage100=advantage;input.validity96=variant==10?-1.f:0.f;
        constexpr unsigned flags[]{0,1,2,2048,2049,2050,2051,2051,2051,0x80000803,2051,2051};input.flags104=flags[variant];input.alternateLayout96=variant==8;
        input.slide208=variant&1?-.375f:0;input.slide212=variant&1?.625f:0;input.frame204=variant==4?40:variant==5?0:variant==11?-1:41;
        OriginalBattleHudAnimation animation;constexpr unsigned counters[]{0,1,49,50,59,60,61,0xffffffff};animation.negativeBlink0CA9B548=counters[(mode+variant)%8];
        memory.write32(hud+104,bank);memory.write32(hud+96,input.alternateLayout96);memory.write32(hud+204,unsigned(input.frame204));memory.writeFloat(hud+208,input.slide208);memory.writeFloat(hud+212,input.slide212);
        memory.write32(hud+228,portrait);memory.write32(hud+232,playerName);memory.write32(hud+236,opponentName);
        memory.write32(state+104,input.flags104);memory.writeFloat(state+96,input.validity96);memory.writeFloat(state+100,input.signedAdvantage100);memory.write32(0x0C31C99C,mode);memory.write32(0x0CA9B548,animation.negativeBlink0CA9B548);
        auto base=original::originalIdentityMatrix();if(variant&1){base.elements[0]=1.125f;base.elements[5]=.875f;base.elements[12]=.0625f;base.elements[13]=-.125f;}
        const auto expected=drawOriginalBattleHud(input,animation,base);std::size_t cursor=0;
        RefCpu cpu(memory);cpu.r[4]=hud;cpu.r[5]=state;cpu.r[15]=stack+0xF000;cpu.pr=stop;
        for(unsigned i=0;i<16;++i)cpu.xf[i]=std::bit_cast<unsigned>(base.elements[i]);
        const auto command=[&](OriginalBattleHudDraw::Kind kind,unsigned index=0){
            if(cursor>=expected.size())throw std::runtime_error("Extra original battle HUD command");const auto& e=expected[cursor++];
            const auto label="mode="+std::to_string(mode)+" variant="+std::to_string(variant)+" advantage="+std::to_string(advantage)+" command="+std::to_string(cursor);
            equal(unsigned(e.kind),unsigned(kind),label+" kind");equal(e.draw.index,index,label+" index");
            for(unsigned i=0;i<16;++i)equal(std::bit_cast<unsigned>(e.draw.matrix.elements[i]),cpu.xf[i],label+" matrix["+std::to_string(i)+"]");
            if(kind==OriginalBattleHudDraw::Kind::game2d||kind==OriginalBattleHudDraw::Kind::portrait228)++draws;
        };
        cpu.callHooks[0x0C0C96A0]=[&](auto& c){equal(c.r[4],hud,"base HUD receiver");equal(c.r[5],state,"base HUD state");command(OriginalBattleHudDraw::Kind::baseHud);};
        cpu.callHooks[0x0C145A00]=[&](auto& c){equal(c.r[4],bank,"projection receiver");equal(c.r[5],6,"projection ID");command(OriginalBattleHudDraw::Kind::beginProjection6);};
        cpu.callHooks[0x0C145AC0]=[&](auto& c){equal(c.r[4],bank,"projection reset receiver");command(OriginalBattleHudDraw::Kind::endProjection);};
        cpu.callHooks[0x0C145AE0]=[&](auto& c){if(c.r[4]!=bank&&c.r[4]!=portrait)throw std::runtime_error("Unknown HUD bank");command(c.r[4]==bank?OriginalBattleHudDraw::Kind::game2d:OriginalBattleHudDraw::Kind::portrait228,c.r[5]);};
        cpu.callHooks[0x0C0CF800]=[&](auto& c){equal(c.r[4],playerName,"player name receiver");command(OriginalBattleHudDraw::Kind::playerName232);};
        cpu.callHooks[0x0C0CFB60]=[&](auto& c){equal(c.r[4],opponentName,"opponent name receiver");command(OriginalBattleHudDraw::Kind::opponentName236);};
        //2223B8 uses PR1. Its bounded positive signed integer divisions are
        // explicit arithmetic boundaries, matching the existing HUD oracle.
        cpu.callHooks[0x0C2223B8]=[&](auto& c){if(!c.r[5])throw std::runtime_error("HUD zero divisor");c.fpul=unsigned(signed32(c.r[4])/signed32(c.r[5]));++divisions;};
        instructions+=cpu.run(0x0C0CAE20,stop,200000);
        equal(unsigned(cursor),unsigned(expected.size()),"Command completion");equal(animation.negativeBlink0CA9B548,memory.read32(0x0CA9B548),"negative blink");equal(memory.read16(0x0C98AD0C),0,"matrix stack balance");
        for(unsigned i=0;i<16;++i)equal(cpu.xf[i],std::bit_cast<unsigned>(base.elements[i]),"enclosing matrix preserved");++cases;
    }
    const auto assets=OriginalBattleHudAssets::load(argv[2]);const std::filesystem::path output=argv[3];std::filesystem::create_directories(output);
    for(unsigned sample=0;sample<3;++sample){
        OriginalBattleHudState s;s.flags104=2051;s.frame204=41;s.signedAdvantage100=sample==0?128.7f:sample==1?-25.3f:0.f;OriginalBattleHudAnimation a;
        const auto commands=drawOriginalBattleHud(s,a);std::vector<unsigned> pixels(640*480,0xff26384au);assets.paintGame2d(pixels,640,480,commands);
        if(std::size_t(std::count(pixels.begin(),pixels.end(),0xff26384au))==pixels.size())throw std::runtime_error("Battle HUD asset composition empty");bmp(output/("battle_hud_"+std::to_string(sample)+".bmp"),pixels,640,480);
    }
    std::cout<<"PASS "<<cases<<" complete0CAE20 cases, "<<checks<<" bit comparisons, "<<instructions<<" original instructions, "<<draws<<" draw submissions; "<<divisions<<" explicit bounded PR1 division hooks. Base HUD, projection, names/portrait and GPU submission are explicit boundaries; all battle selectors, centering, counter and matrix instructions execute directly.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
