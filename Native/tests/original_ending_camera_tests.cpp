#include "original_ending_camera.h"
#include "ending_replay.h"
#include "original_matrix.h"
#include "original_smoke_view.h"
#include "original_rain_glare.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3;
using namespace idas3::reference;
int main(int argc,char** argv)try{
    if(argc!=4)throw std::runtime_error("Native root, canonical image, original camera file required");
    const std::filesystem::path root=argv[1];
    RefMemory memory(argv[2]);
    std::ifstream source(argv[3],std::ios::binary);
    std::vector<unsigned> words(22);source.read(reinterpret_cast<char*>(words.data()),88);
    if(!source||words[0]!=1||words[1]!=80||words[2]!=13)throw std::runtime_error("Original ending camera record");
    constexpr unsigned obj=0xd000000,car=0xd010000,actor=0xd020000,stack=0xd030000,
        frame=0xd040000,context=0xd050000,view=0xd060000,output=0xd070000,
        matrixStack=0xce00000,stop=0xf000000;
    std::ifstream fsca(root/"data/original_physics/fsca_table.bin",std::ios::binary);
    fsca.seekg(16);std::vector<unsigned> wave(32768);fsca.read(reinterpret_cast<char*>(wave.data()),wave.size()*4);
    if(!fsca)throw std::runtime_error("FSCA data unavailable");
    const auto trig=original::OriginalFscaTable::load(root/"data/original_physics/fsca_table.bin");
    std::size_t checks=0,instructions=0;
    const auto equal=[&](unsigned a,unsigned b,const char* label){
        ++checks;if(a!=b){std::cerr<<label<<" native="<<std::hex<<a<<" source="<<b<<std::dec<<" check="<<checks<<'\n';
            throw std::runtime_error("Ending camera differs from original instructions");}
    };
    for(unsigned seed:{0u,1u,0x89123456u}){
        memory.clear();for(auto a:{obj,car,actor,stack,frame,context,view,output,matrixStack})memory.zeroRegion(a,0x10000);
        memory.write32(context+4,context+256);memory.write32(context+260,context+512);memory.write32(context+512,context+512);
        memory.write32(obj,0xc3862ec);memory.write32(obj+244,view);memory.writeFloat(obj+172,1.f);
        memory.write32(car+0x95c,actor);memory.write32(car+0x964+64,0xc380b8c);
        memory.write32(0xc98ad0c,0x00200000);memory.write32(0xc98ad10,matrixStack);memory.write32(0xc98ad14,matrixStack);
        memory.write32(0xc37c778,seed);
        RefCpu cpu(memory);cpu.fscaHalfWave=wave;
        cpu.callHooks[0xc221fc0]=[](RefCpu& c){c.r[0]=context;};
        OriginalEndingCamera camera;
        for(unsigned tick=0;tick<6000;++tick){
            Vec3 p{100.f+float(tick)*.5f,12.f+std::sin(float(tick)*.017f),35.f-float(tick)*.4f};
            Vec3 a{std::sin(float(tick)*.03f)*.14f,std::sin(float(tick)*.016f)*3.3f,std::sin(float(tick)*.043f)*.085f};
            if(tick>=2100&&tick<2300)p.x+=100.f;
            if(tick>=4000){p={50,12,-30};a={};} // camera must keep ticking after a held replay pose
            const float pos[]{p.x,p.y,p.z},angles[]{a.x,a.y,a.z};
            for(unsigned i=0;i<3;++i){memory.writeFloat(actor+i*4,pos[i]);memory.writeFloat(actor+24+i*4,angles[i]);}
            cpu.r[13]=car;cpu.r[14]=frame+0x1000;cpu.r[15]=stack+0xf000;cpu.r[0]=context;
            instructions+=cpu.run(0xc03485c,0xc034924,3000);
            original::OriginalMatrix matrix;
            for(unsigned i=0;i<16;++i)matrix.elements[i]=memory.readFloat(car+0x964+i*4);
            constexpr auto e=OriginalEndingCamera::eyeOffset,t=OriginalEndingCamera::targetOffset;
            const auto eye=original::transformOriginalPoint(matrix,{e.x,e.y,e.z});
            const auto target=original::transformOriginalPoint(matrix,{t.x,t.y,t.z});
            EndingReplay captured;EndingReplayFrame capture;
            capture.cameraActorAngles=a;
            capture.cars[0].actor=p;capture.cars[0].yaw=wrapAngle(a.y+pi);
            capture.cars[0].pitch=-a.x;capture.cars[0].roll=-a.z;
            captured.record(capture,trig,0,0);
            for(unsigned i=0;i<3;++i){
                equal(std::bit_cast<unsigned>((&captured.recentFrame(0).eye.x)[i]),std::bit_cast<unsigned>(eye[i]),"recorded eye anchor");
                equal(std::bit_cast<unsigned>((&captured.recentFrame(0).target.x)[i]),std::bit_cast<unsigned>(target[i]),"recorded target anchor");
            }
            if(tick==0){
                for(unsigned i=0;i<20;++i)memory.write32(stack+0xf000+i*4,words[i+2]);
                memory.write32(stack+0xf000+80,car);
                cpu.r[4]=obj;cpu.r[15]=stack+0xf000;cpu.pr=stop;
                instructions+=cpu.run(0xc0a3380,stop,10000);
                camera.reset({eye[0],eye[1],eye[2]},{target[0],target[1],target[2]},seed);
                for(unsigned i=0;i<3;++i){
                    equal(std::bit_cast<unsigned>(target[i]),memory.read32(obj+84+i*4),"initial target");
                    equal(std::bit_cast<unsigned>((&t.x)[i]),words[16+i],"authored target offset");
                    equal(std::bit_cast<unsigned>((&e.x)[i]),words[19+i],"authored eye offset");
                }
            }
            cpu.r[4]=obj;cpu.r[2]=output;cpu.r[15]=stack+0xf000;cpu.pr=stop;
            instructions+=cpu.run(0xc09d100,stop,12000);
            camera.advance({eye[0],eye[1],eye[2]},{target[0],target[1],target[2]});
            const auto actualEye=camera.eye(),actualTarget=camera.target(),velocity=camera.velocity();
            for(unsigned i=0;i<3;++i){
                equal(std::bit_cast<unsigned>((&actualEye.x)[i]),memory.read32(obj+20+i*4),"eye");
                equal(std::bit_cast<unsigned>((&actualTarget.x)[i]),memory.read32(obj+32+i*4),"target");
                equal(std::bit_cast<unsigned>((&velocity.x)[i]),memory.read32(obj+0xbc+i*4),"spring velocity");
            }
            equal(camera.seed(),memory.read32(0xc37c778),"RNG state");
            const auto sourcePhase=memory.read32(view+152);
            const float sourceFov=float(sourcePhase)*std::bit_cast<float>(0x40c90fdbu)/65536.f;
            equal(std::bit_cast<unsigned>(OriginalEndingCamera::fieldOfView()),std::bit_cast<unsigned>(sourceFov),"projection FOV");
            const auto cameraWorld=original::originalLookAtCameraWorld(
                {actualEye.x,actualEye.y,actualEye.z},{actualTarget.x,actualTarget.y,actualTarget.z});
            for(unsigned i=0;i<16;++i){
                equal(std::bit_cast<unsigned>(cameraWorld.elements[i]),memory.read32(view+i*4),"camera world matrix");
                cpu.xf[i]=memory.read32(view+i*4);
            }
            //1D0880 loads cameraWorld, calls this rigid inverse and writes the
            // result at view+64 before rain/smoke use the view owner.
            cpu.r[15]=stack+0xf000;cpu.pr=stop;
            instructions+=cpu.run(0xc1f66a0,stop,3000);
            const auto worldToView=original::originalCameraWorldToView(cameraWorld);
            const auto runtimeView=camera.worldToView();
            for(unsigned i=0;i<16;++i){
                equal(std::bit_cast<unsigned>(worldToView.elements[i]),cpu.xf[i],"camera view matrix");
                equal(std::bit_cast<unsigned>(runtimeView.elements[i]),cpu.xf[i],"ending runtime view matrix");
                memory.write32(view+64+i*4,cpu.xf[i]);
            }
            memory.writeFloat(view+128,0);memory.writeFloat(view+132,480);
            memory.writeFloat(view+136,0);memory.writeFloat(view+140,640);
            cpu.r[4]=view;cpu.r[15]=stack+0xf000;cpu.pr=stop;
            instructions+=cpu.run(0xc1d09e0,stop,3000);
            const auto effectsView=original::originalSmokeView(worldToView,std::uint16_t(sourcePhase));
            for(unsigned i=0;i<3;++i)
                equal(std::bit_cast<unsigned>(effectsView.planes[0][i]),memory.read32(view+192+i*4),"rain glare view plane");
            for(unsigned i=0;i<3;++i)memory.writeFloat(output+128+i*4,(&p.x)[i]);
            cpu.r[4]=view+192;cpu.r[5]=view+64;cpu.r[6]=output+128;
            cpu.setFloat(4,100.05f);cpu.r[15]=stack+0xf000;cpu.pr=stop;
            instructions+=cpu.run(0xc156ba0,stop,3000);
            equal(std::bit_cast<unsigned>(original::originalRainGlareDepth(worldToView,
                effectsView.planes[0],{p.x,p.y,p.z})),cpu.fr[0],"glare depth through source camera/view setup");
        }
    }
    std::cout<<"PASS "<<checks<<" comparisons across 18000 sequential ending camera frames; "<<instructions
        <<" original instructions. Only exception-context provider hooked; original initialization, actor matrix, camera spring, RNG, camera world/view matrices, projection setup and rain glare depth execute unchanged.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
