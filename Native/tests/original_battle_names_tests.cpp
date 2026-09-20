#include "original_battle_names.h"
#include "original_battle_profile.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3;
using namespace idas3::reference;
namespace {
void bmp(const std::filesystem::path& file,const std::vector<unsigned>& pixels,unsigned width,unsigned height){
    std::ofstream out(file,std::ios::binary);const auto u16=[&](std::uint16_t v){out.write(reinterpret_cast<const char*>(&v),2);};const auto u32=[&](unsigned v){out.write(reinterpret_cast<const char*>(&v),4);};
    out.write("BM",2);u32(54+width*height*4);u32(0);u32(54);u32(40);u32(width);u32(0u-height);u16(1);u16(32);u32(0);u32(width*height*4);u32(0);u32(0);u32(0);u32(0);out.write(reinterpret_cast<const char*>(pixels.data()),pixels.size()*4);
}
}
int main(int argc,char** argv)try{
    if(argc!=4)throw std::invalid_argument("image nativeRoot outputDirectory");
    const auto names=OriginalBattleNames::load(argv[2]);RefMemory memory(argv[1]);std::size_t checks=0,instructions=0;
    const auto equal=[&](unsigned a,unsigned b){++checks;if(a!=b)throw std::runtime_error("Original name mismatch "+hex(a)+" vs "+hex(b));};
    constexpr unsigned object=0x0D000000,font=0x0D010000,records=0x0D020000,text=0x0D030000,stack=0x0D100000,stop=0x00FF0000;
    std::ifstream f(std::filesystem::path(argv[2])/"data/original_assets/hud/names/font_indices.bin",std::ios::binary);std::array<std::uint16_t,9216> table{};f.read(reinterpret_cast<char*>(table.data()),sizeof(table));if(!f)throw std::runtime_error("Missing name map");
    for(unsigned i=0;i<32;++i){
        memory.clear();const bool player=i==31;const unsigned enemy=player?0:i;const auto source=names.sourceName(enemy,player);const auto expected=names.glyphs(enemy,player);
        const auto sourcePointer=memory.read32(player?0x0C33B88C:0x0C33B684+enemy*4);
        for(unsigned j=0;j<source.size();++j)equal(source[j],memory.read8(sourcePointer+j));
        memory.zeroRegion(object,0x40000);memory.zeroRegion(stack,0x10000);
        for(unsigned j=0;j<table.size();++j)memory.write16(font+j*2,table[j]);
        for(unsigned j=0;j<source.size();++j)memory.write8(text+j,source[j]);
        memory.writeFloat(object+16,player?10.5f:16.f);memory.writeFloat(object+24,1.f);memory.writeFloat(object+32,510.f);memory.writeFloat(object+36,player?182.f:146.f);
        memory.write32(object+56,font);memory.write32(object+68,records);
        RefCpu cpu(memory);cpu.r[4]=object;cpu.r[5]=text;cpu.r[15]=stack+0xF000;cpu.pr=stop;
        instructions+=cpu.run(0x0C0C68E0,stop,50000);equal(memory.read32(object+52),unsigned(expected.size()));
        for(unsigned j=0;j<expected.size();++j){equal(memory.read32(records+j*16),expected[j].texture);equal(memory.read32(records+j*16+4),std::bit_cast<unsigned>(expected[j].x));equal(memory.read32(records+j*16+8),std::bit_cast<unsigned>(expected[j].y));}
    }
    // Full0CF620 with its original191640 and226AA0 callees. The source
    // language initialization's pointer table is supplied from268D54;
    // conversion, invalid-ID fallback, copies and termination run unhooked.
    for(unsigned fixture=0;fixture<226;++fixture){
        memory.clear();memory.zeroRegion(object,0x40000);memory.zeroRegion(stack,0x10000);
        for(unsigned id=0;id<221;++id)memory.write32(0x0C33B2E0+id*4,memory.read32(0x0C268D54+id*4));
        auto profile=original::makeOriginalFreshBattleProfile();
        const unsigned length=fixture==225?0:fixture%5+1;profile.setu(76,length);
        memory.write32(object+12,records);memory.write32(object+32,length);
        for(unsigned j=0;j<length;++j){
            const unsigned id=fixture<221?(fixture+j)%221:fixture==221?221:fixture==222?255:fixture==223?0xffffffffu:220;
            profile.setu(44+j*4,id);memory.write32(records+j*4,id);
        }
        const auto expected=names.sourcePlayerName(profile);
        RefCpu cpu(memory);cpu.r[4]=object;cpu.r[15]=stack+0xF000;cpu.pr=stop;
        instructions+=cpu.run(0x0C0CF620,stop,50000);equal(cpu.r[0],object+16);
        for(unsigned j=0;j<expected.size();++j)equal(memory.read8(object+16+j),expected[j]);
        equal(memory.read8(object+16+unsigned(expected.size())),0);

        const auto expectedGlyphs=names.glyphs(0,true,&profile);
        for(unsigned j=0;j<table.size();++j)memory.write16(font+j*2,table[j]);
        for(unsigned j=0;j<expected.size();++j)memory.write8(text+j,expected[j]);
        memory.writeFloat(object+16,10.5f);memory.writeFloat(object+24,1.f);memory.writeFloat(object+32,510.f);memory.writeFloat(object+36,182.f);
        memory.write32(object+52,0);memory.write32(object+56,font);memory.write32(object+68,records);
        RefCpu draw(memory);draw.r[4]=object;draw.r[5]=text;draw.r[15]=stack+0xF000;draw.pr=stop;
        instructions+=draw.run(0x0C0C68E0,stop,50000);equal(memory.read32(object+52),unsigned(expectedGlyphs.size()));
        for(unsigned j=0;j<expectedGlyphs.size();++j){equal(memory.read32(records+j*16),expectedGlyphs[j].texture);equal(memory.read32(records+j*16+4),std::bit_cast<unsigned>(expectedGlyphs[j].x));equal(memory.read32(records+j*16+8),std::bit_cast<unsigned>(expectedGlyphs[j].y));}
    }
    const auto output=std::filesystem::path(argv[3]);std::filesystem::create_directories(output);const auto hud=OriginalBattleHudAssets::load(argv[2]);
    for(unsigned enemy=0;enemy<31;++enemy){
        std::vector<unsigned> pixels(1280*720,0xff26384a);OriginalBattleHudState state;state.flags104=2050;state.frame204=100;state.signedAdvantage100=128.7f;OriginalBattleHudAnimation animation;
        const auto commands=drawOriginalBattleHud(state,animation);hud.paintGame2d(pixels,1280,720,commands,true);
        names.paint(pixels,1280,720,enemy,0,enemy%35,original::originalRival(enemy).car);
        if(enemy==0||enemy==13||enemy==26)bmp(output/("names-"+std::to_string(enemy)+".bmp"),pixels,1280,720);
    }
    auto entered=original::makeOriginalFreshBattleProfile();entered.setu(76,5);
    for(unsigned i=0;i<5;++i)entered.setu(44+i*4,std::array<unsigned,5>{164,169,179,170,180}[i]);
    std::vector<unsigned> customPixels(1280*720,0xff26384a);names.paint(customPixels,1280,720,0,0,0,1,&entered);bmp(output/"custom-player-name.bmp",customPixels,1280,720);
    std::cout<<"PASS32 original name selectors and226 custom-profile0CF620/68E0 cases, "<<checks<<" exact comparisons, "<<instructions<<" original instructions, zero hooks; all31 name/car combinations rasterized. Font projection uses the native640x480 canvas.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}


