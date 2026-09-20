#include "original_course_fog.h"
#include "sh4_scalar_reference.h"
#include <bit>
#include <iostream>
#include <stdexcept>
using namespace idas3::original;
using namespace idas3::reference;
int main(int argc,char**argv)try{
    if(argc!=2)throw std::invalid_argument("canonical image required");
    RefMemory m(argv[1]);constexpr unsigned frame=0x0d008000,owner=0x0d000000,stop=0x0f000000;
    std::size_t checks=0,instructions=0;
    auto check=[&](bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);};
    for(unsigned course=0;course<9;++course)for(unsigned time=0;time<2;++time)
    for(unsigned wet=0;wet<2;++wet)for(unsigned reverse=0;reverse<2;++reverse){
        m.clear();m.zeroRegion(owner,0x20000);
        m.write32(frame+2496,course*2+time);m.write32(frame+2500,reverse);
        m.write32(frame+2504,time);m.write32(frame+2732,wet);
        RefCpu producer(m);producer.r[14]=frame;
        instructions+=producer.run(0x0c0427a4,0x0c0427c6,100);
        check(m.read32(0x0c92ea7c)==course,"original scene course global");
        check(m.read32(0x0c92ea78)==time&&m.read32(0x0c92ea74)==wet,"original time/weather globals");
        m.write32(frame+736,owner);
        RefCpu index(m);index.r[14]=frame;
        instructions+=index.run(0x0c1acc6c,0x0c1accae,100);
        const auto fog=originalCourseFog(course,time!=0,wet!=0);
        check(m.read32(owner)==fog.sourceRow,"source fog row index");
        check(m.read32(frame+744)==fog.sourceAddress,"source table row address");
        check(m.read32(fog.sourceAddress+352)==std::bit_cast<unsigned>(fog.density),"authored density bits");
        check(m.read32(fog.sourceAddress+356)==fog.colorRgb,"authored fog color");
        check(m.read32(fog.sourceAddress+360)==std::bit_cast<unsigned>(fog.maximum),"authored table ceiling bits");
        std::vector<OriginalFogRegisterWrite> reference;
        RefCpu c(m);c.r[14]=frame;c.r[15]=frame;c.r[7]=0x87654321;
        // Only the final PVR writer is substituted. The original density,
        // color, float table and128-entry quantization execute unhooked.
        c.callHooks[0x0c21b460]=[&](auto& q){reference.push_back({q.r[4],q.r[5]});};
        instructions+=c.run(0x0c1ad218,0x0c1ad28e,10000);
        const auto writes=originalCourseFogWrites(fog);
        check(reference.size()==writes.size(),"source fog register count");
        for(unsigned i=0;i<writes.size();++i)check(reference[i]==writes[i],"source fog register value/order");
        for(unsigned i=0;i<128;++i)check(m.read32(frame+152+4*i)==std::bit_cast<unsigned>(fog.samples[i]),"source fog float sample");
        check(m.read32(owner+204)==std::bit_cast<unsigned>(fog.density),"owner fog density retained");
        check(m.read32(owner+208)==std::bit_cast<unsigned>(fog.maximum),"owner fog ceiling retained");
    }
    // Exercise the original packed format on fraction/exponent boundaries as
    // well as the36 authored records. No floating reinterpretation shortcut.
    for(float value:{0.000001f,.125f,.5f,.999f,1.f,1.5f,2.f,127.f,128.f,255.f,256.f,310.f,65535.f,100000.f}){
        RefCpu c(m);c.r[15]=frame;c.pr=stop;c.setFloat(4,value);unsigned packed=~0u;
        c.callHooks[0x0c21b460]=[&](auto& q){check(q.r[4]==0xb8,"density hardware register");packed=q.r[5];};
        instructions+=c.run(0x0c1facc0,stop,200);
        check(originalFogPackedDensity(value)==packed&&c.r[0]==packed,"density pack boundary");
    }
    RefCpu generic(m);generic.r[4]=owner;generic.r[15]=frame;unsigned lightOwner=0;
    generic.callHooks[0x0c1acc40]=[&](auto& q){lightOwner=q.r[4];};
    instructions+=generic.run(0x0c19afa0,0x0c19afb6,30);
    check(lightOwner==owner+108,"generic course reset fog owner ordering");
    for(unsigned invalid:{9u,17u,~0u}){bool rejected=false;try{originalCourseFog(invalid,false,false);}catch(const std::out_of_range&){rejected=true;}check(rejected,"invalid course rejected");}
    std::cout<<"PASS original course fog: "<<checks<<" checks / "<<instructions<<" original instructions;36 authored rows,72 direction/condition cases,14 density boundaries.\n"
        <<"Only final PVR register writer and generic-owner dependency are hooked; no emulator/device/userdata.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
