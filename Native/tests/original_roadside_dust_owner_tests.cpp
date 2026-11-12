#include "original_roadside_dust_owner.h"
#include "original_smoke_emitter.h"
#include "car_catalog.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3;
using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned car=0xc900000,partsAt=car+0x2000,actor=car+0x3000,race=car+0x4000;
constexpr unsigned frame=car+0x8000,tls=car+0x9000,stack=car+0xe000,matrixStack=car+0xa000;
constexpr unsigned emitterAt=car+0x6000,stop=0xcfffffc;
std::uint64_t cases=0,checks=0,instructions=0;
std::uint64_t selectionCases=0,stopCalls=0;
std::uint64_t startCases=0;
void equal(unsigned actual,unsigned expected,const char* label){++checks;if(actual!=expected){
    std::cerr<<"case "<<cases<<' '<<label<<" source="<<std::hex<<actual<<" native="<<expected<<std::dec<<'\n';
    throw std::runtime_error("Original dust owner mismatch");}}
void equalFloat(float actual,float expected,const char* label){equal(std::bit_cast<unsigned>(actual),std::bit_cast<unsigned>(expected),label);}
void selectionCase(RefMemory& m,unsigned mode,unsigned flag,unsigned occupied){
    m.clear();m.zeroRegion(car,0x10000);m.write32(frame+132,race);
    m.write32(race+0x668,mode);m.write8(race+0x494,std::uint8_t(flag));
    m.write32(race+0x418,car);m.write32(race+0x41c,car+0x1000);
    for(unsigned i=0;i<8;++i)m.write32(race+0x4f8+i*4,(occupied&(1u<<(i%4)))?partsAt+i*16:0);
    RefCpu c(m);c.r[14]=frame;c.r[15]=stack;c.r[2]=1;
    const auto native=originalRoadsideDustOwnerSelection(mode,flag!=0);
    unsigned stopped=0;
    // Only the stop request is a boundary; all actual owner selection,
    // ordered slot clearing and gain setup execute in the original parent.
    c.callHooks[0xc13e940]=[&](RefCpu& cpu){
        const auto slot=(cpu.r[4]-partsAt)/16;
        equal(slot/4,1-native.car,"stopped opposite owner");
        equal(unsigned(slot<8&&(occupied&(1u<<(slot%4)))!=0),1,"occupied source slot stopped");
        stopped|=1u<<slot;++stopCalls;
    };
    instructions+=c.run(0xc06088c,0xc060964,1000);
    equal(m.read32(frame+136),car+native.car*0x1000,"selected ACar");
    equal(m.read32(frame+140),native.slotBase,"selected wheel-slot base");
    equalFloat(c.getFloat(14),native.displayGain,"owner-specific display gain");
    equal(stopped,occupied<<((1-native.car)*4),"exact stopped slots");
    for(unsigned i=0;i<8;++i)equal(m.read32(race+0x4f8+i*4),
        i/4==native.car&&(occupied&(1u<<(i%4)))?partsAt+i*16:0,"selected slots retained/opposite cleared");
    ++selectionCases;
}
void runCase(RefMemory& m,const OriginalCarParts& parts,const OriginalMatrix& world,const std::array<float,3>& delta){
    m.clear();m.zeroRegion(car,0x10000);m.write32(car+0x2f0,partsAt);
    unsigned pointer=partsAt;
    for(const auto& part:parts.transforms)for(const auto* values:{&part.scale,&part.rotationDegrees,&part.translation})
        for(float value:*values){m.writeFloat(pointer,value);pointer+=4;}
    m.write32(car+0x964+64,0xc380b8c);
    for(unsigned i=0;i<16;++i)m.writeFloat(car+0x964+i*4,world.elements[i]);
    for(unsigned i=0;i<3;++i)m.writeFloat(actor+12+i*4,delta[i]);
    m.write32(frame+132,race);m.write32(frame+136,car);m.write32(frame+148,actor);
    m.write32(frame+172,tls);m.write32(frame+180,frame+64);
    m.write32(tls+4,tls+64);m.write32(tls+68,tls+128);
    m.write32(0xc98ad0c,0x00200000);m.write32(0xc98ad10,matrixStack);m.write32(0xc98ad14,matrixStack);
    RefCpu c(m);c.r[14]=frame;c.r[15]=stack;c.r[12]=frame+68;c.r[13]=frame+72;
    const auto identity=originalIdentityMatrix();for(unsigned i=0;i<16;++i)c.xf[i]=std::bit_cast<unsigned>(identity.elements[i]);
    // Execute virtual CAMatrix copy, authored dimensions, source FIPR/sqrt,
    // wheel getter, vector copy, matrix stack/translation/multiply/transform.
    // No geometry, math, matrix-stack or other hooks.
    instructions+=c.run(0xc0609cc,0xc060a26,4000);
    const auto origins=originalCarEffectOrigins(parts);const auto out=originalRoadsideDustOwner(world,origins,delta);
    equalFloat(c.getFloat(12),out.displacementMagnitude,"actor displacement");
    for(unsigned i=0;i<3;++i)equalFloat(m.readFloat(frame+64+i*4),out.dimensions[i],"footprint dimensions");
    for(unsigned i=0;i<16;++i)equalFloat(m.readFloat(frame+i*4),out.footprint.elements[i],"unlifted owner footprint");
    instructions+=c.run(0xc060a26,0xc060a4c,1000);
    for(unsigned effect=0;effect<4;++effect){
        m.write32(frame+152,effect);
        instructions+=c.run(0xc060a4c,0xc060aa0,4000);
        for(unsigned axis=0;axis<3;++axis){
            equalFloat(m.readFloat(frame+92+axis*4),origins[effect][axis],"authored wheel origin");
            equalFloat(m.readFloat(frame+76+axis*4),out.positions[effect][axis],"world attachment");
        }
        for(unsigned i=0;i<16;++i){
            equalFloat(m.readFloat(frame+i*4),out.footprint.elements[i],"attachment retains footprint");
            equal(c.xf[i],std::bit_cast<unsigned>(identity.elements[i]),"enclosing matrix restored");
        }
        equal(m.read32(0xc98ad0c),0x00200000,"matrix stack depth");
        equal(m.read32(0xc98ad14),matrixStack,"matrix stack pointer");
        // Exercise the real CarSmokeEvt start and attachment against the
        // just-produced world point. View allocation is deliberately absent
        // here; only its particle fields and owner matrix are compared.
        m.zeroRegion(emitterAt,304);m.write32(0xc8ff16c,0);
        OriginalSmokeEmitter e;e.flags=0x7fab12cdu;e.scale=2.25f;e.force=-.75f;e.lifetimeScale=.8f;
        m.write32(emitterAt+220,e.flags);m.writeFloat(emitterAt+28,e.scale);
        m.writeFloat(emitterAt+32,e.force);m.writeFloat(emitterAt+72,e.lifetimeScale);
        m.write32(emitterAt+176,1);m.write32(emitterAt+180,99);
        RefCpu started(m);started.r[4]=emitterAt;started.r[15]=stack;started.pr=stop;
        for(unsigned i=0;i<16;++i)started.xf[i]=std::bit_cast<unsigned>(identity.elements[i]);
        for(unsigned i=0;i<3;++i)started.r[5+i]=std::bit_cast<unsigned>(out.positions[effect][i]);
        started.setFloat(4,.2f);
        started.callHooks[0xc221fc0]=[](RefCpu& cpu){cpu.r[0]=tls;};
        instructions+=started.run(0xc140660,stop,4000);
        startOriginalCarSmokeEmitter(e,out.positions[effect]);
        const auto compareEmitter=[&](){
            equal(m.read32(emitterAt+220),e.flags,"started flags");
            equal(m.read32(emitterAt+176),e.stopped,"stopped reset");equal(m.read32(emitterAt+180),e.frame,"frame reset");
            equalFloat(m.readFloat(emitterAt+28),e.scale,"motion scale independent of display gain");
            equalFloat(m.readFloat(emitterAt+32),e.force,"motion force retained");
            equalFloat(m.readFloat(emitterAt+72),e.lifetimeScale,"attachment lifetime");
            for(unsigned axis=0;axis<3;++axis){
                equalFloat(m.readFloat(emitterAt+108+axis*4),e.spread[axis],"source spread basis");
                equalFloat(m.readFloat(emitterAt+124+axis*4),e.position[axis],"source emitter position");
                equalFloat(m.readFloat(emitterAt+140+axis*4),e.velocity[axis],"source emitter velocity");
                equalFloat(m.readFloat(emitterAt+164+axis*4),e.previousPosition[axis],"source previous position");
            }
        };
        compareEmitter();
        auto attached=out.positions[effect];for(unsigned axis=0;axis<3;++axis)attached[axis]+=delta[axis];
        RefCpu attachedSource(m);attachedSource.r[4]=emitterAt;attachedSource.r[15]=stack;attachedSource.pr=stop;
        attachedSource.setFloat(4,effect>1?.5f:.3f);
        for(unsigned axis=0;axis<3;++axis)attachedSource.r[5+axis]=std::bit_cast<unsigned>(attached[axis]);
        for(unsigned i=0;i<16;++i)m.writeFloat(stack+i*4,out.footprint.elements[i]);
        instructions+=attachedSource.run(0xc140980,stop,4000);
        attachOriginalSmokeEmitter(e,attached,effect>1?.5f:.3f);compareEmitter();
        for(unsigned i=0;i<16;++i)equalFloat(m.readFloat(emitterAt+240+i*4),out.footprint.elements[i],"attached original footprint copy");
        ++startCases;
        ++cases;
    }
}
}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::invalid_argument("canonical-image native-root required");
    RefMemory memory(argv[1]);const std::filesystem::path root=argv[2];
    for(unsigned mode:{0u,1u,2u,3u,4u,5u,0x7fffffffu,0xffffffffu})
        for(unsigned flag:{0u,1u,2u,128u,255u})for(unsigned occupied=0;occupied<16;++occupied)
            selectionCase(memory,mode,flag,occupied);
    unsigned random=0x060a4c;const auto next=[&](){random^=random<<13;random^=random>>17;random^=random<<5;return random;};
    for(unsigned id=0;id<35;++id){
        auto parts=OriginalCarParts::load(root/"data/original_models"/originalCarFolders[id]/"assembly_parts.bin");
        for(unsigned trial=0;trial<48;++trial){
            auto world=originalIdentityMatrix();
            if(trial){
                // Include banked, pitched and nonuniform bases, so applying
                // the .1 lift in car-local Y cannot pass accidentally.
                for(unsigned col=0;col<3;++col)for(unsigned row=0;row<3;++row)
                    world.elements[col*4+row]=float(int(next()%2049)-1024)*.00390625f;
                for(unsigned i=12;i<15;++i)world.elements[i]=float(int(next()%65537)-32768)*.0625f;
            }
            const std::array<float,3> delta=trial%8==0?std::array<float,3>{0,0,0}:
                trial%8==1?std::array<float,3>{.1f,0,0}:std::array<float,3>{float(int(next()%257)-128)*.015625f,float(int(next()%129)-64)*.0078125f,float(int(next()%257)-128)*.015625f};
            runCase(memory,parts,world,delta);
        }
    }
    std::cout<<"PASS "<<cases<<" original dust wheel attachments, "<<checks<<" bitwise checks, "<<instructions
        <<" original instructions; "<<selectionCases<<" parent owner selections, "<<stopCalls<<" observed opposite-owner stop requests, "<<startCases<<" real emitter start/attachment chains. All35 authored cars, source wheel ordering/heights, world-up lift, unchanged footprint, actor displacement, reset fields and retained motion state. Geometry/math execute with zero hooks; parent stop requests and start exception TLS are boundaries. Display allocation is omitted.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
