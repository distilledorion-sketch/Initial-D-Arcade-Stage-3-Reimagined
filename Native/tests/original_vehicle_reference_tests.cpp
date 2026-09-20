#include "original_math.h"
#include "original_vehicle.h"
#include "sh4_scalar_reference.h"
#include <bit>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <vector>
using namespace idas3;
using namespace idas3::original;
using namespace idas3::reference;
namespace {
std::uint32_t bits(float x){return std::bit_cast<std::uint32_t>(x);}
}
int main(int argc,char**argv)try{
    if(argc!=2)throw std::runtime_error("Usage: original_vehicle_reference_tests canonical_main_image.bin");
    RefMemory memory{std::filesystem::path(argv[1])};
    memory.zeroRegion(0x0C900E00,0xA00);memory.zeroRegion(0x0CAA9870,0x4A0);
    memory.zeroRegion(0x0C91FB0C,0x50);memory.zeroRegion(0x0CFFF000,0x200);
    OriginalVehicleState state;
    OriginalVehicleParameters p;
    constexpr std::uint32_t base=0x0C900F00,vehicle=0,condition=0,mode=0;
    p.frame.table0C28451C=memory.readFloat(0x0C28451C+condition*140+vehicle*4);
    p.frame.table0C28866C=memory.readFloat(0x0C28866C+mode*4);
    p.angular.carRecord0C283F18_08=memory.readFloat(0x0C283F18+vehicle*44+8);
    p.angular.carRecord0C283F18_0C=memory.readFloat(0x0C283F18+vehicle*44+12);
    const auto index=(condition/2)*140+vehicle*4;
    p.angular.table0C285124=memory.readFloat(0x0C285124+index);
    p.angular.table0C285AFC=memory.readFloat(0x0C285AFC+index);
    p.angular.table0C285610=memory.readFloat(0x0C285610+index);
    p.angular.table0C287398=memory.readFloat(0x0C287398+index);
    p.angular.table0C285FE8=memory.readFloat(0x0C285FE8+index);
    p.angular.global0C900E40=1;p.angular.global0C900EC0=1;p.angular.global0C8FF380=1;p.angular.global0C900E54=1;
    p.steeringMemory.table0C2864D4=memory.readFloat(0x0C2864D4+index);
    p.steeringMemory.table0C286EAC=memory.readFloat(0x0C286EAC+index);
    p.steeringMemory.table0C287884=memory.readFloat(0x0C287884+index);
    p.steeringMemory.table0C2869C0=memory.readFloat(0x0C2869C0+index);
    p.steeringMemory.table0C287D70=memory.readFloat(0x0C287D70+index);
    p.steeringMemory.global0C90094C=1;p.steeringMemory.global0C900E30=1;p.steeringMemory.global0C900EF8=1;
    p.steeringMemory.global0C900E4C=1;p.steeringMemory.global0C900EB0=1;
    p.loss.condition0C9015CC=condition;p.loss.cap0C284F80=memory.readFloat(0x0C284F80+vehicle*8);
    p.loss.growth0C284F84=memory.readFloat(0x0C284F84+vehicle*8);
    const auto imageBytes=std::span<const std::byte>(reinterpret_cast<const std::byte*>(memory.image.data()),memory.image.size());
    const auto row=decodeOriginalPowertrainRow(imageBytes.subspan(0x0C28830C-RefMemory::imageBase+vehicle*24,24));
    p.transmission=selectOriginalTransmissionParameters(row,{},false);
    p.profile=decodeOriginalTransmissionProfile(imageBytes.subspan(0x0C28825C-RefMemory::imageBase+row.profileIndex*88,88));
    p.road.condition0C9015CC=condition;p.road.mode0C9015E0=0;
    p.road.lastIndex0C283E88=memory.read32(0x0C283E88+condition*8);
    std::vector<std::array<float,3>> path(p.road.lastIndex0C283E88+1);
    for(std::size_t i=0;i<path.size();++i){path[i]={0,0,-float(i)*2};for(std::size_t axis=0;axis<3;++axis)memory.writeFloat(0x0CFE0000+std::uint32_t(i*12+axis*4),path[i][axis]);}
    p.road.path0C901728=path;
    state.drive.setu(0x1C0,16);state.drive.setu(0x3F0,1);
    state.drive.setf(0x238,10);state.drive.setf(0x23C,10);
    state.transmission.gear00=1;state.transmission.filtered18=800;
    for(const auto off:{0x2D8,0x2E4,0x2F0,0x2FC})state.drive.setf(off+8,1);
    for(std::size_t i=0;i<state.drive.words.size();++i)memory.write32(base+std::uint32_t(i*4),state.drive.words[i]);
    memory.write32(0x0C900E88,1);memory.writeFloat(0x0C900EA0,800);
    memory.write32(0x0C9015CC,condition);memory.write32(0x0C901654,vehicle);memory.write32(0x0C9015F0,mode);
    memory.write32(0x0C901728,0x0CFE0000);memory.write32(0x0C9015E8,128);
    memory.write32(0x0C900954,0x0CFC0000);memory.write32(0x0CFC0050,0x8000);
    memory.write32(0x0C2F4BC8,0);memory.write32(0x0C9015C4,1);
    for(const auto address:{0x0C900E40u,0x0C900EC0u,0x0C8FF380u,0x0C900E54u,0x0C90094Cu,0x0C900E30u,0x0C900EF8u,0x0C900E4Cu,0x0C900EB0u})memory.writeFloat(address,1);
    // Execute actual142520 in its explicit disabled platform-service mode.
    const auto serviceModeAddress=memory.read32(0x0C142564);memory.write8(serviceModeAddress,1);
    std::size_t instructions=0,comparisons=0;
    for(std::uint32_t tick=0;tick<600;++tick){
        OriginalVehicleInputs in;
        in.analog={std::uint16_t((128+int(50*std::sin(float(tick)*.03f)))<<8),std::uint16_t((tick%180<140?139:32)<<8),std::uint16_t((tick%180>=160?100:32)<<8)};
        in.calibration={128,0,0};in.automaticMode=true;in.gearEnabled=true;in.elapsedFrames0C900E84=int(tick)+400;
        in.pressedByte=tick%120==40?0x10:tick%120==80?0x20:0;
        memory.write16(0x0C92F0E0,in.analog.steering);memory.write16(0x0C92F0E2,in.analog.throttle);memory.write16(0x0C92F0E4,in.analog.brake);
        memory.write8(0x0C92ED40,in.pressedByte);memory.write32(0x0C900E84,std::uint32_t(in.elapsedFrames0C900E84));
        RefCpu cpu(memory);cpu.r[15]=0x0CFFF100;cpu.pr=0x0F000000;
        instructions+=cpu.run(0x0C15CEC0,0x0F000000,20000);
        stepOriginalVehicle(state,in,p,{originalSinF32,originalCosF32,originalFiprDot3});
        const auto equal=[&](std::uint32_t address,std::uint32_t actual){
            ++comparisons;const auto expected=memory.read32(address);
            if(expected!=actual){std::cerr<<"FULL FRAME MISMATCH tick="<<tick<<" address="<<std::hex<<address<<" expected="<<expected<<" actual="<<actual<<std::dec<<'\n';throw std::runtime_error("Full original frame differential failed");}
        };
        for(std::size_t i=0;i<state.drive.words.size();++i)equal(base+std::uint32_t(i*4),state.drive.words[i]);
        const auto transmissionWords=std::bit_cast<std::array<std::uint32_t,10>>(state.transmission);
        for(std::size_t i=0;i<transmissionWords.size();++i)equal(0x0C900E88+std::uint32_t(i*4),transmissionWords[i]);
        for(std::size_t i=0;i<64;++i){equal(0x0CAA98E0+std::uint32_t(i*4),bits(state.tail.history0CAA98E0[i]));equal(0x0CAA9BE0+std::uint32_t(i*4),bits(state.tail.steeringHistory0CAA9BE0[i]));}
        for(std::size_t i=0;i<128;++i)equal(0x0CAA99E0+std::uint32_t(i*4),bits(state.tail.throttleHistory0CAA99E0[i]));
        for(std::size_t i=0;i<16;++i)equal(0x0C91FB0C+std::uint32_t(i*4),state.tail.statistics0C91FB0C[i]);
        const auto& g=state.transmissionGlobals;const auto& h=state.tail;const auto& c=state.controls;
        equal(0x0CAA9870,g.phase9870);equal(0x0CAA9874,bits(h.previousSpeed0CAA9874));equal(0x0CAA9878,bits(h.speedDelta0CAA9878));
        equal(0x0CAA9880,bits(state.loss.speedLoss0CAA9880));equal(0x0CAA9884,bits(state.loss.persistentPenalty0CAA9884));
        equal(0x0CAA9888,h.lastNonzeroGear0CAA9888);equal(0x0CAA988C,h.previousGear0CAA988C);
        equal(0x0CAA9894,bits(c.steering));equal(0x0CAA9898,bits(c.throttle));equal(0x0CAA989C,bits(c.throttleAlias));equal(0x0CAA98A0,bits(c.brake));
        equal(0x0CAA98A8,bits(g.shiftDifference98a8));equal(0x0CAA98AC,bits(g.coupledSnapshot98ac));equal(0x0CAA98B0,bits(h.filteredDelta0CAA98B0));equal(0x0CAA98D0,bits(g.coupling98d0));
        equal(0x0CAA9CE0,bits(h.mean0CAA9CE0));equal(0x0CAA9CE4,h.counter0CAA9CE4);equal(0x0CAA9CE8,h.counter0CAA9CE8);equal(0x0CAA9CEC,h.counter0CAA9CEC);
        equal(0x0CAA9CFC,g.downCounter9cfc);equal(0x0C91FB4C,g.flag91fb4c);
    }
    std::cout<<"PASS 600 sequential complete CEC0 frames, "<<comparisons<<" bit-exact state/global/history comparisons, "<<instructions
      <<" decoded original instructions. Zero hooks; actual original trig,142520 disabled service policy and15ECE0 execute. Synthetic road/contact inputs; upstream collision pipeline remains unvalidated.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
