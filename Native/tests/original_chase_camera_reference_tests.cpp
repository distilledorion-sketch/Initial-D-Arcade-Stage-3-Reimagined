#include "original_chase_camera.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3;
using namespace idas3::reference;
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("project-root canonical-image required");
    const std::filesystem::path root=argv[1];
    for(auto drivingView:{OriginalDrivingView::Bumper,OriginalDrivingView::Chase}){
    const bool bumper=drivingView==OriginalDrivingView::Bumper;
    RefMemory memory(argv[2]);auto camera=OriginalChaseCamera::load(root,drivingView);
    constexpr unsigned obj=0xd000000,car=0xd010000,actor=0xd020000,stack=0xd030000,frame=0xd040000,context=0xd050000,view=0xd060000,output=0xd070000,matrixStack=0xce00000,stop=0xf000000;
    for(auto address:{obj,car,actor,stack,frame,context,view,output,matrixStack})memory.zeroRegion(address,0x10000);
    memory.write32(context+4,context+256);memory.write32(context+260,context+512);memory.write32(context+512,context+512);
    memory.write32(obj,0xc386cdc);memory.write32(obj+244,view);memory.writeFloat(obj+172,1.f);
    memory.write32(car+0x95c,actor);memory.write32(car+0x964+64,0xc380b8c);
    memory.write32(0xc98ad0c,0x00200000);memory.write32(0xc98ad10,matrixStack);memory.write32(0xc98ad14,matrixStack);
    std::ifstream fsca(root/"data/original_physics/fsca_table.bin",std::ios::binary);fsca.seekg(16);std::vector<unsigned> wave(32768);fsca.read(reinterpret_cast<char*>(wave.data()),wave.size()*4);
    RefCpu cpu(memory);cpu.fscaHalfWave=wave;cpu.callHooks[0xc221fc0]=[](RefCpu& c){c.r[0]=context;};
    std::size_t instructions=0,comparisons=0;auto equal=[&](unsigned a,unsigned b,const char* label){++comparisons;if(a!=b){std::cerr<<label<<" native="<<std::hex<<a<<" source="<<b<<std::dec<<" comparison="<<comparisons<<'\n';throw std::runtime_error("Original chase differential failed");}};
    // Execute the original chase-profile literal setup and09D640 initializer.
    memory.write32(frame+192,car);memory.write32(frame+216,obj);memory.write32(frame+204,obj);cpu.r[14]=frame;cpu.r[15]=stack+0xf000;
    instructions+=cpu.run(bumper?0xc0a99a4:0xc0a9ae4,bumper?0xc0a9a1a:0xc0a9b5c,2000);
    equal(memory.read32(obj+128),bumper?0x3f800000:0x3fd9999a,"profile height");equal(memory.read32(obj+132),bumper?0xbf800000:0x40933333,"profile distance");
    equal(memory.read32(obj+60),bumper?0x3f800000:0x41f00000,"profile recurrence");equal(memory.read32(obj+64),std::bit_cast<unsigned>(bumper?OriginalChaseCamera::sourceBumperFieldOfView:OriginalChaseCamera::sourceVerticalFieldOfView),"profile FOV");
    for(unsigned tick=0;tick<360;++tick){
        Vec3 position{100.f+float(tick)*.5f,12.f+float(tick%17)*.015f,35.f-float(tick)*.4f};
        Vec3 angles{std::sin(float(tick)*.03f)*.14f,std::sin(float(tick)*.016f)*3.3f,std::sin(float(tick)*.043f)*.085f};
        if(tick>=160&&tick<190)position.x+=100.f; // both teleport directions
        if(tick>=200&&tick<225)angles.y+=6.283185307179586f; // equivalent wrapped headings
        for(unsigned i=0;i<3;++i){memory.writeFloat(actor+i*4,(&position.x)[i]);memory.writeFloat(actor+24+i*4,(&angles.x)[i]);}
        // The real live ACar path builds +964 and copies actor Euler fields
        // into+9B8, before its independent ride-height and draw callbacks.
        cpu.r[13]=car;cpu.r[14]=frame+0x1000;cpu.r[15]=stack+0xf000;cpu.r[0]=context;
        instructions+=cpu.run(0xc03485c,0xc034924,3000);
        for(unsigned i=0;i<3;++i)equal(memory.read32(car+0x9b8+i*4),memory.read32(actor+24+i*4),"raw actor angle binding");
        if(tick==0){ // The source starts with the current actor angles.
            cpu.r[4]=obj;cpu.r[15]=stack+0xf000;cpu.pr=stop;instructions+=cpu.run(0xc09dbc0,stop,1000);
        }
        cpu.r[4]=obj;cpu.r[2]=output;cpu.r[15]=stack+0xf000;cpu.pr=stop;
        instructions+=cpu.run(0xc09d100,stop,10000);
        const auto& native=camera.update(position,angles);const auto smoothed=camera.smoothedAngles();
        for(unsigned i=0;i<3;++i)equal(std::bit_cast<unsigned>((&smoothed.x)[i]),memory.read32(obj+44+i*4),"smoothed angles");
        for(unsigned i=0;i<16;++i)equal(std::bit_cast<unsigned>(native.cameraWorld.elements[i]),memory.read32(output+i*4),"camera matrix");
        const unsigned phase=memory.read32(view+152);const float expected=float(phase)*std::bit_cast<float>(0x40c90fdbu)/65536.f;
        equal(std::bit_cast<unsigned>(native.verticalFieldOfView),std::bit_cast<unsigned>(expected),"projection phase");
    }
    std::cout<<"PASS 360 sequential original "<<(bumper?"bumper":"chase")<<" frames, "<<comparisons<<" bit-exact comparisons, "<<instructions<<" original instructions. Only the exception-context provider is hooked; camera state, actor binding and all matrix/trigonometry execute source bytes.\n";
    }
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
