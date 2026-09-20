#include "original_transmission.h"
#include "sh4_scalar_reference.h"
#include <bit>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace idas3;
namespace {
using idas3::reference::RefCpu;
using idas3::reference::RefMemory;
std::uint32_t bits(float value){return std::bit_cast<std::uint32_t>(value);}
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::uint32_t randomWord(std::uint32_t& seed){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}

void writeState(RefMemory& m,const OriginalTransmissionState& s) {
    const auto values=std::bit_cast<std::array<std::uint32_t,10>>(s);
    for(std::size_t i=0;i<values.size();++i)m.write32(0x0c900e88u+std::uint32_t(i*4),values[i]);
}
void verifyState(const RefMemory& m,const OriginalTransmissionState& s,std::size_t trial) {
    const auto values=std::bit_cast<std::array<std::uint32_t,10>>(s);
    for(std::size_t i=0;i<values.size();++i) {
        const auto actual=m.read32(0x0c900e88u+std::uint32_t(i*4));
        if(actual!=values[i])throw std::runtime_error("Original transmission state mismatch trial="+std::to_string(trial)+
            " offset="+std::to_string(i*4)+" reference="+idas3::reference::hex(actual)+" lifted="+idas3::reference::hex(values[i]));
    }
}

OriginalTransmissionProfile imageProfile(const RefMemory& m,std::uint32_t index) {
    std::array<std::byte,88> bytes{};
    for(std::size_t i=0;i<bytes.size();++i)bytes[i]=std::byte(m.read8(0x0c28825cu+index*88+std::uint32_t(i)));
    return decodeOriginalTransmissionProfile(bytes);
}

float controlledSine(float value,void*) {return std::sin(value);}

void compareFullStage(RefMemory& m) {
    const std::array<OriginalTransmissionProfile,2> profiles={imageProfile(m,0),imageProfile(m,1)};
    std::uint32_t seed=0xe4d38219;
    constexpr std::size_t trials=8192;
    std::size_t sineCalls=0;
    for(std::size_t trial=0;trial<trials;++trial) {
        m.clear();
        OriginalTransmissionState s;
        s.gear00=std::uint32_t(trial%7);
        s.snapshot04=0x12345678;s.snapshot08=0x87654321;s.rangeFlag0c=0x11223344;s.untouched10=0x44332211;
        s.target14=93;s.filtered18=trial%9==0?-1.0f:float(randomWord(seed)%11000);
        s.tach1c=trial%3==0?12000.0f:float(randomWord(seed)%9000);
        s.normalized20=.625f;s.delta24=-.03125f;
        OriginalTransmissionDrive d;
        d.field080=float(randomWord(seed)%1001)/1000.0f;
        d.field12c=(trial%9)==0?1u:0u;d.field130=(trial%5)==0?1u:0u;
        d.field220=20;d.field224=123.5f;d.field228=63.125f;
        d.velocity238=float(randomWord(seed)%20000)/100.0f;
        d.delta240=-.5f;d.selection248=float(randomWord(seed)%11001)/10000.0f;
        d.field400=randomWord(seed)&1u;
        const auto index=std::uint32_t((trial/7)%2);
        s.gear00=std::uint32_t(trial%(index?7:6));
        OriginalTransmissionGlobals g;
        g.previousGear988c=std::uint32_t((trial/3)%7);
        g.loss9880=float(randomWord(seed)%100)/1000.0f;
        g.throttle9898=float(randomWord(seed)%1001)/1000.0f;
        g.shiftDifference98a8=19;g.coupledSnapshot98ac=43;g.coupling98d0=.0125f;
        g.phase9870=trial%2?0xfffedcbau:0x1234u;
        g.downCounter9cfc=std::array<std::uint32_t,4>{0,1,59,60}[(trial/14)%4];
        g.flag91fb4c=randomWord(seed)&1u;
        OriginalTransmissionInputs in;
        in.pressedByte=std::uint8_t(trial&255);in.automaticMode=(trial/256)%2!=0;in.gearEnabled=trial%11!=0;
        in.coefficientFr15=250.0f+float(randomWord(seed)%500);
        OriginalTransmissionParameters p{index,index?6u:5u,7500.0f+float(trial%4)*1000.0f,4000,6500,2};
        writeState(m,s);
        const std::array<std::pair<std::uint32_t,float>,7> driveFloats={{
            {0x080,d.field080},{0x220,d.field220},{0x224,d.field224},{0x228,d.field228},
            {0x238,d.velocity238},{0x240,d.delta240},{0x248,d.selection248}}};
        for(auto [offset,value]:driveFloats)m.writeFloat(0x0c900f00u+offset,value);
        m.write32(0x0c90102cu,d.field12c);m.write32(0x0c901030u,d.field130);m.write32(0x0c901300u,d.field400);
        m.writeFloat(0x0caa9880u,g.loss9880);m.write32(0x0caa988cu,g.previousGear988c);
        m.writeFloat(0x0caa9898u,g.throttle9898);m.writeFloat(0x0caa98a8u,g.shiftDifference98a8);
        m.writeFloat(0x0caa98acu,g.coupledSnapshot98ac);m.writeFloat(0x0caa98d0u,g.coupling98d0);
        m.write32(0x0caa9870u,g.phase9870);
        m.write32(0x0c9015c4u,in.automaticMode?1u:0u);m.write8(0x0c92ed40u,in.pressedByte);
        m.write32(0x0c91fb4cu,g.flag91fb4c);m.write32(0x0caa9cfcu,g.downCounter9cfc);
        m.write32(0x0c900954u,0x0c930000u);m.write32(0x0c930050u,in.gearEnabled?0x8000u:0u);
        RefCpu cpu(m);cpu.r[5]=p.profileIndex;cpu.r[6]=p.maximumGear;cpu.r[8]=0x248;
        cpu.setFloat(14,p.workingBase);cpu.setFloat(8,p.lower);cpu.setFloat(6,p.upper);cpu.setFloat(7,p.divisor);cpu.setFloat(15,in.coefficientFr15);
        // This hook proves surrounding call/return dataflow only. Host sine is
        // deliberately NOT represented as a verified original Sega math helper.
        cpu.callHooks[0x0c1fa0e0u]=[&](RefCpu& called){++sineCalls;called.setFloat(0,controlledSine(called.getFloat(4),nullptr));};
        cpu.run(0x0c15e52eu,0x0c15ea42u,5000);
        stepOriginalTransmission(s,d,g,in,p,profiles[index],controlledSine,nullptr);
        verifyState(m,s,trial);
        const std::array<std::pair<std::uint32_t,std::uint32_t>,18> outputs={{
            {0x0c901300u,d.field400},{0x0c90102cu,d.field12c},{0x0c901030u,d.field130},
            {0x0c901120u,bits(d.field220)},{0x0c901138u,bits(d.velocity238)},
            {0x0c901140u,bits(d.delta240)},{0x0c901148u,bits(d.selection248)},
            {0x0caa9880u,bits(g.loss9880)},{0x0caa988cu,g.previousGear988c},
            {0x0caa9898u,bits(g.throttle9898)},{0x0caa98a8u,bits(g.shiftDifference98a8)},
            {0x0caa98acu,bits(g.coupledSnapshot98ac)},{0x0caa98d0u,bits(g.coupling98d0)},
            {0x0caa9870u,g.phase9870},{0x0caa9cfcu,g.downCounter9cfc},{0x0c91fb4cu,g.flag91fb4c},
            {0x0c901124u,bits(d.field224)},{0x0c901128u,bits(d.field228)}}};
        for(auto [address,value]:outputs)if(m.read32(address)!=value)throw std::runtime_error(
            "Full transmission mismatch trial="+std::to_string(trial)+" address="+idas3::reference::hex(address)+
            " reference="+idas3::reference::hex(m.read32(address))+" lifted="+idas3::reference::hex(value));
    }
    std::cout<<"Original E52E..EA42 full transmission: "<<trials<<" finite scenarios bit-identical under explicit IEEE FMAC and controlled sine boundary; sine calls="<<sineCalls<<".\n";
}

// These tests execute actual verified-image opcodes. E52E..E6F6 has no
// FMAC, ABI math call, race-service call, or device access.
void compareGearDecision(RefMemory& m) {
    const std::array<OriginalTransmissionProfile,2> profiles={imageProfile(m,0),imageProfile(m,1)};
    std::uint32_t seed=0x84633ad2;
    constexpr std::size_t trials=8192;
    for(std::size_t trial=0;trial<trials;++trial) {
        m.clear();
        OriginalTransmissionState s;
        s.gear00=std::uint32_t(trial%7);
        s.snapshot04=0x12345678;s.snapshot08=0x87654321;s.rangeFlag0c=0x11223344;s.untouched10=0x44332211;
        s.target14=93;s.filtered18=trial%9==0?-1.0f:float(randomWord(seed)%11000);s.tach1c=72;s.normalized20=.625f;s.delta24=-.03125f;
        OriginalTransmissionDrive d;
        d.selection248=float(randomWord(seed)%11001)/10000.0f;
        // Include each literal threshold, its exact predecessor and successor.
        const auto index=std::uint32_t((trial/7)%2);
        const auto& profile=profiles[index];
        if(trial%5==0) {
            float threshold=profile.atByteOffset(4+4*(1+trial%5));
            threshold*=profile.atByteOffset(0);
            d.selection248=trial%3==0?threshold:std::nextafter(threshold,trial%3==1?0.0f:2.0f);
        }
        d.field400=randomWord(seed)&1u;
        OriginalTransmissionGlobals g;
        g.downCounter9cfc=std::array<std::uint32_t,4>{0,1,59,60}[(trial/14)%4];
        g.flag91fb4c=randomWord(seed)&1u;
        OriginalTransmissionInputs in;
        in.pressedByte=std::uint8_t(trial&255);
        in.automaticMode=(trial/256)%2!=0;
        in.gearEnabled=trial%11!=0;
        OriginalTransmissionParameters p;
        p.profileIndex=index;p.maximumGear=index?6u:5u;p.workingBase=7500;p.lower=4000;p.upper=6500;p.divisor=2;
        writeState(m,s);
        m.writeFloat(0x0c900f00u+0x248,d.selection248);
        m.write32(0x0c900f00u+0x400,d.field400);
        m.write32(0x0c9015c4u,in.automaticMode?1u:0u);
        m.write8(0x0c92ed40u,in.pressedByte);
        m.write32(0x0c91fb4cu,g.flag91fb4c);
        m.write32(0x0caa9cfcu,g.downCounter9cfc);
        m.write32(0x0c900954u,0x0c930000u);
        m.write32(0x0c930050u,in.gearEnabled?0x8000u:0u);
        RefCpu cpu(m);
        cpu.r[5]=p.profileIndex;cpu.r[6]=p.maximumGear;cpu.r[8]=0x248;
        cpu.setFloat(14,p.workingBase);cpu.setFloat(8,p.lower);cpu.setFloat(6,p.upper);cpu.setFloat(7,p.divisor);
        cpu.run(0x0c15e52eu,0x0c15e6f6u,2000);
        decideOriginalTransmissionGear(s,d,g,in,p,profile);
        verifyState(m,s,trial);
        require(m.read32(0x0c901300u)==d.field400,"drive+400 differs from original opcode reference");
        require(m.read32(0x0c91fb4cu)==g.flag91fb4c,"side-effect flag differs from original opcode reference");
        require(m.read32(0x0caa9cfcu)==g.downCounter9cfc,"downshift counter differs from original opcode reference");
    }
    std::cout<<"Original E52E..E6F6 gear decision: "<<trials<<" actual-image opcode scenarios bit-identical.\n";
}

void controlledFullStage(const OriginalTransmissionProfile& profile) {
    OriginalTransmissionParameters p{0,5,7500,4000,6500,2};
    OriginalTransmissionState s;s.gear00=2;
    OriginalTransmissionDrive d;d.field080=.5f;d.field220=17;
    OriginalTransmissionGlobals g;g.previousGear988c=2;g.throttle9898=1;
    OriginalTransmissionInputs in;in.coefficientFr15=1000;
    stepOriginalTransmission(s,d,g,in,p,profile);
    // Actual profile rise[gear2]=24, doubled at low filtered value =>48.
    float span=7500.0f;span-=800.0f;span-=500.0f;
    float expected=800.0f;expected+=span;expected/=48.0f;
    require(bits(s.filtered18)==bits(expected),"missing original E884/E886 doubled rise divisor");
    require(d.velocity238>0&&d.delta240>0,"original engine-state feedback must update velocity");
    require(s.snapshot04==2&&s.snapshot08==2,"snapshot ordering changed");
    require(d.field220==17,"unselected cancellation may not clear drive state");
    OriginalPowertrainRow row{1,6,9000,5000,7500,2},overrideRow{0,5,11000,7000,8500,3};
    require(selectOriginalTransmissionParameters(row,overrideRow,false).workingBase==8500,"ordinary row subtracts 500");
    require(selectOriginalTransmissionParameters(row,overrideRow,true).workingBase==11000,"override row must not subtract 500");
    s={};s.gear00=1;s.tach1c=10000;g={};g.previousGear988c=1;d={};d.field080=.5f;
    bool threw=false;try{stepOriginalTransmission(s,d,g,in,p,profile);}catch(const std::runtime_error&){threw=true;}
    require(threw,"tach overshoot must request the original sine boundary");
    std::cout<<"Full-stage controlled semantic checks passed; original sine/exceptional FPU behavior remains pending.\n";
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2)throw std::runtime_error("Pass the verified user-owned 4 MiB image path");
        RefMemory m(argv[1]);
        auto profile=imageProfile(m,0);
        compareGearDecision(m);
        compareFullStage(m);
        controlledFullStage(profile);
        std::cout<<"This validates bounded original transmission logic, not complete vehicle/game fidelity.\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
