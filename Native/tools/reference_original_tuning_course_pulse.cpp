#include "sh4_scalar_reference.h"
#include <fstream>
#include <iostream>

using namespace idas3::reference;

int main(int argc,char** argv){
    try{
        if(argc!=3)throw std::runtime_error("Expected a program and output path");
        RefMemory memory(argv[1],RefMemory::SupportedImages::EnglishAndJapanRevC);
        memory.zeroRegion(0xd000000,0x20000);
        const bool ja=memory.japanRevC;
        std::ofstream output(argv[2],std::ios::binary);
        output.exceptions(std::ios::badbit|std::ios::failbit);
        unsigned long long total=0;
        for(unsigned frame=0;frame<4096;++frame){
            RefCpu cpu(memory);
            cpu.r[4]=0xd000000;cpu.r[15]=0xd01f000;cpu.pr=0xff0000;
            memory.write32(0xd000000,0x80035);memory.write32(0xd000004,frame);
            unsigned color=0,calls=0;
            for(auto address:{ja?0xc1d20c0u:0xc1baf40u,ja?0xc1d2860u:0xc1bb6e0u,
                              ja?0xc1fd750u:0xc1f6610u,ja?0xc1fd700u:0xc1f65c0u})
                cpu.callHooks[address]=[](auto& c){c.r[0]=1;};
            cpu.callHooks[ja?0xc05a5a0:0xc05a8e0]=[](auto& c){c.r[0]=c.r[5];};
            cpu.callHooks[ja?0xc1d5760:0xc1be600]=[&](auto& c){color=c.r[5];++calls;};
            total+=cpu.run(ja?0xc1d5680:0xc1be520,0xff0000,30000);
            if(calls!=2)throw std::runtime_error("Unexpected tuning pulse draw count");
            output.write(reinterpret_cast<const char*>(&color),4);
        }
        std::ofstream angles(std::filesystem::path(argv[2]).parent_path()/"cursor_scale.bin",std::ios::binary);
        angles.exceptions(std::ios::badbit|std::ios::failbit);
        for(unsigned i=0;i<128;++i){
            RefCpu cpu(memory);
            cpu.r[4]=(i+1)*512;cpu.r[15]=0xd01f000;cpu.pr=0xff0000;
            total+=cpu.run(ja?0xc201120:0xc1f9fe0,0xff0000,1000);
            float value=cpu.getFloat(0);
            angles.write(reinterpret_cast<const char*>(&value),4);
        }
        std::cout<<total<<" instructions\n";
    }catch(const std::exception& error){
        std::cerr<<error.what()<<"\n";
        return 1;
    }
}
