#include "original_transmission.h"
#include "original_driving_float.h"
#include "original_math.h"
#include "sh4_scalar_reference.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

using namespace idas3;
using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned driveBase=0x0c900f00,transmissionBase=0x0c900e88;
std::uint64_t checks=0,instructions=0,cases=0,sequenceFrames=0;
std::uint64_t tachCases=0,cancelCases=0,holdCases=0,gearChanges=0;
unsigned contextCar=0,contextVariant=0,contextTick=0;
unsigned bits(float v){return std::bit_cast<unsigned>(v);}
void equal(unsigned expected,unsigned actual,unsigned address){
    ++checks;
    if(expected!=actual)throw std::runtime_error("Transmission differential car="+std::to_string(contextCar)+
        " variant="+std::to_string(contextVariant)+" tick="+std::to_string(contextTick)+
        " address="+hex(address)+" original="+hex(expected)+" native="+hex(actual));
}
float sine(float value,void*){return originalSinF32(value);}

struct Fixture {
    RefMemory& m;
    OriginalTransmissionState s;
    OriginalTransmissionDrive d;
    OriginalTransmissionGlobals g;
    OriginalTransmissionInputs in;
    OriginalTransmissionParameters p;
    OriginalTransmissionProfile profile;
    unsigned car;bool special;
    Fixture(RefMemory& memory,unsigned selectedCar,bool engine):m(memory),car(selectedCar),special(engine){
        const auto image=std::span<const std::byte>(reinterpret_cast<const std::byte*>(m.image.data()),m.image.size());
        const auto ordinary=decodeOriginalPowertrainRow(image.subspan(0x0c28830c-RefMemory::imageBase+car*24,24));
        const auto overrideRow=decodeOriginalPowertrainRow(image.subspan(0x0c288654-RefMemory::imageBase,24));
        p=selectOriginalTransmissionParameters(ordinary,overrideRow,special);
        profile=decodeOriginalTransmissionProfile(image.subspan(0x0c28825c-RefMemory::imageBase+p.profileIndex*88,88));
    }
    void reset(){
        m.clear();m.zeroRegion(0x0c900e00,0xa00);m.zeroRegion(0x0caa9870,0x4a0);
        m.zeroRegion(0x0c91fb0c,0x50);m.zeroRegion(0x0cfff000,0x1000);
        m.write32(0x0c901654,car);m.write32(0x0c9015f4,special?1:0);
        m.write32(0x0c900954,0x0c930000);
    }
    void publishState(){
        const auto words=std::bit_cast<std::array<unsigned,10>>(s);
        for(unsigned i=0;i<words.size();++i)m.write32(transmissionBase+i*4,words[i]);
        for(auto [offset,value]:std::array<std::pair<unsigned,float>,7>{{
            {0x080,d.field080},{0x220,d.field220},{0x224,d.field224},{0x228,d.field228},
            {0x238,d.velocity238},{0x240,d.delta240},{0x248,d.selection248}}})m.writeFloat(driveBase+offset,value);
        m.write32(driveBase+0x12c,d.field12c);m.write32(driveBase+0x130,d.field130);m.write32(driveBase+0x400,d.field400);
        for(auto [address,value]:std::array<std::pair<unsigned,unsigned>,9>{{
            {0x0caa9880,bits(g.loss9880)},{0x0caa988c,g.previousGear988c},{0x0caa9898,bits(g.throttle9898)},
            {0x0caa98a8,bits(g.shiftDifference98a8)},{0x0caa98ac,bits(g.coupledSnapshot98ac)},
            {0x0caa98d0,bits(g.coupling98d0)},{0x0caa9870,g.phase9870},
            {0x0caa9cfc,g.downCounter9cfc},{0x0c91fb4c,g.flag91fb4c}}})m.write32(address,value);
    }
    void compare(){
        const auto words=std::bit_cast<std::array<unsigned,10>>(s);
        for(unsigned i=0;i<words.size();++i)equal(m.read32(transmissionBase+i*4),words[i],transmissionBase+i*4);
        for(auto [address,value]:std::array<std::pair<unsigned,unsigned>,19>{{
            {driveBase+0x080,bits(d.field080)},{driveBase+0x12c,d.field12c},{driveBase+0x130,d.field130},
            {driveBase+0x220,bits(d.field220)},{driveBase+0x224,bits(d.field224)},{driveBase+0x228,bits(d.field228)},
            {driveBase+0x238,bits(d.velocity238)},{driveBase+0x240,bits(d.delta240)},
            {driveBase+0x248,bits(d.selection248)},{driveBase+0x400,d.field400},
            {0x0caa9880,bits(g.loss9880)},{0x0caa988c,g.previousGear988c},{0x0caa9898,bits(g.throttle9898)},
            {0x0caa98a8,bits(g.shiftDifference98a8)},{0x0caa98ac,bits(g.coupledSnapshot98ac)},
            {0x0caa98d0,bits(g.coupling98d0)},{0x0caa9870,g.phase9870},
            {0x0caa9cfc,g.downCounter9cfc},{0x0c91fb4c,g.flag91fb4c}}})equal(m.read32(address),value,address);
    }
    void step(){
        m.write32(0x0c9015c4,in.automaticMode?1:0);m.write8(0x0c92ed40,in.pressedByte);
        m.write32(0x0c930050,in.gearEnabled?0x8000:0);
        RefCpu c(m);c.r[1]=0x0c901654;c.r[11]=0x0c28830c;c.r[8]=0x248;c.r[15]=0x0cfff800;
        c.setFloat(15,in.coefficientFr15);
        // Execute the original table selector rather than injecting native p.
        instructions+=c.run(0x0c15e4e6,0x0c15e52e,200);
        equal(c.r[5],p.profileIndex,0x0c15e52e);equal(c.r[6],p.maximumGear,0x0c15e52e);
        equal(c.fr[14],bits(p.workingBase),0x0c15e52e);equal(c.fr[8],bits(p.lower),0x0c15e52e);
        equal(c.fr[6],bits(p.upper),0x0c15e52e);equal(c.fr[7],bits(p.divisor),0x0c15e52e);
        const unsigned previous=s.gear00;
        if(s.tach1c>p.workingBase+50)++tachCases;
        if(d.field12c==1&&d.field130==1)++cancelCases;
        if(in.automaticMode&&(in.pressedByte&0x10||g.downCounter9cfc))++holdCases;
        // No hooks: original sine body executes, including its own FPU modes.
        instructions+=c.run(0x0c15e52e,0x0c15ea42,6000);
        stepOriginalTransmission(s,d,g,in,p,profile,sine,nullptr);
        compare();if(previous!=s.gear00)++gearChanges;
    }
};

std::vector<float> neighbors(float value){return {std::nextafter(value,-INFINITY),value,std::nextafter(value,INFINITY)};}
void thresholdMatrix(RefMemory& m){
    for(unsigned row=0;row<36;++row){
        contextCar=row==35?0:row;Fixture f(m,contextCar,row==35);
        std::vector<float> speeds;
        for(unsigned gear=1;gear<=f.p.maximumGear;++gear){
            const float at=f.profile.atByteOffset(4+4*gear)*f.profile.atByteOffset(0);
            for(float multiplier:{1.f,std::bit_cast<float>(0x3f733333u)})
                for(float value:neighbors(at*multiplier))speeds.push_back(value);
        }
        std::vector<float> rpms{-1,0,800,f.p.workingBase};
        for(float boundary:{3500.f,f.p.lower,f.p.upper})for(float value:neighbors(boundary))rpms.push_back(value);
        for(bool automatic:{false,true})for(unsigned gear=0;gear<=f.p.maximumGear;++gear)
        for(float speed:speeds)for(unsigned edge:{0u,0x10u,0x20u,0x30u})for(bool enabled:{false,true}){
            contextVariant=unsigned(cases);contextTick=0;f.reset();f.s={};f.d={};f.g={};f.in={};
            f.s.gear00=gear;f.s.filtered18=rpms[cases%rpms.size()];f.s.tach1c=f.p.workingBase+(cases%3?70.f:5000.f);
            f.s.untouched10=0x5a8c27e1;f.s.target14=-19;f.s.snapshot04=0xa321;f.s.snapshot08=0xb321;
            f.s.normalized20=.5f;f.s.delta24=.2f;f.s.rangeFlag0c=3;
            f.in.automaticMode=automatic;f.in.gearEnabled=enabled;f.in.pressedByte=std::uint8_t(edge);
            f.in.coefficientFr15=m.readFloat(0x0c28451c+(unsigned(cases/13)%18)*140+contextCar*4);
            f.d.velocity238=speed*f.in.coefficientFr15*.278f;f.d.selection248=speed;
            f.d.field080=.5f;f.d.field220=4.5f;f.d.field224=.2f;f.d.field228=.4f;f.d.delta240=.25f;
            f.d.field12c=unsigned(cases/2)%3;f.d.field130=unsigned(cases/5)%3;f.d.field400=unsigned(cases)&1;
            f.g.phase9870=unsigned(cases%2?10000:0xffff0000);f.g.previousGear988c=unsigned(cases/17)%(f.p.maximumGear+1);
            f.g.loss9880=float(cases%7)*.05f;f.g.throttle9898=float(cases%5)*.25f;
            f.g.downCounter9cfc=std::array<unsigned,6>{0,1,2,59,60,61}[cases%6];f.g.flag91fb4c=unsigned(cases)&1;
            f.g.shiftDifference98a8=.9f;f.g.coupledSnapshot98ac=200;f.g.coupling98d0=.2f;
            f.publishState();f.step();++cases;
        }
    }
}
void persistentSequences(RefMemory& m){
    // Preserve transmission state/hold/transition history through long mode,
    // throttle and shift changes. Only omitted caller-owned inputs are updated.
    for(unsigned row=0;row<36;++row){
        contextCar=row==35?0:row;contextVariant=0x10000+row;Fixture f(m,contextCar,row==35);
        f.reset();f.s.gear00=1;f.s.filtered18=800;f.s.tach1c=800;f.d.field080=.5f;
        f.in.coefficientFr15=m.readFloat(0x0c28451c+6*140+contextCar*4);f.publishState();
        for(unsigned tick=0;tick<2400;++tick){
            contextTick=tick;
            f.in.automaticMode=(tick/120)%2!=0;f.in.gearEnabled=tick%601>20;
            const unsigned phase=tick%240;
            f.in.pressedByte=std::uint8_t(phase<60?(tick%3==0?0x20:0):phase<120?(tick%3==0?0x10:0):
                phase<180?(tick%2?0x20:0x10):(tick%3==0?0x30:0));
            f.g.throttle9898=tick%400<240?1.f:tick%400<320?.5f:0.f;
            f.g.loss9880=tick%400>=350?.2f:.001f;
            f.d.field12c=tick%240>=200?1:0;f.d.field130=tick%240>=220?1:0;
            f.d.selection248=f.d.velocity238/(.278f*f.in.coefficientFr15);
            m.writeFloat(0x0caa9898,f.g.throttle9898);m.writeFloat(0x0caa9880,f.g.loss9880);
            m.write32(driveBase+0x12c,f.d.field12c);m.write32(driveBase+0x130,f.d.field130);
            m.writeFloat(driveBase+0x248,f.d.selection248);
            f.step();++sequenceFrames;
            f.g.previousGear988c=f.s.gear00;m.write32(0x0caa988c,f.g.previousGear988c);
        }
    }
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::runtime_error("Usage: original_transmission_boundary_reference_tests canonical_image");
    const OriginalDrivingFloatScope floatingPoint;RefMemory m(argv[1]);
    thresholdMatrix(m);persistentSequences(m);
    if(!tachCases||!cancelCases||!holdCases||!gearChanges)throw std::runtime_error("Insufficient transmission branch coverage");
    std::cout<<"PASS "<<cases<<" threshold/table scenarios and "<<sequenceFrames<<" persistent transmission frames, "<<checks
        <<" bit comparisons, "<<instructions<<" original instructions; tach_inputs="<<tachCases<<" loss_cancel_inputs="<<cancelCases
        <<" automatic_hold_inputs="<<holdCases<<" gear_changes="<<gearChanges<<". Zero hooks.\n"
        <<"Scope: real original table selector and E52E..EA42 with original sine; caller speed/loss/coefficient inputs are explicit. This is not whole-car or cabinet-input parity.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
