#include "original_engine_control.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <set>
using namespace idas3::original;
using namespace idas3::reference;
namespace {
constexpr unsigned owner=0xd000000,actor=0xd010000,stack=0xd020000,stop=0xf000000;
std::size_t comparisons=0,instructions=0;
void equal(unsigned actual,unsigned expected,const std::string& what){++comparisons;if(actual!=expected)throw std::runtime_error(what+" native="+hex(actual)+" source="+hex(expected));}
void bind(RefMemory& m,const OriginalEngineControlState& s){
    m.writeFloat(0xc2fb3a4,s.previousThrottle);m.write32(0xc2fb3a8,s.fallingRpmFrames);m.write32(0xc2fb3ac,s.shiftFrames);
    m.write32(0xc2fb3b0,s.previousGear);m.writeFloat(0xc2fb3b4,s.shiftOffset);m.writeFloat(0xc2fb3b8,s.previousRpm);
    m.write32(0xc2fb3bc,s.auxiliaryLevel);m.write32(0xc2fb3c0,s.decayFrames);m.write32(0xc2fb3c4,s.heldVolume);
    m.write32(0xc2fb3c8,s.recoveryFrames);m.write8(0xc2fb3cc,s.decayLatched);m.write32(0xc2fb3d0,s.roadFrames);
    m.write32(0xc2fbb1c,s.backfireFrames);m.write32(0xc2fbb20,s.previousRoadFrames);m.write16(0xca9b4e8,s.backfirePattern);
    for(unsigned i=0;i<4;++i){m.write32(0xca9b4ec+8+i*16,s.lastVolume[i]);m.write32(0xca9b4ec+12+i*16,s.lastPitch[i]);}
}
void compare(RefMemory& m,const OriginalEngineControlState& s){
    const auto integer=[&](unsigned a,int v){equal(std::uint32_t(v),m.read32(a),"state"+hex(a));};
    const auto floating=[&](unsigned a,float v){equal(std::bit_cast<unsigned>(v),m.read32(a),"state"+hex(a));};
    floating(0xc2fb3a4,s.previousThrottle);integer(0xc2fb3a8,s.fallingRpmFrames);integer(0xc2fb3ac,s.shiftFrames);
    integer(0xc2fb3b0,s.previousGear);floating(0xc2fb3b4,s.shiftOffset);floating(0xc2fb3b8,s.previousRpm);
    integer(0xc2fb3bc,s.auxiliaryLevel);integer(0xc2fb3c0,s.decayFrames);integer(0xc2fb3c4,s.heldVolume);
    integer(0xc2fb3c8,s.recoveryFrames);equal(s.decayLatched,m.read8(0xc2fb3cc),"decay latch");integer(0xc2fb3d0,s.roadFrames);
    integer(0xc2fbb1c,s.backfireFrames);integer(0xc2fbb20,s.previousRoadFrames);equal(s.backfirePattern,m.read16(0xca9b4e8),"backfire pattern");
    for(unsigned i=0;i<4;++i){integer(0xca9b4ec+8+i*16,s.lastVolume[i]);integer(0xca9b4ec+12+i*16,s.lastPitch[i]);}
}
}
int main(int argc,char** argv)try{
    if(argc!=3)throw std::invalid_argument("project-root canonical-image required");
    RefMemory m(argv[2]);auto tables=OriginalEngineTables::load(argv[1]);
    // Exercise the complete non-I/O reset suffix with stale race state.
    for(unsigned iteration=0;iteration<32;++iteration){
        m.clear();m.zeroRegion(0xca9b4e0,0x100);OriginalEngineControlState s;
        s.previousThrottle=.8f;s.shiftOffset=-.7f;s.previousRpm=7193.f;s.fallingRpmFrames=9;s.shiftFrames=17;s.previousGear=5;
        s.auxiliaryLevel=100;s.decayFrames=19;s.heldVolume=87;s.recoveryFrames=5;s.decayLatched=false;s.roadFrames=4;
        s.backfireFrames=int(iteration);s.previousRoadFrames=int(iteration%5);s.backfirePattern=std::uint16_t(iteration*1777);
        s.lastVolume={77,113,91,18};s.lastPitch={255,9,73,138};bind(m,s);
        RefCpu cpu(m);instructions+=cpu.run(0xc0c4114,0xc0c4160,200);
        resetOriginalEngineControl(s);compare(m,s);
    }
    m.clear();
    m.zeroRegion(stack,0x10000);
    for(unsigned family=0;family<36;++family){
        const unsigned address=m.read32(0xc2fba84+family*4),descriptor=m.read32(0xc2fb3d4+family*4);
        equal(std::bit_cast<unsigned>(tables.families[family].maximumRpm),m.read32(0xc25cd08+family*24),"RPM table");
        for(unsigned j=0;j<2;++j)equal(tables.families[family].pitchLimits[j],m.read32(m.read32(descriptor+24*j+4)+20),"pitch limit");
        for(unsigned curve=0;curve<4;++curve)for(unsigned sample=0;sample<=512;++sample){
            const float input=float(sample)/512.f;
            RefCpu cpu(m);cpu.r[4]=address;cpu.r[5]=curve;cpu.setFloat(4,input);cpu.r[15]=stack+0xf000;cpu.pr=stop;
            instructions+=cpu.run(0xc0c3b60,stop,200);
            equal(evaluateOriginalEngineCurve(tables.families[family].curves[curve],input),cpu.r[0],"curve"+std::to_string(family)+"/"+std::to_string(curve));
        }
    }
    for(unsigned family=0;family<36;++family)for(int level=0;level<=6;++level)for(unsigned options=0;options<16;++options){
        m.clear();m.zeroRegion(stack,0x10000);m.zeroRegion(0xca9b500,0x100);
        m.write32(0xca9b52c,family);m.write32(0xca9b534,level);m.write8(0xc31ca34,options&3);
        m.write8(0xc31ca3e,(options>>2)&1);m.write8(0xc31ca42,(options>>3)&1);
        RefCpu cpu(m);cpu.r[15]=stack+0xf000;cpu.pr=stop;instructions+=cpu.run(0xc0c3c60,stop,500);
        const auto config=configureOriginalEngine(family,level,options&3,(options>>2)&1,(options>>3)&1);
        equal(config.originalCar,m.read32(0xca9b52c),"original car");
        equal(std::bit_cast<unsigned>(config.pitch0),m.read32(0xca9b540),"pitch gain");
        equal(std::bit_cast<unsigned>(config.volume0),m.read32(0xca9b53c),"volume gain");
        equal(std::bit_cast<unsigned>(config.volumePitch1),m.read32(0xca9b544),"second gain");
        equal(config.auxiliaryLoop,m.read8(0xca9b538),"auxiliary family"+std::to_string(family)+" level"+std::to_string(level));
        equal(config.releaseCue,m.read8(0xca9b539),"release family"+std::to_string(family)+" level"+std::to_string(level)+" options"+std::to_string(options));
        equal(config.backfire,m.read8(0xca9b53a),"backfire family");
    }
    std::size_t frames=0,events=0;std::set<unsigned> eventTypes;
    for(unsigned family=0;family<36;++family)for(unsigned variant=0;variant<3;++variant){
        m.clear();m.zeroRegion(stack,0x10000);m.zeroRegion(owner,0x10000);m.zeroRegion(actor,0x10000);m.zeroRegion(0xca9b4e0,0x100);
        const auto config=configureOriginalEngine(family,variant*2,variant==1?3:0,1,1);
        m.write32(0xca9b52c,config.originalCar);m.write32(0xca9b530,config.family);
        m.writeFloat(0xca9b53c,config.volume0);m.writeFloat(0xca9b540,config.pitch0);m.writeFloat(0xca9b544,config.volumePitch1);
        m.write8(0xca9b538,config.auxiliaryLoop);m.write8(0xca9b539,config.releaseCue);m.write8(0xca9b53a,config.backfire);
        m.write32(0xc900954,actor);m.write32(owner+8,owner+20);m.write32(owner+12,2);
        for(unsigned i=0;i<2;++i)m.write32(owner+20+i*28+24,tables.families[family].pitchLimits[i]);
        OriginalEngineControlState state;bind(m,state);std::uint32_t seed=19337+family*991+variant;m.write32(0xc37c778,seed);
        for(unsigned frame=0;frame<720;++frame){
            OriginalEngineControlInput input;
            const unsigned phase=frame%120;
            input.rpm=phase<12?float(phase)*70.f:phase<80?1000.f+float(phase-12)*110.f:8500.f-float(phase-80)*170.f;
            input.throttle=phase<10?0.f:phase<70?1.f:phase<108?0.f:.5f;
            input.gear=phase<40?1:phase<80?2:phase<100?1:3;
            input.suppressShiftRelease=(frame/120)==4;
            if((frame%36)<16)input.wheelSurface[(frame/36)%4]=6;
            if((frame%36)==17){input.wheelSurface[0]=2;input.wheelSurface[3]=4;}
            input.handles={4,7,11,14};
            for(unsigned i=0;i<4;++i){m.write8(actor+116+i,input.wheelSurface[i]);m.write32(0xca9b4ec+4+i*16,input.handles[i]);}
            m.write8(0xc2f4de0,input.suppressShiftRelease);
            RefCpu cpu(m);cpu.r[4]=owner;cpu.r[5]=input.gear;cpu.setFloat(4,input.rpm);cpu.setFloat(5,input.throttle);cpu.r[15]=stack+0xf000;cpu.pr=stop;
            std::vector<OriginalEngineCommand> original;
            cpu.callHooks[0xc1ed6a0]=[&](RefCpu& r){original.push_back({OriginalEngineCommandTarget::Continuous,r.r[4],r.r[5],std::bit_cast<int>(r.r[6])});};
            cpu.callHooks[0xc1424a0]=[&](RefCpu& r){original.push_back({OriginalEngineCommandTarget::RaceCue1424A0,0,0,std::bit_cast<int>(r.r[4])});};
            cpu.callHooks[0xc1424e0]=[&](RefCpu& r){original.push_back({OriginalEngineCommandTarget::RaceCue1424E0,0,0,std::bit_cast<int>(r.r[4])});};
            cpu.callHooks[0xc2223b8]=[](RefCpu& r){r.fpul=r.r[4]/r.r[5];};
            instructions+=cpu.run(0xc0c4360,stop,3000);
            const auto native=stepOriginalEngineControl(tables,config,state,input,seed);
            try{
                compare(m,state);equal(seed,m.read32(0xc37c778),"shared RNG");
                if(native!=original){
                    for(const auto& e:original)std::cerr<<"source "<<int(e.target)<<" "<<hex(e.handle)<<" "<<hex(e.command)<<" "<<e.value<<'\n';
                    for(const auto& e:native)std::cerr<<"native "<<int(e.target)<<" "<<hex(e.handle)<<" "<<hex(e.command)<<" "<<e.value<<'\n';
                    throw std::runtime_error("Command sequence differs");
                }
            }catch(const std::exception& e){throw std::runtime_error("family"+std::to_string(family)+" variant"+std::to_string(variant)+" frame"+std::to_string(frame)+": "+e.what());}
            ++frames;events+=native.size();for(const auto& e:native)eventTypes.insert(unsigned(e.target)*65536+e.command);
        }
    }
    for(unsigned type:{0x10a5u,0xa6u,0x40a5u,65536u,131072u})if(!eventTypes.contains(type))throw std::runtime_error("Uncovered command family"+hex(type));
    std::cout<<"PASS36 engine families,73872 curve evaluations,4032 configurations,"<<frames<<" sequential frames,"<<events<<" ordered commands,"<<comparisons<<" state/data comparisons,"<<instructions<<" original instructions. Hooks:1ED6A0 output,1424A0/1424E0 cue output,2223B8 integer quotient. RNG and complete0C4360 control flow execute original bytes. This verifies controls, not ICS playback/DSP or integration timing.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
