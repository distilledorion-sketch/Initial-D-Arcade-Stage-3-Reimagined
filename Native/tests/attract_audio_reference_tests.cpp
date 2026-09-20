#include "original_attract_audio.h"
#include "original_stream_fade.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <vector>
using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned owner=0x0cd00000,packet=0x0cd08000,vt=0x0cd09000,stack=0x0cfff000;
std::uint64_t checks{},cases{},instructions{};
void eq(unsigned a,unsigned b,const char* what){++checks;if(a!=b)throw std::runtime_error(std::string(what)+": "+hex(a)+" != "+hex(b));}
void compare(const std::vector<OriginalAttractSoundCommand>& actual,const OriginalAttractSoundCommands& expected){
    eq(unsigned(actual.size()),expected.count,"command count");
    for(unsigned i=0;i<actual.size();++i){eq(unsigned(actual[i].operation),unsigned(expected.commands[i].operation),"operation");eq(actual[i].value,expected.commands[i].value,"argument");}
}
void seed(RefMemory& m){m.clear();m.zeroRegion(owner,0x10000);m.zeroRegion(stack-0x4000,0x5000);
    m.write32(owner+12,vt);m.write32(owner+1060+12,vt);m.write16(vt+40,0);m.write32(vt+44,0x00ed0000);}
void run(RefCpu& c,unsigned start,unsigned end){try{instructions+=c.run(start,end,10000);}catch(const std::exception& e){throw std::runtime_error(hex(start)+" at "+hex(c.pc)+": "+e.what());}}
void hooks(RefCpu& c,std::vector<OriginalAttractSoundCommand>& commands){
    c.callHooks[0x0c141cc0]=[&](auto& x){commands.push_back({OriginalAttractSoundOperation::Play,x.r[4]});};
    c.callHooks[0x0c141d80]=[&](auto&){commands.push_back({OriginalAttractSoundOperation::Stop,0});};
    c.callHooks[0x0c141e40]=[&](auto& x){commands.push_back({OriginalAttractSoundOperation::Volume,x.r[4]});};
    c.callHooks[0x00ed0000]=[](auto&){};
}
void rosso(RefMemory& m){
    for(unsigned config:{0u,1u,2u,0xffffffffu})for(unsigned frame=0;frame<401;++frame){
        seed(m);m.write32(packet+4,config);m.write32(owner+84,frame);
        RefCpu c(m);c.r[8]=owner+64;c.r[10]=owner;c.r[14]=stack;c.r[15]=stack;
        std::vector<OriginalAttractSoundCommand> commands;hooks(c,commands);
        c.callHooks[0x0c202140]=[](auto& x){x.r[0]=packet;};
        run(c,0x0c073a5a,0x0c073a92);
        compare(commands,originalAttractSoundCommands(4,frame,config==1));eq(m.read32(owner+84),frame+1,"Rosso source frame increment");++cases;
    }
}
void demo(RefMemory& m){
    for(unsigned config:{0u,1u,2u,0xffffffffu})for(unsigned frame:{0u,1u,500u,535u,3150u,4770u,5550u,5720u,5831u,0xffffffffu}){
        seed(m);m.write32(packet+4,config);m.write32(owner+0x4a4,frame);
        RefCpu c(m);c.r[0]=packet;c.r[12]=owner;c.r[11]=0;c.r[14]=stack;c.r[15]=stack;
        std::vector<OriginalAttractSoundCommand> commands;hooks(c,commands);
        // Explicit non-audio geometry activation boundaries in this switch.
        for(unsigned literal:{0x0c0e7148u,0x0c0e714cu,0x0c0e71f0u})c.callHooks[m.read32(literal)]=[](auto&){};
        run(c,0x0c0e701c,0x0c0e716c);
        compare(commands,originalAttractSoundCommands(7,frame,config==1));++cases;
    }
    for(unsigned frame:{0u,5800u,5830u,5831u,6000u,0xffffffffu}){
        seed(m);m.write32(stack+304,owner+0x2000);m.write32(owner+0x2000+40,frame);
        RefCpu c(m);c.r[12]=owner;c.r[14]=stack;c.r[15]=stack;
        std::vector<OriginalAttractSoundCommand> commands;hooks(c,commands);
        run(c,0x0c0e7620,0x0c0e7642);
        compare(commands,originalAttractSoundCommands(7,frame,false,std::int32_t(frame)>5830));++cases;
    }
}
void streamFades(RefMemory& m){
    constexpr unsigned ramp=0x0cd00000,output=0x0cd08000,done=0x00ed1000;
    for(unsigned level:{0u,1u,2u,63u,127u})for(unsigned delay:{0u,1u,8u,15u}){
        seed(m);m.writeFloat(ramp+12,float(level));m.writeFloat(ramp+16,0);m.writeFloat(ramp+20,level? -1.f:1.f);
        m.write32(ramp+24,0);m.write32(ramp+28,delay);
        OriginalStreamFade native;native.begin(level,delay);bool released=false;
        for(unsigned frame=0;frame<2200&&!released;++frame){
            RefCpu c(m);c.r[4]=ramp;c.r[5]=output;c.r[15]=stack;c.pr=done;
            c.callHooks[0x0c1cfb00]=[](auto&){}; // unlink finished stream task
            c.callHooks[0x0c1d00c0]=[&](auto&){released=true;}; // release task
            run(c,0x0c1cfb60,done);
            eq(native.tick(),m.read32(output),"stream fade output volume");
            eq(unsigned(native.active),unsigned(!released),"stream fade lifetime");
            if(!released){eq(native.next,unsigned(m.readFloat(ramp+12)),"stream fade next level");eq(native.wait,m.read32(ramp+24),"stream fade wait ticks");}
            ++cases;
        }
        eq(unsigned(released),1,"stream ramp must complete");
    }
}
void descriptors(RefMemory& m){
    const auto records=originalStreamDescriptors();eq(unsigned(records.size()),18,"source record count");
    for(unsigned i=0;i<records.size();++i){const auto address=0x0c31f250+72*i;const auto& record=records[i];
        for(unsigned j=0;j<record.filename.size();++j)eq(m.read8(address+j),unsigned(record.filename[j]),"source stream filename");
        eq(m.read8(address+unsigned(record.filename.size())),0,"source filename terminator");
        eq(m.read32(address+64),record.loop,"source loop argument");eq(m.read16(address+68),record.channel,"source channel");eq(m.read16(address+70),unsigned(record.volume),"source volume");
    }
}
}
int main(int argc,char** argv){try{if(argc!=2)throw std::runtime_error("canonical image required");RefMemory m(argv[1]);descriptors(m);rosso(m);demo(m);streamFades(m);
    std::cout<<"Attract audio: "<<cases<<" source cases, "<<checks<<" comparisons, "<<instructions<<" original instructions; all18 stream descriptors match source bytes\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
