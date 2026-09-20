#include "original_math.h"
#include "original_dynamics.h"
#include "sh4_scalar_reference.h"
#include <array>
#include <bit>
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <stdexcept>
using namespace idas3::original;
using namespace idas3::reference;
namespace {
std::uint32_t randomState=0x34DA1723;
std::uint32_t randomWord(){randomState^=randomState<<13;randomState^=randomState>>17;randomState^=randomState<<5;return randomState;}
float randomFloat(float scale=1){return float(randomWord()%65537)/65536.0f*scale;}
std::uint32_t bits(float x){return std::bit_cast<std::uint32_t>(x);}
}
int main(int argc,char**argv)try{
    if(argc!=2){std::cerr<<"Usage: original_dynamics_reference_tests canonical_main_image.bin\n";return 2;}
    RefMemory memory{std::filesystem::path(argv[1])};
    constexpr std::uint32_t base=0x0C900F00;
    std::size_t instructionCount=0, comparisons=0;
    for(std::uint32_t sample=0;sample<1400;++sample){
        memory.clear();memory.zeroRegion(base,OriginalDriveState::byteSize);
        OriginalDriveState state;
        const auto vehicle=sample%35,condition=(sample/35)%8,mode=(sample/280)%5;
        OriginalFrameParameters p;
        p.table0C28451C=memory.readFloat(0x0C28451C+condition*140+vehicle*4);
        p.table0C28866C=memory.readFloat(0x0C28866C+mode*4);
        p.global0CAA987C=randomFloat(20)-10;
        p.global0C901650=sample%7==0?0.0f:randomFloat(240)-180;
        p.global0C9015E4=(sample/2)%2;p.global0C9015D4=(sample/4)%2;
        state.setf(0x214,randomFloat());state.setf(0x218,randomFloat());
        for(std::size_t word=0;word<state.words.size();++word)memory.write32(base+std::uint32_t(word*4),state.words[word]);
        memory.write32(0x0C9015CC,condition);memory.write32(0x0C901654,vehicle);memory.write32(0x0C9015F0,mode);
        memory.write32(0x0C9015E4,p.global0C9015E4);memory.write32(0x0C9015D4,p.global0C9015D4);
        memory.writeFloat(0x0C901650,p.global0C901650);memory.writeFloat(0x0CAA987C,p.global0CAA987C);
        memory.zeroRegion(0x0CFFF000,256);memory.zeroRegion(0x0C92F0E0,8);memory.write32(0x0C2F4BC8,0);
        RefCpu cpu(memory);cpu.r[15]=0x0CFFF100;
        instructionCount+=cpu.run(0x0C15CEC0,0x0C15CF88);
        const float coefficient=prepareOriginalFrame(state,p);
        ++comparisons;if(bits(coefficient)!=cpu.fr[15])throw std::runtime_error("Original frame coefficient mismatch at "+std::to_string(sample));
        for(std::size_t word=0;word<state.words.size();++word){
            ++comparisons;if(state.words[word]!=memory.read32(base+std::uint32_t(word*4)))throw std::runtime_error("Original frame state mismatch at "+std::to_string(sample));
        }
    }
    std::cout<<"PASS 1400 original frame-setup cases, "<<comparisons<<" bit-exact scalar/state comparisons, "<<instructionCount
      <<" decoded original instructions, zero hooks. Contract: finite F32 / normal FPSCR.\n";
    instructionCount=0;comparisons=0;
    for(std::uint32_t sample=0;sample<1400;++sample){
        memory.clear();memory.zeroRegion(base,OriginalDriveState::byteSize);
        memory.zeroRegion(0x0CAA9880,0x24);
        OriginalDriveState state;
        const auto vehicle=sample%35;
        OriginalLossParameters p;
        p.condition0C9015CC=(sample/35)%6;
        p.normalizedBrake0CAA98A0=randomFloat();
        p.cap0C284F80=memory.readFloat(0x0C284F80+vehicle*8);
        p.growth0C284F84=memory.readFloat(0x0C284F84+vehicle*8);
        OriginalLossState loss{randomFloat(2.0f),randomFloat(200.0f)};
        constexpr std::array<float,7> boundaries{-0.125f,0.0f,0.1f,0.10001f,0.5f,1.0f,1.25f};
        state.setf(0x248,sample%3==0?boundaries[(sample/3)%boundaries.size()]:randomFloat());
        state.setf(0x214,randomFloat(1.5f));
        state.setf(0x258,randomFloat(4)-2);
        state.setf(0x25C,randomFloat(4)-2);
        state.setf(0x1F4,randomFloat(5));
        state.setu(0x1A8,sample%5==0?1:0);
        state.setu(0x1AC,sample%7==0?1:0);
        state.setu(0x43C,sample%11==0?1:0);
        for(std::size_t word=0;word<state.words.size();++word)memory.write32(base+std::uint32_t(word*4),state.words[word]);
        memory.write32(0x0C9015CC,p.condition0C9015CC);
        memory.write32(0x0C901654,vehicle);
        memory.writeFloat(0x0CAA9880,loss.speedLoss0CAA9880);
        memory.writeFloat(0x0CAA9884,loss.persistentPenalty0CAA9884);
        memory.writeFloat(0x0CAA98A0,p.normalizedBrake0CAA98A0);
        RefCpu cpu(memory);cpu.r[0]=base;cpu.r[12]=0x0CAA9884;
        instructionCount+=cpu.run(0x0C15E370,0x0C15E4E6);
        updateOriginalLongitudinalLoss(state,loss,p);
        auto equal=[&](std::uint32_t address,std::uint32_t actual){
            ++comparisons;const auto expected=memory.read32(address);
            if(actual!=expected){
                std::cerr<<"LOSS MISMATCH sample="<<sample<<" vehicle="<<vehicle<<" condition="<<p.condition0C9015CC
                  <<" address=0x"<<std::hex<<address<<" expected=0x"<<expected<<" actual=0x"<<actual<<std::dec
                  <<" expectedFloat="<<std::bit_cast<float>(expected)<<" actualFloat="<<std::bit_cast<float>(actual)<<'\n';
                throw std::runtime_error("Original loss opcode differential failed");
            }
        };
        for(std::size_t word=0;word<state.words.size();++word)equal(base+std::uint32_t(word*4),state.words[word]);
        equal(0x0CAA9880,bits(loss.speedLoss0CAA9880));
        equal(0x0CAA9884,bits(loss.persistentPenalty0CAA9884));
    }
    std::cout<<"PASS 1400 original loss cases, "<<comparisons<<" bit-exact state/global comparisons, "<<instructionCount
      <<" decoded original instructions, zero hooks. Contract: finite F32 / normal FPSCR; complete-car parity not implied.\n";
    instructionCount=0;comparisons=0;
    for(std::uint32_t sample=0;sample<1400;++sample){
        memory.clear();memory.zeroRegion(base,OriginalDriveState::byteSize);
        OriginalDriveState state;
        const auto vehicle=sample%35;
        OriginalAngularParameters p;
        p.carRecord0C283F18_08=memory.readFloat(0x0C283F18+vehicle*44+8);
        p.carRecord0C283F18_0C=memory.readFloat(0x0C283F18+vehicle*44+12);
        state.setf(0x22C,randomFloat(20)-10);state.setf(0x230,randomFloat(20)-10);
        state.setf(0x248,randomFloat(1.5f)-.25f);state.setf(0x1BC,randomFloat(1.5f)-.25f);
        for(std::size_t word=0;word<state.words.size();++word)memory.write32(base+std::uint32_t(word*4),state.words[word]);
        memory.write32(0x0C901654,vehicle);memory.write8(0x0C92ED40,0);
        RefCpu cpu(memory);cpu.r[0]=base;
        instructionCount+=cpu.run(0x0C15D922,0x0C15DA20);
        const float actual=computeOriginalMotionScale(state,p);
        ++comparisons;
        if(bits(actual)!=cpu.fr[6])throw std::runtime_error("Original motion scalar opcode differential failed at case "+std::to_string(sample));
        for(std::size_t word=0;word<state.words.size();++word){
            ++comparisons;if(state.words[word]!=memory.read32(base+std::uint32_t(word*4)))throw std::runtime_error("Unexpected original motion scalar state write");
        }
    }
    std::cout<<"PASS 1400 original motion-scalar cases, "<<comparisons<<" bit-exact scalar/state comparisons, "<<instructionCount
      <<" decoded original instructions, zero hooks. Contract: finite F32 / normal FPSCR.\n";
    instructionCount=0;comparisons=0;
    for(std::uint32_t sample=0;sample<1400;++sample){
        memory.clear();memory.zeroRegion(base,OriginalDriveState::byteSize);
        OriginalDriveState state;
        OriginalRoadParameters road;
        road.condition0C9015CC=sample%14;road.mode0C9015E0=(sample/14)%35;
        road.lastIndex0C283E88=memory.read32(0x0C283E88+road.condition0C9015CC*8);
        std::vector<std::array<float,3>> path(road.lastIndex0C283E88+1);
        constexpr std::uint32_t pathBase=0x0CFE0000;
        for(std::size_t point=0;point<path.size();++point){
            path[point]={float(point)*.125f,0,float(point)*.25f};
            for(std::size_t axis=0;axis<3;++axis)memory.writeFloat(pathBase+std::uint32_t(point*12+axis*4),path[point][axis]);
        }
        road.path0C901728=path;
        for(std::size_t word=0;word<state.words.size();++word)state.setf(word*4,randomFloat(2)-1);
        state.setu(0x118,sample%road.lastIndex0C283E88);
        state.setf(0x000,float(sample%road.lastIndex0C283E88)*.125f+.01f);
        state.setf(0x008,float(sample%road.lastIndex0C283E88)*.25f+.1f);
        state.setf(0x010,randomFloat(20)-10);
        state.setf(0x238,sample%7==0?0.0f:randomFloat(60));state.setf(0x23C,randomFloat(60));
        state.setf(0x248,randomFloat(2));state.setf(0x27C,randomFloat());
        for(const auto off:{0x174,0x178,0x17C,0x180})state.setu(off,randomWord()%300);
        for(const auto off:{0x164,0x168,0x16C,0x170})state.setu(off,randomWord()%256);
        state.setu(0x1A8,(sample/2)%2);state.setu(0x140,(sample/4)%2);state.setu(0x434,(sample/8)%2);
        state.setu(0x144,(sample/16)%130);
        OriginalPreparedControls controls{randomFloat(2)-1,randomFloat(),randomFloat()};
        const float coefficient=150.0f+randomFloat(40);
        for(std::size_t word=0;word<state.words.size();++word)memory.write32(base+std::uint32_t(word*4),state.words[word]);
        memory.write32(0x0C9015CC,road.condition0C9015CC);memory.write32(0x0C9015E0,road.mode0C9015E0);
        memory.write32(0x0C901728,pathBase);memory.write32(0x0C91FB40,0);
        memory.writeFloat(0x0CAA9894,controls.steering);memory.writeFloat(0x0CAA9898,controls.throttle);memory.writeFloat(0x0CAA98A0,controls.brake);
        memory.zeroRegion(0x0CFFF000,128);
        RefCpu cpu(memory);cpu.r[5]=0x0CAA9894;cpu.r[14]=0x0CFFF000;cpu.setFloat(15,coefficient);
        instructionCount+=cpu.run(0x0C15D124,0x0C15D922);
        const auto result=prepareOriginalDriveState(state,controls,coefficient,road,{originalSinF32,originalCosF32,originalFiprDot3});
        for(std::size_t word=0;word<state.words.size();++word){
            ++comparisons;const auto address=base+std::uint32_t(word*4),expected=memory.read32(address),actual=state.words[word];
            if(actual!=expected){
                std::cerr<<"PREPARATION MISMATCH sample="<<sample<<" condition="<<road.condition0C9015CC
                  <<" address=0x"<<std::hex<<address<<" expected=0x"<<expected<<" actual=0x"<<actual<<std::dec
                  <<" expectedFloat="<<std::bit_cast<float>(expected)<<" actualFloat="<<std::bit_cast<float>(actual)<<'\n';
                throw std::runtime_error("Original preparation opcode differential failed");
            }
        }
        comparisons+=3;
        if(bits(controls.throttle)!=memory.read32(0x0CAA9898)||bits(coefficient)!=cpu.fr[15]
            ||memory.read32(0x0C91FB40)!=(result.setServiceFlag0C91FB40?1u:0u))throw std::runtime_error("Original preparation global/register output mismatch");
    }
    std::cout<<"PASS 1400 original preparation cases, "<<comparisons<<" bit-exact state/global/register comparisons, "<<instructionCount
      <<" decoded original instructions. Zero hooks; original trig, FIPR/FSRRA and path-search instructions execute directly.\n";
    instructionCount=0;comparisons=0;
    for(std::uint32_t sample=0;sample<4200;++sample){
        memory.clear();memory.zeroRegion(base,OriginalDriveState::byteSize);
        OriginalDriveState state;
        const auto vehicle=sample%35,condition=(sample/35)%6;
        const auto index=(condition/2)*140+vehicle*4;
        OriginalSteeringMemoryParameters p;
        p.table0C2864D4=memory.readFloat(0x0C2864D4+index);
        p.table0C286EAC=memory.readFloat(0x0C286EAC+index);
        p.table0C287884=memory.readFloat(0x0C287884+index);
        p.table0C2869C0=memory.readFloat(0x0C2869C0+index);
        p.table0C287D70=memory.readFloat(0x0C287D70+index);
        p.global0C90094C=randomFloat(2);
        p.global0C900E30=randomFloat(2);
        p.global0C900EF8=randomFloat(2);
        p.global0C900E4C=randomFloat(2);
        p.global0C900EB0=randomFloat(2);
        p.mask0C900EBC=sample%4;
        p.mask0CAA9CF0=(sample/4)%4;
        p.shiftDownPressed=(sample/16)%2!=0;
        constexpr std::array<float,7> boundaries{-0.125f,0.0f,0.1f,0.10001f,0.5f,1.0f,1.25f};
        state.setf(0x248,sample%3==0?boundaries[(sample/3)%boundaries.size()]:randomFloat());
        state.setf(0x0DC,randomFloat(.04f)-.02f);
        state.setf(0x274,randomFloat(2)-1);
        state.setf(0x278,randomFloat(2)-1);
        state.setf(0x27C,sample%5==0?0.1f:randomFloat());
        state.setf(0x284,randomFloat(160)-80);
        state.setf(0x288,sample%11==0?0:randomFloat(20)-10);
        state.setf(0x1B8,randomFloat());
        state.setf(0x1C4,randomFloat());
        state.setu(0x15C,sample%5==0?1:0);
        state.setu(0x150,sample%7==0?1:0);
        state.setu(0x114,(sample/7)%5);
        for(std::size_t word=0;word<state.words.size();++word)memory.write32(base+std::uint32_t(word*4),state.words[word]);
        memory.write32(0x0C9015CC,condition);memory.write32(0x0C901654,vehicle);
        memory.writeFloat(0x0C90094C,p.global0C90094C);
        memory.writeFloat(0x0C900E30,p.global0C900E30);
        memory.writeFloat(0x0C900EF8,p.global0C900EF8);
        memory.writeFloat(0x0C900E4C,p.global0C900E4C);
        memory.writeFloat(0x0C900EB0,p.global0C900EB0);
        memory.write32(0x0C900EBC,p.mask0C900EBC);
        memory.write32(0x0CAA9CF0,p.mask0CAA9CF0);
        RefCpu cpu(memory);cpu.r[5]=0x248;cpu.r[8]=0x0C90094C;cpu.r[13]=0x0C2864D4;
        cpu.r[3]=vehicle;cpu.r[7]=condition;cpu.setFloat(5,state.f(0x248));
        instructionCount+=cpu.run(p.shiftDownPressed?0x0C15D9EA:0x0C15DA20,0x0C15DEA8);
        updateOriginalSteeringMemory(state,p);
        for(std::size_t word=0;word<state.words.size();++word){
            ++comparisons;const auto address=base+std::uint32_t(word*4),expected=memory.read32(address),actual=state.words[word];
            if(actual!=expected){
                std::cerr<<"STEERING MEMORY MISMATCH sample="<<sample<<" vehicle="<<vehicle<<" condition="<<condition
                  <<" address=0x"<<std::hex<<address<<" expected=0x"<<expected<<" actual=0x"<<actual<<std::dec
                  <<" expectedFloat="<<std::bit_cast<float>(expected)<<" actualFloat="<<std::bit_cast<float>(actual)<<'\n';
                throw std::runtime_error("Original steering memory opcode differential failed");
            }
        }
    }
    std::cout<<"PASS 4200 original steering-memory cases, "<<comparisons<<" bit-exact state comparisons, "<<instructionCount
      <<" decoded original instructions, zero hooks. Contract: finite F32 / normal FPSCR.\n";
    instructionCount=0;comparisons=0;
    for(std::uint32_t sample=0;sample<4200;++sample){
        memory.clear();memory.zeroRegion(base,OriginalDriveState::byteSize);
        OriginalDriveState state;
        const auto vehicle=sample%35,condition=(sample/35)%6,index=(condition/2)*140+vehicle*4;
        OriginalAngularParameters p;
        p.table0C285124=memory.readFloat(0x0C285124+index);
        p.table0C285AFC=memory.readFloat(0x0C285AFC+index);
        p.table0C285610=memory.readFloat(0x0C285610+index);
        p.table0C287398=memory.readFloat(0x0C287398+index);
        p.table0C285FE8=memory.readFloat(0x0C285FE8+index);
        p.global0C900E40=randomFloat(2);p.global0C900EC0=randomFloat(2);
        p.global0C8FF380=randomFloat(2);p.global0C900E54=randomFloat(2);
        const float motion=randomFloat(4);
        for(const auto off:{0x0DC,0x0E0})state.setf(off,randomFloat(.04f)-.02f);
        for(const auto off:{0x010,0x108,0x10C,0x110})state.setf(off,randomFloat(20)-10);
        for(const auto off:{0x1BC,0x1C4,0x248,0x27C})state.setf(off,randomFloat(1.5f)-.25f);
        for(const auto off:{0x260,0x264})state.setf(off,randomFloat(2)-1);
        state.setf(0x284,randomFloat(240)-120);
        state.setf(0x1CC,randomFloat(2.5f)-1.25f);
        state.setf(0x274,randomFloat(2)-1);
        state.setu(0x434,(sample/2)%2);state.setu(0x438,(sample/4)%2);
        state.setu(0x114,(sample/7)%5);state.setu(0x148,(sample/8)%2);state.setu(0x140,(sample/16)%2);
        for(std::size_t word=0;word<state.words.size();++word)memory.write32(base+std::uint32_t(word*4),state.words[word]);
        memory.write32(0x0C9015CC,condition);memory.write32(0x0C901654,vehicle);
        memory.writeFloat(0x0C900E40,p.global0C900E40);memory.writeFloat(0x0C900EC0,p.global0C900EC0);
        memory.writeFloat(0x0C8FF380,p.global0C8FF380);memory.writeFloat(0x0C900E54,p.global0C900E54);
        constexpr std::uint32_t stack=0x0CFFF000;
        memory.zeroRegion(stack,64);
        memory.write32(stack+24,stack+4);memory.write32(stack+28,stack+8);
        memory.write32(stack+32,stack+12);memory.write32(stack+36,stack+16);memory.write32(stack+40,stack+20);
        RefCpu cpu(memory);cpu.r[14]=stack;cpu.setFloat(6,motion);
        std::uint32_t feedback=0;float strength=0,feedbackSpeed=0;int services=0;
        cpu.callHooks[0x0C142520]=[&](RefCpu& c){++services;feedback=c.r[5];strength=c.getFloat(4);feedbackSpeed=memory.readFloat(base+0x238)*3.6f;if(c.r[4]!=0)throw std::runtime_error("Unexpected feedback channel");};
        instructionCount+=cpu.run(0x0C15DEA8,0x0C15E370);
        const auto result=updateOriginalAngular(state,p,{originalSinF32,originalCosF32,originalFiprDot3},motion);
        for(std::size_t word=0;word<state.words.size();++word){
            ++comparisons;const auto address=base+std::uint32_t(word*4),expected=memory.read32(address),actual=state.words[word];
            if(actual!=expected){
                std::cerr<<"ANGULAR MISMATCH sample="<<sample<<" vehicle="<<vehicle<<" condition="<<condition
                  <<" address=0x"<<std::hex<<address<<" expected=0x"<<expected<<" actual=0x"<<actual<<std::dec
                  <<" expectedFloat="<<std::bit_cast<float>(expected)<<" actualFloat="<<std::bit_cast<float>(actual)<<'\n';
                throw std::runtime_error("Original angular opcode differential failed");
            }
        }
        comparisons+=4;
        if(services!=1||feedback!=result.feedbackArgument||bits(strength)!=bits(result.feedbackStrength)||bits(feedbackSpeed)!=bits(result.feedbackSpeed))throw std::runtime_error("Original angular service emission mismatch");
    }
    std::cout<<"PASS 4200 original angular cases, "<<comparisons<<" bit-exact state/service comparisons, "<<instructionCount
      <<" decoded original instructions. Captured feedback service hook only; original trig and FIPR execute under verified finite Flycast contract.\n";
    instructionCount=0;comparisons=0;
    for(std::uint32_t sample=0;sample<1400;++sample){
        memory.clear();memory.zeroRegion(base,OriginalDriveState::byteSize);
        OriginalDriveState state;OriginalLossState loss;OriginalTailState tail;OriginalTailInputs inputs;
        for(std::size_t word=0;word<state.words.size();++word)state.setf(word*4,randomFloat(4)-2);
        constexpr std::array<std::uint32_t,8> counts{1,2,4,8,16,32,64,128};
        state.setu(0x1C0,counts[sample%counts.size()]);
        state.setu(0x1A8,(sample/2)%2);state.setu(0x3F0,1+randomWord()%1000);
        state.setf(0x238,sample%9==0?0.0f:sample%9==1?-1.0f:sample%9==2?12.0f:randomFloat(60));
        state.setf(0x1B8,randomFloat());state.setf(0x1C4,randomFloat());
        state.setf(0x108,randomFloat(20)-10);
        for(auto& value:tail.history0CAA98E0)value=randomFloat(40)-20;
        for(auto& value:tail.throttleHistory0CAA99E0)value=randomFloat();
        for(auto& value:tail.steeringHistory0CAA9BE0)value=randomFloat(2)-1;
        tail.counter0CAA9CE4=randomWord();tail.counter0CAA9CE8=randomWord();tail.counter0CAA9CEC=randomWord();
        tail.previousSpeed0CAA9874=randomFloat(60);
        tail.lastNonzeroGear0CAA9888=randomWord()%6;tail.previousGear0CAA988C=randomWord()%6;
        for(auto& word:tail.statistics0C91FB0C)word=randomWord();
        inputs.gear=sample%6;inputs.transmissionFiltered18=randomFloat(10000);inputs.transmissionDelta24=randomFloat(200)-100;
        inputs.priorFiltered0CAA98AC=randomFloat(10000);inputs.gearEnabled=(sample/4)%2!=0;
        inputs.elapsedFrames0C900E84=std::int32_t(randomWord()%720);
        loss.speedLoss0CAA9880=randomFloat(10);loss.persistentPenalty0CAA9884=randomFloat(20);
        for(std::size_t word=0;word<state.words.size();++word)memory.write32(base+std::uint32_t(word*4),state.words[word]);
        for(std::size_t i=0;i<64;++i){memory.writeFloat(0x0CAA98E0+std::uint32_t(i*4),tail.history0CAA98E0[i]);memory.writeFloat(0x0CAA9BE0+std::uint32_t(i*4),tail.steeringHistory0CAA9BE0[i]);}
        for(std::size_t i=0;i<128;++i)memory.writeFloat(0x0CAA99E0+std::uint32_t(i*4),tail.throttleHistory0CAA99E0[i]);
        for(std::size_t i=0;i<16;++i)memory.write32(0x0C91FB0C+std::uint32_t(i*4),tail.statistics0C91FB0C[i]);
        memory.write32(0x0CAA9CE4,tail.counter0CAA9CE4);memory.write32(0x0CAA9CE8,tail.counter0CAA9CE8);memory.write32(0x0CAA9CEC,tail.counter0CAA9CEC);
        memory.writeFloat(0x0CAA9874,tail.previousSpeed0CAA9874);
        memory.write32(0x0CAA9888,tail.lastNonzeroGear0CAA9888);memory.write32(0x0CAA988C,tail.previousGear0CAA988C);
        memory.write32(0x0C900E88,inputs.gear);memory.writeFloat(0x0C900EA0,inputs.transmissionFiltered18);memory.writeFloat(0x0C900EAC,inputs.transmissionDelta24);
        memory.writeFloat(0x0CAA98AC,inputs.priorFiltered0CAA98AC);memory.write32(0x0C900E84,std::bit_cast<std::uint32_t>(inputs.elapsedFrames0C900E84));
        memory.write32(0x0C900954,0x0CFC0000);memory.write32(0x0CFC0050,inputs.gearEnabled?0x8000:0);
        memory.writeFloat(0x0CAA9880,loss.speedLoss0CAA9880);memory.writeFloat(0x0CAA9884,loss.persistentPenalty0CAA9884);
        memory.zeroRegion(0x0CFFF000,256);memory.write32(0x0CFFF040+44,0x0F000000);
        RefCpu cpu(memory);cpu.r[2]=0x17C;cpu.r[14]=0x0CFFF040;cpu.r[15]=0x0CFFF040;
        instructionCount+=cpu.run(0x0C15EA42,0x0F000000);
        finishOriginalDriveState(state,loss,tail,inputs,{originalSinF32,originalCosF32,originalFiprDot3});
        auto equal=[&](std::uint32_t address,std::uint32_t actual){
            ++comparisons;const auto expected=memory.read32(address);
            if(actual!=expected){
                std::cerr<<"TAIL MISMATCH sample="<<sample<<" address=0x"<<std::hex<<address<<" expected=0x"<<expected<<" actual=0x"<<actual<<std::dec
                    <<" expectedFloat="<<std::bit_cast<float>(expected)<<" actualFloat="<<std::bit_cast<float>(actual)<<'\n';
                throw std::runtime_error("Original tail opcode differential failed");
            }
        };
        for(std::size_t word=0;word<state.words.size();++word)equal(base+std::uint32_t(word*4),state.words[word]);
        for(std::size_t i=0;i<64;++i){equal(0x0CAA98E0+std::uint32_t(i*4),bits(tail.history0CAA98E0[i]));equal(0x0CAA9BE0+std::uint32_t(i*4),bits(tail.steeringHistory0CAA9BE0[i]));}
        for(std::size_t i=0;i<128;++i)equal(0x0CAA99E0+std::uint32_t(i*4),bits(tail.throttleHistory0CAA99E0[i]));
        for(std::size_t i=0;i<16;++i)equal(0x0C91FB0C+std::uint32_t(i*4),tail.statistics0C91FB0C[i]);
        equal(0x0CAA9CE4,tail.counter0CAA9CE4);equal(0x0CAA9CE8,tail.counter0CAA9CE8);equal(0x0CAA9CEC,tail.counter0CAA9CEC);
        equal(0x0CAA9CE0,bits(tail.mean0CAA9CE0));equal(0x0CAA9874,bits(tail.previousSpeed0CAA9874));equal(0x0CAA9878,bits(tail.speedDelta0CAA9878));
        equal(0x0CAA98B0,bits(tail.filteredDelta0CAA98B0));equal(0x0CAA9888,tail.lastNonzeroGear0CAA9888);equal(0x0CAA988C,tail.previousGear0CAA988C);
        equal(0x0CAA9880,bits(loss.speedLoss0CAA9880));equal(0x0CAA9884,bits(loss.persistentPenalty0CAA9884));
    }
    std::cout<<"PASS 1400 original tail-through-return cases, "<<comparisons<<" bit-exact state/history/global/statistics comparisons, "<<instructionCount
      <<" decoded original instructions. Zero hooks; original trig and complete called0C15ECE0 execute directly.\n";
    return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
