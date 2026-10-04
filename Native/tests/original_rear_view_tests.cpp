#include "original_rear_view.h"
#include "original_chase_camera.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3;
using namespace idas3::reference;
int main(int argc,char** argv)try{
    if(argc!=3)throw std::invalid_argument("project-root canonical-image required");
    RefMemory m(argv[2]);const auto trig=original::OriginalFscaTable::load(std::filesystem::path(argv[1])/"data/original_physics/fsca_table.bin");
    constexpr unsigned view=0xd000000,race=0xd010000,car=0xd020000,stack=0xd030000,stop=0xf000000;
    std::size_t instructions=0,comparisons=0;
    for(unsigned i=0;i<720;++i){
        m.clear();for(auto a:{view,race,car,stack})m.zeroRegion(a,0x10000);
        m.write32(view+316,race);m.write32(race+1048,car);m.write32(car+0x964+64,0xc380b8c);
        const auto pose=original::originalActorMatrix({float(i)*.73f,20.f+std::sin(float(i))*.7f,300.f-float(i)*.37f},
            {std::sin(float(i)*.1f)*.3f,float(i)*.038f,std::cos(float(i)*.06f)*.2f},trig);
        for(unsigned j=0;j<16;++j)m.writeFloat(car+0x964+j*4,pose.elements[j]);
        RefCpu cpu(m);cpu.r[4]=view;cpu.r[15]=stack+0xf000;cpu.pr=stop;
        instructions+=cpu.run(0xc05b520,stop,1000);
        const auto native=originalRearViewFrame(pose);
        for(unsigned j=0;j<16;++j){++comparisons;if(std::bit_cast<unsigned>(native.cameraWorld.elements[j])!=cpu.xf[j])throw std::runtime_error("Rear camera matrix differs from05B520");}
    }
    // Exercise the real live actor -> ACar -> rear-view chain. Supplying an
    // already-lifted body matrix would pass the isolated callback comparison
    // above while placing the mirror roughly 30 cm too high in the game.
    const auto camera=OriginalChaseCamera::load(argv[1]);
    constexpr unsigned actor=0xd040000,matrixStack=0xce00000;
    std::ifstream waveFile(std::filesystem::path(argv[1])/"data/original_physics/fsca_table.bin",std::ios::binary);
    waveFile.seekg(16);std::vector<unsigned> wave(32768);waveFile.read(reinterpret_cast<char*>(wave.data()),wave.size()*4);
    if(!waveFile)throw std::runtime_error("FSCA reference table unavailable");
    for(unsigned i=0;i<720;++i){
        m.clear();for(auto a:{view,race,car,stack,actor,matrixStack})m.zeroRegion(a,0x10000);
        m.write32(view+316,race);m.write32(race+1048,car);m.write32(car+0x95c,actor);m.write32(car+0x964+64,0xc380b8c);
        m.write32(0xc98ad0c,0x00200000);m.write32(0xc98ad10,matrixStack);m.write32(0xc98ad14,matrixStack);
        const Vec3 p{float(i)*.73f,20.f+std::sin(float(i))*.7f,300.f-float(i)*.37f};
        const Vec3 angles{std::sin(float(i)*.1f)*.3f,float(i)*.038f,std::cos(float(i)*.06f)*.2f};
        for(unsigned j=0;j<3;++j){m.writeFloat(actor+j*4,(&p.x)[j]);m.writeFloat(actor+24+j*4,(&angles.x)[j]);}
        RefCpu cpu(m);cpu.fscaHalfWave=wave;cpu.r[13]=car;cpu.r[14]=stack+0xf000;cpu.r[15]=stack+0xf000;
        instructions+=cpu.run(0xc03485c,0xc0348dc,2000);
        cpu.r[4]=view;cpu.r[15]=stack+0xf000;cpu.pr=stop;instructions+=cpu.run(0xc05b520,stop,1000);
        const auto native=camera.rearView(p,angles);
        for(unsigned j:{4u,5u,6u,12u,13u,14u}){++comparisons;
            if(std::bit_cast<unsigned>(native.cameraWorld.elements[j])!=cpu.xf[j])throw std::runtime_error("Live rear camera eye/up differs from actor callback chain");}
        // Remove only the source model half-turn, not its pitch, roll or eye.
        for(unsigned j:{8u,9u,10u}){++comparisons;
            if(std::abs(native.cameraWorld.elements[j]+std::bit_cast<float>(cpu.xf[j]))>.00001f)throw std::runtime_error("Rear facing basis differs from source half-turn");}
    }
    // Original view tile rectangle setup, followed by its mirror aspect/FOV.
    m.clear();m.zeroRegion(view,0x10000);m.zeroRegion(stack,0x10000);
    RefCpu cpu(m);cpu.r[4]=view;cpu.r[5]=5;cpu.r[6]=1;cpu.r[7]=14;cpu.r[15]=stack+0xf000;cpu.pr=stop;
    m.write32(cpu.r[15],2);m.write32(cpu.r[15]+4,31);m.write32(cpu.r[15]+8,2);
    // Clip/projection arithmetic consumes an initialized view model. The
    // viewport check stops at that dependency; no hardware commands execute.
    cpu.callHooks[0xc1d09e0]=[](RefCpu&){};
    instructions+=cpu.run(0xc0242c0,stop,1000);
    const auto model=view+52;
    const auto equal=[&](float a,float b){++comparisons;if(a!=b)throw std::runtime_error("Rear viewport mismatch");};
    equal(m.readFloat(model+136),OriginalRearViewFrame::left);equal(m.readFloat(model+128),OriginalRearViewFrame::top);
    equal(m.readFloat(model+140)-m.readFloat(model+136),OriginalRearViewFrame::width);
    equal(m.readFloat(model+132)-m.readFloat(model+128),OriginalRearViewFrame::height);
    if(m.read16(0xc0640ae)!=2367||m.read32(0xc0640c4)!=0xc0a00000||m.read32(0xc0640c8)!=0xbe4ccccd)throw std::runtime_error("Rear projection literal mismatch");
    std::cout<<"PASS 720 original rear-view matrices and 720 live actor bindings, "<<comparisons<<" comparisons, original320x64 viewport and mirrored projection literals; "<<instructions<<" original instructions. Only the viewport projection dependency is hooked.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
