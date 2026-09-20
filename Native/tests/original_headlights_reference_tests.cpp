#include "car_presentation.h"
#include "car_catalog.h"
#include "original_rival_appearance_catalog.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3;
using namespace idas3::reference;
template<class T>T read(std::istream& f){T x{};if(!f.read(reinterpret_cast<char*>(&x),sizeof x))throw std::runtime_error("Headlight fixture truncated");return x;}
int main(int argc,char**argv)try{
    if(argc!=4)throw std::runtime_error("project-root canonical-image HOSTFS required");
    const std::filesystem::path root=argv[1],host=argv[3];RefMemory memory(argv[2]);
    std::ifstream fsca(root/"data/original_physics/fsca_table.bin",std::ios::binary);fsca.seekg(16);
    std::vector<std::uint32_t> wave(32768);for(auto& x:wave)x=read<std::uint32_t>(fsca);
    constexpr unsigned obj=0xd000000,frame=0xd020000,parts=0xd010000,stack=0xd030000,matrices=0xce00000;
    memory.zeroRegion(obj,0x1000);memory.zeroRegion(frame,0x1000);memory.zeroRegion(parts,0x1000);memory.zeroRegion(stack,0x1000);memory.zeroRegion(matrices,0x10000);
    memory.write32(frame+92,obj+81);memory.write32(frame+36,obj+0x2bc);memory.write32(obj+0x2f0,parts);
    std::size_t frames=0,comparisons=0,instructions=0,matrixWords=0,appearances=0;
    auto equal=[&](unsigned a,unsigned b,const char* what){++comparisons;if(a!=b){std::cerr<<what<<" actual="<<std::hex<<a<<" expected="<<b<<std::dec<<'\n';throw std::runtime_error("Original headlight differential failed");}};
    for(unsigned appearance=0;appearance<66;++appearance){
        const bool rival=appearance>=35;const unsigned enemy=rival?appearance-35:0,car=rival?originalRivalAppearances[enemy].car:appearance;
        const std::string folder(originalCarFolders[car]);auto model=NativeModel::load(root/"data/original_models"/folder/(folder+".idasmesh"));
        auto presentation=rival?CarPresentation::loadRival(root,car,enemy,model.chunks.size()):CarPresentation::load(root,car,model.chunks.size());
        const auto base=rival?root/"data/original_models/rivals_v2"/("enemy_"+std::string(enemy<10?"0":"")+std::to_string(enemy)):root/"data/original_models"/folder/"appearance_v2/color_00";
        std::ifstream program(base/"headlights.bin",std::ios::binary);program.seekg(12);const unsigned instance=read<unsigned>(program);const unsigned maximum=read<unsigned>(program);
        std::array<unsigned,4> sourceInstances{};sourceInstances.fill(0xffffffffu);
        std::ifstream lighting(base/"lighting.bin",std::ios::binary);lighting.seekg(16);
        for(unsigned state=0;state<4;++state){const auto extras=read<unsigned>(lighting);lighting.seekg(extras*68,std::ios::cur);const auto count=read<unsigned>(lighting);for(unsigned i=0;i<count;++i)if(read<unsigned>(lighting)==instance)sourceInstances[state]=i;}
        equal(maximum,memory.read32(0xc298320+car*4),"phase table");
        std::ifstream partfile(host/"parts"/(folder+".bin"),std::ios::binary);for(unsigned i=0;i<828;++i)memory.write8(parts+i,read<std::uint8_t>(partfile));
        memory.write32(obj+0x34c,car);memory.write32(obj+0x6c4,maximum?1:0);memory.write32(obj+0x6cc,40);
        RefCpu cpu(memory);cpu.fscaHalfWave=wave;
        // Original graphics submission alone is intercepted. Matrix helpers
        // and the source 191440 maximum-angle lookup execute original bytes.
        std::array<unsigned,16> drawMatrix{};unsigned drawSlot=0,draws=0;
        cpu.callHooks[0xc026040]=[&](RefCpu& c){++draws;drawSlot=c.r[5];drawMatrix=c.xf;};
        cpu.callHooks[0xc2223b8]=[](RefCpu& c){if(!c.r[5])throw std::runtime_error("Original divide by zero");c.fpul=c.r[4]/c.r[5];};
        for(unsigned freshNight=0;freshNight<2;++freshNight){
            presentation.resetHeadlights();memory.write32(obj+0x6c8,0xffffffffu);
            for(unsigned tick=0;tick<90;++tick){
                const bool lights=tick<45?freshNight!=0:freshNight==0;
                memory.write8(obj+81,lights?1:0);cpu.r[13]=obj;cpu.r[14]=frame;cpu.r[15]=stack+0xf00;
                instructions+=cpu.run(0xc027080,0xc0271bc,1000);++frames;
                if(tick) presentation.advanceOriginalFrame(lights);
                const auto& assembly=presentation.pose({},lights,false);const auto& state=presentation.headlightState();
                equal(unsigned(state.counter),memory.read32(obj+0x6c8),"counter");equal(state.phase,memory.read32(obj+0x6d0),"phase");
                equal(state.visible?1:0,memory.read32(obj+0x6d4),"visible");equal(std::bit_cast<unsigned>(state.fraction),memory.read32(obj+0x6d8),"fraction");
                // Rendering multiple host frames must not advance the motor.
                presentation.pose({},lights,false);equal(unsigned(state.counter),memory.read32(obj+0x6c8),"render-independent counter");
                if(instance==0xffffffffu)continue;
                memory.write32(0xc98ad0c,0x00200000);memory.write32(0xc98ad10,matrices);memory.write32(0xc98ad14,matrices);
                for(unsigned i=0;i<16;++i)cpu.xf[i]=std::bit_cast<unsigned>(i%5==0?1.f:0.f);
                cpu.setFloat(12,memory.readFloat(0xc027388));cpu.setFloat(13,memory.readFloat(0xc02738c));cpu.setFloat(14,memory.readFloat(0xc027390));
                cpu.r[13]=obj;cpu.r[14]=frame;cpu.r[15]=stack+0xf00;draws=0;
                instructions+=cpu.run(0xc0272e8,0xc027436,2000);equal(draws,1,"headlight draw count");equal(drawSlot,state.visible?19:18,"headlight semantic");
                const auto& native=assembly.instances.at(sourceInstances[lights?2:0]).transform;
                for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col){equal(std::bit_cast<unsigned>(native[row*4+col]),drawMatrix[col*4+row],"headlight matrix");++matrixWords;}
            }
        }
        ++appearances;
    }
    std::cout<<"PASS "<<appearances<<" appearances, "<<frames<<" original headlight frames, "<<comparisons<<" exact comparisons ("<<matrixWords<<" matrix words), "<<instructions<<" source instructions; graphics/division hooks explicit.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
