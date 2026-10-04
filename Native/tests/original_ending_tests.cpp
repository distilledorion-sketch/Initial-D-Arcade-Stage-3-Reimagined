#include "original_ending.h"
#include "sh4_scalar_reference.h"
#include <iostream>
using namespace idas3;
using namespace idas3::reference;
int main(int argc,char** argv)try{
    if(argc!=2)throw std::runtime_error("Canonical image required");
    RefMemory memory(argv[1]);
    constexpr unsigned owner=0xd000000,stack=0xd010000,parent=0xd020000,done=0xf000000;
    unsigned checks=0;std::size_t instructions=0;
    const auto check=[&](bool yes,const char* why){++checks;if(!yes)throw std::runtime_error(why);};
    // The independent table is read from the image, not the port's constants.
    check(memory.read32(0xc262738)==3&&memory.read32(0xc26275c)==4,"Credit chunk table");
    for(int skipAt:{-1,0,600,4700,4860,5200}){
        memory.clear();memory.zeroRegion(owner,0x10000);memory.zeroRegion(stack,0x10000);memory.zeroRegion(parent,0x100);
        memory.write32(owner+0x430,parent);memory.write32(parent+44,done);
        original::OriginalEndingTimeline native;
        float credits=0,photos=0;
        for(unsigned tick=0;tick<5500&&!native.finished;++tick){
            const bool skip=int(tick)==skipAt;
            const bool final=native.phase>6;
            if(!final){
                for(unsigned i=0;i<2;++i){
                    const auto table=0xc262738+i*36;
                    const auto begin=memory.read32(table+4),end=memory.read32(table+8);
                    if(native.totalFrame<begin||native.totalFrame>end)continue;
                    RefCpu scroll(memory);scroll.r[11]=table+28;scroll.r[3]=table+8;scroll.r[7]=begin;
                    scroll.r[0]=0;scroll.r[8]=stack;scroll.setFloat(1,memory.readFloat(table+16));
                    memory.writeFloat(stack,i?photos:credits);
                    instructions+=scroll.run(0xc0ebc6c,0xc0ebc80,100);
                    (i?photos:credits)=memory.readFloat(stack);
                }
            }
            auto phase=native.phase,frame=native.frame;
            if(skip){if(final)phase=10;else{phase=6;frame=4737;}}
            memory.write32(owner+0x480,phase);memory.write32(owner+0x478,frame);
            memory.writeFloat(owner+0x49c,native.alpha);
            RefCpu cpu(memory);cpu.r[10]=owner;cpu.r[15]=stack+0xf000;
            bool finished=false;
            // Hardware/draw dependencies only. Phase branches and float fade
            // arithmetic execute the source instructions unchanged.
            cpu.callHooks[0xc222300]=[](RefCpu& c){c.r[0]=unsigned(c.getFloat(4));};
            cpu.callHooks[0xc0c5200]=[](RefCpu&){};
            cpu.callHooks[0xc2223b8]=[](RefCpu& c){c.fpul=c.r[4]/c.r[5];};
            cpu.callHooks[0xc141d80]=[](RefCpu&){};
            cpu.callHooks[0xc1416a0]=[](RefCpu&){};
            cpu.callHooks[done]=[&](RefCpu&){finished=true;};
            instructions+=cpu.run(0xc0eb532,0xc0eb7aa,1000);
            native.step(skip);
            check(native.phase==memory.read32(owner+0x480),"Ending phase differs from source");
            if(phase!=10)check(std::bit_cast<unsigned>(native.alpha)==memory.read32(owner+0x49c),"Ending fade differs from source");
            check(native.frame==frame+1&&native.totalFrame==tick+1,"Ending clocks");
            check(native.creditsY==credits&&native.photosY==photos,"Ending scroll differs from source");
            check(native.finished==finished,"Ending completion differs from source");
            check(native.finalCard==final,"Final artwork switch");
            check(native.fadeStream==(skip&&!final),"Skip fade event");
        }
        check(native.finished&&native.stopStream,"Ending failed to return");
        const auto frame=native.frame;native.step();check(native.frame==frame&&!native.stopStream,"Finished ending repeated notification");
    }
    std::cout<<"PASS "<<checks<<" ending checks; "<<instructions<<" source instructions. Draw, integer conversion/division, sound and parent-notification dependencies hooked.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
