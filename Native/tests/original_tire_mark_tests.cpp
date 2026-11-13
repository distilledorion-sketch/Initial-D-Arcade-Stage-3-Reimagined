#include "original_tire_marks.h"
#include "original_car_assembly.h"
#include "original_roadside_dust_owner.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned base=0xc900000,stack=base+0x1f000,stop=0xcfffffc,matrixStack=base+0x10000;
unsigned cases=0;std::uint64_t checks=0,instructions=0;
void equal(unsigned a,unsigned b,const char* what){++checks;if(a!=b)throw std::runtime_error(
    std::string(what)+" case="+std::to_string(cases)+" source="+hex(a)+" host="+hex(b));}
void equal(float a,float b,const char* what){equal(std::bit_cast<unsigned>(a),std::bit_cast<unsigned>(b),what);}
void checkEdges(RefMemory& memory){
    for(float x:{-10000.f,-1.f,-.001f,0.f,.001f,1.f,10000.f})
    for(float z:{-10000.f,-1.f,-.001f,0.f,.001f,1.f,10000.f})
    for(float y:{-250.f,0.f,.1f,250.f})for(float width:{.1f,.2f}){
        memory.clear();memory.zeroRegion(base,0x20000);
        const OriginalTirePoint previous{17.25f,-5.75f,-37.125f},current{previous[0]+x,previous[1]+y,previous[2]+z};
        OriginalTireEdges out{{{11,12,13},{14,15,16}}};
        memory.writeFloat(base+52,width);
        RefCpu cpu(memory);cpu.r[4]=base;cpu.r[15]=stack;cpu.pr=stop;
        for(unsigned i=0;i<3;++i){cpu.r[5+i]=std::bit_cast<unsigned>(previous[i]);memory.writeFloat(stack+4*i,current[i]);}
        memory.write32(stack+12,base+0x100);
        for(unsigned edge=0;edge<2;++edge)for(unsigned i=0;i<3;++i)memory.writeFloat(base+0x100+12*edge+4*i,out[edge][i]);
        instructions+=cpu.run(0xc1502a0,stop,2000);originalTireEdges(previous,current,width,out);
        for(unsigned edge=0;edge<2;++edge)for(unsigned i=0;i<3;++i)
            equal(memory.readFloat(base+0x100+12*edge+4*i),out[edge][i],"Ribbon edge");
        ++cases;
    }
}
void checkCaller(RefMemory& memory,const OriginalFscaTable& trig,const std::vector<unsigned>& wave,
        const std::filesystem::path& nativeRoot){
    constexpr unsigned race=base+0x20000,partsAt=race+0x1000,tls=race+0x3000,manager=race+0x4000,
        car0=race+0x5000,actor0=race+0x7000;
    const auto parts=OriginalCarParts::load(nativeRoot/"data/original_models/toyota_ae86t/assembly_parts.bin");
    for(int count:{-1,0,1,2,3}){
        memory.clear();memory.zeroRegion(base,0x40000);
        memory.write32(0xc98ad0c,0x00200000);memory.write32(0xc98ad10,matrixStack);memory.write32(0xc98ad14,matrixStack);
        memory.write32(tls+4,tls+64);memory.write32(tls+68,tls+128);
        memory.write32(race+0x118,unsigned(count));memory.write32(race+0x9c,manager);
        unsigned p=partsAt;for(const auto& t:parts.transforms)for(const auto* values:{&t.scale,&t.rotationDegrees,&t.translation})
            for(float v:*values){memory.writeFloat(p,v);p+=4;}
        OriginalReplayTireAdmission host;unsigned sourceAcquired=0,hostAcquired=0;
        for(unsigned tick=0;tick<128;++tick){
            std::array<OriginalTireCar,2> cars;
            for(unsigned i=0;i<2;++i){
                const auto at=car0+i*0x1000,actor=actor0+i*0x1000;
                auto& car=cars[i];car.origins=originalCarEffectOrigins(parts);
                car.world=originalActorMatrix({float(tick)*.37f,float(i)-3.5f,27.f},
                    {float(tick%4)*-.17f,float(tick)*.21f,float(tick%5)*.13f},trig);
                memory.write32(race+0x2a0+i*4,at);memory.write32(at+0x2f0,partsAt);
                memory.write32(at+0x95c,actor);memory.write32(at+0x960,tick%2?actor:0);
                memory.write32(at+0x964+64,0xc380b8c);
                for(unsigned j=0;j<16;++j)memory.writeFloat(at+0x964+j*4,car.world.elements[j]);
                for(unsigned wheel=0;wheel<4;++wheel){car.intensity[wheel]=std::uint8_t((tick+wheel+i)%7==0?0:tick%2?255:37);
                    memory.write8(actor+88+wheel,car.intensity[wheel]);}
            }
            struct Submitted{unsigned handle;OriginalTireContact contact;unsigned intensity;};std::vector<Submitted> submitted;
            RefCpu cpu(memory);cpu.fscaHalfWave=wave;cpu.r[4]=race;cpu.r[15]=stack;cpu.pr=stop;
            const auto identity=originalIdentityMatrix();for(unsigned i=0;i<16;++i)cpu.xf[i]=std::bit_cast<unsigned>(identity.elements[i]);
            cpu.callHooks[0xc221fc0]=[&](RefCpu& c){c.r[0]=tls;};
            cpu.callHooks[0xc1514e0]=[&](RefCpu& c){equal(c.r[4],manager,"Manager");c.r[0]=++sourceAcquired;};
            cpu.callHooks[0xc150b80]=[&](RefCpu& c){Submitted s;s.handle=c.r[4];s.intensity=memory.read32(c.r[15]+12);
                for(unsigned i=0;i<3;++i){s.contact.position[i]=std::bit_cast<float>(c.r[5+i]);s.contact.normal[i]=memory.readFloat(c.r[15]+i*4);}submitted.push_back(s);};
            try{instructions+=cpu.run(0xc0e4000,stop,20000);}
            catch(const std::exception& e){throw std::runtime_error(std::string(e.what())+" caller PC="+hex(cpu.pc));}
            unsigned index=0;host.advance(count,cars,[&]{return ++hostAcquired;},[&](unsigned handle,const OriginalTireContact& c,unsigned intensity){
                if(index>=submitted.size())throw std::runtime_error("Missing source mark");const auto& s=submitted[index++];
                equal(s.handle,handle,"Stroke identity");equal(s.intensity,intensity,"Intensity");
                for(unsigned i=0;i<3;++i){equal(s.contact.position[i],c.position[i],"Contact position");equal(s.contact.normal[i],c.normal[i],"Contact normal");}
            });
            equal(index,unsigned(submitted.size()),"Rear-wheel admission count");equal(sourceAcquired,hostAcquired,"Stroke acquisition count");
            for(unsigned car=0;car<2;++car)for(unsigned rear=0;rear<2;++rear){
                equal(memory.read32(race+0xcc+car*16+rear*4),unsigned(host.active()[car][rear]),"Active stroke");
                equal(memory.read32(race+0xa8+car*16+rear*4),host.handles()[car][rear],"Stored stroke");
            }
            ++cases;
        }
    }
}
}
#include "original_tire_stroke_checks.inl"
int main(int argc,char**argv)try{
    if(argc!=3)throw std::runtime_error("Canonical image and Native root required");
    RefMemory memory(argv[1]);const auto table=std::filesystem::path(argv[2])/"data/original_physics/fsca_table.bin";
    const auto trig=OriginalFscaTable::load(table);std::ifstream file(table,std::ios::binary);std::vector<unsigned>wave(32768);
    file.seekg(16);file.read(reinterpret_cast<char*>(wave.data()),131072);if(!file)throw std::runtime_error("FSCA read failed");
    checkEdges(memory);checkCaller(memory,trig,wave,argv[2]);checkStroke(memory);
    std::cout<<"PASS "<<cases<<" tire-mark cases / "<<checks<<" bitwise checks / "<<instructions<<" original instructions. Rear-wheel admission, ribbon math, stroke/strip limits, vertex pools, cadence, material words, colors, bounds, camera culling and order. Lifecycle boundaries: TLS, heap, renderer cache binding. The separate caller observes stroke acquisition/submission.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
