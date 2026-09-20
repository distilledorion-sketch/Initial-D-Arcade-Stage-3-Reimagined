#include "original_course_lighting.h"
#include "original_course_fog.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <random>
using namespace idas3::original;
using namespace idas3::reference;
int main(int argc,char**argv)try{
    if(argc!=3)throw std::runtime_error("canonical image and FSCA table required");
    RefMemory m(argv[1]);std::vector<unsigned> fsca(32768);std::ifstream fs(argv[2],std::ios::binary);fs.seekg(16);fs.read(reinterpret_cast<char*>(fsca.data()),131072);if(!fs)throw std::runtime_error("FSCA table");
    constexpr unsigned obj=0x0d000000,ex=obj+0x2000,node=ex+0x100,query=obj+0x3000,matrixAddress=obj+0x50000,stop=0x0f000000;
    std::size_t checks=0,instructions=0;std::mt19937 rng(0x041320);
    auto eq=[&](unsigned a,unsigned b,const char*name){++checks;if(a!=b)throw std::runtime_error(std::string(name)+" original="+hex(a)+" native="+hex(b));};
    auto bits=[](float f){return std::bit_cast<unsigned>(f);};
    // Independent startup-prefix proof. Only unrelated static constructors,
    // TLS lookup, and atexit registration are hooked; the actual array walk,
    // once guard, Happo wrapper and all initializer stores execute unchanged.
    eq(m.read32(0x0c02009c),0x0c1ce6e0,"canonical entry target");
    eq(m.read32(0x0c2eeec0),0x0c041ce0,"Happo static constructor table entry");
    for(unsigned poison:{0u,0x7fc12345u,0xbf800000u}){
        m.clear();m.zeroRegion(obj,0x100000);m.zeroRegion(0x0cb534c0,4);
        for(unsigned j=0;j<4;++j)m.write32(0x0c2eff30+4*j,poison);
        RefCpu startup(m);startup.r[15]=obj+0x80000;startup.pr=stop;
        std::vector<unsigned> actual,expected;unsigned registrations=0;
        startup.callHooks[0x0c221fc0]=[&](auto&q){q.r[0]=ex;};
        startup.callHooks[0x0c226760]=[&](auto&q){eq(q.r[4],0x0c221de0,"destructor registration");++registrations;};
        for(unsigned p=0x0c2eef14;p>0x0c2eeeb4;){p-=4;const auto entry=m.read32(p);
            if(entry==0x0c041ce0)continue;
            expected.push_back(entry);startup.callHooks[entry]=[&,entry](auto&){actual.push_back(entry);};
        }
        instructions+=startup.run(0x0c1ce6e0,0x0c1ce706,10000);
        eq(unsigned(actual.size()),23,"other constructor count");
        for(unsigned j=0;j<expected.size();++j)eq(actual.at(j),expected[j],"reverse constructor order");
        eq(registrations,1,"atexit once");eq(m.read32(0x0cb534c0),1,"startup once flag");
        for(unsigned j=0;j<3;++j)eq(m.read32(0x0c2eff30+4*j),0x3e4ccccd,"initialized ambient RGB");
        eq(m.read32(0x0c2eff3c),0x0c380f44,"initialized vector vtable");
        // A later guarded call must not replay constructors or overwrite a
        // changed runtime vector. This is not a per-course ambient override.
        m.write32(0x0c2eff30,poison);actual.clear();startup.r[15]=obj+0x80000;startup.pr=stop;
        instructions+=startup.run(0x0c221e80,stop,1000);
        eq(unsigned(actual.size()),0,"no second constructor pass");
        eq(m.read32(0x0c2eff30),poison,"once guard preserves runtime state");eq(registrations,1,"no second atexit");
    }
    // Audit every data store made by the entire Happo initializer. Its only
    // non-stack writes are the16-byte RGB/vector object, including its vtable.
    m.clear();m.zeroRegion(obj,0x100000);
    {RefCpu init(m);init.r[15]=obj+0x80000;init.pr=stop;instructions+=init.run(0x0c041ce0,stop,1000);
        unsigned dataBytes=0;for(const auto&[address,value]:m.writes){(void)value;
            if(address>=obj&&address<obj+0x100000)continue;
            if(address<0x0c2eff30||address>=0x0c2eff40)throw std::runtime_error("Unexpected Happo initializer data store");
            ++dataBytes;
        }eq(dataBytes,16,"complete Happo initializer data extent");
    }
    for(unsigned condition=0;condition<4;++condition){
        const bool night=condition/2,wet=condition%2;
        m.clear();m.zeroRegion(obj,0x100000);m.zeroRegion(0x0c92f000,0x10000);m.zeroRegion(0x0c98ad0c,12);m.zeroRegion(0x0ce00000,0x10000);
        m.write16(0x0c98ad0e,32);m.write32(0x0c98ad10,0x0ce00000);m.write32(0x0c98ad14,0x0ce00000);
        m.write32(ex+4,node);m.write32(node,node);m.write32(node+4,node);
        RefCpu c(m);c.r[15]=obj+0x80000;c.fscaHalfWave=fsca;
        c.callHooks[0x0c221fc0]=[&](auto&q){q.r[0]=ex;};unsigned allocation=obj+0x10000;
        c.callHooks[0x0c021960]=[&](auto&q){q.r[0]=allocation;allocation+=0x1000;};c.callHooks[0x0c1dbe80]=[](auto&){};
        std::vector<OriginalFogRegisterWrite> writes;c.callHooks[0x0c21b460]=[&](auto&q){writes.push_back({q.r[4],q.r[5]});};
        auto run=[&](unsigned entry){c.pr=stop;instructions+=c.run(entry,stop,100000);};
        run(0x0c041ce0);
        c.r[4]=obj;run(0x0c03a0a0);m.write32(obj,0x0c38213c);m.write32(obj+52,night);m.write32(obj+56,wet);
        c.r[4]=obj;run(0x0c040d60);
        auto native=originalCourseLighting(4,night,wet);const auto set=m.read32(obj+60);
        eq(m.read32(set+68),native.count,"actual Happo registration count");
        for(unsigned j=0;j<3;++j)eq(m.read32(set+72+4*j),bits(native.ambient[j]),"actual Happo ambient");
        auto compareLights=[&](){for(unsigned i=0;i<native.count;++i){const auto p=m.read32(set+4+4*i);const auto&l=native.lights[i];
            eq(m.read8(p+24),l.enabled,"enable");for(unsigned j=0;j<3;++j){eq(m.read32(p+32+4*j),bits(l.color[j]),"color");eq(m.read32(p+(l.kind==OriginalCourseLightKind::Spot?68:44)+4*j),bits(l.incomingDirection[j]),"direction");}
            if(l.kind==OriginalCourseLightKind::Spot){for(unsigned j=0;j<3;++j)eq(m.read32(p+44+4*j),bits(l.position[j]),"spot position");eq(m.read32(p+60),bits(l.distance0),"near");eq(m.read32(p+64),bits(l.distance1),"far");eq(m.read32(p+84),l.angle0,"angle0");eq(m.read32(p+88),l.angle1,"angle1");}
        }};compareLights();
        auto fog=originalCourseFog(2,false,false);for(unsigned j=0;j<128;++j){fog.table[j]=std::uint16_t((j*773u)^0xaabb);fog.samples[j]=float(j)*.0123f;}
        const auto table=fog.table;const auto savedSamples=fog.samples;const float maximum=fog.maximum;
        applyHappoFogRegisters(fog,night,wet);unsigned density=0,color=0;for(auto w:writes){if(w.offset==0xb8)density=w.value;else if(w.offset==0xb0)color=w.value;else throw std::runtime_error("Unexpected actual Happo fog-table write");}
        eq(density,fog.packedDensity,"actual fog density");eq(color,fog.colorRgb,"actual fog color");eq(bits(maximum),bits(fog.maximum),"preserved fog maximum");
        for(unsigned j=0;j<128;++j){eq(table[j],fog.table[j],"preserved fog table");eq(bits(savedSamples[j]),bits(fog.samples[j]),"preserved fog samples");}
        m.write32(obj+24,query);m.write32(obj+64,123);m.write32(query,query+128);m.write32(query+128+44,0x0f000040);m.write32(obj+80,matrixAddress);
        OriginalLightVector reference{};unsigned queries=0;
        c.callHooks[0x0f000040]=[&](auto&q){eq(q.r[4],query,"path owner");eq(q.r[5],123,"prior path index");for(unsigned j=0;j<3;++j)m.writeFloat(q.r[6]+4*j,reference[j]);++queries;};
        c.callHooks[0x0c1f6610]=[](auto&){};c.callHooks[0x0c1f65c0]=[](auto&){};
        for(unsigned sample=0;sample<128;++sample){
            auto matrix=originalLightIdentityMatrix;for(unsigned j=0;j<16;++j)if(j%4!=3)matrix[j]=float(int(rng()%20001)-10000)/10000.f;
            for(unsigned j=0;j<16;++j)m.writeFloat(matrixAddress+4*j,matrix[j]);
            if(night&&sample<10){const auto p=m.read32(obj+1028+4*sample);for(unsigned j=0;j<3;++j)reference[j]=m.readFloat(p+44+4*j);}
            else reference={float(int(rng()%40001)-20000)/10.f,float(rng()%16000)/10.f,float(int(rng()%40001)-20000)/10.f};
            c.r[4]=obj;run(0x0c041320);updateOriginalHappoLighting(native,matrix,reference);compareLights();
            if(sample%16==0){
                run(0x0c1cefe0);c.callHooks[0x0c1cf0a0]=[](auto&){};c.r[4]=set;run(0x0c0538a0);
                const auto packet=originalCourseLightingPacket(native,matrix);
                m.write32(0x0c9801ec,1);m.write32(0x0c980264,0x09000000);c.r[4]=0x0c92f704;c.pr=stop;
                instructions+=c.run(0x0c1d3d00,0x0c1d3e10,10000);for(unsigned j=0;j<8;++j)eq(m.read32(c.r[7]+4*j),packet.glm[j],"Happo GLM");
                c.callHooks.erase(0x0c1cf0a0);c.callHooks[0x0c1d0fc0]=[](auto&){};c.callHooks[0x0c1d1000]=[](auto&){};c.callHooks[0x0c1d3d00]=[](auto&){};
                c.callHooks[0x0c1d3a60]=[&](auto&q){const unsigned index=m.read8(q.r[4]);if(index>=packet.count)return;
                    RefCpu d(m);d.r[15]=obj+0x70000;d.r[4]=q.r[4];d.r[5]=obj+0x60000;d.pr=stop;d.fscaHalfWave=fsca;
                    d.callHooks[0x0c2223e0]=[](auto&q){if(q.r[4]!=0xff000000||q.r[5]!=256)throw std::runtime_error("Unexpected divider");q.fpul=q.r[4]/q.r[5];};
                    m.write32(0x0c99a16c,32);instructions+=d.run(0x0c1d3860,stop,10000);
                    for(unsigned j=0;j<8;++j){auto w=m.read32(obj+0x60000+4*(7-j));if(j==1)w|=index;eq(w,packet.lights[index][j],"Happo light packet");}
                };
                for(unsigned j=0;j<16;++j)c.xf[j]=bits(matrix[j]);run(0x0c1cf0a0);c.callHooks.erase(0x0c1d3d00);c.callHooks.erase(0x0c1d3a60);
            }
        }
        eq(queries,night?128u:0u,"night path query gate");
    }
    std::cout<<"PASS actual startup1CE6E0/221E20/041CE0, Happo040D60 constructor/fog plus512 original041320 updates; "<<checks<<" exact comparisons, "<<instructions<<" original instructions.\n";
    return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
